///<------------------------------------------------------------------------------
//< @file  : cxx_clean.cpp
//< @author: ������
//< @date  : 2016��1��2��
//< @brief :
//< Copyright (c) 2016. All rights reserved.
///<------------------------------------------------------------------------------

#include <sstream>

#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Tooling/Tooling.h"
#include "clang/Lex/Preprocessor.h"
#include "clang/Lex/HeaderSearch.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/Signals.h"
#include "clang/Basic/Version.h"
#include "clang/Frontend/TextDiagnosticPrinter.h"

#include "tool.h"
#include "vs_project.h"
#include "parsing_cpp.h"
#include "project.h"
#include "html_log.h"

using namespace clang;
using namespace clang::driver;
using namespace clang::tooling;
using namespace llvm;


using namespace std;
using namespace llvm::sys;
using namespace llvm::sys::path;
using namespace llvm::sys::fs;

namespace cxxcleantool
{
	static llvm::cl::OptionCategory g_optionCategory("cxx-clean-include category");

	// Ԥ����������#define��#if��#else��Ԥ����ؼ��ֱ�Ԥ����ʱʹ�ñ�Ԥ������
	class CxxCleanPreprocessor : public PPCallbacks
	{
	public:
		CxxCleanPreprocessor(ParsingFile *mainFile)
			: m_main(mainFile)
		{
		}

	public:
		void FileChanged(SourceLocation loc, FileChangeReason reason,
		                 SrcMgr::CharacteristicKind FileType,
		                 FileID prevFileID = FileID()) override
		{
			if (reason != PPCallbacks::ExitFile)
			{
				// ע�⣺PPCallbacks::EnterFileʱ��prevFileID��Ч���޷���Ч�������Բ��迼��
				return;
			}

			/*
				���ں��������ĺ��壬��a.cpp����#include "b.h"Ϊ��

				��˴���prevFileID����b.h��loc���ʾa.cpp��#include "b.h"���һ��˫���ŵĺ���һ��λ��

				���磺
			        #include "b.h"
			                      ^
			                      loc�����λ��
			*/

			SourceManager &srcMgr		= m_main->GetSrcMgr();

			FileID curFileID = srcMgr.getFileID(loc);
			if (!prevFileID.isValid() || !curFileID.isValid())
			{
				return;
			}

			m_main->AddFile(prevFileID);
			m_main->AddFile(curFileID);

			PresumedLoc presumed_loc	= srcMgr.getPresumedLoc(loc);
			string curFileName			= presumed_loc.getFilename();

			if (curFileName.empty() || nullptr == srcMgr.getFileEntryForID(prevFileID))
			{
				// llvm::outs() << "    now: " << presumed_loc.getFilename() << " line = " << presumed_loc.getLine() << ", exit = " << m_main->get_file_name(prevFileID) << ", loc = " << m_main->debug_loc_text(loc) << "\n";
				return;
			}

			if (curFileName[0] == '<' || *curFileName.rbegin() == '>')
			{
				curFileID = srcMgr.getMainFileID();
			}

			if (prevFileID == curFileID)
			{
				// llvm::outs() << "same file\n";
				return;
			}

			m_main->AddParent(prevFileID, curFileID);
			m_main->AddInclude(curFileID, prevFileID);
		}

		void FileSkipped(const FileEntry &SkippedFile,
		                 const Token &FilenameTok,
		                 SrcMgr::CharacteristicKind FileType) override
		{
		}

		// ����#include
		void InclusionDirective(SourceLocation HashLoc /*#��λ��*/, const Token &includeTok,
		                        StringRef fileName, bool isAngled/*�Ƿ�<>�����������Ǳ�""��Χ*/, CharSourceRange filenameRange,
		                        const FileEntry *file, StringRef searchPath, StringRef relativePath, const clang::Module *Imported) override
		{
			// ע��������ʱ����-include<c++ͷ�ļ�>ѡ���ȴ���Ҳ�����ͷ�ļ�ʱ��������file��Ч�������ﲻӰ��
			if (nullptr == file)
			{
				return;
			}

			FileID curFileID = m_main->GetSrcMgr().getFileID(HashLoc);
			if (curFileID.isInvalid())
			{
				return;
			}

			SourceRange range(HashLoc, filenameRange.getEnd());

			if (filenameRange.getBegin() == filenameRange.getEnd())
			{
				llvm::outs() << "InclusionDirective filenameRange.getBegin() == filenameRange.getEnd()\n";
			}

			m_main->AddIncludeLoc(filenameRange.getBegin(), range);
		}

		// ����꣬��#define DEBUG
		void Defined(const Token &macroName, const MacroDefinition &definition, SourceRange range) override
		{
			m_main->UseMacro(macroName.getLocation(), definition, macroName);
		}

		// ����#define
		void MacroDefined(const Token &macroName, const MacroDirective *direct) override
		{
		}

		// �걻#undef
		void MacroUndefined(const Token &macroName, const MacroDefinition &definition) override
		{
			m_main->UseMacro(macroName.getLocation(), definition, macroName);
		}

		/// \brief Called by Preprocessor::HandleMacroExpandedIdentifier when a
		/// macro invocation is found.
		void MacroExpands(const Token &macroName,
		                  const MacroDefinition &definition,
		                  SourceRange range,
		                  const MacroArgs *args) override
		{
			m_main->UseMacro(range.getBegin(), definition, macroName, args);

			/*
			SourceManager &srcMgr = m_main->GetSrcMgr();
			if (srcMgr.isInMainFile(range.getBegin()))
			{
				SourceRange spellingRange(srcMgr.getSpellingLoc(range.getBegin()), srcMgr.getSpellingLoc(range.getEnd()));

				llvm::outs() << "<pre>text = " << m_main->GetSourceOfRange(range) << "</pre>\n";
				llvm::outs() << "<pre>macroName = " << macroName.getIdentifierInfo()->getName().str() << "</pre>\n";
			}
			*/
		}

