///<------------------------------------------------------------------------------
//< @file:   parsing_cpp.h
//< @author: ������
//< @date:   2016��2��22��
//< @brief:
//< Copyright (c) 2016. All rights reserved.
///<------------------------------------------------------------------------------

#ifndef _parsing_cpp_h_
#define _parsing_cpp_h_

#include <string>
#include <vector>
#include <set>
#include <map>

#include <clang/Basic/SourceLocation.h>
#include <clang/Basic/SourceManager.h>

#include "project_history.h"

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
	class UsingDirectiveDecl;
}

namespace cxxcleantool
{
	// ��ǰ���ڽ�����c++�ļ���Ϣ
	class ParsingFile
	{
		// [�ļ���] -> [·�����ϵͳ·�����û�·��]
		typedef std::map<string, SrcMgr::CharacteristicKind> IncludeDirMap;

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

		struct NamespaceInfo
		{
			std::string ns_decl;		// �����ռ���������磺namespace A{ namespace B { namespace C {} } }
			std::string ns_name;		// �����ռ�����ƣ��磺A::B::C
		};

	public:
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

		// ��ӡ���ļ��ڵĿɱ�ɾ#include��¼
		static void PrintUnusedIncludeOfFiles(const FileHistoryMap &files);

		// ��ӡ���ļ��ڵĿ�����ǰ��������¼
		static void PrintCanForwarddeclOfFiles(const FileHistoryMap &files);

		// ��ӡ���ļ��ڵĿɱ��滻#include��¼
		static void PrintCanReplaceOfFiles(const FileHistoryMap &files);

		// ��ʼ��������
		bool Init();

		// ��Ӹ��ļ���ϵ
		void AddParent(FileID child, FileID parent) { m_parentIDs[child] = parent; }

		// ��Ӱ����ļ���¼
		void AddInclude(FileID file, FileID beInclude) { m_includes[file].insert(beInclude); }

		// ���#include��λ�ü�¼
		void AddIncludeLoc(SourceLocation loc, SourceRange range) { m_includeLocs[loc] = range; }

		// ��ӳ�Ա�ļ�
		void AddFile(FileID file) { m_files.insert(file); }

		inline clang::SourceManager& GetSrcMgr() const { return *m_srcMgr; }

		// ���ɸ��ļ��Ĵ������¼
		void GenerateResult();

		// ��ǰ�ļ�ʹ��Ŀ���ļ�
		void UseInclude(FileID file, FileID beusedFile, const char* name = nullptr, int line = 0);

		// ��ǰλ��ʹ��ָ���ĺ�
		void UseMacro(SourceLocation loc, const MacroDefinition &macro, const Token &macroName);

		// ����ʹ�ñ�����¼
		void UseVar(SourceLocation loc, const QualType &var);

		// ����ʹ��������¼
		void OnUseDecl(SourceLocation loc, const NamedDecl *nameDecl);

		// ����ʹ��class��struct��union��¼
		void OnUseRecord(SourceLocation loc, const CXXRecordDecl *record);

		// curλ�õĴ���ʹ��srcλ�õĴ���
		void Use(SourceLocation cur, SourceLocation src, const char* name = nullptr);
		
		// ��ǰλ��ʹ��Ŀ�����ͣ�ע��QualType������ĳ�����͵�const��volatile��static�ȵ����Σ�
		void UseQualType(SourceLocation loc, const QualType &t);

		// ��ǰλ��ʹ��Ŀ�����ͣ�ע��Type����ĳ�����ͣ�������const��volatile��static�ȵ����Σ�
		void UseType(SourceLocation loc, const Type *t);

		// ��ǰλ��ʹ�ö�λ
		void UseQualifier(SourceLocation loc, const NestedNameSpecifier*);

		// �����������ռ�
		void DeclareNamespace(const NamespaceDecl *d);

		// using�������ռ�
		void UsingNamespace(const UsingDirectiveDecl *d);

		// ��ȡ����ȱʧ��using namespace
		bool GetMissingNamespace(SourceLocation loc, std::map<std::string, std::string> &miss) const;

		// ��ȡ����ȱʧ��using namespace
		bool GetMissingNamespace(SourceLocation topLoc, SourceLocation oldLoc, 
			std::map<std::string, std::string> &frontMiss, std::map<std::string, std::string> &backMiss) const;

		// ��ȡ�����ռ��ȫ��·�������磬����namespace A{ namespace B{ class C; }}
		std::string GetNestedNamespace(const NamespaceDecl *d);

		// ��ʼ�����ļ������Ķ�c++Դ�ļ���
		void Clean();
		
