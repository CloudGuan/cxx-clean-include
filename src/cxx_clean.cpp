//<------------------------------------------------------------------------------
//< @file  : cxx_clean.cpp
//< @author: ������
//< @date  : 2016��1��2��
//< @brief : ʵ��clang����������﷨���йصĸ��ֻ�����
//< Copyright (c) 2016. All rights reserved.
///<------------------------------------------------------------------------------

#include "cxx_clean.h"

#include <sstream>

#include "clang/Lex/HeaderSearch.h"
#include "llvm/Option/ArgList.h"
#include "clang/Basic/Version.h"
#include "clang/Driver/ToolChain.h"
#include "clang/Driver/Driver.h"
#include "clang/Parse/ParseDiagnostic.h"
#include "lib/Driver/ToolChains.h"

#include "tool.h"
#include "vs_project.h"
#include "parsing_cpp.h"
#include "project.h"
#include "html_log.h"

namespace cxxcleantool
{
	// Ԥ����������#define��#if��#else��Ԥ����ؼ��ֱ�Ԥ����ʱʹ�ñ�Ԥ������
	CxxCleanPreprocessor::CxxCleanPreprocessor(ParsingFile *rootFile)
		: m_root(rootFile)
	{
	}

	// �ļ��л�
	void CxxCleanPreprocessor::FileChanged(SourceLocation loc, FileChangeReason reason,
	                                       SrcMgr::CharacteristicKind FileType,
	                                       FileID prevFileID /* = FileID() */)
	{
		// ע�⣺��ΪPPCallbacks::EnterFileʱ��prevFileID����Ч��
		if (reason != PPCallbacks::EnterFile)
		{
			return;
		}

		SourceManager &srcMgr	= m_root->GetSrcMgr();
		FileID curFileID		= srcMgr.getFileID(loc);

		if (curFileID.isInvalid())
		{
			return;
		}

		// ����Ҫע�⣬�е��ļ��ǻᱻFileChanged��©���ģ����ǰ�HeaderSearch::ShouldEnterIncludeFile������Ϊÿ�ζ�����true
		m_root->AddFile(curFileID);

		FileID parentID = srcMgr.getFileID(srcMgr.getIncludeLoc(curFileID));
		if (parentID.isInvalid())
		{
			return;
		}

		if (m_root->IsForceIncluded(curFileID))
		{
			parentID = srcMgr.getMainFileID();
		}

		if (curFileID == parentID)
		{
			return;
		}

		m_root->AddParent(curFileID, parentID);
		m_root->AddInclude(parentID, curFileID);
	}

	// �ļ�������
	void CxxCleanPreprocessor::FileSkipped(const FileEntry &SkippedFile,
	                                       const Token &FilenameTok,
	                                       SrcMgr::CharacteristicKind FileType)
	{
		SourceManager &srcMgr		= m_root->GetSrcMgr();
		FileID curFileID = srcMgr.getFileID(FilenameTok.getLocation());

		m_root->AddFile(curFileID);
	}

	// ����#include
	void CxxCleanPreprocessor::InclusionDirective(SourceLocation HashLoc /*#��λ��*/, const Token &includeTok,
	        StringRef fileName, bool isAngled/*�Ƿ�<>�����������Ǳ�""��Χ*/, CharSourceRange filenameRange,
	        const FileEntry *file, StringRef searchPath, StringRef relativePath, const clang::Module *Imported)
	{
		// ע��������ʱ����-include<c++ͷ�ļ�>ѡ���ȴ���Ҳ�����ͷ�ļ�ʱ��������file��Ч�������ﲻӰ��
		if (nullptr == file)
		{
			return;
		}

		FileID curFileID = m_root->GetSrcMgr().getFileID(HashLoc);
		if (curFileID.isInvalid())
		{
			return;
		}

		SourceRange range(HashLoc, filenameRange.getEnd());

		if (filenameRange.getBegin() == filenameRange.getEnd())
		{
			cxx::log() << "InclusionDirective filenameRange.getBegin() == filenameRange.getEnd()\n";
		}

		m_root->AddIncludeLoc(filenameRange.getBegin(), range);
	}

	// ����꣬��#if defined DEBUG
	void CxxCleanPreprocessor::Defined(const Token &macroName, const MacroDefinition &definition, SourceRange range)
	{
		m_root->UseMacro(macroName.getLocation(), definition, macroName);
	}

