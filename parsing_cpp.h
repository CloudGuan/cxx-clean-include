///<------------------------------------------------------------------------------
//< @file:   parsing_cpp.h
//< @author: ������
//< @date:   2016��2��22��
//< @brief:
//< Copyright (c) 2015 game. All rights reserved.
///<------------------------------------------------------------------------------

#ifndef _parsing_cpp_h_
#define _parsing_cpp_h_

#include <string>
#include <vector>
#include <set>
#include <map>

#include <clang/Basic/SourceLocation.h>
#include <clang/Basic/SourceManager.h>

#include "whole_project.h"

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

	string to_linux_path(const char *path);

	string fix_path(const string& path);

	// ��ǰ���ڽ�����c++�ļ����ڲ�#include��Ϣ
	class ParsingFile
	{
	public:
		struct UseNameInfo
		{
			void add_name(const char* name)
			{
				if (names.find(name) != names.end())
				{
					return;
				}

				names.insert(name);
				nameVec.push_back(name);
			}

			FileID				file;
			std::vector<string>	nameVec;
			std::set<string>	names;
		};

		std::vector<HeaderSearchPath> ComputeHeaderSearchPaths(clang::HeaderSearch &headerSearch);

		std::vector<HeaderSearchPath> SortHeaderSearchPath(const std::map<string, SrcMgr::CharacteristicKind>& include_dirs_map);

		std::string FindFileInSearchPath(const vector<HeaderSearchPath>& searchPaths, const std::string& fileName);

		void generate_unused_top_include();

		inline bool is_loc_be_used(SourceLocation loc) const;

		void direct_use_include(FileID ancestor, FileID beusedFile);

		void generate_result();

		void generate_root_cycle_use();

		void generate_all_cycle_use();

		bool try_add_ancestor_of_cycle_use();

		void generate_root_cycle_used_children();

		// ���������ļ������ؽ����ʾ�Ƿ�������һЩ��������ļ�����true����false
		bool expand_all_cycle_use(FileID newCycleUse);

		FileID get_can_replace_to(FileID top);

		void get_remain_node(std::set<FileID> &remain, const std::set<FileID> &history);

		void generate_direct_use_include();

		void generate_unused_include();

		// ��ָ�����ļ��б����ҵ����ڴ����ļ��ĺ��
		std::set<FileID> get_children(FileID ancestor, std::set<FileID> all_children/* ������ancestor���ӵ��ļ� */);

		// ���ļ�������ҵ���ָ���ļ�ʹ�õ������ļ�
		std::set<FileID> get_depend_on_file(FileID ancestor, FileID child);

		void get_cycle_use_file(FileID top, std::set<FileID> &out);

		int get_depth(FileID child);

		// ��ȡ�뺢��������Ĺ�ͬ����
		FileID get_common_ancestor(const std::set<FileID> &children);

		// ��ȡ2������������Ĺ�ͬ����
		FileID get_common_ancestor(FileID child_1, FileID child_2);

		// �ҵ�Ŀ���ļ���λ��beforeǰ�����һ��#include��λ��
		SourceLocation get_last_include_loc_before(FileID at, SourceLocation before);

		// �ҵ�Ŀ���ļ������һ��#include��λ��
		SourceLocation get_last_include_loc(FileID at);

		// ��ȡtop�����б������ĺ����ļ�
		// ��ȡtop�����к����ļ���child�������Ĳ���
		std::set<FileID> get_family_of(FileID top);

		void generate_used_children();

		void generate_can_replace();

		void generate_can_replace_include_in_root();

		void generate_can_forward_declaration();

		void generate_count();

		std::string get_source_of_range(SourceRange range);

		std::string get_source_of_line(SourceLocation loc);

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
		SourceRange get_cur_line(SourceLocation loc);

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
		SourceRange get_cur_line_with_linefeed(SourceLocation loc);

		// ���ݴ���Ĵ���λ�÷�����һ�еķ�Χ
		SourceRange get_next_line(SourceLocation loc);

		bool is_empty_line(SourceRange line);

		int get_line_no(SourceLocation loc);

		// �Ƿ��ǻ��з�
		bool is_new_line_word(SourceLocation loc);

		/*
			��ȡ��Ӧ�ڴ����ļ�ID��#include�ı�
			���磬������b.cpp���������£�
				1. #include "./a.h"
				2. #include "a.h"

			���е�1�к͵�2�а�����a.h����Ȼͬһ���ļ�����FileID�ǲ�һ����

			�ִ����һ��a.h��FileID������������
			#include "./a.h"
		*/
		std::string get_raw_include_str(FileID file);

		// curλ�õĴ���ʹ��srcλ�õĴ���
		void use(SourceLocation cur, SourceLocation src, const char* name = nullptr);

		void use_name(FileID file, FileID beusedFile, const char* name = nullptr);

		// ���ݵ�ǰ�ļ������ҵ�2������ȣ���rootΪ��һ�㣩������ǰ�ļ������ڵ�2�㣬�򷵻ص�ǰ�ļ�
		FileID get_lvl_2_ancestor(FileID file, FileID root);

		// ��ȡ���ļ����������ֵ�ͬ��������
		FileID get_same_lvl_ancestor(FileID child, FileID ancestor_brother);

		// �ļ�2�Ƿ����ļ�1������
		bool is_ancestor(FileID yound, FileID old) const;

		// �ļ�2�Ƿ����ļ�1������
		bool is_ancestor(FileID yound, const char* old) const;

		// �ļ�2�Ƿ����ļ�1������
		bool is_ancestor(const char* yound, FileID old) const;

		bool is_common_ancestor(const std::set<FileID> &children, FileID old);

		bool has_parent(FileID child);

		FileID get_parent(FileID child);

		void direct_use_include_up_by_up(FileID file, FileID beusedFile);

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
		void direct_use_include_by_family(FileID file, FileID beusedFile);

		// ��ǰ�ļ�ʹ��Ŀ���ļ�
		void use_include(FileID file, FileID beusedFile, const char* name = nullptr);

		void use_macro(SourceLocation loc, const MacroDefinition &macro, const Token &macroName);

		// ��ǰλ��ʹ��Ŀ������
		void use_type(SourceLocation loc, const QualType &t);

		string get_cxxrecord_name(const CXXRecordDecl &cxxRecordDecl);

		bool LexSkipComment(SourceLocation Loc, Token &Result);

		// ���ز���ǰ�����������еĿ�ͷ
		SourceLocation get_insert_forward_line(FileID at, const CXXRecordDecl &cxxRecord);

		void use_forward(SourceLocation loc, const CXXRecordDecl *cxxRecordDecl);

		void use_var(SourceLocation loc, const QualType &var);

		void on_use_decl(SourceLocation loc, const NamedDecl *nameDecl);

		void on_use_record(SourceLocation loc, const CXXRecordDecl *record);

		void print_parents_by_id();

		string debug_file_include_text(FileID file, bool is_absolute_name = false);

		string debug_file_direct_use_text(FileID file);

		string debug_loc_text(SourceLocation loc);

		string debug_loc_include_text(SourceLocation loc);

		void print_use();

		void print_used_names();

		void print_root_cycle_used_names();

		void print_used_children();

		void print_all_file();

		void print_direct_used_include();

		void print_used_top_include();

		void print_top_include_by_id();

		static void print_unused_include_of_files(FileMap &files);

		static void print_can_forwarddecl_of_files(FileMap &files);

		static void print_can_replace_of_files(FileMap &files);

		void print_unused_include();

		void print_can_forwarddecl();

		void print_unused_top_include();

		const char* get_file_name(FileID file) const;

		static string get_absolute_file_name(const char *raw_file_name);

		string get_absolute_file_name(FileID file) const;

		inline bool is_blank(char c)
		{
			return (c == ' ' || c == '\t');
		}

		const char* get_include_value(const char* include_str);

		std::string get_relative_include_str(FileID f1, FileID f2);

		bool is_absolute_path(const string& path);

		// ��·��
		// d:/a/b/c/../../d/ -> d:/d/
		static std::string simplify_path(const char* path);

		static string get_absolute_path(const char *path);

		static string get_absolute_path(const char *base_path, const char* relative_path);

		string get_parent_path(const char* path);

		// ����ͷ�ļ�����·����������·��ת��Ϊ˫���Ű�Χ���ı�����
		// ���磺������ͷ�ļ�����·��"d:/a/b/c" ��"d:/a/b/c/d/e.h" -> "d/e.h"
		string get_quoted_include_str(const string& absoluteFilePath);

		// ɾ��ָ��λ�����ڵ����д���
		void erase_line_by_loc(SourceLocation loc);

		void clean_unused_include_by_loc(SourceLocation unused_loc);

		void clean_unused_include_in_all_file();

		void clean_unused_include_in_root();

		void clean_can_replace_in_root();

		void add_forward_declaration(FileID file);

		void clean_by_forwarddecl_in_all_file();

		void clean();

		void replace_text(FileID file, int beg, int end, string text);

		void insert_text(FileID file, int loc, string text);

		void remove_text(FileID file, int beg, int end);

		void clean_by_unused_line(const EachFile &eachFile, FileID file);

		void clean_by_forward(const EachFile &eachFile, FileID file);

		void clean_by_replace(const EachFile &eachFile, FileID file);

		void clean_by(const FileMap &files);

		void clean_all_file();

		void clean_main_file();

		void print_can_replace();

		void print_header_search_path();

		void print_relative_include();

		void print_range_to_filename();

		// ���ڵ��Ը���
		void print_not_found_include_loc();

		// ���ڵ��Ը���
		void print_same_line();

		void print();

		bool init_header_search_path(clang::SourceManager* srcMgr, clang::HeaderSearch &header_search);

		// �ļ���ʽ�Ƿ���windows��ʽ�����з�Ϊ[\r\n]����Unix��Ϊ[\n]
		bool IsWindowsFormat(FileID);

		void merge_to(FileMap &old);

		void merge_unused_line_to(const EachFile &newFile, EachFile &oldFile) const;

		void merge_forward_line_to(const EachFile &newFile, EachFile &oldFile) const;

		void merge_replace_line_to(const EachFile &newFile, EachFile &oldFile) const;

		void merge_count_to(FileMap &oldFiles) const;

		void take_all_info_to(FileMap &out);

		// ����������а��ļ����д��
		void take_unused_line_by_file(FileMap &out);

		// ��������ǰ���������ļ����д��
		void take_new_forwarddecl_by_file(FileMap &out);

		bool is_force_included(FileID file);

		typedef std::map<FileID, std::set<FileID>> ChildrenReplaceMap;
		typedef std::map<FileID, ChildrenReplaceMap> ReplaceFileMap;
		void split_replace_by_file(ReplaceFileMap &replaces);

		void take_be_replace_of_file(EachFile &eachFile, FileID top, const ChildrenReplaceMap &childernReplaces);

		// ȡ�����ļ���#include�滻��Ϣ
		void take_replace_by_file(FileMap &out);

		bool is_cycly_used(FileID file);

		bool is_replaced(FileID file);

	public:
		// ѭ�������б�
		std::map<FileID, std::set<FileID>>			m_uses;					// 1.  use�б�
		std::set<FileID>							m_rootCycleUse;			// 1.  ���ļ�ѭ�����õ��ļ��б�
		std::set<FileID>							m_allCycleUse;			// 1.  ���ļ�ѭ�����õ��ļ��б�
		std::map<FileID, std::set<FileID>>			m_cycleUsedChildren;	// 1.  ���ļ�ѭ������ȫ�����õĺ���

		//
		std::map<FileID, std::set<FileID>>			m_direct_uses;			// 2.  ֱ��use�б���a.cppʹ����#include��b.h��#include��c.h����a.cppֱ��ʹ����b.h
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
		std::map<FileID, std::set<const CXXRecordDecl*>>	m_forwardDecls;			// 15. �ļ� -> ��ʹ�õ�ǰ������
		// std::map<FileID, std::set<CXXRecordDecl*>>	m_f;
		std::map<FileID, std::vector<UseNameInfo>>	m_useNames;					// 1.  use�б�

		Rewriter*								m_rewriter;
		SourceManager*							m_srcMgr;

		static bool								m_isOverWriteOption;	// �Ƿ񸲸�ԭ����c++�ļ�
		int										m_i;					// ��ǰ��ӡ��������������־����
	};
}

using namespace cxxcleantool;

#endif // _parsing_cpp_h_