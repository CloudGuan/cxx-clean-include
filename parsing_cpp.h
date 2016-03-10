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
}

namespace cxxcleantool
{
	struct HeaderSearchPath
	{
		HeaderSearchPath(const string& dir, SrcMgr::CharacteristicKind pathType)
			: path(dir)
			, path_type(pathType)
		{
		}

		string						path;
		SrcMgr::CharacteristicKind	path_type;
	};

	string ToLinuxPath(const char *path);

	string FixPath(const string& path);

	// ��ǰ���ڽ�����c++�ļ����ڲ�#include��Ϣ
	class ParsingFile
	{
	public:
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

		std::vector<HeaderSearchPath> ComputeHeaderSearchPaths(clang::HeaderSearch &headerSearch);

		std::vector<HeaderSearchPath> SortHeaderSearchPath(const std::map<string, SrcMgr::CharacteristicKind>& include_dirs_map);

		std::string FindFileInSearchPath(const vector<HeaderSearchPath>& searchPaths, const std::string& fileName);

		void GenerateUnusedTopInclude();

		inline bool IsLocBeUsed(SourceLocation loc) const;

		void GenerateResult();

		/*
			���ݼ��彨��ֱ�����ù�ϵ����a.h����b.h����a.h������b.h�е����ݣ����a.hֱ������b.h��ע�⣬ֱ�����õ�˫�������Ǹ��ӹ�ϵ
			ĳЩ�����Ƚ����⣬���磬���������ļ�ͼ��
			����->��ʾ#include���磺b.h-->c.h��ʾb.h����c.h����->>��ʾֱ�����ã���

			�������1��
			    a.h --> b1.h --> c1.h                                     a.h --> b1.h -->  c1.h
					|                     [��ͼ����a.h����c.h��������ͼ]       |
					--> b.h  --> c.h                                          ->> b.h  ->> c.h

				����a.hֱ������b.h��b.hֱ������c.h

			�������2��
			    a.h --> b1.h --> c1.h --> d1.h                                        a.h ->> b1.h ->> c1.h ->> d1.h
				    |                     ^		    [��ͼ����d.h����d1.h��������ͼ]        |                      ^
					--> b.h  --> c.h  --> d.h	                                     	  ->> b.h  ->> c.h  ->> d.h
				��^��ʾ���ù�ϵ����ͼ�б�ʾd.h���õ�d1.h�е����ݣ�

			    ����a.hֱ������b1.h��b.h��b1.hֱ������c1.h��c1.hֱ������d1.h����������
		*/
		void GenerateRootCycleUse();

		void GenerateAllCycleUse();

		bool TryAddAncestorOfCycleUse();

		void GenerateCycleUsedChildren();

		// ���������ļ������ؽ����ʾ�Ƿ�������һЩ��������ļ�����true����false
		bool ExpandAllCycleUse(FileID newCycleUse);

		FileID GetCanReplaceTo(FileID top);

		void GenerateUnusedInclude();

		// ��ָ�����ļ��б����ҵ����ڴ����ļ��ĺ��
		std::set<FileID> GetChildren(FileID ancestor, std::set<FileID> all_children/* ������ancestor���ӵ��ļ� */);

		// ���ļ�������ҵ���ָ���ļ�ʹ�õ������ļ�
		std::set<FileID> GetDependOnFile(FileID ancestor, FileID child);

		void GetCycleUseFile(FileID top, std::set<FileID> &out);

		int GetDepth(FileID child);

		// ��ȡ�뺢��������Ĺ�ͬ����
		FileID GetCommonAncestor(const std::set<FileID> &children);

		// ��ȡ2������������Ĺ�ͬ����
		FileID GetCommonAncestor(FileID child_1, FileID child_2);

		// ��ǰ�ļ�֮ǰ�Ƿ������ļ������˸�class��struct��union
		bool HasRecordBefore(FileID cur, const CXXRecordDecl &cxxRecord);

		void GenerateCanReplace();

		void GenerateCanForwardDeclaration();

		void GenerateCount();

		std::string GetSourceOfRange(SourceRange range);

		std::string GetSourceOfLine(SourceLocation loc);

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
		SourceRange GetCurLine(SourceLocation loc);

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
		SourceRange GetCurLineWithLinefeed(SourceLocation loc);

		// ���ݴ���Ĵ���λ�÷�����һ�еķ�Χ
		SourceRange GetNextLine(SourceLocation loc);

		bool IsEmptyLine(SourceRange line);

		int GetLineNo(SourceLocation loc);

		// �Ƿ��ǻ��з�
		bool IsNewLineWord(SourceLocation loc);

		/*
			��ȡ��Ӧ�ڴ����ļ�ID��#include�ı�
			���磬������b.cpp���������£�
				1. #include "./a.h"
				2. #include "a.h"

			���е�1�к͵�2�а�����a.h����Ȼͬһ���ļ�����FileID�ǲ�һ����

			�ִ����һ��a.h��FileID������������
			#include "./a.h"
		*/
		std::string GetRawIncludeStr(FileID file);