		// #ifdef
		void Ifdef(SourceLocation loc, const Token &macroName, const MacroDefinition &definition) override
		{
			m_main->UseMacro(loc, definition, macroName);
		}

		// #ifndef
		void Ifndef(SourceLocation loc, const Token &macroName, const MacroDefinition &definition) override
		{
			m_main->UseMacro(loc, definition, macroName);
		}

		ParsingFile *m_main;
	};

	// ͨ��ʵ��RecursiveASTVisitor���࣬�Զ������c++�����﷨��ʱ�Ĳ���
	class DeclASTVisitor : public RecursiveASTVisitor<DeclASTVisitor>
	{
	public:
		DeclASTVisitor(Rewriter &rewriter, ParsingFile *mainFile)
			: m_rewriter(rewriter), m_main(mainFile)
		{
		}

		void PrintStmt(Stmt *s)
		{
			SourceLocation loc = s->getLocStart();

			llvm::outs() << "<pre>source = " << m_main->DebugRangeText(s->getSourceRange()) << "</pre>\n";
			llvm::outs() << "<pre>";
			s->dump(llvm::outs());
			llvm::outs() << "</pre>";
		}

		// ���ʵ������
		bool VisitStmt(Stmt *s)
		{
			SourceLocation loc = s->getLocStart();

			/*
			if (m_rewriter.getSourceMgr().isInMainFile(loc))
			{
				llvm::outs() << "<pre>------------ VisitStmt ------------:</pre>\n";
				PrintStmt(s);
			}
			*/

			// �μ���http://clang.llvm.org/doxygen/classStmt.html
			if (isa<CastExpr>(s))
			{
				CastExpr *castExpr = cast<CastExpr>(s);

				QualType castType = castExpr->getType();
				m_main->UseQualType(loc, castType);

				/*
				if (m_rewriter.getSourceMgr().isInMainFile(loc))
				{
					llvm::outs() << "<pre>------------ CastExpr ------------:</pre>\n";
					llvm::outs() << "<pre>";
					llvm::outs() << castExpr->getCastKindName();
					llvm::outs() << "</pre>";
				}
				*/

				// ����ע�⣬CastExpr��һ��getSubExpr()��������ʾ�ӱ��ʽ�����˴�����Ҫ����������Ϊ�ӱ��ʽ����ΪStmt��Ȼ�ᱻVisitStmt���ʵ�
			}
			else if (isa<CallExpr>(s))
			{
				CallExpr *callExpr = cast<CallExpr>(s);

				Decl *calleeDecl = callExpr->getCalleeDecl();
				if (NULL == calleeDecl)
				{
					return true;
				}

				if (isa<ValueDecl>(calleeDecl))
				{
					ValueDecl *namedDecl = cast<ValueDecl>(calleeDecl);
					m_main->UseValueDecl(loc, namedDecl);

					{
						//llvm::outs() << "<pre>------------ CallExpr: NamedDecl ------------:</pre>\n";
						//PrintStmt(s);
					}
				}
			}
			else if (isa<DeclRefExpr>(s))
			{
				DeclRefExpr *declRefExpr = cast<DeclRefExpr>(s);
				m_main->UseValueDecl(loc, declRefExpr->getDecl());
			}
			// ������ǰ��Χȡ��Ա��䣬���磺this->print();
			else if (isa<CXXDependentScopeMemberExpr>(s))
			{
				CXXDependentScopeMemberExpr *expr = cast<CXXDependentScopeMemberExpr>(s);
				m_main->UseQualType(loc, expr->getBaseType());
			}
			// this
			else if (isa<CXXThisExpr>(s))
			{
				CXXThisExpr *expr = cast<CXXThisExpr>(s);
				m_main->UseQualType(loc, expr->getType());
			}
			/// �ṹ���union�ĳ�Ա�����磺X->F��X.F.
			else if (isa<MemberExpr>(s))
			{
				MemberExpr *memberExpr = cast<MemberExpr>(s);
				m_main->UseValueDecl(loc, memberExpr->getMemberDecl());
			}
			// delete���
			else if (isa<CXXDeleteExpr>(s))
			{
				CXXDeleteExpr *expr = cast<CXXDeleteExpr>(s);
				m_main->UseQualType(loc, expr->getDestroyedType());
			}
			// ����ȡԪ����䣬���磺a[0]��4[a]
			else if (isa<ArraySubscriptExpr>(s))
			{
				ArraySubscriptExpr *expr = cast<ArraySubscriptExpr>(s);
				m_main->UseQualType(loc, expr->getType());
			}
			// typeid��䣬���磺typeid(int) or typeid(*obj)
			else if (isa<CXXTypeidExpr>(s))
			{
				CXXTypeidExpr *expr = cast<CXXTypeidExpr>(s);
				m_main->UseQualType(loc, expr->getType());
			}
			// �����
			else if (isa<CXXConstructExpr>(s))
			{
				CXXConstructExpr *cxxConstructExpr = cast<CXXConstructExpr>(s);
				CXXConstructorDecl *decl = cxxConstructExpr->getConstructor();
				if (nullptr == decl)
				{
					llvm::errs() << "------------ CXXConstructExpr->getConstructor() = null ------------:\n";
					s->dumpColor();
					return false;
				}

				m_main->UseValueDecl(loc, decl);
			}
			/*
			// ע�⣺������һ��κ���Ҫ�������Ժ���־����

			// ������DeclRefExpr��Ҫ��ʵ����ʱ��֪�����ͣ����磺T::
			else if (isa<DependentScopeDeclRefExpr>(s))
			{
			}
			// return��䣬���磺return 4;��return;
			else if (isa<ReturnStmt>(s))
			{
				// ����ReturnStmt����Ҫ����������Ϊreturn��������Ȼ�ᱻVisitStmt���ʵ�
			}
			// ���ű��ʽ�����磺(1)��(a + b)
			else if (isa<ParenExpr>(s))
			{
				// ����ParenExpr����Ҫ����������Ϊ�����ڵ������Ȼ�ᱻVisitStmt���ʵ�
			}
			// ���ϱ��ʽ���ɲ�ͬ���ʽ��϶���
			else if(isa<CompoundStmt>(s))
			{
				// ����CompoundStmt����Ҫ����������Ϊ���������Ȼ�ᱻVisitStmt���ʵ�
			}
			// ��Ԫ���ʽ�����磺"x + y" or "x <= y"����������δ�����أ���������BinaryOperator���������������أ������ͽ���CXXOperatorCallExpr
			else if (isa<BinaryOperator>(s))
			{
				// ����BinaryOperator����Ҫ����������Ϊ��2���Ӳ���������Ϊ��䱻VisitStmt���ʵ�
				// ����CXXOperatorCallExprҲ����Ҫ����������Ϊ�佫��ΪCallExpr��VisitStmt���ʵ�
			}
			// һԪ���ʽ�����磺����+������-������++���Լ�--,�߼��ǣ�����λȡ��~��ȡ������ַ&��ȡָ����ֵָ*��
			else if (isa<UnaryOperator>(s))
			{
				// ����UnaryOperator����Ҫ����������Ϊ��1���Ӳ���������Ϊ��䱻VisitStmt���ʵ�
			}
			else if (isa<IntegerLiteral>(s))
			{
			}
			// ��Ԫ���ʽ�����������ʽ�����磺 x ? y : z
			else if (isa<ConditionalOperator>(s))
			{
			}
			// for��䣬���磺for(;;){ int i = 0; }
			else if (isa<ForStmt>(s))
			{
			}
			else if (isa<UnaryExprOrTypeTraitExpr>(s) || isa<InitListExpr>(s) || isa<MaterializeTemporaryExpr>(s))
			{
			}
			// ����δ֪���͵Ĺ��캯�������磺�����T(a1)
			// template<typename T, typename A1>
			// inline T make_a(const A1& a1)
			// {
			//     return T(a1);
			// }
			else if (isa<CXXUnresolvedConstructExpr>(s))
			{
			}
			// c++11�Ĵ����չ��䣬�������...ʡ�Ժţ����磺�����static_cast<Types&&>(args)...����PackExpansionExpr
			// template<typename F, typename ...Types>
			// void forward(F f, Types &&...args)
			// {
			//     f(static_cast<Types&&>(args)...);
			// }
			else if (isa<PackExpansionExpr>(s))
			{
			}
			else if (isa<UnresolvedLookupExpr>(s) || isa<CXXBindTemporaryExpr>(s) || isa<ExprWithCleanups>(s))
			{
			}
			else if (isa<ParenListExpr>(s))
			{
			}
			else if (isa<DeclStmt>(s))
			{
			}
			else if (isa<IfStmt>(s) || isa<SwitchStmt>(s) || isa<CXXTryStmt>(s) || isa<CXXCatchStmt>(s) || isa<CXXThrowExpr>(s))
			{
			}
			else if (isa<StringLiteral>(s) || isa<CharacterLiteral>(s) || isa<CXXBoolLiteralExpr>(s) || isa<FloatingLiteral>(s))
			{
			}
			else if (isa<NullStmt>(s))
			{
			}
			else if (isa<CXXDefaultArgExpr>(s))
			{
			}
			//	����c++�ĳ�Ա������䣬���ʿ�������ʽ����ʽ�����磺
			//	struct A
			//	{
			//		int a, b;
			//		int explicitAccess() { return this->a + this->A::b; }
			//		int implicitAccess() { return a + A::b; }
			//	};
			else if (isa<UnresolvedMemberExpr>(s))
			{
			}
			// new�ؼ���
			else if (isa<CXXNewExpr>(s))
			{
				// ע�����ﲻ��Ҫ������֮���CXXConstructExpr����
			}
			else
			{
				llvm::outs() << "<pre>------------ havn't support stmt ------------:</pre>\n";
				PrintStmt(s);
			}
			*/

			return true;
		}

