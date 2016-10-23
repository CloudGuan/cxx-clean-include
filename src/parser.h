//------------------------------------------------------------------------------
// �ļ�: parser.h
// ����: ������
// ˵��: ������ǰcpp�ļ�
// Copyright (c) 2016 game. All rights reserved.
//------------------------------------------------------------------------------

#ifndef _parser_h_
#define _parser_h_

#include <string>
#include <vector>
#include <set>
#include <map>

#include <clang/Basic/SourceLocation.h>
#include <clang/Basic/SourceManager.h>

#include "history.h"

using namespace std;
using namespace clang;

namespace clang
{
	class HeaderSearch;
	class MacroDefinition;
	class Token;
	class QualType;
	class CXXRecordDecl;
	class NamedDecl;
	class Rewriter;
	class CompilerInstance;
	class NestedNameSpecifier;
	class Type;
	class NamespaceDecl;
	class NamespaceAliasDecl;
	class UsingDirectiveDecl;
	class FunctionDecl;
	class MacroArgs;
	class VarDecl;
	class ValueDecl;
	class RecordDecl;
	class UsingDecl;
	class TemplateArgument;
	class TemplateArgumentList;
	class TemplateDecl;
	class CXXConstructorDecl;
	class DeclContext;
}

namespace cxxclean
{
	// ��ǰ���ڽ�����c++�ļ���Ϣ
	class ParsingFile
	{
		// [�ļ���] -> [·�����ϵͳ·�����û�·��]
		typedef std::map<string, SrcMgr::CharacteristicKind> IncludeDirMap;

		// [�ļ�] -> [�ɱ��滻�����ļ�]
		typedef std::map<FileID, FileID> ChildrenReplaceMap;

		// [�ļ�] -> [���ļ�����Щ�ļ��ɱ��滻]
		typedef std::map<FileID, ChildrenReplaceMap> ReplaceFileMap;

		// [λ��] -> [ʹ�õ�class��struct���û�ָ��]
		typedef std::map<SourceLocation, std::set<const CXXRecordDecl*>> UseRecordsMap;

		// [�ļ�] -> [���ļ���ʹ�õ�class��struct��unionָ�������]
		typedef std::map<FileID, UseRecordsMap> UseRecordsByFileMap;

		// �ļ���
		typedef std::set<FileID> FileSet;

		// ���ڵ��ԣ����õ�����
		struct UseNameInfo
		{
			void AddName(const char* name, int line)
			{
				if (nameMap.find(name) == nameMap.end())
				{
					nameVec.push_back(name);
				}

				nameMap[name].insert(line);
			}

			FileID									file;
			std::vector<string>						nameVec;
			std::map<string, std::set<int>>			nameMap;
		};

		// ����ռ���Ϣ
		struct NamespaceInfo
		{
			NamespaceInfo()
				: ns(nullptr)
			{
			}

			std::string			name;		// �����ռ���������磺namespace A{ namespace B { namespace C {} } }
			const NamespaceDecl	*ns;		// �����ռ�Ķ���
		};

	public:
		// ͷ�ļ�����·��
		struct HeaderSearchDir
		{
			HeaderSearchDir(const string& dir, SrcMgr::CharacteristicKind dirType)
				: m_dir(dir)
				, m_dirType(dirType)
			{
			}

			string						m_dir;
			SrcMgr::CharacteristicKind	m_dirType;
		};

	public:
		ParsingFile(clang::Rewriter &rewriter, clang::CompilerInstance &compiler);

		~ParsingFile();

		inline clang::SourceManager& GetSrcMgr() const { return *m_srcMgr; }

		// ��Ӹ��ļ���ϵ
		void AddParent(FileID child, FileID parent);

		// ��Ӱ����ļ���¼
		void AddInclude(FileID file, FileID beInclude);

		// ���#include��λ�ü�¼
		void AddIncludeLoc(SourceLocation loc, SourceRange range);

		// ��ӳ�Ա�ļ�
		void AddFile(FileID file);

		// Ϊ��ǰcpp�ļ���������ǰ��׼��
		void InitCpp();

		// ��ʼ����
		void Analyze();

		// �Ƿ�����������ļ�
		inline bool HasAnyInclude(FileID a) const;

		// �����ļ����
		int GetDeepth(FileID file) const;

		// �Ƿ�Ϊ��ǰ������������
		bool IsForwardType(const QualType &var);

