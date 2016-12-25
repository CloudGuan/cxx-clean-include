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
#include <clang/Rewrite/Core/Rewriter.h>

#include "history.h"
#include <unordered_set>

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

// [�ļ���] -> [·�����ϵͳ·�����û�·��]
typedef std::map<string, SrcMgr::CharacteristicKind> IncludeDirMap;

// class��struct��union����
typedef std::set<const CXXRecordDecl*> RecordSet;

// [λ��] -> [ʹ�õ�class��struct���û�ָ��]
typedef std::map<SourceLocation, RecordSet> LocUseRecordsMap;

// [�ļ�] -> [ʹ�õ�class��struct���û�ָ��]
typedef std::map<FileID, RecordSet> FileUseRecordsMap;

// [�ļ�] -> [���ļ���ʹ�õ�class��struct��unionָ�������]
typedef std::map<FileID, LocUseRecordsMap> UseRecordsByFileMap;

// �ļ���
typedef std::set<FileID> FileSet;

// �ļ�����
typedef std::set<std::string> FileNameSet;

// set����set
template <typename Container1, typename Container2>
inline void Add(Container1 &a, const Container2 &b)
{
	a.insert(b.begin(), b.end());
}

template <typename Container, typename Key>
inline bool Has(Container& container, const Key &key)
{
	return container.find(key) != container.end();
}

// ��ǰ���ڽ�����c++�ļ���Ϣ
class ParsingFile
{
	// ���ڵ��ԣ����õ�����
	struct UseNameInfo
	{
		inline void AddName(const char* name, int line)
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
			: ns(nullptr) {}

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
		{}

		string						m_dir;
		SrcMgr::CharacteristicKind	m_dirType;
	};

public:
	ParsingFile(clang::CompilerInstance &compiler);

	~ParsingFile();

	inline clang::SourceManager& GetSrcMgr() const { return *m_srcMgr; }

	// ��ӳ�Ա�ļ�
	void AddFile(FileID file);

	// ��ǰcpp�ļ�������ʼ
	void Begin();

	// ��ǰcpp�ļ���������
	void End();

	// ����
	void Analyze();

	// �����ļ����
	int GetDeepth(FileID file) const;

	// �Ƿ�Ϊ��ǰ������������
	bool IsForwardType(const QualType &var);

	// �Ƿ����е��޶������������ռ䣨����::std::vector<int>::�е�vector�Ͳ��������ռ䣩
	bool IsAllQualifierNamespace(const NestedNameSpecifier *specifier);

	// a�ļ�ʹ��b�ļ�
	inline void UseInclude(FileID a, FileID b, const char* name = nullptr, int line = 0);

	// ��ǰλ��ʹ��ָ���ĺ�
	void UseMacro(SourceLocation loc, const MacroDefinition &macro, const Token &macroName, const MacroArgs *args = nullptr);

	// ȥ��ָ�룬��ȡ��������ָ�������
	QualType GetPointeeType(const QualType &var);

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

	// aλ�õĴ���ʹ��bλ�õĴ���
	inline void Use(SourceLocation a, SourceLocation b, const char* name = nullptr);

	// ��ǰλ��ʹ��Ŀ�����ͣ�ע��QualType������ĳ�����͵�const��volatile��static�ȵ����Σ�
	void UseQualType(SourceLocation loc, const QualType &t);

	// ��ǰλ��ʹ��Ŀ�����ͣ�ע��Type����ĳ�����ͣ�������const��volatile��static�ȵ����Σ�
	void UseType(SourceLocation loc, const Type *t);

	// ���������ģ��������ռ�
	void UseContext(SourceLocation loc, const DeclContext*);

	// ����Ƕ���������η�
	void UseQualifier(SourceLocation loc, const NestedNameSpecifier*);

	// ����Ƕ���������η�
	void UseUsingQualifier(SourceLocation loc, const NestedNameSpecifier*);

	// ���������ռ�����
	void UseNamespaceDecl(SourceLocation loc, const NamespaceDecl*);

	// ����using namespace����
	void UseUsingNamespace(SourceLocation loc, const NamespaceDecl*);