		bool VisitFunctionDecl(FunctionDecl *f)
		{
			// ʶ�𷵻�ֵ����
			{
				// �����ķ���ֵ
				QualType returnType = f->getReturnType();
				m_main->UseVarType(f->getLocStart(), returnType);
			}

			// ʶ��������
			{
				// ���α����������������ù�ϵ
				for (FunctionDecl::param_iterator itr = f->param_begin(), end = f->param_end(); itr != end; ++itr)
				{
					ParmVarDecl *vardecl = *itr;
					QualType vartype = vardecl->getType();
					m_main->UseVarType(f->getLocStart(), vartype);
				}
			}

			/*
				�����ҵ�������ԭ������
				�磺
					1. class Hello
					2.	{
					3.		void print();
					4.	}
					5. void Hello::print() {}

				���5���Ǻ���ʵ�֣�������λ�ڵ�3��
			*/
			{
				/*
					ע�⣺���ຯ�����Է���������������
						int hello();
						int hello();
						int hello();

					�������ע����ĺ�������
				*/
				{
					FunctionDecl *oldestFunc = nullptr;

					for (FunctionDecl *prevFunc = f->getPreviousDecl(); prevFunc; prevFunc = prevFunc->getPreviousDecl())
					{
						oldestFunc = prevFunc;
					}

					if (nullptr == oldestFunc)
					{
						return true;
					}

					// ���ú���ԭ��
					// m_main->on_use_decl(f->getLocStart(), oldestFunc);
				}

				// �Ƿ�����ĳ����ĳ�Ա����
				if (isa<CXXMethodDecl>(f))
				{
					CXXMethodDecl *method = cast<CXXMethodDecl>(f);
					if (nullptr == method)
					{
						return false;
					}

					// ���ö�Ӧ�ķ���
					m_main->UseFuncDecl(f->getLocStart(),	method);

					// �����ҵ��ó�Ա����������struct/union/class.
					CXXRecordDecl *record = method->getParent();
					if (nullptr == record)
					{
						return false;
					}

					// ���ö�Ӧ��struct/union/class.
					m_main->UseRecord(f->getLocStart(),	record);
				}
			}

			// ModifyFunc(f);
			return true;
		}