		// a�ļ�ʹ��b�ļ�
		inline void UseInclude(FileID a, FileID b, const char* name = nullptr, int line = 0);

		// ��ǰλ��ʹ��ָ���ĺ�
		void UseMacro(SourceLocation loc, const MacroDefinition &macro, const Token &macroName, const MacroArgs *args = nullptr);

		// ����ʹ�ñ�����¼
		void UseVarType(SourceLocation loc, const QualType &var);

		// ���ù��캯��
		void UseConstructor(SourceLocation loc, const CXXConstructorDecl *constructor);

		// ���ñ�������
		void UseVarDecl(SourceLocation loc, const VarDecl *var);

		// ���ñ���������Ϊ��ֵ����������ʾ��enum����
		void UseValueDecl(SourceLocation loc, const ValueDecl *valueDecl);

		// ���ô������Ƶ�����
		void UseNameDecl(SourceLocation loc, const NamedDecl *nameDecl);

		// ����ʹ�ú���������¼
		void UseFuncDecl(SourceLocation loc, const FunctionDecl *funcDecl);

		// ����ģ�����
		void UseTemplateArgument(SourceLocation loc, const TemplateArgument &arg);

		// ����ģ������б�
		void UseTemplateArgumentList(SourceLocation loc, const TemplateArgumentList *args);

		// ����ģ�嶨��
		void UseTemplateDecl(SourceLocation loc, const TemplateDecl *decl);

		// ����ʹ��class��struct��union��¼
		void UseRecord(SourceLocation loc, const RecordDecl *record);

		// curλ�õĴ���ʹ��srcλ�õĴ���
		inline void Use(SourceLocation cur, SourceLocation src, const char* name = nullptr);

		// ��ǰλ��ʹ��Ŀ�����ͣ�ע��QualType������ĳ�����͵�const��volatile��static�ȵ����Σ�
		void UseQualType(SourceLocation loc, const QualType &t);

		// ��ǰλ��ʹ��Ŀ�����ͣ�ע��Type����ĳ�����ͣ�������const��volatile��static�ȵ����Σ�
		void UseType(SourceLocation loc, const Type *t);

		// ���������ģ��������ռ�
		void UseContext(SourceLocation loc, const DeclContext*);

		// ����Ƕ���������η�
		void UseQualifier(SourceLocation loc, const NestedNameSpecifier*);

		// ���������ռ�����
		void UseNamespaceDecl(SourceLocation loc, const NamespaceDecl*);

		// ���������ռ����
		void UseNamespaceAliasDecl(SourceLocation loc, const NamespaceAliasDecl*);

		// �����������ռ�
		void DeclareNamespace(const NamespaceDecl *d);

		// using�������ռ䣬���磺using namespace std;
		void UsingNamespace(const UsingDirectiveDecl *d);

		// using�������ռ��µ�ĳ�࣬���磺using std::string;
		void UsingXXX(const UsingDecl *d);

		// ��ȡ�����ռ��ȫ��·�������磬����namespace A{ namespace B{ class C; }}
		std::string GetNestedNamespace(const NamespaceDecl *d);

		// �ļ�b�Ƿ�ֱ��#include�ļ�a
		bool IsIncludedBy(FileID a, FileID b);

		// ��ȡ�ļ�a��ָ�����Ƶ�ֱ�Ӱ����ļ�
		FileID GetIncludeByName(FileID a, const char* includeName) const;

		// ��ȡ�ļ�a��ָ�����Ƶĺ���ļ�
		FileID GetChildByName(FileID a, const char* childFileName) const;

		// ��aʹ��bʱ�����b��Ӧ���ļ���������Σ���b��ͬ���ļ���ѡȡһ����õ��ļ�
		FileID GetBestSameFile(FileID a, FileID b) const;

		// ��ʼ�����ļ������Ķ�c++Դ�ļ���
		void Clean();

		// ����������д��c++Դ�ļ������أ�true��д�ļ�ʱ�������� false��д�ɹ�
		// �����ӿڿ�����Rewriter::overwriteChangedFiles��Ψһ�Ĳ�ͬ�ǻ�д�ɹ�ʱ��ɾ���ļ����棩
		bool Overwrite();

		// ��ӡ���� + 1
		std::string AddPrintIdx() const;

		// ��ӡ��Ϣ
		void Print();