	// ����#define
	void CxxCleanPreprocessor::MacroDefined(const Token &macroName, const MacroDirective *direct)
	{
	}

	// �걻#undef
	void CxxCleanPreprocessor::MacroUndefined(const Token &macroName, const MacroDefinition &definition)
	{
		m_root->UseMacro(macroName.getLocation(), definition, macroName);
	}

	// ����չ
	void CxxCleanPreprocessor::MacroExpands(const Token &macroName,
	                                        const MacroDefinition &definition,
	                                        SourceRange range,
	                                        const MacroArgs *args)
	{
		m_root->UseMacro(range.getBegin(), definition, macroName, args);

		/*
		SourceManager &srcMgr = m_root->GetSrcMgr();
		if (srcMgr.isInMainFile(range.getBegin()))
		{
			SourceRange spellingRange(srcMgr.getSpellingLoc(range.getBegin()), srcMgr.getSpellingLoc(range.getEnd()));

			cxx::log() << "<pre>text = " << m_root->GetSourceOfRange(range) << "</pre>\n";
			cxx::log() << "<pre>macroName = " << macroName.getIdentifierInfo()->getName().str() << "</pre>\n";
		}
		*/
	}

	// #ifdef
	void CxxCleanPreprocessor::Ifdef(SourceLocation loc, const Token &macroName, const MacroDefinition &definition)
	{
		m_root->UseMacro(loc, definition, macroName);
	}

	// #ifndef
	void CxxCleanPreprocessor::Ifndef(SourceLocation loc, const Token &macroName, const MacroDefinition &definition)
	{
		m_root->UseMacro(loc, definition, macroName);
	}

	CxxCleanASTVisitor::CxxCleanASTVisitor(ParsingFile *rootFile)
		: m_root(rootFile)
	{
	}

	// ���ڵ��ԣ���ӡ������Ϣ
	void CxxCleanASTVisitor::PrintStmt(Stmt *s)
	{
		SourceLocation loc = s->getLocStart();

		cxx::log() << "<pre>source = " << m_root->DebugRangeText(s->getSourceRange()) << "</pre>\n";
		cxx::log() << "<pre>";
		s->dump(cxx::log());
		cxx::log() << "</pre>";
	}