		// ��ӡ���� + 1
		std::string AddPrintIdx() const;

		// ��ӡ��Ϣ
		void Print();
		
		// ����ǰcpp�ļ������Ĵ������¼��֮ǰ����cpp�ļ������Ĵ������¼�ϲ�
		void MergeTo(FileHistoryMap &old) const;

	private:
		// ��ȡͷ�ļ�����·��
		std::vector<HeaderSearchDir> TakeHeaderSearchPaths(clang::HeaderSearch &headerSearch) const;

		// ��ͷ�ļ�����·�����ݳ����ɳ���������
		std::vector<HeaderSearchDir> SortHeaderSearchPath(const IncludeDirMap& include_dirs_map) const;

		// ָ��λ�õĶ�Ӧ��#include�Ƿ��õ�
		inline bool IsLocBeUsed(SourceLocation loc) const;

		/*
			�������ļ�ֱ�������ͼ���������ļ���

			���磺
				���赱ǰ���ڽ���hello.cpp��hello.cpp�������ļ��İ�����ϵ���£���-->��ʾ��������
				hello.cpp --> a.h  --> b.h
				          |
						  --> c.h  --> d.h --> e.h

				���hello.cpp����b.h����b.h������e.h�����ڱ����У��������ɵ������ļ��б�Ϊ��
					hello.cpp��b.h��e.h

		*/
		void GenerateRootCycleUse();

		// �������ļ���������ϵ����������ļ��������ļ���
		void GenerateAllCycleUse();

		// ������Ӹ����������ļ��������ļ�������ֵ��true��ʾ�����ļ��������š�false��ʾ�����ļ��в���
		bool TryAddAncestor();

		// ��¼���ļ��ı���������ļ�
		void GenerateCycleUsedChildren();

		// ���������ļ������ؽ����ʾ�Ƿ�������һЩ��������ļ�����true����false
		bool ExpandAllCycleUse(FileID newCycleUse);

		// ��ȡ���ļ��ɱ��滻�����ļ������޷����滻���򷵻ؿ��ļ�id
		FileID GetCanReplaceTo(FileID top) const;

		// ��������#include�ļ�¼
		void GenerateUnusedInclude();

		// ��ָ�����ļ��б����ҵ����ڴ����ļ��ĺ��
		std::set<FileID> GetChildren(FileID ancestor, std::set<FileID> all_children/* ������ancestor���ӵ��ļ� */);

		// ���ļ�������ҵ���ָ���ļ�ʹ�õ������ļ�
		std::set<FileID> GetDependOnFile(FileID ancestor, FileID child);

		// ��ȡָ���ļ�ֱ�������ͼ���������ļ���
		// ��������ǣ�
		//     ��ȡtop���������ļ�����ѭ����ȡ��Щ�����ļ��������������ļ���ֱ�����еı������ļ����ѱ�����
		void GetCycleUseFile(FileID top, std::set<FileID> &out) const;

		// ��ȡ�ļ�����ȣ������ļ��ĸ߶�Ϊ0��
		int GetDepth(FileID child) const;

		// ��ȡ�뺢��������Ĺ�ͬ����
		FileID GetCommonAncestor(const std::set<FileID> &children) const;

		// ��ȡ2������������Ĺ�ͬ����
		FileID GetCommonAncestor(FileID child_1, FileID child_2) const;

		// ��ǰ�ļ�֮ǰ�Ƿ������ļ������˸�class��struct��union
		bool HasRecordBefore(FileID cur, const CXXRecordDecl &cxxRecord) const;

		// �����ļ��滻�б�
		void GenerateCanReplace();

		// ����Ӧ������using namespace
		void GenerateRemainUsingNamespace();

		// ��������ǰ�������б�
		void GenerateCanForwardDeclaration();

		// ��ȡָ����Χ���ı�
		std::string GetSourceOfRange(SourceRange range) const;

		// ��ȡָ��λ�������е��ı�
		std::string GetSourceOfLine(SourceLocation loc) const;