		// ��ȡ���ļ��ı��������ʷ
		CompileErrorHistory& GetCompileErrorHistory() { return m_compileErrorHistory; }

		// ��ȡָ����Χ���ı�
		std::string GetSourceOfRange(SourceRange range) const;

		// ��ȡָ��λ�õ��ı�
		const char* GetSourceAtLoc(SourceLocation loc) const;

		// ��ȡ�÷�ΧԴ�����Ϣ���ı��������ļ������к�
		std::string DebugRangeText(SourceRange range) const;

		// ���ļ��Ƿ��Ǳ�-includeǿ�ư���
		bool IsForceIncluded(FileID file) const;

		// ��ȡ�ļ�����ͨ��clang��ӿڣ��ļ���δ�������������Ǿ���·����Ҳ���������·����
		// ���磺
		//     ���ܷ��أ�d:/hello.h
		//     Ҳ���ܷ��أ�./hello.h
		const char* GetFileName(FileID file) const;

		// ��ȡ�ļ��ľ���·��
		string GetAbsoluteFileName(FileID file) const;

		// ���ڵ��ԣ���ȡ�ļ��ľ���·���������Ϣ
		string GetDebugFileName(FileID file) const;

	private:
		// ��ȡͷ�ļ�����·��
		std::vector<HeaderSearchDir> TakeHeaderSearchPaths(clang::HeaderSearch &headerSearch) const;

		// ��ͷ�ļ�����·�����ݳ����ɳ���������
		std::vector<HeaderSearchDir> SortHeaderSearchPath(const IncludeDirMap& include_dirs_map) const;

		// ָ��λ�õĶ�Ӧ��#include�Ƿ��õ�
		inline bool IsLocBeUsed(SourceLocation loc) const;

		// ���ļ��Ƿ񱻰������
		inline bool HasSameFileByName(const char *file) const;

		// 2���ļ��Ƿ��ļ���һ��
		inline bool IsSameName(FileID a, FileID b) const;

		// ���ļ����Ƿ񱻰������
		inline bool HasSameFile(FileID file) const;

		// �ļ�a�Ƿ�ʹ�õ��ļ�b
		bool IsUse(FileID a, FileID b) const;

		// �ļ�a�Ƿ�ֱ�Ӱ����ļ�b
		bool IsInclude(FileID a, FileID b) const;

		// ��ָ�����ļ��б����ҵ����ڴ����ļ��ĺ��
		FileSet GetChildren(FileID ancestor, FileSet all_children/* ������ancestor���ӵ��ļ� */);

		// ��ȡ�ļ�����ȣ������ļ��ĸ߶�Ϊ0��
		int GetDepth(FileID child) const;

		// ��ȡ�뺢��������Ĺ�ͬ����
		FileID GetCommonAncestor(const FileSet &children) const;

		// ��ȡ2������������Ĺ�ͬ����
		FileID GetCommonAncestor(FileID child_1, FileID child_2) const;

		FileID GetCommonAncestorBySame(FileID a, FileID b) const;

		// ��ȡָ��λ�������е��ı�
		std::string GetSourceOfLine(SourceLocation loc) const;

		/*
			���ݴ���Ĵ���λ�÷��ظ��е����ķ�Χ���������з�����[���п�ͷ������ĩ]
			���磺
				windows��ʽ��
					int			a		=	100;\r\nint b = 0;
					^			^				^
					����			�����λ��		��ĩ

				linux��ʽ��
					int			a		=	100;\n
					^			^				^
					����			�����λ��		��ĩ
		*/
		SourceRange GetCurLine(SourceLocation loc) const;

		/*
			���ݴ���Ĵ���λ�÷��ظ��еķ�Χ���������з�����[���п�ͷ����һ�п�ͷ]
			���磺
				windows��ʽ��
					int			a		=	100;\r\nint b = 0;
					^			^				    ^
					����			�����λ��		    ��ĩ

				linux��ʽ��
					int			a		=	100;\n
					^			^				  ^
					����			�����λ��		  ��ĩ
		*/
		SourceRange GetCurFullLine(SourceLocation loc) const;

		// ���ݴ���Ĵ���λ�÷�����һ�еķ�Χ
		SourceRange GetNextLine(SourceLocation loc) const;

		// �Ƿ�Ϊ����
		bool IsEmptyLine(SourceRange line);

		// ��ȡ�к�
		int GetLineNo(SourceLocation loc) const;