	// ����using����
	void UseUsing(SourceLocation loc, const NamedDecl*);

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

	// ��aʹ��bʱ���跨�ҵ�һ��������a���Ϲ�ϵ��b���ⲿ����
	inline FileID GetBestAncestor(FileID a, FileID b) const;

	// ��ʼ�����ļ������Ķ�c++Դ�ļ���
	void Clean();

	// ����������д��c++Դ�ļ������أ�true��д�ɹ���false��дʧ��
	// �����ӿڲο���Rewriter::overwriteChangedFiles��
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

	// ���ļ��Ƿ��Ǳ�-includeǿ�ư���
	inline bool IsForceIncluded(FileID file) const;

	// ��ȡ�ļ�����ͨ��clang��ӿڣ��ļ���δ�������������Ǿ���·����Ҳ���������·����
	// ���磺���ܷ��أ�d:/hello.h��Ҳ���ܷ��أ�./hello.h
	inline const char* GetFileName(FileID file) const;

	// ��ȡ�ļ��ľ���·��
	inline string GetAbsoluteFileName(FileID file) const;

	// ��ȡ�ļ��ľ���·��
	inline const char* GetFileNameInCache(FileID file) const;

	// ��ȡ�ļ���Сд����·��
	inline const char* GetLowerFileNameInCache(FileID file) const;

	// ���ڵ��ԣ���ȡ�ļ��ľ���·���������Ϣ
	string GetDebugFileName(FileID file) const;

	// ��ȡ���ļ��ı�������Ϣ���������ݰ��������ļ��������ļ����������ļ�#include���кš������ļ�#include��ԭʼ�ı���
	std::string DebugBeIncludeText(FileID file) const;

private:
	// ��ȡͷ�ļ�����·��
	std::vector<HeaderSearchDir> TakeHeaderSearchPaths(const clang::HeaderSearch &headerSearch) const;

	// ��ͷ�ļ�����·�����ݳ����ɳ���������
	std::vector<HeaderSearchDir> SortHeaderSearchPath(const IncludeDirMap& include_dirs_map) const;

	// ��ȡͬ���ļ��б�
	FileSet GetAllSameFiles(FileID file) const;

	// ���ļ��Ƿ���ͬ���ļ�
	bool HasSameFile(FileID file) const;

	// 2���ļ��Ƿ��ļ���һ��
	inline bool IsSameName(FileID a, FileID b) const;

	// ��ȡ�ļ�����ȣ������ļ��ĸ߶�Ϊ0��
	int GetDepth(FileID child) const;

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

	// ͬGetCurLine�����������з���[���п�ͷ����һ�п�ͷ]
	SourceRange GetCurFullLine(SourceLocation loc) const;

	// ���ݴ���Ĵ���λ�÷�����һ�еķ�Χ
	SourceRange GetNextLine(SourceLocation loc) const;

	// ��ȡ�к�
	int GetLineNo(SourceLocation loc) const;

	// ��ȡ�ļ���Ӧ�ı������к�
	int GetIncludeLineNo(FileID) const;

	// ��ȡ�ļ���Ӧ��#include����Χ
	SourceRange GetIncludeRange(FileID) const;

	// �Ƿ��ǻ��з�
	bool IsNewLineWord(SourceLocation loc) const;

	// ��ȡ�ļ���Ӧ��#include���ڵ��У��������з���
	// ע�⣺������һ��cpp�ļ����������£����е�1�к͵�2�а�����a.h����Ȼͬһ���ļ�����FileID�ǲ�һ����
	//		1. #include "./a.h"
	//		2. #include "a.h"
	//	�ִ����һ��a.h��Ӧ��FileID������ = #include "./a.h"
	std::string GetBeIncludeLineText(FileID file) const;

	inline void UseName(FileID file, FileID beusedFile, const char* name = nullptr, int line = 0);

	// ��2���ļ��Ƿ��ǵ�1���ļ������ȣ����ļ������������ļ������ȣ�
	inline bool IsAncestor(FileID yound, FileID old) const;