		// ����class��struct��union��enumʱ
		bool VisitCXXRecordDecl(CXXRecordDecl *r)
		{
			if (!r->hasDefinition())
			{
				return true;
			}

			// �������л���
			for (CXXRecordDecl::base_class_iterator itr = r->bases_begin(), end = r->bases_end(); itr != end; ++itr)
			{
				CXXBaseSpecifier &base = *itr;
				m_main->UseQualType(r->getLocStart(), base.getType());
			}

			// ������Ա������ע�⣺������static��Ա������static��Ա��������VisitVarDecl�б����ʵ���
			for (CXXRecordDecl::field_iterator itr = r->field_begin(), end = r->field_end(); itr != end; ++itr)
			{
				FieldDecl *field = *itr;
				m_main->UseValueDecl(r->getLocStart(), field);
			}

			// ��Ա��������Ҫ�������������ΪVisitFunctionDecl������ʳ�Ա����
			return true;
		}

		// �����ֱ�������ʱ�ýӿڱ�����
		bool VisitVarDecl(VarDecl *var)
		{
			/*
				ע�⣺
					������������
						1. �����β�
						2. �����Ա�ı�������
						3. ���Ա������static���η�

					��:int a�� A a�ȣ����Ѿ���������������class Aģ����ĳ�Ա����color�������

					template<typename T>
					class A
					{
						enum Color
						{
							// constants for file positioning options
							Green,
							Yellow,
							Blue,
							Red
						};

					private:
						static const Color g_color = (Color)2;
					};

					template<typename T>
					const typename A<T>::Color A<T>::g_color;
			*/

			// ���ñ���������
			m_main->UseVarDecl(var->getLocStart(), var);

			// ���static��Ա������֧��ģ����ĳ�Ա��
			if (var->isCXXClassMember())
			{
				/*
					��Ϊstatic��Ա������ʵ�֣���������������ע��Ҳ������isStaticDataMember�������ж�var�Ƿ�Ϊstatic��Ա������
					���磺
							1. class Hello
							2.	{
							3.		static int g_num;
							4.	}
							5. static int Hello::g_num;

					���5�е�����λ�ڵ�3�д�
				*/
				const VarDecl *prevVar = var->getPreviousDecl();
				m_main->UseVarDecl(var->getLocStart(), prevVar);
			}

			return true;
		}

		// ���磺typedef int A;
		bool VisitTypedefDecl(clang::TypedefDecl *d)
		{
			// llvm::errs() << "Visiting " << d->getDeclKindName() << " " << d->getName() << "\n";

			m_main->UseQualType(d->getLocStart(), d->getUnderlyingType());
			return true;
		}

		// ���磺namespace A{}
		bool VisitNamespaceDecl(clang::NamespaceDecl *d)
		{
			m_main->DeclareNamespace(d);
			return true;
		}

		// ���磺using namespace std;
		bool VisitUsingDirectiveDecl(clang::UsingDirectiveDecl *d)
		{
			m_main->UsingNamespace(d);
			return true;
		}

		// ���ʳ�Ա����
		bool VisitFieldDecl(FieldDecl *decl)
		{
			// m_main->use_var(decl->getLocStart(), decl->getType());
			return true;
		}

		bool VisitCXXConversionDecl(CXXConversionDecl *decl)
		{
			return true;
		}

		bool VisitCXXConstructorDecl(CXXConstructorDecl *decl)
		{
			for (auto itr = decl->init_begin(), end = decl->init_end(); itr != end; ++itr)
			{
				CXXCtorInitializer *initializer = *itr;
				if (initializer->isAnyMemberInitializer())
				{
					m_main->UseValueDecl(initializer->getSourceLocation(), initializer->getAnyMember());
				}
				else if (initializer->isBaseInitializer())
				{
					m_main->UseType(initializer->getSourceLocation(), initializer->getBaseClass());
				}
				else
				{
					decl->dump();
				}
			}

			return true;
		}

	private:
		ParsingFile*	m_main;
		Rewriter&	m_rewriter;
	};