		// ��ȡ�ļ���Ӧ�ı������к�
		int GetIncludeLineNo(FileID) const;

		// ��ȡ�ļ���Ӧ��#include����Χ
		SourceRange GetIncludeRange(FileID) const;

		// �Ƿ��ǻ��з�
		bool IsNewLineWord(SourceLocation loc) const;

		/*
			��ȡ��Ӧ�ڴ����ļ�ID��#include�ı�
			���磺
				������b.cpp����������
					1. #include "./a.h"
					2. #include "a.h"

				���е�1�к͵�2�а�����a.h����Ȼͬһ���ļ�����FileID�ǲ�һ����

				�ִ����һ��a.h��FileID������������
					#include "./a.h"
		*/
		std::string GetIncludeLine(FileID file) const;

		// ��ȡ�ļ���Ӧ��#include���ڵ�����
		std::string GetIncludeFullLine(FileID file) const;

		inline void UseName(FileID file, FileID beusedFile, const char* name = nullptr, int line = 0);

		// ���ݵ�ǰ�ļ������ҵ�2������ȣ���rootΪ��һ�㣩������ǰ�ļ��ĸ��ļ���Ϊ���ļ����򷵻ص�ǰ�ļ�
		FileID GetLvl2Ancestor(FileID file, FileID root) const;

		// ��2���ļ��Ƿ��ǵ�1���ļ������ȣ����ļ������������ļ������ȣ�
		bool IsAncestor(FileID yound, FileID old) const;

		// ��2���ļ��Ƿ��ǵ�1���ļ�������
		bool IsAncestor(FileID yound, const char* old) const;

		// ��2���ļ��Ƿ��ǵ�1���ļ�������
		bool IsAncestor(const char* yound, FileID old) const;

		// �Ƿ�Ϊ�����ļ��Ĺ�ͬ����
		bool IsCommonAncestor(const FileSet &children, FileID old) const;

		// ��ȡ���ļ������ļ�û�и��ļ���
		FileID GetParent(FileID child) const;

		// ��ȡc++��class��struct��union��ȫ������������������ռ�
		// ���磺
		//     ������C��C���������ռ�A�е������ռ�B�����������أ�namespace A{ namespace B{ class C; }}
		string GetRecordName(const RecordDecl &recordDecl) const;

		// ��ָ��λ��������֮���ע�ͣ�ֱ�������һ��token
		bool LexSkipComment(SourceLocation Loc, Token &Result);

		// ����ʹ��ǰ��������¼�����ڲ���Ҫ��ӵ�ǰ����������֮���������
		void UseForward(SourceLocation loc, const CXXRecordDecl *cxxRecordDecl);

		// ��ӡ���ļ��ĸ��ļ�
		void PrintParent();

		// ��ӡ���ļ��ĺ����ļ�
		void PrintChildren();

		// �Ƿ����������c++�ļ������������������ļ����ݲ������κα仯��
		bool CanClean(FileID file) const;

		// ��ȡ���ļ��ı�������Ϣ���������ݰ��������ļ��������ļ����������ļ�#include���кš������ļ�#include��ԭʼ�ı���
		string DebugBeIncludeText(FileID file) const;

		// ��ȡ���ļ��ı�ֱ�Ӱ�����Ϣ���������ݰ��������ļ����������ļ�#include���кš������ļ�#include��ԭʼ�ı���
		string DebugBeDirectIncludeText(FileID file) const;

		// ��ȡ��λ�������е���Ϣ�������е��ı��������ļ������к�
		string DebugLocText(SourceLocation loc) const;

		// ��ȡ�ļ���ʹ��������Ϣ���ļ�������ʹ�õ����������������������Լ���Ӧ�к�
		void DebugUsedNames(FileID file, const std::vector<UseNameInfo> &useNames) const;

		// �Ƿ��б�Ҫ��ӡ���ļ�
		bool IsNeedPrintFile(FileID) const;

		// ��ȡ���ڱ���Ŀ������������ļ���
		int GetCanCleanFileCount() const;

		// ��ȡƴдλ��
		SourceLocation GetSpellingLoc(SourceLocation loc) const;

		// ��ȡ��������չ���λ��
		SourceLocation GetExpasionLoc(SourceLocation loc) const;

		// ��ȡ�ļ�ID
		FileID GetFileID(SourceLocation loc) const;