	// ��ȡ���ļ������ļ�û�и��ļ���
	FileID GetParent(FileID child) const;

	// ��ȡc++��class��struct��union��ȫ������������������ռ�
	// ���磺������C��C���������ռ�A�е������ռ�B�����������أ�namespace A{ namespace B{ class C; }}
	string GetRecordName(const RecordDecl &recordDecl) const;

	// ����ʹ��ǰ��������¼�����ڲ���Ҫ��ӵ�ǰ����������֮���������
	inline void UseForward(SourceLocation loc, const CXXRecordDecl *cxxRecordDecl);

	// ��ӡ���ļ��ĸ��ļ�
	void PrintParent();

	// ��ӡ���ļ��ĺ����ļ�
	void PrintKids();

	// �Ƿ����������c++�ļ������������������ļ����ݲ������κα仯��
	inline bool CanClean(FileID file) const;
	inline bool CanClean(const char *fileName) const;

	// ��ȡ�ļ���Ϣ
	std::string DebugParentFileText(FileID file, int n) const;

	// ��ȡ��λ�������е���Ϣ�������е��ı��������ļ������к�
	std::string DebugLocText(SourceLocation loc) const;

	// ��ȡ�ļ���ʹ��������Ϣ���ļ�������ʹ�õ����������������������Լ���Ӧ�к�
	void DebugUsedNames(FileID file, const std::vector<UseNameInfo> &useNames) const;

	// �Ƿ��б�Ҫ��ӡ���ļ�
	bool IsNeedPrintFile(FileID) const;

	// ��ȡƴдλ��
	inline SourceLocation GetSpellingLoc(SourceLocation loc) const;

	// ��ȡ��������չ���λ��
	inline SourceLocation GetExpasionLoc(SourceLocation loc) const;

	// ��ȡ�ļ�ID
	inline FileID GetFileID(SourceLocation loc) const;

	// ��ȡ��1���ļ�#include��2���ļ����ı���
	std::string GetRelativeIncludeStr(FileID f1, FileID f2) const;

	// ����ͷ�ļ�����·����������·��ת��Ϊ˫���Ű�Χ���ı�����
	// ���磺������ͷ�ļ�����·��"d:/a/b/c" ��"d:/a/b/c/d/e.h" -> "d/e.h"
	string GetQuotedIncludeStr(const char *absoluteFilePath) const;

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

	// ��ӡͷ�ļ�����·��
	void PrintHeaderSearchPath() const;

	// ���ڵ��ԣ���ӡ���ļ����õ��ļ�������ڸ��ļ���#include�ı�
	void PrintRelativeInclude() const;

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

	// a�ļ��Ƿ���b�ļ�֮ǰ
	bool IsFileBeforeFile(FileID a, FileID b) const;

	// ��ӡ���ü�¼
	void PrintUse() const;

	// ��ӡ#include��¼
	void PrintInclude() const;

	// ��ӡ�����������������������ȵļ�¼
	void PrintUseName() const;

	// ��ӡ��תΪǰ����������ָ������ü�¼
	void PrintUseRecord() const;

	// ��ӡ���յ�ǰ��������¼
	void PrintForwardClass() const;

	// ��ӡ��������������ļ��б�
	void PrintAllFile() const;

	// ��ӡ������־
	void PrintHistory() const;

	// ��ӡ���ļ��ڵ������ռ�
	void PrintNamespace() const;

	// ��ӡ���ļ��ڵ�using namespace
	void PrintUsingNamespace() const;

	// ����ǰcpp�ļ������Ĵ������¼��֮ǰ����cpp�ļ������Ĵ������¼�ϲ�
	void MergeTo(FileHistoryMap &old) const;

	// �Ƿ��ֹ�Ķ�ĳ�ļ�
	bool IsSkip(FileID file) const;

	// ��2���ļ��Ƿ��ǵ�1���ļ������ȣ�����ͬ���ļ���
	inline bool IsAncestorByName(FileID yound, FileID old) const;

	// ���ļ�������ͬ���ļ��Ƿ�������ͬһ�ļ��ɱ�������Σ�
	inline bool HasMinKidBySameName(FileID top, FileID kid) const;