	// ����������ʵ��ASTConsumer�ӿ����ڶ�ȡclang���������ɵ�ast�﷨��
	class ListDeclASTConsumer : public ASTConsumer
	{
	public:
		ListDeclASTConsumer(Rewriter &r, ParsingFile *mainFile) : m_main(mainFile), m_visitor(r, mainFile) {}

		// ���ǣ�������������ķ���
		bool HandleTopLevelDecl(DeclGroupRef declgroup) override
		{
			return true;
		}

		// �����������ÿ��Դ�ļ���������һ�Σ����磬����һ��hello.cpp��#include�����ͷ�ļ���Ҳֻ�����һ�α�����
		void HandleTranslationUnit(ASTContext& context) override
		{
			/*
			llvm::outs() << "<pre>------------ HandleTranslationUnit ------------:</pre>\n";
			llvm::outs() << "<pre>";
			context.getTranslationUnitDecl()->dump(llvm::outs());
			llvm::outs() << "</pre>";
			*/

			m_visitor.TraverseDecl(context.getTranslationUnitDecl());
		}

	public:
		ParsingFile*	m_main;
		DeclASTVisitor	m_visitor;
	};

	// `TextDiagnosticPrinter`���Խ�������Ϣ��ӡ�ڿ���̨�ϣ�Ϊ�˵��Է����Ҵ�����������
	class CxxcleanDiagnosticConsumer : public TextDiagnosticPrinter
	{
	public:
		CxxcleanDiagnosticConsumer(DiagnosticOptions *diags)
			: TextDiagnosticPrinter(g_log, diags)
		{
		}

		void Clear()
		{
			g_log.flush();
			g_errorTip.clear();
		}

		void BeginSourceFile(const LangOptions &LO, const Preprocessor *PP) override
		{
			TextDiagnosticPrinter::BeginSourceFile(LO, PP);

			Clear();

			NumErrors		= 0;
			NumWarnings		= 0;
		}

		virtual void EndSourceFile() override
		{
			TextDiagnosticPrinter::EndSourceFile();

			CompileErrorHistory &errHistory = ParsingFile::g_atFile->GetCompileErrorHistory();
			errHistory.m_errNum				= NumErrors;
		}

		// �Ƿ������ر��������ʱ�ò�����
		bool IsFatalError(int errid)
		{
			return (diag::DIAG_START_PARSE < errid && errid < diag::DIAG_START_AST);
		}

		// ��һ��������ʱ������ô˺�����������������¼�������ʹ����
		virtual void HandleDiagnostic(DiagnosticsEngine::Level diagLevel, const Diagnostic &info)
		{
			TextDiagnosticPrinter::HandleDiagnostic(diagLevel, info);

			CompileErrorHistory &errHistory = ParsingFile::g_atFile->GetCompileErrorHistory();

			int errId = info.getID();
			if (errId == diag::fatal_too_many_errors)
			{
				errHistory.m_hasTooManyError = true;
			}

			if (diagLevel < DiagnosticIDs::Error)
			{
				Clear();
				return;
			}

			std::string tip = g_errorTip;
			Clear();

			if (diagLevel >= DiagnosticIDs::Fatal)
			{
				errHistory.m_fatalErrors.insert(errId);

				tip += strtool::get_text(cn_fatal_error_num_tip, htmltool::get_number_html(errId).c_str());
			}
			else if (diagLevel >= DiagnosticIDs::Error)
			{
				tip += strtool::get_text(cn_error_num_tip, htmltool::get_number_html(errId).c_str());
			}

			errHistory.m_errTips.push_back(tip);

		}

		static std::string			g_errorTip;
		static raw_string_ostream	g_log;
	};

	std::string CxxcleanDiagnosticConsumer::g_errorTip;
	raw_string_ostream CxxcleanDiagnosticConsumer::g_log(g_errorTip);

	// ����ClangTool���յ���ÿ��Դ�ļ�������newһ��ListDeclAction
	class ListDeclAction : public ASTFrontendAction
	{
	public:
		ListDeclAction()
		{
		}

		bool BeginSourceFileAction(CompilerInstance &compiler, StringRef filename) override
		{
			if (ProjectHistory::instance.m_isFirst)
			{
				bool only1Step = (!Project::instance.m_isDeepClean || Project::instance.m_onlyHas1File);
				if (only1Step)
				{
					llvm::errs() << "cleaning file: " << filename << " ...\n";
				}
				else
				{
					llvm::errs() << "step 1 of 2. analyze file: " << filename << " ...\n";
				}
			}
			else
			{
				llvm::errs() << "step 2 of 2. cleaning file: " << filename << " ...\n";
			}

			return true;
		}

		void EndSourceFileAction() override
		{
			SourceManager &srcMgr	= m_rewriter.getSourceMgr();
			ASTConsumer &consumer	= this->getCompilerInstance().getASTConsumer();
			ListDeclASTConsumer *c	= (ListDeclASTConsumer*)&consumer;

			// llvm::errs() << "** EndSourceFileAction for: " << srcMgr.getFileEntryForID(srcMgr.getMainFileID())->getName() << "\n";
			// CompilerInstance &compiler = this->getCompilerInstance();

			c->m_main->GenerateResult();

			if (ProjectHistory::instance.m_isFirst)
			{
				ProjectHistory::instance.AddFile(c->m_main);
				c->m_main->Print();
			}

			bool can_clean	 = false;
			can_clean		|= Project::instance.m_onlyHas1File;;
			can_clean		|= !Project::instance.m_isDeepClean;
			can_clean		|= !ProjectHistory::instance.m_isFirst;

			if (can_clean)
			{
				c->m_main->Clean();
			}

			// llvm::errs() << "end clean file: " << c->m_main->get_file_name(srcMgr.getMainFileID()) << "\n";

			// Now emit the rewritten buffer.
			// m_rewriter.getEditBuffer(srcMgr.getMainFileID()).write(llvm::errs());
			delete c->m_main;
		}