	// ���ʵ������
	bool CxxCleanASTVisitor::VisitStmt(Stmt *s)
	{
		SourceLocation loc = s->getLocStart();

		/*
		if (m_root->GetSrcMgr().isInMainFile(loc))
		{
			cxx::log() << "<pre>------------ VisitStmt ------------:</pre>\n";
			PrintStmt(s);
		}
		*/

		// �μ���http://clang.llvm.org/doxygen/classStmt.html
		if (isa<CastExpr>(s))
		{
			CastExpr *castExpr = cast<CastExpr>(s);

			QualType castType = castExpr->getType();
			m_root->UseQualType(loc, castType);

			/*
			if (m_root->GetSrcMgr().isInMainFile(loc))
			{
				cxx::log() << "<pre>------------ CastExpr ------------:</pre>\n";
				cxx::log() << "<pre>";
				cxx::log() << castExpr->getCastKindName();
				cxx::log() << "</pre>";
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
				ValueDecl *valueDecl = cast<ValueDecl>(calleeDecl);
				m_root->UseValueDecl(loc, valueDecl);

				{
					//cxx::log() << "<pre>------------ CallExpr: NamedDecl ------------:</pre>\n";
					//PrintStmt(s);
				}
			}
		}
		else if (isa<DeclRefExpr>(s))
		{
			DeclRefExpr *declRefExpr = cast<DeclRefExpr>(s);

			if (declRefExpr->hasQualifier())
			{
				m_root->UseQualifier(loc, declRefExpr->getQualifier());
			}

			m_root->UseValueDecl(loc, declRefExpr->getDecl());
		}
		// ������ǰ��Χȡ��Ա��䣬���磺this->print();
		else if (isa<CXXDependentScopeMemberExpr>(s))
		{
			CXXDependentScopeMemberExpr *expr = cast<CXXDependentScopeMemberExpr>(s);
			m_root->UseQualType(loc, expr->getBaseType());
		}
		// this
		else if (isa<CXXThisExpr>(s))
		{
			CXXThisExpr *expr = cast<CXXThisExpr>(s);
			m_root->UseQualType(loc, expr->getType());
		}
		/// �ṹ���union�ĳ�Ա�����磺X->F��X.F.
		else if (isa<MemberExpr>(s))
		{
			MemberExpr *memberExpr = cast<MemberExpr>(s);
			m_root->UseValueDecl(loc, memberExpr->getMemberDecl());
		}
		// delete���
		else if (isa<CXXDeleteExpr>(s))
		{
			CXXDeleteExpr *expr = cast<CXXDeleteExpr>(s);
			m_root->UseQualType(loc, expr->getDestroyedType());
		}
		// ����ȡԪ����䣬���磺a[0]��4[a]
		else if (isa<ArraySubscriptExpr>(s))
		{
			ArraySubscriptExpr *expr = cast<ArraySubscriptExpr>(s);
			m_root->UseQualType(loc, expr->getType());
		}
		// typeid��䣬���磺typeid(int) or typeid(*obj)
		else if (isa<CXXTypeidExpr>(s))
		{
			CXXTypeidExpr *expr = cast<CXXTypeidExpr>(s);
			m_root->UseQualType(loc, expr->getType());
		}
		// �����
		else if (isa<CXXConstructExpr>(s))
		{
			CXXConstructExpr *cxxConstructExpr = cast<CXXConstructExpr>(s);
			CXXConstructorDecl *decl = cxxConstructExpr->getConstructor();
			if (nullptr == decl)
			{
				// llvm::errs() << "------------ CXXConstructExpr->getConstructor() = null ------------:\n";
				// s->dumpColor();
				return false;
			}

			m_root->UseValueDecl(loc, decl);
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
			cxx::log() << "<pre>------------ havn't support stmt ------------:</pre>\n";
			PrintStmt(s);
		}
		*/

		return true;
	}

	// ���ʺ�������
	bool CxxCleanASTVisitor::VisitFunctionDecl(FunctionDecl *f)
	{
		m_root->UseFuncDecl(f->getLocStart(), f);


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
				FunctionDecl *oldestFunction = nullptr;

				for (FunctionDecl *prevFunc = f->getPreviousDecl(); prevFunc; prevFunc = prevFunc->getPreviousDecl())
				{
					oldestFunction = prevFunc;
				}

				if (nullptr == oldestFunction)
				{
					return true;
				}

				// ���ú���ԭ��
				m_root->UseFuncDecl(f->getLocStart(), oldestFunction);
			}

			// �Ƿ�����ĳ����ĳ�Ա����
			if (isa<CXXMethodDecl>(f))
			{
				CXXMethodDecl *method = cast<CXXMethodDecl>(f);
				if (nullptr == method)
				{
					return false;
				}

				// �����ҵ��ó�Ա����������struct/union/class.
				CXXRecordDecl *record = method->getParent();
				if (nullptr == record)
				{
					return false;
				}

				// ���ö�Ӧ��struct/union/class.
				m_root->UseRecord(f->getLocStart(),	record);
			}
		}