		// ��ȡ��1���ļ�#include��2���ļ����ı���
		std::string GetRelativeIncludeStr(FileID f1, FileID f2) const;

		// ����ͷ�ļ�����·����������·��ת��Ϊ˫���Ű�Χ���ı�����
		// ���磺������ͷ�ļ�����·��"d:/a/b/c" ��"d:/a/b/c/d/e.h" -> "d/e.h"
		string GetQuotedIncludeStr(const string& absoluteFilePath) const;

		// �滻ָ����Χ�ı�
		void ReplaceText(FileID file, int beg, int end, const char* text);

		// ���ı����뵽ָ��λ��֮ǰ
		// ���磺������"abcdefg"�ı�������c������123�Ľ�����ǣ�"ab123cdefg"
		void InsertText(FileID file, int loc, const char* text);

		// �Ƴ�ָ����Χ�ı������Ƴ��ı�����б�Ϊ���У��򽫸ÿ���һ���Ƴ�
		void RemoveText(FileID file, int beg, int end);

		// �Ƴ�ָ���ļ��ڵ�������
		void CleanByDelLine(const FileHistory &history, FileID file);

		// ��ָ���ļ������ǰ������
		void CleanByForward(const FileHistory &history, FileID file);

		// ��ָ���ļ����滻#include
		void CleanByReplace(const FileHistory &history, FileID file);

		// ��ָ���ļ���������
		void CleanByAdd(const FileHistory &history, FileID file);

		// ������ʷ����ָ���ļ�
		void CleanByHistory(const FileHistoryMap &historys);

		// ���������б�Ҫ������ļ�
		void CleanAllFile();

		// �������ļ�
		void CleanMainFile();

		// ��ӡͷ�ļ�����·��
		void PrintHeaderSearchPath() const;

		// ���ڵ��ԣ���ӡ���ļ����õ��ļ�������ڸ��ļ���#include�ı�
		void PrintRelativeInclude() const;

		// ���ڵ��Ը��٣���ӡ�Ƿ����ļ��ı�����������©
		void PrintNotFoundIncludeLocForDebug();

		// �ļ���ʽ�Ƿ���windows��ʽ�����з�Ϊ[\r\n]����Unix��Ϊ[\n]
		bool IsWindowsFormat(FileID) const;

		void InitHistory(FileID file, FileHistory &history) const;

		// ȡ���Ե�ǰcpp�ļ��ķ������
		void TakeHistorys(FileHistoryMap &out) const;

		// ���ļ��Ƿ���Ԥ����ͷ�ļ�
		bool IsPrecompileHeader(FileID file) const;

		// ȡ�����ļ��ı��������ʷ
		void TakeCompileErrorHistory(FileHistoryMap &out) const;

		// a�ļ��Ƿ���bλ��֮ǰ
		bool IsFileBeforeLoc(FileID a, SourceLocation b) const;

		// ��ӡ���ü�¼
		void PrintUse() const;

		// ��ӡ#include��¼
		void PrintInclude() const;

		// ��ӡ�����������������������ȵļ�¼
		void PrintUsedNames() const;

		// ��ӡ��תΪǰ����������ָ������ü�¼
		void PrintForwardDecl() const;

		// ��ӡ��������������ļ��б�
		void PrintAllFile() const;

		// ��ӡ������־
		void PrintCleanLog() const;

		// ��ӡ���ļ��ڵ������ռ�
		void PrintNamespace() const;

		// ��ӡ���ļ��ڵ�using namespace
		void PrintUsingNamespace() const;

		//================== ģʽ2����Ľӿڣ���project.h�е�CleanMode�� ==================//

		// �������ļ���������ϵ����������ļ��������ļ���
		void GenerateRely();

		// ������Ӹ����������ļ��������ļ�������ֵ��true��ʾ�����ļ��������š�false��ʾ�����ļ��в���
		bool TryAddAncestor();

		// ��¼���ļ��ı���������ļ�
		void GenerateRelyChildren();

		// ���������ļ������ؽ����ʾ�Ƿ�������һЩ��������ļ�����true����false
		bool ExpandRely(FileID top);

		// ��ȡ���ļ��ɱ��滻�����ļ������޷����滻���򷵻ؿ��ļ�id
		FileID GetCanReplaceTo(FileID top) const;

		// ��������#include�ļ�¼
		void GenerateUnusedInclude();

