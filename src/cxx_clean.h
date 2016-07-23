///<------------------------------------------------------------------------------
//< @file:   cxx_clean.h
//< @author: ������
//< @date:   2016��2��24��
//< @brief:  ʵ��clang����������﷨���йصĸ��ֻ�����
//< Copyright (c) 2016 game. All rights reserved.
///<------------------------------------------------------------------------------

#ifndef _cxx_clean_h_
#define _cxx_clean_h_

#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Lex/Preprocessor.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Path.h"
#include "clang/Frontend/TextDiagnosticPrinter.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Tooling/Tooling.h"

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
	class ParsingFile;
	class Vsproject;
}

namespace cxxcleantool
{
	static llvm::cl::OptionCategory g_optionCategory("cxx-clean-include category");

	// Ԥ����������#define��#if��#else��Ԥ����ؼ��ֱ�Ԥ����ʱʹ�ñ�Ԥ������
	class CxxCleanPreprocessor : public PPCallbacks
	{
	public:
		explicit CxxCleanPreprocessor(ParsingFile *mainFile);

	public:
		// �ļ��л�
		void FileChanged(SourceLocation loc, FileChangeReason reason,
		                 SrcMgr::CharacteristicKind FileType,
		                 FileID prevFileID = FileID()) override;

		// �ļ�������
		void FileSkipped(const FileEntry &SkippedFile,
		                 const Token &FilenameTok,
		                 SrcMgr::CharacteristicKind FileType) override;

		// ����#include
		void InclusionDirective(SourceLocation HashLoc /*#��λ��*/, const Token &includeTok,
		                        StringRef fileName, bool isAngled/*�Ƿ�<>�����������Ǳ�""��Χ*/, CharSourceRange filenameRange,
		                        const FileEntry *file, StringRef searchPath, StringRef relativePath, const clang::Module *Imported) override;

		// ����꣬��#if defined DEBUG
		void Defined(const Token &macroName, const MacroDefinition &definition, SourceRange range) override;

		// ����#define
		void MacroDefined(const Token &macroName, const MacroDirective *direct) override;

		// �걻#undef
		void MacroUndefined(const Token &macroName, const MacroDefinition &definition) override;

		// ����չ
		void MacroExpands(const Token &macroName,
		                  const MacroDefinition &definition,
		                  SourceRange range,
		                  const MacroArgs *args) override;

		// #ifdef
		void Ifdef(SourceLocation loc, const Token &macroName, const MacroDefinition &definition) override;

		// #ifndef
		void Ifndef(SourceLocation loc, const Token &macroName, const MacroDefinition &definition) override;

		// ��ǰ���ڽ�����cpp�ļ���Ϣ
		ParsingFile *m_root;
	};

	// ͨ��ʵ��RecursiveASTVisitor���࣬�Զ������c++�����﷨��ʱ�Ĳ���
	class CxxCleanASTVisitor : public RecursiveASTVisitor<CxxCleanASTVisitor>
	{
	public:
		explicit CxxCleanASTVisitor(ParsingFile *rootFile);

		// ���ڵ��ԣ���ӡ������Ϣ
		void PrintStmt(Stmt *s);

		// ���ʵ������
		bool VisitStmt(Stmt *s);

		// ���ʺ�������
		bool VisitFunctionDecl(FunctionDecl *f);

		// ����class��struct��union��enumʱ
		bool VisitCXXRecordDecl(CXXRecordDecl *r);

		// �����ֱ�������ʱ�ýӿڱ�����
		bool VisitVarDecl(VarDecl *var);

		// ���磺typedef int A;
		bool VisitTypedefDecl(clang::TypedefDecl *d);

		// ���磺namespace A{}
		bool VisitNamespaceDecl(clang::NamespaceDecl *d);

		// ���磺using namespace std;
		bool VisitUsingDirectiveDecl(clang::UsingDirectiveDecl *d);

		// ���磺using std::string;
		bool VisitUsingDecl(clang::UsingDecl *d);

		// ���ʳ�Ա����
		bool VisitFieldDecl(FieldDecl *decl);

		// ��������
		bool VisitCXXConstructorDecl(CXXConstructorDecl *decl);

	private:
		// ��ǰ���ڽ�����cpp�ļ���Ϣ
		ParsingFile*	m_root;
	};

	// ����������ʵ��ASTConsumer�ӿ����ڶ�ȡclang���������ɵ�ast�﷨��
	class CxxCleanASTConsumer : public ASTConsumer
	{
	public:
		explicit CxxCleanASTConsumer(ParsingFile *rootFile);