		// ModifyFunc(f);
		return true;
	}

	// ����class��struct��union��enumʱ
	bool CxxCleanASTVisitor::VisitCXXRecordDecl(CXXRecordDecl *r)
	{
		if (!r->hasDefinition())
		{
			return true;
		}

		// �������л���
		for (CXXRecordDecl::base_class_iterator itr = r->bases_begin(), end = r->bases_end(); itr != end; ++itr)
		{
			CXXBaseSpecifier &base = *itr;
			m_root->UseQualType(r->getLocStart(), base.getType());
		}

		// ������Ա������ע�⣺������static��Ա������static��Ա��������VisitVarDecl�б����ʵ���
		for (CXXRecordDecl::field_iterator itr = r->field_begin(), end = r->field_end(); itr != end; ++itr)
		{
			FieldDecl *field = *itr;
			m_root->UseValueDecl(r->getLocStart(), field);
		}

		// ��Ա��������Ҫ�������������ΪVisitFunctionDecl������ʳ�Ա����
		return true;
	}

	// �����ֱ�������ʱ�ýӿڱ�����
	bool CxxCleanASTVisitor::VisitVarDecl(VarDecl *var)
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
		m_root->UseVarDecl(var->getLocStart(), var);

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
			m_root->UseVarDecl(var->getLocStart(), prevVar);
		}

		return true;
	}

	// ���磺typedef int A;
	bool CxxCleanASTVisitor::VisitTypedefDecl(clang::TypedefDecl *d)
	{
		// llvm::errs() << "Visiting " << d->getDeclKindName() << " " << d->getName() << "\n";

		m_root->UseQualType(d->getLocStart(), d->getUnderlyingType());
		return true;
	}

	// ���磺namespace A{}
	bool CxxCleanASTVisitor::VisitNamespaceDecl(clang::NamespaceDecl *d)
	{
		m_root->DeclareNamespace(d);
		return true;
	}

	// ���磺using namespace std;
	bool CxxCleanASTVisitor::VisitUsingDirectiveDecl(clang::UsingDirectiveDecl *d)
	{
		m_root->UsingNamespace(d);
		return true;
	}

	// ���磺using std::string;
	bool CxxCleanASTVisitor::VisitUsingDecl(clang::UsingDecl *d)
	{
		m_root->UsingXXX(d);
		return true;
	}

	// ���ʳ�Ա����
	bool CxxCleanASTVisitor::VisitFieldDecl(FieldDecl *decl)
	{
		// m_root->use_var(decl->getLocStart(), decl->getType());
		return true;
	}

	// ��������
	bool CxxCleanASTVisitor::VisitCXXConstructorDecl(CXXConstructorDecl *decl)
	{
		for (auto itr = decl->init_begin(), end = decl->init_end(); itr != end; ++itr)
		{
			CXXCtorInitializer *initializer = *itr;
			if (initializer->isAnyMemberInitializer())
			{
				m_root->UseValueDecl(initializer->getSourceLocation(), initializer->getAnyMember());
			}
			else if (initializer->isBaseInitializer())
			{
				m_root->UseType(initializer->getSourceLocation(), initializer->getBaseClass());
			}
			else if (initializer->isDelegatingInitializer())
			{
				if (initializer->getTypeSourceInfo())
				{
					m_root->UseQualType(initializer->getSourceLocation(), initializer->getTypeSourceInfo()->getType());
				}
			}
			else
			{
				// decl->dump();
			}
		}

		return true;
	}

	CxxCleanASTConsumer::CxxCleanASTConsumer(ParsingFile *rootFile)
		: m_root(rootFile), m_visitor(rootFile)
	{}

	// ���ǣ�������������ķ���
	bool CxxCleanASTConsumer::HandleTopLevelDecl(DeclGroupRef declgroup)
	{
		return true;
	}

	// �����������ÿ��Դ�ļ���������һ�Σ����磬����һ��hello.cpp��#include�����ͷ�ļ���Ҳֻ�����һ�α�����
	void CxxCleanASTConsumer::HandleTranslationUnit(ASTContext& context)
	{
		/*
		cxx::log() << "<pre>------------ HandleTranslationUnit ------------:</pre>\n";
		cxx::log() << "<pre>";
		context.getTranslationUnitDecl()->dump(cxx::log());
		cxx::log() << "</pre>";
		*/

		if (!ProjectHistory::instance.m_isFirst)
		{
			return;
		}

		m_visitor.TraverseDecl(context.getTranslationUnitDecl());
	}

	CxxcleanDiagnosticConsumer::CxxcleanDiagnosticConsumer(DiagnosticOptions *diags)
		: m_log(m_errorTip)
		, TextDiagnosticPrinter(m_log, diags, false)
	{
	}

	void CxxcleanDiagnosticConsumer::Clear()
	{
		m_log.flush();
		m_errorTip.clear();
	}

	void CxxcleanDiagnosticConsumer::BeginSourceFile(const LangOptions &LO, const Preprocessor *PP)
	{
		TextDiagnosticPrinter::BeginSourceFile(LO, PP);

		Clear();

		NumErrors		= 0;
		NumWarnings		= 0;
	}

	void CxxcleanDiagnosticConsumer::EndSourceFile()
	{
		TextDiagnosticPrinter::EndSourceFile();

		CompileErrorHistory &errHistory = ParsingFile::g_atFile->GetCompileErrorHistory();
		errHistory.m_errNum				= NumErrors;
	}

	// �Ƿ������ر������
	bool CxxcleanDiagnosticConsumer::IsFatalError(int errid)
	{
		if (errid == clang::diag::err_expected_lparen_after_type)
		{
			// �����unsigned int(0)���͵Ĵ���
			// int n =  unsigned int(0);
			//          ~~~~~~~~ ^
			// error: expected '(' for function-style cast or type construction
			return true;
		}

		// Ȼ�����ﻹ��һ���������Ҫע��ģ�
		// ����ͼ���ú������ص���ʱ����clang��ֱ�ӱ��������
		// ��������У�
		//     struct B{};
		//     B f(){ return B() }
		// ��������������������
		//     B &b = f();
		// ���������ᱻclang��diag::err_lvalue_reference_bind_to_temporary����
		//     non-const lvalue reference to type 'B' cannot bind to a temporary of type 'B'
		//         B &b = f();
		//            ^   ~~~
		// ����clang������f()����������������ʶ�𲻵�f�����ĵ�����
		// ����������취�ƹ�ȥ��ͨ��ֱ�Ӹ�clang��Դ�룬��llvm\tools\clang\lib\Sema\SemaInit.cppɾ����4296�����if�жϺ���
		//     if (isLValueRef && !(T1Quals.hasConst() && !T1Quals.hasVolatile())) {
		// ���θ��౨��

		return false;
	}

	// ��һ��������ʱ������ô˺�����������������¼�������ʹ����
	void CxxcleanDiagnosticConsumer::HandleDiagnostic(DiagnosticsEngine::Level diagLevel, const Diagnostic &info)
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

		std::string tip = m_errorTip;
		Clear();

		int errNum = errHistory.m_errTips.size() + 1;

		if (diagLevel >= DiagnosticIDs::Fatal || IsFatalError(errId))
		{
			errHistory.m_fatalErrors.insert(errId);

			tip += strtool::get_text(cn_fatal_error_num_tip, strtool::itoa(errNum).c_str(), htmltool::get_number_html(errId).c_str());
		}
		else if (diagLevel >= DiagnosticIDs::Error)
		{
			tip += strtool::get_text(cn_error_num_tip, strtool::itoa(errNum).c_str(), htmltool::get_number_html(errId).c_str());
		}

		errHistory.m_errTips.push_back(tip);
	}

	// ��ʼ�ļ�����
	bool CxxCleanAction::BeginSourceFileAction(CompilerInstance &compiler, StringRef filename)
	{
		++ProjectHistory::instance.g_printFileNo;

		if (ProjectHistory::instance.m_isFirst)
		{
			bool only1Step = !Project::instance.m_need2Step;
			if (only1Step)
			{
				llvm::errs() << "cleaning file: ";
			}
			else
			{
				llvm::errs() << "step 1 of 2. analyze file: ";
			}
		}
		else
		{
			llvm::errs() << "step 2 of 2. cleaning file: ";
		}

		llvm::errs() << ProjectHistory::instance.g_printFileNo
		             << "/" << Project::instance.m_cpps.size() << ". " << filename << " ...\n";

		return true;
	}

	// �����ļ�����
	void CxxCleanAction::EndSourceFileAction()
	{
		// llvm::errs() << "** EndSourceFileAction for: " << srcMgr.getFileEntryForID(srcMgr.getMainFileID())->getName() << "\n";

		// ��2��ʱ����Ҫ�ٷ����ˣ���Ϊ��1���Ѿ���������
		if (ProjectHistory::instance.m_isFirst)
		{
			m_root->GenerateResult();
			m_root->Print();
		}

		bool can_clean	 = false;
		can_clean		|= Project::instance.m_onlyHas1File;;
		can_clean		|= !Project::instance.m_isDeepClean;
		can_clean		|= !ProjectHistory::instance.m_isFirst;

		if (can_clean)
		{
			m_root->Clean();
		}

		delete m_root;
		m_root = nullptr;
	}

	// ���������﷨��������
	std::unique_ptr<ASTConsumer> CxxCleanAction::CreateASTConsumer(CompilerInstance &compiler, StringRef file)
	{
		m_rewriter.setSourceMgr(compiler.getSourceManager(), compiler.getLangOpts());

		m_root = new ParsingFile(m_rewriter, compiler);

		HtmlLog::instance.m_newDiv.Clear();

		compiler.getPreprocessor().addPPCallbacks(llvm::make_unique<CxxCleanPreprocessor>(m_root));
		return llvm::make_unique<CxxCleanASTConsumer>(m_root);
	}

	// ���ڵ��ԣ���ӡ�����ļ�
	void CxxCleanAction::PrintIncludes()
	{
		cxx::log() << "\n////////////////////////////////\n";
		cxx::log() << "print fileinfo_iterator include:\n";
		cxx::log() << "////////////////////////////////\n";

		SourceManager &srcMgr = m_rewriter.getSourceMgr();

		typedef SourceManager::fileinfo_iterator fileinfo_iterator;

		fileinfo_iterator beg = srcMgr.fileinfo_begin();
		fileinfo_iterator end = srcMgr.fileinfo_end();

		for (fileinfo_iterator itr = beg; itr != end; ++itr)
		{
			const FileEntry *fileEntry = itr->first;
			SrcMgr::ContentCache *cache = itr->second;

			//printf("#include = %s\n", fileEntry->getName());
			cxx::log() << "    #include = "<< fileEntry->getName()<< "\n";
		}
	}

	// ���ڵ��ԣ���ӡ���ļ��İ����ļ�
	void CxxCleanAction::PrintTopIncludes()
	{
		cxx::log() << "\n////////////////////////////////\n";
		cxx::log() << "print top getLocalSLocEntry include:\n";
		cxx::log() << "////////////////////////////////\n";

		SourceManager &srcMgr = m_rewriter.getSourceMgr();
		int include_size = srcMgr.local_sloc_entry_size();

		for (int i = 1; i < include_size; i++)
		{
			const SrcMgr::SLocEntry &locEntry = srcMgr.getLocalSLocEntry(i);
			if (!locEntry.isFile())
			{
				continue;
			}

			cxx::log() << "    #include = "<< srcMgr.getFilename(locEntry.getFile().getIncludeLoc()) << "\n";
		}
	}

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
	        cl::desc("verbose level, level can be 0 ~ 5, default is 1, higher level will print more detail"),
	        cl::NotHidden);

	void PrintVersion()
	{
		cxx::log() << "cxx-clean-include version is 1.0\n";
		cxx::log() << clang::getClangToolFullVersion("clang lib version is") << '\n';
	}

	// ����ѡ����������������Ӧ�Ķ�����Ӧ��;�˳��򷵻�true�����򷵻�false
	bool CxxCleanOptionsParser::ParseOptions(int &argc, const char **argv, llvm::cl::OptionCategory &category)
	{
		// ʹ��-helpѡ��ʱ������ӡ�����ߵ�ѡ��
		cl::HideUnrelatedOptions(category);
		cl::SetVersionPrinter(cxxcleantool::PrintVersion);

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

		if (g_printVsConfig)
		{
			Vsproject::instance.Print();
			return false;
		}

		if (g_printProject)
		{
			HtmlLog::instance.BeginLog();
			Project::instance.Print();
			return false;
		}

		if (Project::instance.m_cpps.empty())
		{
			llvm::errs() << "cxx-clean-include: \n    try use -help argument to see more information.\n";
			return 0;
		}

		HtmlLog::instance.BeginLog();

		if (Project::instance.m_verboseLvl >= VerboseLvl_2)
		{
			Project::instance.Print();
		}

		return true;
	}

	// ��ִ���������в�����"--"�ָ���ǰ��������в������������߽���������������в�������clang�����
	// ע�⣺argc��������Ϊ"--"�ָ���ǰ�Ĳ�������
	// ���磺
	//		����ʹ��cxxclean -clean ./hello/ -- -include log.h
	//		��-clean ./hello/���������߽�����-include log.h����clang�����
	FixedCompilationDatabase *CxxCleanOptionsParser::SplitCommandLine(int &argc, const char *const *argv, Twine directory /* = "." */)
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

	// ����vs�����ļ������clang�Ĳ���
	bool CxxCleanOptionsParser::AddCleanVsArgument(const Vsproject &vs, ClangTool &tool)
	{
		if (vs.m_configs.empty())
		{
			return false;
		}

		const VsConfiguration &vsconfig = vs.m_configs[0];

		for (const std::string &dir	: vsconfig.searchDirs)
		{
			std::string arg			= "-I" + dir;

			ArgumentsAdjuster argAdjuster = getInsertArgumentAdjuster(arg.c_str(), ArgumentInsertPosition::BEGIN);
			tool.appendArgumentsAdjuster(argAdjuster);
		}

		for (const std::string &force_include : vsconfig.forceIncludes)
		{
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

		AddVsSearchDir(tool);

		pathtool::cd(vs.m_project_dir.c_str());
		return true;
	}

	// ��ȡvisual studio�İ�װ·��
	std::string CxxCleanOptionsParser::GetVsInstallDir()
	{
		std::string vsInstallDir;

#if defined(LLVM_ON_WIN32)
		DiagnosticsEngine engine(
		    IntrusiveRefCntPtr<clang::DiagnosticIDs>(new DiagnosticIDs()), nullptr,
		    nullptr, false);

		clang::driver::Driver d("", "", engine);
		llvm::opt::InputArgList args(nullptr, nullptr);
		toolchains::MSVCToolChain mscv(d, Triple(), args);

		mscv.getVisualStudioInstallDir(vsInstallDir);

		if (!pathtool::exist(vsInstallDir))
		{
			const std::string maybeList[] =
			{
				"C:\\Program Files\\Microsoft Visual Studio 12.0",
				"C:\\Program Files\\Microsoft Visual Studio 11.0",
				"C:\\Program Files\\Microsoft Visual Studio 10.0",
				"C:\\Program Files\\Microsoft Visual Studio 9.0",
				"C:\\Program Files\\Microsoft Visual Studio 8",

				"C:\\Program Files (x86)\\Microsoft Visual Studio 12.0",
				"C:\\Program Files (x86)\\Microsoft Visual Studio 11.0",
				"C:\\Program Files (x86)\\Microsoft Visual Studio 10.0",
				"C:\\Program Files (x86)\\Microsoft Visual Studio 9.0",
				"C:\\Program Files (x86)\\Microsoft Visual Studio 8"
			};

			for (const std::string &maybePath : maybeList)
			{
				if (pathtool::exist(maybePath))
				{
					return maybePath;
				}
			}
		}
#endif

		return vsInstallDir;
	}

	// ���visual studio�Ķ���İ���·������Ϊclang����©��һ������·���ᵼ��#include <atlcomcli.h>ʱ�Ҳ���ͷ�ļ�
	void CxxCleanOptionsParser::AddVsSearchDir(ClangTool &tool)
	{
		if (Vsproject::instance.m_configs.empty())
		{
			return;
		}

		std::string vsInstallDir = GetVsInstallDir();
		if (vsInstallDir.empty())
		{
			return;
		}

		const char* search[] =
		{
			"VC\\atlmfc\\include",
			"VC\\PlatformSDK\\Include",
			"VC\\include"
		};

		for (const char *try_path : search)
		{
			std::string path = pathtool::append_path(vsInstallDir.c_str(), try_path);
			if (!pathtool::exist(path))
			{
				continue;
			}

			std::string arg = "-I" + path;
			tool.appendArgumentsAdjuster(getInsertArgumentAdjuster(arg.c_str(), ArgumentInsertPosition::BEGIN));
		}
	}

	// ����-srcѡ��
	bool CxxCleanOptionsParser::ParseSrcOption()
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

			cxx::init_log(strtool::get_text(cn_cpp_file, pathtool::get_file_name(src).c_str()));
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

			std::string log_file = strtool::replace(src, "/", "_");
			log_file = strtool::replace(src, ".", "_");
			cxx::init_log(strtool::get_text(cn_folder, log_file.c_str()));
		}

		return true;
	}

	// ����-cleanѡ��
	bool CxxCleanOptionsParser::ParseCleanOption()
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

				// cxx::log() << "parse vs project<" << clean_option << "> succeed!\n";

				HtmlLog::instance.SetHtmlTitle(strtool::get_text(cn_project, clean_option.c_str()));
				HtmlLog::instance.SetBigTitle(strtool::get_text(cn_project, htmltool::get_file_html(clean_option).c_str()));

				cxx::init_log(strtool::get_text(cn_project_1, llvm::sys::path::stem(clean_option).str().c_str()));
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

					HtmlLog::instance.SetHtmlTitle(strtool::get_text(cn_folder, clean_option.c_str()));
					HtmlLog::instance.SetBigTitle(strtool::get_text(cn_folder, htmltool::get_file_html(clean_option).c_str()));

					cxx::init_log(strtool::get_text(cn_folder, clean_option.c_str()));
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

		Project::instance.m_need2Step = Project::instance.m_isDeepClean && !Project::instance.m_onlyHas1File && Project::instance.m_isOverWrite;
		return true;
	}

	// ����-vѡ��
	bool CxxCleanOptionsParser::ParseVerboseOption()
	{
		if (g_verbose.getNumOccurrences() == 0)
		{
			Project::instance.m_verboseLvl = VerboseLvl_1;
			return true;
		}

		Project::instance.m_verboseLvl = g_verbose;

		if (g_verbose < 0 || g_verbose > VerboseLvl_Max)
		{
			return false;
		}

		return true;
	}

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
}