		/*
			��ȡָ���ļ�ֱ�������ͼ���������ļ���

			��������ǣ�
				��ȡtop���������ļ�����ѭ����ȡ��Щ�����ļ��������������ļ���ֱ�����еı������ļ����ѱ�����

			���磨-->��ʾ��������
				hello.cpp --> a.h  --> b.h
				          |
						  --> c.h  --> d.h --> e.h

				���hello.cpp����b.h����b.h������e.h�����ڱ����У�hello.cpp�������ļ��б�Ϊ��
					hello.cpp��b.h��e.h
		*/
		void GetTopRelys(FileID top, FileSet &out) const;

		// ��ǰ�ļ�֮ǰ�Ƿ������ļ������˸�class��struct��union
		bool HasRecordBefore(FileID cur, const CXXRecordDecl &cxxRecord) const;

		// �Ƿ�Ӧ������ǰλ�����õ�class��struct��union��ǰ������
		bool IsNeedClass(SourceLocation, const CXXRecordDecl &cxxRecord) const;

		// �����ļ��滻�б�
		void GenerateReplace();

		// ��������ǰ�������б�
		void GenerateForwardClass();

		// ��������ǰ�������б�
		void GenerateMinForwardClass();

		// ����ǰcpp�ļ������Ĵ������¼��֮ǰ����cpp�ļ������Ĵ������¼�ϲ�
		void MergeTo(FileHistoryMap &old) const;

		// �ϲ��ɱ��Ƴ���#include��
		void MergeUnusedLine(const FileHistory &newFile, FileHistory &oldFile) const;

		// �ϲ���������ǰ������
		void MergeForwardLine(const FileHistory &newFile, FileHistory &oldFile) const;

		// �ϲ����滻��#include
		void MergeReplaceLine(const FileHistory &newFile, FileHistory &oldFile) const;

		// ���ز���ǰ�����������еĿ�ͷ
		SourceLocation GetInsertForwardLine(FileID at, const CXXRecordDecl &cxxRecord) const;

		// ��ĳЩ�ļ��е�һЩ�б��Ϊ�����޸�
		void SkipRelyLines(const FileSkipLineMap&) const;

		// ����������а��ļ����д��
		void TakeUnusedLine(FileHistoryMap &out) const;

		// ��������ǰ���������ļ����д��
		void TakeForwardClass(FileHistoryMap &out) const;

		// ���ļ�ǰ��������¼���ļ����й���
		void SplitForwardByFile(UseRecordsByFileMap&) const;

		// ȡ��ָ���ļ���#include�滻��Ϣ
		void TakeBeReplaceOfFile(FileHistory &history, FileID top, const ChildrenReplaceMap &childernReplaces) const;

		// ȡ�����ļ���#include�滻��Ϣ
		void TakeReplace(FileHistoryMap &out) const;

		// �Ƿ��ֹ�Ķ�ĳ�ļ�
		bool IsSkip(FileID file) const;

		// ���ļ��Ƿ�����
		inline bool IsRely(FileID file) const;

		// ���ļ�������ͬ���ļ��Ƿ�������ͬһ�ļ��ɱ�������Σ�
		bool IsRelyBySameName(FileID file) const;

		// ���ļ��Ƿ����ļ�ѭ�����õ�
		bool IsRelyByTop(FileID file) const;

		// ���ļ��Ƿ�ɱ��滻
		bool IsReplaced(FileID file) const;

		// ��ӡ���ļ��������ļ���
		void PrintTopRely() const;

		// ��ӡ�����ļ���
		void PrintRely() const;

		// ��ӡ���ļ���Ӧ�����ú����ļ���¼
		void PrintRelyChildren() const;


		//================== ģʽ3����Ľӿڣ���project.h�е�CleanMode�� ==================//

		// ���ݵ�ǰ�ļ������ҵ�2������ȣ���rootΪ��һ�㣩������ǰ�ļ��ĸ��ļ���Ϊ���ļ����򷵻ص�ǰ�ļ�
		FileID GetLvl2AncestorBySame(FileID kid, FileID top) const;

		// ��2���ļ��Ƿ��ǵ�1���ļ������ȣ�����ͬ���ļ���
		bool IsAncestorBySame(FileID yound, FileID old) const;

		// ���ļ�������ͬ���ļ��Ƿ�������ͬһ�ļ��ɱ�������Σ�
		bool HasMinKidBySameName(FileID top, FileID kid) const;