	// �Ƿ�Ӧ������λ�����õ�class��struct��union��ǰ������
	bool IsShouldKeepForwardClass(FileID, const CXXRecordDecl &cxxRecord) const;

	FileSet GetUseChain(const std::map<FileID, FileSet> &use, FileID top) const;

	bool MergeMin();

	inline bool IsUserFile(FileID file) const;

	inline bool IsOuterFile(FileID file) const;

	inline FileID GetOuterFileAncestor(FileID file) const;

	inline FileID SearchOuterFileAncestor(FileID file) const;

	void GenerateForceIncludes();

	void GenerateOutFileAncestor();

	void GenerateUserUse();

	void GenerateMinUse();

	// ��������ǰ�������б�
	void GenerateForwardClass();

	// �ü�ǰ�������б�
	void MinimizeForwardClass();

	void GetUseRecordsInKids(FileID top, const FileUseRecordsMap &recordMap, RecordSet &records);

	void GetKidsBySame(const std::map<FileID, FileSet> &userKids, FileID top, FileSet &kids) const;

	// ȡ�������ļ��Ŀ�ɾ��#include��
	void TakeDel(FileHistory &history, const FileSet &dels) const;

	void TakeReplaceLine(ReplaceLine &replaceLine, FileID from, FileID to) const;

	void TakeForwardClass(FileHistory &history, FileID insertAfter, FileID top) const;

	void TakeAdd(FileHistory &history, FileID top, FileID insertAfter, const FileSet &adds) const;

	// �����ļ����������ȳ��ֵ�ͬ���ļ�����ǰ��
	void SortFilesByLocation(FileSet &files) const;

	// �����Ӧ���ĸ��ļ���Ӧ��#include�������ı�
	FileID CalcInsertLoc(const FileSet &includes, const FileSet &dels) const;

	// ȡ����ָ���ļ��ķ������
	void TakeHistory(FileID top, FileHistory &out) const;

	// �����ļ��Ƿ�ǿ�ư���
	inline bool IsAncestorForceInclude(FileID file) const;

	// ��ȡ��ǿ�ư��������ļ�
	inline FileID GetAncestorForceInclude(FileID file) const;

	// ��ӡ���ļ��ĺ����ļ�
	void PrintUserKids();

	// ��ӡ
	void PrintKidsByName();

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
	static ParsingFile *g_nowFile;

private:
	//================== ���շ������ ==================//

	// [���շ������]. ������ǰcpp�ļ��Ľ����[c++�ļ���] -> [���ļ���������]
	FileHistoryMap								m_historys;

	// ������������ļ�����С�����ļ��б�[�ļ�ID] -> [���ļ���Ӧֱ�Ӱ������ļ�ID�б�]
	std::map<FileID, FileSet>					m_min;

	// ���������ÿ���ļ�����Ӧ������ǰ������
	FileUseRecordsMap							m_fowardClass;

	// 1. ���ļ��ĺ���ļ������û��ļ�����[�ļ�ID] -> [���ļ�������ȫ������ļ�ID�����û��ļ���]
	std::map<FileID, FileSet>					m_userKids;

	// 2. ���ļ��ĺ���ļ����б�[�ļ���] -> [���ļ�������ȫ������ļ�]
	std::map<std::string, FileNameSet>			m_kidsByName;

	// 3. ���ļ�Ӧ�����ĺ���ļ��б�[�ļ�ID] -> [���ļ�Ӧ�����ĺ���ļ�ID�б�]
	std::map<FileID, FileSet>					m_minKids;

	// 4. ������Ŀ���ļ���������Ŀ���ļ���[�ļ�ID] -> [��Ӧ�������ⲿ�ļ�ID]���ɱ��޸ĵ��ļ���Ϊ��Ŀ�ڲ��ļ�����ֹ���޸ĵ��ļ���Ϊ�ⲿ�ļ������磬����ĳ�ļ�����#include <vector>����Ϊ<vector>�ǿ��ļ�����ֹ���Ķ�������vector���ⲿ�ļ���
	std::map<FileID, FileID>					m_outFileAncestor;