		std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &compiler, StringRef file) override
		{
			m_rewriter.setSourceMgr(compiler.getSourceManager(), compiler.getLangOpts());

			ParsingFile *parsingCpp	= new ParsingFile(m_rewriter, compiler);
			parsingCpp->Init();

			HtmlLog::instance.m_newDiv.Clear();

			compiler.getPreprocessor().addPPCallbacks(llvm::make_unique<CxxCleanPreprocessor>(parsingCpp));
			return llvm::make_unique<ListDeclASTConsumer>(m_rewriter, parsingCpp);
		}

	private:
		void PrintIncludes()
		{
			llvm::outs() << "\n////////////////////////////////\n";
			llvm::outs() << "print fileinfo_iterator include:\n";
			llvm::outs() << "////////////////////////////////\n";

			SourceManager &srcMgr = m_rewriter.getSourceMgr();

			typedef SourceManager::fileinfo_iterator fileinfo_iterator;

			fileinfo_iterator beg = srcMgr.fileinfo_begin();
			fileinfo_iterator end = srcMgr.fileinfo_end();

			for (fileinfo_iterator itr = beg; itr != end; ++itr)
			{
				const FileEntry *fileEntry = itr->first;
				SrcMgr::ContentCache *cache = itr->second;

				//printf("#include = %s\n", fileEntry->getName());
				llvm::outs() << "    #include = "<< fileEntry->getName()<< "\n";
			}
		}

		void PrintTopIncludes()
		{
			llvm::outs() << "\n////////////////////////////////\n";
			llvm::outs() << "print top getLocalSLocEntry include:\n";
			llvm::outs() << "////////////////////////////////\n";

			SourceManager &srcMgr = m_rewriter.getSourceMgr();
			int include_size = srcMgr.local_sloc_entry_size();

			for (int i = 1; i < include_size; i++)
			{
				const SrcMgr::SLocEntry &locEntry = srcMgr.getLocalSLocEntry(i);
				if (!locEntry.isFile())
				{
					continue;
				}

				llvm::outs() << "    #include = "<< srcMgr.getFilename(locEntry.getFile().getIncludeLoc()) << "\n";
			}
		}

	private:
		Rewriter m_rewriter;
	};
}

using namespace cxxcleantool;

static cl::opt<string>	g_cleanOption	("clean",
        cl::desc("you can use this option to:\n"
                 "    1. clean directory, for example: -clean ../hello/\n"
                 "    2. clean visual studio project(version 2005 or upper): for example: -clean ./hello.vcproj or -clean ./hello.vcxproj\n"),
        cl::cat(g_optionCategory));

static cl::opt<string>	g_src			("src",
        cl::desc("target c++ source file to be cleaned, must have valid path, if this option was valued, only the target c++ file will be cleaned\n"
                 "this option can be used with -clean option, for example, you can use: \n"
                 "    cxxclean -clean hello.vcproj -src ./hello.cpp\n"
                 "it will clean hello.cpp and using hello.vcproj configuration"),
        cl::cat(g_optionCategory));

static cl::opt<bool>	g_noWriteOption	("no",
        cl::desc("means no overwrite, if this option is checked, all of the c++ file will not be changed"),
        cl::cat(g_optionCategory));

static cl::opt<bool>	g_onlyCleanCpp	("onlycpp",
        cl::desc("only allow clean cpp file(cpp, cc, cxx), don't clean the header file(h, hxx, hh)"),
        cl::cat(g_optionCategory));

static cl::opt<bool>	g_printVsConfig	("print-vs",
        cl::desc("print vs configuration, for example, print header search path, print c++ file list, print predefine macro and so on"),
        cl::cat(g_optionCategory));

static cl::opt<bool>	g_printProject	("print-project",
        cl::desc("print project configuration, for example, print c++ source list to be cleaned, print allowed clean directory or allowed clean c++ file list, and so on"),
        cl::cat(g_optionCategory));

static cl::opt<int>		g_verbose		("v",
        cl::desc("verbose level, level can be 1 ~ 5, default is 1, higher level will print more detail"),
        cl::NotHidden);

// cxx-clean-include�����в���������������clang���CommonOptionParser��ʵ�ֶ���
class CxxCleanOptionsParser
{
public:
	CxxCleanOptionsParser() {}

	// ����ѡ����������������Ӧ�Ķ�����Ӧ��;�˳��򷵻�true�����򷵻�false
	bool ParseOptions(int &argc, const char **argv, llvm::cl::OptionCategory &category)
	{
		m_compilation.reset(CxxCleanOptionsParser::SplitCommandLine(argc, argv));

		cl::ParseCommandLineOptions(argc, argv, nullptr);

		if (!ParseVerboseOption())
		{
			return false;
		}

		if (!ParseSrcOption())
		{
			return false;
		}

		if (!ParseCleanOption())
		{
			return false;
		}

		HtmlLog::instance.BeginLog();

		if (g_printVsConfig)
		{
			Vsproject::instance.Print();
			return false;
		}

		if (g_printProject)
		{
			Project::instance.Print();
			return false;
		}

		if (Project::instance.m_cpps.empty())
		{
			llvm::errs() << "cxx-clean-include: \n    try use -help argument to see more information.\n";
			return 0;
		}

		if (Project::instance.m_verboseLvl >= VerboseLvl_2)
		{
			Project::instance.Print();
		}

		return true;
	}