		// curλ�õĴ���ʹ��srcλ�õĴ���
		void Use(SourceLocation cur, SourceLocation src, const char* name = nullptr);

		void UseName(FileID file, FileID beusedFile, const char* name = nullptr, int line = 0);

		// ���ݵ�ǰ�ļ������ҵ�2������ȣ���rootΪ��һ�㣩������ǰ�ļ������ڵ�2�㣬�򷵻ص�ǰ�ļ�
		FileID GetLvl2Ancestor(FileID file, FileID root);

		// ��2���ļ��Ƿ��ǵ�1���ļ�������
		bool IsAncestor(FileID yound, FileID old) const;

		// ��2���ļ��Ƿ��ǵ�1���ļ�������
		bool IsAncestor(FileID yound, const char* old) const;

		// ��2���ļ��Ƿ��ǵ�1���ļ�������
		bool IsAncestor(const char* yound, FileID old) const;

		bool IsCommonAncestor(const std::set<FileID> &children, FileID old);

		bool HasParent(FileID child);

		FileID GetParent(FileID child);

		// ��ǰ�ļ�ʹ��Ŀ���ļ�
		void UseIinclude(FileID file, FileID beusedFile, const char* name = nullptr, int line = 0);

		void UseMacro(SourceLocation loc, const MacroDefinition &macro, const Token &macroName);

		// ��ǰλ��ʹ��Ŀ������
		void UseType(SourceLocation loc, const QualType &t);

		string GetCxxrecordName(const CXXRecordDecl &cxxRecordDecl);

		// ��ָ��λ��������֮���ע�ͣ�ֱ�������һ��token
		bool LexSkipComment(SourceLocation Loc, Token &Result);

		// ���ز���ǰ�����������еĿ�ͷ
		SourceLocation GetInsertForwardLine(FileID at, const CXXRecordDecl &cxxRecord);

		void UseForward(SourceLocation loc, const CXXRecordDecl *cxxRecordDecl);

		void UseVar(SourceLocation loc, const QualType &var);

		void OnUseDecl(SourceLocation loc, const NamedDecl *nameDecl);

		void OnUseRecord(SourceLocation loc, const CXXRecordDecl *record);

		void PrintParentsById();

		bool CanClean(FileID file);

		string DebugFileIncludeText(FileID file, bool is_absolute_name = false);

		string DebugFileDirectUseText(FileID file);

		string DebugLocText(SourceLocation loc);

		string DebugLocIncludeText(SourceLocation loc);

		void DebugUsedNames(FileID file, const std::vector<UseNameInfo> &useNames);

		bool IsNeedPrintFile(FileID);

		void PrintUse();

		void PrintUsedNames();

		void PrintRootCycleUsedNames();

		void PrintAllCycleUsedNames();

		void PrintRootCycleUse();

		void PrintAllCycleUse();

		void PrintUsedChildren();

		void PrintAllFile();

		void PrintUsedTopInclude();

		void PrintTopIncludeById();

		static void PrintUnusedIncludeOfFiles(FileHistoryMap &files);

		static void PrintCanForwarddeclOfFiles(FileHistoryMap &files);

		static void PrintCanReplaceOfFiles(FileHistoryMap &files);

		void PrintUnusedInclude();

		void PrintCanForwarddecl();

		void PrintUnusedTopInclude();

		const char* GetFileName(FileID file) const;

		static string GetAbsoluteFileName(const char *raw_file_name);

		string GetAbsoluteFileName(FileID file) const;

		inline bool IsBlank(char c)
		{
			return (c == ' ' || c == '\t');
		}

		const char* GetIncludeValue(const char* include_str);

		std::string GetRelativeIncludeStr(FileID f1, FileID f2);

		bool IsAbsolutePath(const string& path);

		// ��·��
		// d:/a/b/c/../../d/ -> d:/d/
		static std::string SimplifyPath(const char* path);

		static string GetAbsolutePath(const char *path);

		static string GetAbsolutePath(const char *base_path, const char* relative_path);

		string GetParentPath(const char* path);

		// ����ͷ�ļ�����·����������·��ת��Ϊ˫���Ű�Χ���ı�����
		// ���磺������ͷ�ļ�����·��"d:/a/b/c" ��"d:/a/b/c/d/e.h" -> "d/e.h"
		string GetQuotedIncludeStr(const string& absoluteFilePath);

		void Clean();

		void ReplaceText(FileID file, int beg, int end, string text);

		void InsertText(FileID file, int loc, string text);

		void RemoveText(FileID file, int beg, int end);

		void CleanByUnusedLine(const FileHistory &eachFile, FileID file);

		void CleanByForward(const FileHistory &eachFile, FileID file);

		void CleanByReplace(const FileHistory &eachFile, FileID file);

		void CleanBy(const FileHistoryMap &files);

		void CleanAllFile();

		void CleanMainFile();

		void PrintCanReplace();

		void PrintHeaderSearchPath();