	// 5. ��Ŀ���ļ������ù�ϵ��[��Ŀ���ļ�ID] -> [�����õ���Ŀ���ļ�ID�б� + ��Ŀ���ļ�ID�б�]
	std::map<FileID, FileSet>					m_userUses;

	// 6. ��ǿ�ư������ļ�ID�б�
	FileSet										m_forceIncludes;

	//================== һЩ��ʱ���� ==================//

	// 1.1 ���ļ����������ļ��ļ�¼��[�ļ�ID] -> [���õ������ļ��б�]�����磬����A.h�õ���B.h�е�class B������ΪA.h������B.h��
	std::map<FileID, FileSet>					m_uses;

	// 1.2 �����ڴ�ӡ�����ļ���ʹ�õ��������������������ȵ����Ƽ�¼��[�ļ�ID] -> [���ļ���ʹ�õ������ļ��е�����������������������������]
	std::map<FileID, std::vector<UseNameInfo>>	m_useNames;

	// 2.1 ���ļ����������ļ��б�[�ļ���] -> [��include���ļ�]
	std::map<std::string, FileSet>				m_includes;

	// 2.2 �����ļ�ID
	FileSet										m_files;

	// 2.3 ���ļ���ϵ��[�ļ�ID] -> [���ļ�ID]
	std::map<FileID, FileID>					m_parents;

	// 2.4 ���ļ��ĺ����[�ļ�ID] -> [���ļ�������ȫ������ļ�ID]
	std::map<FileID, FileSet>					m_kids;

	// 2.5 ͬһ���ļ�����Ӧ�Ĳ�ͬ�ļ�ID��[�ļ���] -> [ͬ���ļ�ID�б�]
	std::map<std::string, FileSet>				m_sameFiles;

	// 3. ͷ�ļ�����·���б�
	std::vector<HeaderSearchDir>				m_headerSearchPaths;

	// 4.1 ÿ��λ����ʹ�õ�class��struct��ָ�롢���ã�����������ǰ��������[λ��] -> [��ʹ�õ�class��struct��unionָ�������]
	LocUseRecordsMap							m_locUseRecordPointers;

	// 4.2 ÿ���ļ���ʹ�õ�class��struct��ָ�롢���ã�����������ǰ��������[�ļ�] -> [��ʹ�õ�class��struct��unionָ�������]
	FileUseRecordsMap							m_fileUseRecordPointers;

	// 4.3 ÿ���ļ���ʹ�õ�class��struct����ָ�롢�����ã������ڱ������ɶ����ǰ������
	FileUseRecordsMap							m_fileUseRecords;

	// 5.1 using namespace��¼�����磺using namespace std;����[using namespace��λ��] -> [��Ӧ��namespace����]
	map<SourceLocation, const NamespaceDecl*>	m_usingNamespaces;
	
	// 5.2 using��¼�����磺using std::string;����[using��Ŀ���Ӧ��λ��] -> [using����]
	map<const NamedDecl*, const UsingDecl*>		m_usings;

	// 5.3 �����ڴ�ӡ�����ļ��������������ռ��¼��[�ļ�] -> [���ļ��ڵ������ռ��¼]
	std::map<FileID, std::set<std::string>>		m_namespaces;

	// 6.1 �����ļ�ID��Ӧ���ļ�����[�ļ�ID] -> [�ļ���]
	std::map<FileID, std::string>				m_fileNames;

	// 6.2 �����ļ�ID��Ӧ���ļ�����[�ļ�ID] -> [Сд�ļ���]
	std::map<FileID, std::string>				m_lowerFileNames;
	
private:
	// ���ļ�id
	FileID										m_root;

	// �ļ���д�࣬�����޸�c++Դ������
	clang::Rewriter								m_rewriter;
	clang::SourceManager*						m_srcMgr;
	clang::CompilerInstance*					m_compiler;

	// ���ļ��ı��������ʷ
	CompileErrorHistory							m_compileErrorHistory;

	// ��ǰ��ӡ��������������־��ӡ
	mutable int									m_printIdx;
};

#endif // _parser_h_