		// �Ƿ�Ӧ������λ�����õ�class��struct��union��ǰ������
		bool IsNeedMinClass(SourceLocation, const CXXRecordDecl &cxxRecord) const;

		void GetUseChain(FileID top, FileSet &chain) const;

		bool MergeMin();

		inline bool IsMinUse(FileID a, FileID b) const;

		inline bool IsUserFile(FileID file) const;

		inline bool IsOuterFile(FileID file) const;

		inline FileID GetTopOuterFileAncestor(FileID file) const;

		inline FileID SearchOuterFileAncestor(FileID file) const;

		void GenerateKidBySame();

		void GenerateForceIncludes();

		void GenerateOutFileAncestor();

		void GenerateUserUse();

		void GenerateMinUse();

		void GetKidsBySame(FileID top, FileSet &kids) const;

		// ���ز���ǰ�����������еĿ�ͷ
		SourceLocation GetMinInsertForwardLine(FileID at, const CXXRecordDecl &cxxRecord) const;

		bool HasMinKid(FileID top, FileID kid) const;

		void TakeOneReplace(ReplaceLine &replaceLine, FileID from, FileID to) const;

		// ȡ����¼��ʹ�ø��ļ��������Լ�����Ҫ��ͷ�ļ�
		void TakeNeed(FileID top, FileHistory &out) const;

		// �����ļ��Ƿ�ǿ�ư���
		bool IsAncestorForceInclude(FileID file) const;

		// ��ȡ��ǿ�ư��������ļ�
		FileID GetAncestorForceInclude(FileID file) const;

		// ��ӡ���ļ��ĺ����ļ�
		void PrintUserChildren();

		// ��ӡ
		void PrintSameChildren();

		// ��ӡ��������ε��ļ�
		void PrintSameFile() const;

		// ��ӡ
		void PrintMinUse() const;

		// ��ӡ
		void PrintMinKid() const;

		// ��ӡ
		void PrintOutFileAncestor() const;

		// ��ӡ
		void PrintUserUse() const;

	public:
		// ��ǰ���ڽ������ļ�
		static ParsingFile *g_atFile;

	private:
		//================== ���շ������ ==================//

		// [���շ������]. ������ǰcpp�ļ��Ľ����[c++�ļ���] -> [���ļ���������]
		FileHistoryMap								m_historys;


		//================== ģʽ1��2�����ݣ���project.h�е�CleanMode������ɾ������ͷ�ļ�������������ǰ�������� ==================//

		// ģʽ1���������ȫ��û�õ���#includeλ�ã����ڲ�ѯĳ#include�Ƿ�Ӧ�������֮���Բ�ʹ��FileID��ΪĳЩ�ظ�#include�ᱻ�Զ����������²�δ����FileID��
		std::set<SourceLocation>					m_unusedLocs;

		// ģʽ2����������ɱ��滻��#include�б�[�ļ�ID] -> [���ļ��ɱ��滻Ϊ���ļ�ID]
		std::map<FileID, FileID>					m_replaces;

		// 1. ���ļ��������ļ������ü����Ǳհ��ģ����ü����е���һ�ļ����������ļ����ڼ����ڣ�
		FileSet										m_topRelys;

		// 2. �����ļ������ü����Ǳհ��ģ����ü����е���һ�ļ����������ļ����ڼ����ڣ�
		FileSet										m_relys;

		// 3. ���ļ��ı����õĺ���ļ���[�ļ�ID] -> [����ļ��б�ѭ�����õĲ���]
		std::map<FileID, FileSet>					m_relyChildren;

		// 4. ȫ�����õ�#includeλ�ã����ڲ�ѯĳ#include�Ƿ����ã�
		std::set<SourceLocation>					m_usedLocs;

		// 5. ���ļ��пɱ��û���#include�б�[�ļ�ID] -> [���ļ�����Щ�ļ��ɱ��滻��Ϊ��������Щ�ļ�]
		ReplaceFileMap								m_splitReplaces;


		//================== ģʽ3�����ݣ���project.h�е�CleanMode����ÿ���ļ�����ֻ�����Լ��õ����ļ������Զ�����ǰ�������� ==================//

		// ģʽ3������������ļ�����С�����ļ��б�[�ļ�ID] -> [���ļ���Ӧֱ�Ӱ������ļ�ID�б�]
		std::map<FileID, FileSet>					m_min;