		void PrintRelativeInclude();

		void PrintRangeToFileName();

		// ���ڵ��Ը���
		void PrintNotFoundIncludeLoc();

		// ���ڵ��Ը���
		void PrintSameLine();

		void Print();

		bool InitHeaderSearchPath(clang::SourceManager* srcMgr, clang::HeaderSearch &header_search);

		// �ļ���ʽ�Ƿ���windows��ʽ�����з�Ϊ[\r\n]����Unix��Ϊ[\n]
		bool IsWindowsFormat(FileID);

		void MergeTo(FileHistoryMap &old);

		void MergeUnusedLineTo(const FileHistory &newFile, FileHistory &oldFile) const;

		void MergeForwardLineTo(const FileHistory &newFile, FileHistory &oldFile) const;

		void MergeReplaceLineTo(const FileHistory &newFile, FileHistory &oldFile) const;

		void MergeCountTo(FileHistoryMap &oldFiles) const;

		void TakeAllInfoTo(FileHistoryMap &out);

		// ����������а��ļ����д��
		void TakeUnusedLineByFile(FileHistoryMap &out);

		// ��������ǰ���������ļ����д��
		void TakeNewForwarddeclByFile(FileHistoryMap &out);

		bool IsForceIncluded(FileID file);

		typedef std::map<FileID, std::set<FileID>> ChildrenReplaceMap;
		typedef std::map<FileID, ChildrenReplaceMap> ReplaceFileMap;
		void SplitReplaceByFile(ReplaceFileMap &replaces);

		void TakeBeReplaceOfFile(FileHistory &eachFile, FileID top, const ChildrenReplaceMap &childernReplaces);

		// ȡ�����ļ���#include�滻��Ϣ
		void TakeReplaceByFile(FileHistoryMap &out);

		bool IsCyclyUsed(FileID file);

		bool IsReplaced(FileID file);

	public:
		// ѭ�������б�
		std::map<FileID, std::set<FileID>>			m_uses;					// 1.  use�б�
		std::set<FileID>							m_rootCycleUse;			// 1.  ���ļ�ѭ�����õ��ļ��б�
		std::set<FileID>							m_allCycleUse;			// 1.  ���ļ�ѭ�����õ��ļ��б�
		std::map<FileID, std::set<FileID>>			m_cycleUsedChildren;	// 1.  ���ļ�ѭ������ȫ�����õĺ���

		//
		std::set<FileID>							m_topUsedIncludes;		// 3.  �������õ�include FileID�б�
		std::vector<FileID>							m_topIncludeIDs;		// 4.  ����ȫ����include FileID�б�
		std::map<string, FileID>					m_files;				// 4.  ȫ����include FileID�б�
		std::map<SourceLocation, const char*>		m_locToFileName;		// 5.  [#include "xxxx.h"�Ĵ���λ��] -> [��Ӧ��xxxx.h�ļ�·��]
		std::map<SourceLocation, SourceRange>		m_locToRange;			// 6.  [#include "xxxx.h"��"xxxx.h"��λ��] -> [#�����һ��˫���ŵ�λ��]
		std::vector<SourceLocation>					m_topUnusedIncludeLocs;	// 7.  ���ļ���ȫ��û�õ���includeλ�ã�֮���Բ�ʹ��FileID��ΪĳЩ�ظ�#include�ᱻ�Զ����������²�δ����FileID��
		std::set<FileID>							m_usedFiles;			// 8.  ȫ�����õ��ļ�
		std::map<FileID, std::set<FileID>>			m_usedChildren;			// 8.  ȫ�����õĺ���
		std::set<SourceLocation>					m_usedLocs;				// 9.  ȫ�����õ�includeλ�ã����ڲ�ѯĳλ���Ƿ����ã�
		std::set<SourceLocation>					m_unusedLocs;			// 10. ȫ��û�õ���includeλ�ã�֮���Բ�ʹ��FileID��ΪĳЩ�ظ�#include�ᱻ�Զ����������²�δ����FileID��
		std::map<FileID, FileID>					m_parentIDs;			// 11. ���ļ�IDӳ���
		std::map<FileID, std::set<FileID>>			m_replaces;				// 12. �ɱ��û���#include�б�
		ReplaceFileMap								m_splitReplaces;		//
		std::vector<HeaderSearchPath>				m_headerSearchPaths;	// 13.
		std::map<FileID, set<const CXXRecordDecl*>>	m_forwardDecls;			// 15. �ļ� -> ��ʹ�õ�ǰ������
		std::map<FileID, std::vector<UseNameInfo>>	m_useNames;				// 1.  use�б�

		Rewriter*									m_rewriter;
		SourceManager*								m_srcMgr;
		// CompilerInstance*							m_compiler;

		// ��ǰ��ӡ��������������־����
		int											m_i;

		// �Ƿ񸲸�ԭ����c++�ļ�
		static bool									m_isOverWriteOption;
	};
}

using namespace cxxcleantool;

#endif // _parsing_cpp_h_