	// ��ִ���������в�����"--"�ָ���ǰ��������в�������cxx-clean-include����������������в�������clang�����
	// ע�⣺argc��������Ϊ"--"�ָ���ǰ�Ĳ�������
	// ���磺
	//		����ʹ��cxx-clean-include -clean ./hello/ -- -include log.h
	//		��-clean ./hello/���������߽�����-include log.h����clang�����
	static FixedCompilationDatabase *CxxCleanOptionsParser::SplitCommandLine(int &argc, const char *const *argv, Twine directory = ".")
	{
		const char *const *doubleDash = std::find(argv, argv + argc, StringRef("--"));
		if (doubleDash == argv + argc)
		{
			return new FixedCompilationDatabase(directory, std::vector<std::string>());
		}

		std::vector<const char *> commandLine(doubleDash + 1, argv + argc);
		argc = doubleDash - argv;

		std::vector<std::string> strippedArgs;
		strippedArgs.reserve(commandLine.size());

		for (const char * arg : commandLine)
		{
			strippedArgs.push_back(arg);
		}

		return new FixedCompilationDatabase(directory, strippedArgs);
	}

	bool AddCleanVsArgument(const Vsproject &vs, ClangTool &tool)
	{
		if (vs.m_configs.empty())
		{
			return false;
		}

		const VsConfiguration &vsconfig = vs.m_configs[0];

		for (int i = 0, size = vsconfig.searchDirs.size(); i < size; i++)
		{
			const std::string &dir	= vsconfig.searchDirs[i];
			std::string arg			= "-I" + dir;

			ArgumentsAdjuster argAdjuster = getInsertArgumentAdjuster(arg.c_str(), ArgumentInsertPosition::BEGIN);
			tool.appendArgumentsAdjuster(argAdjuster);
		}

		for (int i = 0, size = vsconfig.forceIncludes.size(); i < size; i++)
		{
			const std::string &force_include	= vsconfig.forceIncludes[i];
			std::string arg						= "-include" + force_include;

			ArgumentsAdjuster argAdjuster = getInsertArgumentAdjuster(arg.c_str(), ArgumentInsertPosition::BEGIN);
			tool.appendArgumentsAdjuster(argAdjuster);
		}

		for (auto predefine : vsconfig.preDefines)
		{
			std::string arg = "-D" + predefine;

			ArgumentsAdjuster argAdjuster = getInsertArgumentAdjuster(arg.c_str(), ArgumentInsertPosition::BEGIN);
			tool.appendArgumentsAdjuster(argAdjuster);
		}

		tool.appendArgumentsAdjuster(getInsertArgumentAdjuster("-fms-extensions", ArgumentInsertPosition::BEGIN));
		tool.appendArgumentsAdjuster(getInsertArgumentAdjuster("-fms-compatibility", ArgumentInsertPosition::BEGIN));
		tool.appendArgumentsAdjuster(getInsertArgumentAdjuster("-fms-compatibility-version=18", ArgumentInsertPosition::BEGIN));

		pathtool::cd(vs.m_project_dir.c_str());
		return true;
	}

	// ����-srcѡ��
	bool ParseSrcOption()
	{
		std::string src = g_src;
		if (src.empty())
		{
			return true;
		}

		if (pathtool::exist(src))
		{
			HtmlLog::instance.SetHtmlTitle(strtool::get_text(cn_cpp_file, src.c_str()));
			HtmlLog::instance.SetBigTitle(strtool::get_text(cn_cpp_file, htmltool::get_file_html(src).c_str()));
			m_sourceList.push_back(src);
		}
		else
		{
			bool ok = pathtool::ls(src, m_sourceList);
			if (!ok)
			{
				llvm::errs() << "error: parse argument -src " << src << " failed, not found the c++ files.\n";
				return false;
			}

			HtmlLog::instance.SetHtmlTitle(strtool::get_text(cn_folder, src.c_str()));
			HtmlLog::instance.SetBigTitle(strtool::get_text(cn_folder, htmltool::get_file_html(src).c_str()));
		}

		return true;
	}

	// ����-cleanѡ��
	bool ParseCleanOption()
	{
		Vsproject &vs				= Vsproject::instance;
		Project &project			= Project::instance;

		project.m_isDeepClean		= !g_onlyCleanCpp;
		project.m_isOverWrite		= !g_noWriteOption;
		project.m_workingDir		= pathtool::get_current_path();
		project.m_cpps				= m_sourceList;

		std::string clean_option	= g_cleanOption;

		if (!clean_option.empty())
		{
			const string ext = strtool::get_ext(clean_option);
			if (ext == "vcproj" || ext == "vcxproj")
			{
				if (!vs.ParseVs(clean_option))
				{
					llvm::errs() << "parse vs project<" << clean_option << "> failed!\n";
					return false;
				}

				// llvm::outs() << "parse vs project<" << clean_option << "> succeed!\n";

				HtmlLog::instance.SetHtmlTitle(strtool::get_text(cn_project, clean_option.c_str()));
				HtmlLog::instance.SetBigTitle(strtool::get_text(cn_project, htmltool::get_file_html(clean_option).c_str()));

				vs.TakeSourceListTo(project);
			}
			else if (llvm::sys::fs::is_directory(clean_option))
			{
				std::string folder = pathtool::get_absolute_path(clean_option.c_str());

				if (!strtool::end_with(folder, "/"))
				{
					folder += "/";
				}

				Project::instance.m_allowCleanDir = folder;

				if (project.m_cpps.empty())
				{
					bool ok = pathtool::ls(folder, project.m_cpps);
					if (!ok)
					{
						llvm::errs() << "error: -clean " << folder << " failed!\n";
						return false;
					}

					HtmlLog::instance.SetHtmlTitle(strtool::get_text(cn_project, clean_option.c_str()));
					HtmlLog::instance.SetBigTitle(strtool::get_text(cn_project, htmltool::get_file_html(clean_option).c_str()));
				}
			}
			else
			{
				llvm::errs() << "unsupport parsed <" << clean_option << ">!\n";
				return false;
			}
		}
		else
		{
			project.GenerateAllowCleanList();
		}

		// �Ƴ���c++��׺��Դ�ļ�
		project.Fix();

		if (project.m_cpps.size() == 1)
		{
			project.m_onlyHas1File = true;
		}

		return true;
	}