		/*
			���ݴ���Ĵ���λ�÷��ظ��е����ķ�Χ���������з�����[���п�ͷ������ĩ���������з��� + 1]
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
		SourceRange GetCurLineWithLinefeed(SourceLocation loc) const;

		// ���ݴ���Ĵ���λ�÷�����һ�еķ�Χ
		SourceRange GetNextLine(SourceLocation loc) const;

		// �Ƿ�Ϊ����
		bool IsEmptyLine(SourceRange line);

		// ��ȡ�к�
		int GetLineNo(SourceLocation loc) const;

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
		std::string GetRawIncludeStr(FileID file) const;

		void UseName(FileID file, FileID beusedFile, const char* name = nullptr, int line = 0);

		// ���ݵ�ǰ�ļ������ҵ�2������ȣ���rootΪ��һ�㣩������ǰ�ļ��ĸ��ļ���Ϊ���ļ����򷵻ص�ǰ�ļ�
		FileID GetLvl2Ancestor(FileID file, FileID root) const;

		// ��2���ļ��Ƿ��ǵ�1���ļ������ȣ����ļ������������ļ������ȣ�
		bool IsAncestor(FileID yound, FileID old) const;

		// ��2���ļ��Ƿ��ǵ�1���ļ�������
		bool IsAncestor(FileID yound, const char* old) const;

		// ��2���ļ��Ƿ��ǵ�1���ļ�������
		bool IsAncestor(const char* yound, FileID old) const;

		// �Ƿ�Ϊ�����ļ��Ĺ�ͬ����
		bool IsCommonAncestor(const std::set<FileID> &children, FileID old) const;

		// ��ȡ���ļ������ļ�û�и��ļ���
		FileID GetParent(FileID child) const;

		// ��ȡc++��class��struct��union��ȫ������������������ռ�
		// ���磺
		//     ������C��C���������ռ�A�е������ռ�B�����������أ�namespace A{ namespace B{ class C; }}
		string GetCxxrecordName(const CXXRecordDecl &cxxRecordDecl) const;

		// ��ָ��λ��������֮���ע�ͣ�ֱ�������һ��token
		bool LexSkipComment(SourceLocation Loc, Token &Result);

		// ���ز���ǰ�����������еĿ�ͷ
		SourceLocation GetInsertForwardLine(FileID at, const CXXRecordDecl &cxxRecord) const;

		// ����ʹ��ǰ��������¼
		void UseForward(SourceLocation loc, const CXXRecordDecl *cxxRecordDecl);

		// ��ӡ���ļ��ĸ��ļ�
		void PrintParentsById();

		// �Ƿ����������c++�ļ������������������ļ����ݲ������κα仯��
		bool CanClean(FileID file) const;

		// ��ȡ���ļ��ı�������Ϣ���������ݰ��������ļ��������ļ����������ļ�#include���кš������ļ�#include��ԭʼ�ı���
		string DebugBeIncludeText(FileID file, bool isAbsoluteName = false) const;

		// ��ȡ���ļ��ı�ֱ�Ӱ�����Ϣ���������ݰ��������ļ����������ļ�#include���кš������ļ�#include��ԭʼ�ı���
		string DebugBeDirectIncludeText(FileID file) const;

		// ��ȡ��λ�������е���Ϣ�������е��ı��������ļ������к�
		string DebugLocText(SourceLocation loc) const;

		// ��ȡ��λ���������#include��Ϣ
		string DebugLocIncludeText(SourceLocation loc) const;

		// ��ȡ�ļ���ʹ��������Ϣ���ļ�������ʹ�õ����������������������Լ���Ӧ�к�
		void DebugUsedNames(FileID file, const std::vector<UseNameInfo> &useNames) const;

		// �Ƿ��б�Ҫ��ӡ���ļ�
		bool IsNeedPrintFile(FileID) const;

		// ��ȡ���ڱ���Ŀ������������ļ���
		int GetCanCleanFileCount() const;

		// ��ӡ���ü�¼
		void PrintUse() const;

		// ��ӡ#include��¼
		void PrintInclude() const;

		// ��ӡ�����������������������ȵļ�¼
		void PrintUsedNames() const;

		// ��ӡ���ļ�ѭ�����õ����Ƽ�¼
		void PrintRootCycleUsedNames() const;

		// ��ӡѭ�����õ����Ƽ�¼
		void PrintAllCycleUsedNames() const;

		// ��ӡ���ļ�ѭ�����õ��ļ��б�
		void PrintRootCycleUse() const;

		// ��ӡѭ�����õ��ļ��б�
		void PrintAllCycleUse() const;

		// ��ӡ���ļ���Ӧ�����ú����ļ���¼
		void PrintCycleUsedChildren() const;

		// ��ӡ��������������ļ��б�
		void PrintAllFile() const;

		// ��ӡ��Ŀ�ܵĿɱ�ɾ#include��¼
		void PrintUnusedInclude() const;

		// ��ӡ��Ŀ�ܵĿ�����ǰ��������¼
		void PrintCanForwarddecl() const;

		// ��ӡ���ļ��ڵ������ռ�
		void PrintNamespace() const;

		// ��ӡ���ļ��ڵ�using namespace
		void PrintUsingNamespace() const;

		// ��ӡ���ļ���Ӧ������using namespace
		void PrintRemainUsingNamespace() const;

		// ��ȡ�ļ�����ͨ��clang��ӿڣ��ļ���δ�������������Ǿ���·����Ҳ���������·����
		// ���磺
		//     ���ܷ��أ�d:/hello.h
		//     Ҳ���ܷ��أ�./hello.h
		const char* GetFileName(FileID file) const;

		// ��ȡ�ļ��ľ���·��
		string GetAbsoluteFileName(FileID file) const;

		// ��ȡ��1���ļ�#include��2���ļ����ı���
		std::string GetRelativeIncludeStr(FileID f1, FileID f2) const;

		// ����ͷ�ļ�����·����������·��ת��Ϊ˫���Ű�Χ���ı�����
		// ���磺������ͷ�ļ�����·��"d:/a/b/c" ��"d:/a/b/c/d/e.h" -> "d/e.h"
		string GetQuotedIncludeStr(const string& absoluteFilePath) const;

		// �滻ָ����Χ�ı�
		void ReplaceText(FileID file, int beg, int end, string text);

		// ���ı����뵽ָ��λ��֮ǰ
		// ���磺������"abcdefg"�ı�������c������123�Ľ�����ǣ�"ab123cdefg"
		void InsertText(FileID file, int loc, string text);

		// �Ƴ�ָ����Χ�ı������Ƴ��ı�����б�Ϊ���У��򽫸ÿ���һ���Ƴ�
		void RemoveText(FileID file, int beg, int end);

		// �Ƴ�ָ���ļ��ڵ�����#include
		void CleanByUnusedLine(const FileHistory &eachFile, FileID file);

		// ��ָ���ļ������ǰ������
		void CleanByForward(const FileHistory &eachFile, FileID file);

		// ��ָ���ļ����滻#include
		void CleanByReplace(const FileHistory &eachFile, FileID file);

		// ����ָ���ļ�
		void CleanBy(const FileHistoryMap &files);

		// ���������б�Ҫ������ļ�
		void CleanAllFile();

		// �������ļ�
		void CleanMainFile();

		// ��ӡ���滻��¼
		void PrintCanReplace() const;

		// ��ӡͷ�ļ�����·��
		void PrintHeaderSearchPath() const;

		// ���ڵ��ԣ���ӡ���ļ����õ��ļ�������ڸ��ļ���#include�ı�
		void PrintRelativeInclude() const;

		// ���ڵ��Ը��٣���ӡ�Ƿ����ļ��ı�����������©
		void PrintNotFoundIncludeLocForDebug();

		// ���ڵ��Ը��٣���ӡ���ļ����Ƿ�ͬһ�г�����2��#include
		void PrintSameLineForDebug();

		// �ļ���ʽ�Ƿ���windows��ʽ�����з�Ϊ[\r\n]����Unix��Ϊ[\n]
		bool IsWindowsFormat(FileID) const;

		// �ϲ��ɱ��Ƴ���#include��
		void MergeUnusedLineTo(const FileHistory &newFile, FileHistory &oldFile) const;

		// �ϲ���������ǰ������
		void MergeForwardLineTo(const FileHistory &newFile, FileHistory &oldFile) const;

		// �ϲ����滻��#include
		void MergeReplaceLineTo(const FileHistory &newFile, FileHistory &oldFile) const;

		void MergeCountTo(FileHistoryMap &oldFiles) const;

		// ȡ����ǰcpp�ļ������Ĵ������¼
		void TakeAllInfoTo(FileHistoryMap &out) const;

		// ����������а��ļ����д��
		void TakeUnusedLineByFile(FileHistoryMap &out) const;

		// ��������ǰ���������ļ����д��
		void TakeNewForwarddeclByFile(FileHistoryMap &out) const;

		// ���ļ��Ƿ��Ǳ�-includeǿ�ư���
		bool IsForceIncluded(FileID file) const;

		// ���ļ��滻��¼�����ļ����й���
		typedef std::map<FileID, std::set<FileID>> ChildrenReplaceMap;
		typedef std::map<FileID, ChildrenReplaceMap> ReplaceFileMap;
		void SplitReplaceByFile(ReplaceFileMap &replaces) const;

		// ȡ��ָ���ļ���#include�滻��Ϣ
		void TakeBeReplaceOfFile(FileHistory &eachFile, FileID top, const ChildrenReplaceMap &childernReplaces) const;

		// ȡ�����ļ���#include�滻��Ϣ
		void TakeReplaceByFile(FileHistoryMap &out) const;

		// ���ļ��Ƿ�ѭ�����õ�
		bool IsCyclyUsed(FileID file) const;

		// ���ļ��Ƿ����ļ�ѭ�����õ�
		bool IsRootCyclyUsed(FileID file) const;

		// ���ļ��Ƿ�ɱ��滻
		bool IsReplaced(FileID file) const;

	private:
		// 1. ���ļ����������ļ��ļ�¼�б�[�ļ�] -> [���õ������ļ��б�]
		std::map<FileID, std::set<FileID>>			m_uses;

		// 2. ���ļ�ѭ�����õ��ļ����������ļ�ֱ�����û������õ��ļ��б��������е������ļ��������õ���һ�ļ���Ȼ�ڸ��ļ�����)
		std::set<FileID>							m_rootCycleUse;

		// 3. ������ѭ�������ļ������������е������ļ��������õ���һ�ļ���Ȼ�ڸ��ļ����ڣ�
		std::set<FileID>							m_allCycleUse;

		// 4. ���ļ��ı����õĺ���ļ���[�ļ�] -> [����ļ��б�ѭ�����õĲ���]
		std::map<FileID, std::set<FileID>>			m_cycleUsedChildren;

		// 5. ���ļ����������ļ��б�[�ļ�] -> [��include���ļ�]
		std::map<FileID, std::set<FileID>>			m_includes;

		// 6. ��ǰc++�ļ��Լ��������������ļ�
		std::set<FileID>							m_files;

		// 7. ��ǰc++�ļ����漰��������#include�ı���λ���Լ�����Ӧ�ķ�Χ�� [#include "xxxx.h"�е�һ��˫���Ż�<���ŵ�λ��] -> [#include�����ı����ķ�Χ]
		std::map<SourceLocation, SourceRange>		m_includeLocs;

		// 8. ȫ�����õ�#includeλ�ã����ڲ�ѯĳ#include�Ƿ����ã�
		std::set<SourceLocation>					m_usedLocs;

		// 9. ȫ��û�õ���#includeλ�ã����ڲ�ѯĳ#include�Ƿ�Ӧ�������֮���Բ�ʹ��FileID��ΪĳЩ�ظ�#include�ᱻ�Զ����������²�δ����FileID��
		std::set<SourceLocation>					m_unusedLocs;

		// 10. ���ļ�IDӳ���[�ļ�] -> [���ļ�]
		std::map<FileID, FileID>					m_parentIDs;

		// 11. �ɱ��û���#include�б�[�ļ�] -> [���ļ��ɱ��滻Ϊ���ļ�]
		std::map<FileID, std::set<FileID>>			m_replaces;

		// 12. ���ļ��пɱ��û���#include�б�[�ļ�] -> [���ļ�����Щ�ļ��ɱ��滻��Ϊ��������Щ�ļ�]
		ReplaceFileMap								m_splitReplaces;

		// 13. ͷ�ļ�����·���б�
		std::vector<HeaderSearchDir>				m_headerSearchPaths;

		// 14. ���ļ���ʹ�õ�class��struct��unionָ������õļ�¼��[�ļ�] -> [���ļ���ʹ�õ�class��struct��unionָ�������]
		std::map<FileID, set<const CXXRecordDecl*>>	m_forwardDecls;

		// 15. ���ļ���ʹ�õ��������������������ȵ����Ƽ�¼��[�ļ�] -> [���ļ���ʹ�õ���������������������]
		std::map<FileID, std::vector<UseNameInfo>>	m_useNames;

		// 16. ���ļ��������������ռ��¼��[�ļ�] -> [���ļ��ڵ������ռ��¼]
		std::map<FileID, std::set<std::string>>		m_namespaces;

		// 17. ���ļ���������using namespace��¼��[�ļ�] -> [���ļ��ڵ�using namespace��¼]
		std::map<FileID, std::set<std::string>>		m_usingNamespaces;

		// 18. Ӧ������using namespace��¼��[using namespace��λ��] -> [using namespace��ȫ��]
		std::map<SourceLocation, NamespaceInfo>		m_remainUsingNamespaces;

	private:
		clang::Rewriter*							m_rewriter;
		clang::SourceManager*						m_srcMgr;
		clang::CompilerInstance*					m_compiler;

		// ��ǰ��ӡ��������������־����
		mutable int									m_i;

		// ��ǰ��ӡ��������������־��ӡ
		mutable int									m_printIdx;
	};
}

using namespace cxxcleantool;

#endif // _parsing_cpp_h_