		// ���ǣ�������������ķ���
		bool HandleTopLevelDecl(DeclGroupRef declgroup) override;

		// �����������ÿ��Դ�ļ���������һ�Σ����磬����һ��hello.cpp��#include�����ͷ�ļ���Ҳֻ�����һ�α�����
		void HandleTranslationUnit(ASTContext& context) override;

	public:
		// ��ǰ���ڽ�����cpp�ļ���Ϣ
		ParsingFile*		m_root;

		// �����﷨��������
		CxxCleanASTVisitor	m_visitor;
	};

	// `TextDiagnosticPrinter`���Խ�������Ϣ��ӡ�ڿ���̨�ϣ�Ϊ�˵��Է����Ҵ�����������
	class CxxcleanDiagnosticConsumer : public TextDiagnosticPrinter
	{
	public:
		explicit CxxcleanDiagnosticConsumer(DiagnosticOptions *diags);

		void Clear();

		void BeginSourceFile(const LangOptions &LO, const Preprocessor *PP) override;

		virtual void EndSourceFile() override;

		// �Ƿ������ر��������ʱ�ò�����
		bool IsFatalError(int errid);

		// ��һ��������ʱ������ô˺�����������������¼�������ʹ����
		virtual void HandleDiagnostic(DiagnosticsEngine::Level diagLevel, const Diagnostic &info) override;

		std::string			m_errorTip;
		raw_string_ostream	m_log;
	};

	// ����ClangTool���յ���ÿ��cppԴ�ļ�������newһ��CxxCleanAction
	class CxxCleanAction : public ASTFrontendAction
	{
	public:
		CxxCleanAction()
			: m_root(nullptr)
		{
		}

		// ��ʼ�ļ�����
		bool BeginSourceFileAction(CompilerInstance &compiler, StringRef filename) override;

		// �����ļ�����
		void EndSourceFileAction() override;

		// ���������﷨��������
		std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &compiler, StringRef file) override;

	private:
		// ���ڵ��ԣ���ӡ�����ļ�
		void PrintIncludes();

		// ���ڵ��ԣ���ӡ���ļ��İ����ļ�
		void PrintTopIncludes();

	private:
		// �ļ���д�࣬�����޸�c++Դ������
		Rewriter	m_rewriter;

		// ��ǰ���ڽ�����cpp�ļ���Ϣ
		ParsingFile	*m_root;
	};

	// �����ߵ������в���������������clang���CommonOptionParser��ʵ�ֶ���
	class CxxCleanOptionsParser
	{
	public:
		CxxCleanOptionsParser() {}

		// ����ѡ����������������Ӧ�Ķ�����Ӧ��;�˳��򷵻�true�����򷵻�false
		bool ParseOptions(int &argc, const char **argv, llvm::cl::OptionCategory &category);

		// ��ִ���������в�����"--"�ָ���ǰ��������в������������߽���������������в�������clang�����
		// ע�⣺argc��������Ϊ"--"�ָ���ǰ�Ĳ�������
		// ���磺
		//		����ʹ��cxxclean -clean ./hello/ -- -include log.h
		//		��-clean ./hello/���������߽�����-include log.h����clang�����
		static FixedCompilationDatabase *CxxCleanOptionsParser::SplitCommandLine(int &argc, const char *const *argv, Twine directory = ".");

		// ����vs�����ļ������clang�Ĳ���
		bool AddCleanVsArgument(const Vsproject &vs, ClangTool &tool) const;

		// ��ȡvisual studio�İ�װ·��
		std::string GetVsInstallDir() const;

		// ���visual studio�Ķ���İ���·������Ϊclang����©��һ������·���ᵼ��#include <atlcomcli.h>ʱ�Ҳ���ͷ�ļ�
		void AddVsSearchDir(ClangTool &tool) const;

		// ����-srcѡ��
		bool ParseSrcOption();

		// ����-cleanѡ��
		bool ParseCleanOption();

		// ����-vѡ��
		bool ParseVerboseOption();

		CompilationDatabase &getCompilations() const {return *m_compilation;}

		std::vector<std::string>& getSourcePathList() {return m_sourceList;}

	private:
		std::unique_ptr<CompilationDatabase>	m_compilation;
		std::vector<std::string>				m_sourceList;
	};
}

#endif // _cxx_clean_h_