	// ����-vѡ��
	bool ParseVerboseOption()
	{
		Project::instance.m_verboseLvl = g_verbose;

		if (g_verbose < 0 || g_verbose > VerboseLvl_Max)
		{
			return false;
		}

		if (0 == g_verbose)
		{
			Project::instance.m_verboseLvl = 1;
		}

		return true;
	}

	/// Returns a reference to the loaded compilations database.
	CompilationDatabase &getCompilations() {return *m_compilation;}

	/// Returns a list of source file paths to process.
	std::vector<std::string>& getSourcePathList() {return m_sourceList;}

	static const char *const HelpMessage;

public:
	bool									m_exit;

private:
	std::unique_ptr<CompilationDatabase>	m_compilation;
	std::vector<std::string>				m_sourceList;
};

static cl::extrahelp MoreHelp(
    "\n"
    "\nExample Usage:"
    "\n"
    "\n    there are 2 ways to use cxx-clean-include"
    "\n"
    "\n    1. if your project is msvc project(visual studio 2005 and upper)"
    "\n        if you want to clean the whole vs project: you can use:"
    "\n            cxxclean -clean hello.vcproj -src hello.cpp"
    "\n"
    "\n        if your only want to clean a single c++ file, and use the vs configuration, you can use:"
    "\n            cxxclean -clean hello.vcproj -src hello.cpp"
    "\n"
    "\n    2. if all your c++ file is in a directory\n"
    "\n        if you wan to clean all c++ file in the directory, you can use:"
    "\n            cxxclean -clean ./hello"
    "\n"
    "\n        if your only want to clean a single c++ file, you can use:"
    "\n            cxxclean -clean ./hello -src hello.cpp"
);

static void PrintVersion()
{
	llvm::outs() << "cxx-clean-include version is 1.0\n";
	llvm::outs() << clang::getClangToolFullVersion("clang lib which cxx-clean-include using is") << '\n';
}

int main(int argc, const char **argv)
{
	llvm::sys::PrintStackTraceOnErrorSignal();
	cl::HideUnrelatedOptions(g_optionCategory);	// ʹ��-helpѡ��ʱ������ӡcxx-clean-include���ߵ�ѡ��

	llvm::InitializeNativeTarget();
	llvm::InitializeNativeTargetAsmParser();	// ֧�ֽ���asm

	cl::SetVersionPrinter(PrintVersion);

	CxxCleanOptionsParser optionParser;

	bool ok = optionParser.ParseOptions(argc, argv, cxxcleantool::g_optionCategory);
	if (!ok)
	{
		return 0;
	}

	ClangTool tool(optionParser.getCompilations(), Project::instance.m_cpps);
	tool.clearArgumentsAdjusters();
	tool.appendArgumentsAdjuster(getClangSyntaxOnlyAdjuster());

	optionParser.AddCleanVsArgument(Vsproject::instance, tool);

	tool.appendArgumentsAdjuster(getInsertArgumentAdjuster("-fcxx-exceptions",			ArgumentInsertPosition::BEGIN));
	tool.appendArgumentsAdjuster(getInsertArgumentAdjuster("-Winvalid-source-encoding", ArgumentInsertPosition::BEGIN));
	tool.appendArgumentsAdjuster(getInsertArgumentAdjuster("-Wdeprecated-declarations", ArgumentInsertPosition::BEGIN));
	tool.appendArgumentsAdjuster(getInsertArgumentAdjuster("-nobuiltininc",				ArgumentInsertPosition::BEGIN));	// ��ֹʹ��clang���õ�ͷ�ļ�
	tool.appendArgumentsAdjuster(getInsertArgumentAdjuster("-w",						ArgumentInsertPosition::BEGIN));	// ���þ���
	tool.appendArgumentsAdjuster(getInsertArgumentAdjuster("-ferror-limit=5",			ArgumentInsertPosition::BEGIN));	// ���Ƶ���cpp�����ı�����������������ٱ���

	DiagnosticOptions diagnosticOptions;
	diagnosticOptions.ShowOptionNames = 1;
	tool.setDiagnosticConsumer(new CxxcleanDiagnosticConsumer(&diagnosticOptions));

	std::unique_ptr<FrontendActionFactory> factory = newFrontendActionFactory<cxxcleantool::ListDeclAction>();
	tool.run(factory.get());

	if (Project::instance.m_isDeepClean && !Project::instance.m_onlyHas1File && Project::instance.m_isOverWrite)
	{
		ProjectHistory::instance.m_isFirst = false;

		tool.run(factory.get());
	}

	ProjectHistory::instance.Print();
	return 0;
}