		// 1. ���ļ��ĺ���ļ������û��ļ�����[�ļ�ID] -> [���ļ�������ȫ������ļ�ID�����û��ļ���]
		std::map<FileID, FileSet>					m_userChildren;

		// 2. ���ļ��ĺ���ļ����Ѷ�ͬ���ļ����д�����[�ļ�ID] -> [���ļ�������ȫ������ļ�ID���Ѷ�ͬ���ļ����д���]
		std::map<FileID, FileSet>					m_childrenBySame;

		// 3. ���ļ�Ӧ�����ĺ���ļ��б�[�ļ�ID] -> [���ļ�Ӧ�����ĺ���ļ�ID�б�]
		std::map<FileID, FileSet>					m_minKids;

		// 4. ������Ŀ���ļ���������Ŀ���ļ���[�ļ�ID] -> [��Ӧ�������ⲿ�ļ�ID]�����磬������Ŀ�ļ�A����#include <vector>����ô<vector>�ļ������������к���ļ������ȶ���<vector>�ļ���
		std::map<FileID, FileID>					m_outFileAncestor;

		// 5. ��Ŀ���ļ������ù�ϵ��[��Ŀ���ļ�ID] -> [�����õ���Ŀ���ļ�ID�б� + ��Ŀ���ļ�ID�б�]
		std::map<FileID, FileSet>					m_userUses;

		// 6. ��ǿ�ư������ļ�ID�б�
		FileSet										m_forceIncludes;


		//================== һЩͨ�õ���ʱ���� ==================//

		// 1. ���ļ����������ļ��ļ�¼��[�ļ�ID] -> [���õ������ļ��б�]�����磬����A.h�õ���B.h�е�class B������ΪA.h������B.h��
		std::map<FileID, FileSet>					m_uses;

		// 2. ���ļ����������ļ��б�[�ļ�ID] -> [��include���ļ�]
		std::map<FileID, FileSet>					m_includes;

		// 3. �����ļ�ID
		FileSet										m_files;

		// 4. ����#include��λ�ö�Ӧ�ķ�Χ�� [#include "xxxx.h"�е�һ��˫���Ż�<���ŵ�λ��] -> [#include�����ı����ķ�Χ]
		std::map<SourceLocation, SourceRange>		m_includeLocs;

		// 5. ���ļ���ϵ��[�ļ�ID] -> [���ļ�ID]
		std::map<FileID, FileID>					m_parents;

		// 6. ͷ�ļ�����·���б�
		std::vector<HeaderSearchDir>				m_headerSearchPaths;

		// 7. ÿ��λ����ʹ�õ�class��struct��unionָ������õļ�¼����������ǰ��������ע�⣺ֻ��עָ������ã���[λ��] -> [��ʹ�õ�class��struct��unionָ�������]
		UseRecordsMap								m_useRecords;

		// 8. using namespace��¼��[using namespace��λ��] -> [��Ӧ��namespace����]
		map<SourceLocation, const NamespaceDecl*>	m_usingNamespaces;

		// 9. ���ļ��ĺ����[�ļ�ID] -> [���ļ�������ȫ������ļ�ID]
		std::map<FileID, FileSet>					m_children;

		// 10. �����ڴ�ӡ�����ļ���ʹ�õ��������������������ȵ����Ƽ�¼��[�ļ�ID] -> [���ļ���ʹ�õ������ļ��е�����������������������������]
		std::map<FileID, std::vector<UseNameInfo>>	m_useNames;

		// 11. �����ڴ�ӡ�����ļ��������������ռ��¼��[�ļ�] -> [���ļ��ڵ������ռ��¼]
		std::map<FileID, std::set<std::string>>		m_namespaces;

		// 12. ͬһ���ļ�����Ӧ�Ĳ�ͬ�ļ�ID��[�ļ���] -> [ͬ���ļ�ID�б�]
		std::map<std::string, FileSet>				m_sameFiles;

	private:
		clang::Rewriter*							m_rewriter;
		clang::SourceManager*						m_srcMgr;
		clang::CompilerInstance*					m_compiler;

		// ���ļ��ı��������ʷ
		CompileErrorHistory							m_compileErrorHistory;

		// ��ǰ��ӡ��������������־��ӡ
		mutable int									m_printIdx;
	};
}

using namespace cxxclean;

#endif // _parser_h_