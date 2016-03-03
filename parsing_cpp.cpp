///<------------------------------------------------------------------------------
//< @file:   parsing_cpp.cpp
//< @author: ������
//< @date:   2016��2��22��
//< @brief:
//< Copyright (c) 2015 game. All rights reserved.
///<------------------------------------------------------------------------------

#include "parsing_cpp.h"

#include <sstream>

#include <clang/AST/DeclTemplate.h>
#include <clang/AST/Decl.h>
#include <clang/AST/Type.h>
#include <clang/AST/TemplateName.h>
#include <clang/Lex/HeaderSearch.h>
#include <clang/Lex/MacroInfo.h>
#include <clang/Lex/Lexer.h>
#include <clang/Rewrite/Core/Rewriter.h>
#include <llvm/Support/Path.h>

#include "tool.h"
#include "vs_config.h"

namespace cxxcleantool
{
	bool ParsingFile::m_isOverWriteOption = false;

	// ��·��ת����linux·����ʽ����·���е�ÿ��'\'�ַ����滻Ϊ'/'
	string to_linux_path(const char *path)
	{
		string ret = path;

		// ��'\'�滻Ϊ'/'
		for (size_t i = 0; i < ret.size(); ++i)
		{
			if (ret[i] == '\\')
				ret[i] = '/';
		}

		return ret;
	}

	string fix_path(const string& path)
	{
		string ret = to_linux_path(path.c_str());

		if (!end_with(ret, "/"))
			ret += "/";

		return ret;
	}

	vector<HeaderSearchPath> ParsingFile::ComputeHeaderSearchPaths(clang::HeaderSearch &headerSearch)
	{
		std::map<string, SrcMgr::CharacteristicKind> search_path_map;

		for (clang::HeaderSearch::search_dir_iterator
		        itr = headerSearch.system_dir_begin(), end = headerSearch.system_dir_end();
		        itr != end; ++itr)
		{
			if (const DirectoryEntry* entry = itr->getDir())
			{
				const string path = fix_path(entry->getName());
				search_path_map.insert(make_pair(path, SrcMgr::C_System));
			}
		}

		for (clang::HeaderSearch::search_dir_iterator
		        itr = headerSearch.search_dir_begin(), end = headerSearch.search_dir_end();
		        itr != end; ++itr)
		{
			if (const DirectoryEntry* entry = itr->getDir())
			{
				const string path = fix_path(entry->getName());
				search_path_map.insert(make_pair(path, SrcMgr::C_User));
			}
		}

		return SortHeaderSearchPath(search_path_map);
	}

	static bool sort_by_longest_length_first(const HeaderSearchPath& left, const HeaderSearchPath& right)
	{
		return left.path.length() > right.path.length();
	}

	std::vector<HeaderSearchPath> ParsingFile::SortHeaderSearchPath(const std::map<string, SrcMgr::CharacteristicKind>& include_dirs_map)
	{
		std::vector<HeaderSearchPath> include_dirs;

		for (auto itr : include_dirs_map)
		{
			const string &path						= itr.first;
			SrcMgr::CharacteristicKind path_type	= itr.second;

			string absolute_path = get_absolute_file_name(path.c_str());

			include_dirs.push_back(HeaderSearchPath(absolute_path, path_type));
		}

		sort(include_dirs.begin(), include_dirs.end(), &sort_by_longest_length_first);
		return include_dirs;
	}

	std::string ParsingFile::FindFileInSearchPath(const vector<HeaderSearchPath>& searchPaths, const std::string& fileName)
	{
		if (llvm::sys::fs::exists(fileName))
		{
			return get_absolute_path(fileName.c_str());
		}
		// ��Ϊ���·��
		else if (!is_absolute_path(fileName))
		{
			// ������·���в���
			for (const HeaderSearchPath &search_path : searchPaths)
			{
				string candidate = get_absolute_path(search_path.path.c_str(), fileName.c_str());
				if (llvm::sys::fs::exists(candidate))
				{
					return candidate;
				}
			}
		}

		return fileName;
	}

	string ParsingFile::get_quoted_include_str(const string& absoluteFilePath)
	{
		string path = simplify_path(absoluteFilePath.c_str());

		for (const HeaderSearchPath &itr :m_headerSearchPaths)
		{
			if (strtool::try_strip_left(path, itr.path))
			{
				if (itr.path_type == SrcMgr::C_System)
				{
					return "<" + path + ">";
				}
				else
				{
					return "\"" + path + "\"";
				}
			}
		}

		return "";
	}

	void ParsingFile::generate_unused_top_include()
	{
		for (SourceLocation loc : m_unusedLocs)
		{
			if (!m_srcMgr->isInMainFile(loc))
			{
				continue;
			}

			m_topUnusedIncludeLocs.push_back(loc);
		}
	}

	inline bool ParsingFile::is_loc_be_used(SourceLocation loc) const
	{
		return m_usedLocs.find(loc) != m_usedLocs.end();
	}

	void ParsingFile::direct_use_include(FileID top, FileID beusedFile)
	{
		if (top == beusedFile)
		{
			return;
		}

		// �ӱ�ʹ���ļ��ĸ��ڵ㣬������Ͻ������ø������ù�ϵ
		for (FileID child = beusedFile; has_parent(child);)
		{
			//				if (bool hasProcessed = (processedList.find(child) != processedList.end()))
			//				{
			//					break;
			//				}

			if (child == top)
			{
				break;
			}

			FileID parent = m_parentIDs[child];

			m_direct_uses[parent].insert(child);
			m_usedLocs.insert(m_srcMgr->getIncludeLoc(child));

			child = parent;
		}
	}

	void ParsingFile::generate_result()
	{
		generate_root_cycle_use();
		generate_root_cycle_used_children();
		generate_all_cycle_use();
		generate_direct_use_include();
		generate_used_children();
		generate_unused_include();
		generate_can_forward_declaration();
		generate_can_replace();
	}

	void ParsingFile::generate_root_cycle_use()
	{
		if (m_uses.empty())
		{
			return;
		}

		FileID mainFileID = m_srcMgr->getMainFileID();

		get_cycle_use_file(mainFileID, m_rootCycleUse);

		if (m_rootCycleUse.empty())
		{
			return;
		}
	}

	void ParsingFile::generate_root_cycle_used_children()
	{
		for (FileID usedFile : m_rootCycleUse)
		{
			if (!has_parent(usedFile))
			{
				continue;
			}

			FileID parent = m_parentIDs[usedFile];
			if (!Vsproject::instance.has_file(get_absolute_file_name(parent)))
			{
				continue;
			}

			for (FileID child = usedFile, parent; has_parent(child); child = parent)
			{
				parent = m_parentIDs[child];
				m_cycleUsedChildren[parent].insert(usedFile);
			}
		}
	}

	FileID ParsingFile::get_can_replace_to(FileID top)
	{
		// ���ļ�����Ҳ��ʹ�õ������޷����滻
		if (is_cycly_used(top))
		{
			return FileID();
		}

		auto itr = m_cycleUsedChildren.find(top);
		if (itr == m_cycleUsedChildren.end())
		{
			return FileID();
		}

		const std::set<FileID> &children = itr->second;		// ���õĺ���ļ�

		// 1. �����õĺ���ļ� -> ֱ������������������ᷢ����
		if (children.empty())
		{
			return FileID();
		}
		// 2. ����һ�����õĺ���ļ� -> ��ǰ�ļ��ɱ��ú���滻
		else if (children.size() == 1)
		{
			return *(children.begin());
		}
		// 3. �ж�����õĺ���ļ������ձ������� -> ���滻Ϊ����ļ��Ĺ�ͬ����
		else
		{
			// ��ȡ���к���ļ��������ͬ����
			FileID ancestor = get_common_ancestor(children);
			if (ancestor == top)
			{
				return FileID();
			}

			// ������ļ��ǵ������ͬ�����Ծ��ǵ�ǰ���ȵĺ�������滻Ϊ����ļ��ǵĹ�ͬ����
			if (is_ancestor(ancestor, top))
			{
				return ancestor;
			}
		}

		return FileID();
	}

	bool ParsingFile::try_add_ancestor_of_cycle_use()
	{
		FileID mainFile = m_srcMgr->getMainFileID();

		for (auto itr = m_rootCycleUse.rbegin(); itr != m_rootCycleUse.rend(); ++itr)
		{
			FileID file = *itr;

			for (FileID top = mainFile, lv2Top; top != file; top = lv2Top)
			{
				lv2Top = get_lvl_2_ancestor(file, top);

				if (is_cycly_used(lv2Top) || is_replaced(lv2Top))
				{
					continue;
				}

				FileID canReplaceTo = get_can_replace_to(lv2Top);

				bool expand = false;

				// �����ɱ��滻
				if (canReplaceTo.isInvalid())
				{
					expand = expand_all_cycle_use(lv2Top);
					m_replaces[lv2Top].insert(canReplaceTo);
				}
				// ���ɱ��滻
				else
				{
					expand = expand_all_cycle_use(canReplaceTo);
				}

				if (expand)
				{
					return true;
				}
			}
		}

		return false;
	}

	void ParsingFile::generate_all_cycle_use()
	{
		while (try_add_ancestor_of_cycle_use())
		{
		};
	}

	bool ParsingFile::expand_all_cycle_use(FileID newCycleUse)
	{
		std::set<FileID> cycleUseList;
		get_cycle_use_file(newCycleUse, cycleUseList);

		int oldSize = m_allCycleUse.size();

		m_allCycleUse.insert(cycleUseList.begin(), cycleUseList.end());

		int newSize = m_allCycleUse.size();

		return newSize > oldSize;
	}

	bool ParsingFile::is_cycly_used(FileID file)
	{
		if (file == m_srcMgr->getMainFileID())
		{
			return true;
		}

		return m_allCycleUse.find(file) != m_allCycleUse.end();
	}

	bool ParsingFile::is_replaced(FileID file)
	{
		return m_replaces.find(file) != m_replaces.end();
	}

	void ParsingFile::get_remain_node(std::set<FileID> &remain, const std::set<FileID> &history)
	{
		for (FileID file : history)
		{
			// �ӱ�ʹ���ļ��ĸ��ڵ㣬������ϲ���δ��ʹ�õ�
			for (FileID child = file; has_parent(child);)
			{
				FileID parent = m_parentIDs[child];

				child = parent;

				if (history.find(parent) != history.end())
				{
					continue;
				}

				remain.insert(parent);
			}
		}
	}

	void ParsingFile::generate_direct_use_include()
	{
		std::set<FileID> history = m_rootCycleUse;
		std::set<FileID> remainList;

		{
			history.insert(m_srcMgr->getMainFileID());

			get_remain_node(remainList, history);

			while (!remainList.empty())
			{
				for (FileID remain : remainList)
				{
					get_cycle_use_file(remain, history);
					history.insert(remain);
				}

				remainList.clear();
				get_remain_node(remainList, history);
			}
		}

		m_allCycleUse = history;

		for (FileID beusedFile : history)
		{
			// �ӱ�ʹ���ļ��ĸ��ڵ㣬������Ͻ������ø������ù�ϵ
			for (FileID child = beusedFile; has_parent(child);)
			{
				FileID parent = m_parentIDs[child];

				m_direct_uses[parent].insert(child);
				m_usedLocs.insert(m_srcMgr->getIncludeLoc(child));

				child = parent;
			}
		}

		////////////////////////

		/*
			for (auto itr : m_uses)
			{
				const FileID file					= itr.first;
				const std::set<FileID> &beusedFiles	= itr.second;

				if (!is_cycly_used(file))
				{
					continue;
				}

				for (FileID beusedFile : beusedFiles)
				{
					const FileID ancestor = get_common_ancestor(file, beusedFile);
					direct_use_include(ancestor, beusedFile);

					m_usedFiles.insert(beusedFile);
				}
			}
		*/
	}

	void ParsingFile::generate_unused_include()
	{
		for (auto locItr : m_locToRange)
		{
			SourceLocation loc = locItr.first;
			FileID file = m_srcMgr->getFileID(loc);

			if (!is_cycly_used(file))
			{
				continue;
			}

			if (!is_loc_be_used(loc))
			{
				m_unusedLocs.insert(loc);
			}
		}

		generate_unused_top_include();
	}

	// ��ָ�����ļ��б����ҵ����ڴ����ļ��ĺ��
	std::set<FileID> ParsingFile::get_children(FileID ancestor, std::set<FileID> all_children/* ������ancestor���ӵ��ļ� */)
	{
		// ����ancestor�ĺ���ļ��б�
		std::set<FileID> children;

		// llvm::outs() << "ancestor = " << get_file_name(ancestor) << "\n";

		// ��ȡ���ڸ��ļ��ĺ���б����ļ�ʹ�õ�һ����
		for (FileID child : all_children)
		{
			if (!is_ancestor(child, ancestor))
			{
				continue;
			}

			// llvm::outs() << "    child = " << get_file_name(child) << "\n";
			children.insert(child);
		}

		return children;
	}

	void ParsingFile::get_cycle_use_file(FileID top, std::set<FileID> &out)
	{
		auto topUseItr = m_uses.find(top);
		if (topUseItr == m_uses.end())
		{
			return;
		}

		std::set<FileID> left;
		std::set<FileID> history;

		const std::set<FileID> &topUseFiles = topUseItr->second;
		left.insert(topUseFiles.begin(), topUseFiles.end());

		// ѭ����ȡchild���õ��ļ��������ϻ�ȡ�������ļ����������ļ�
		while (!left.empty())
		{
			FileID cur = *left.begin();
			left.erase(left.begin());

			history.insert(cur);

			auto useItr = m_uses.find(cur);
			if (useItr == m_uses.end())
			{
				continue;
			}

			const std::set<FileID> &useFiles = useItr->second;

			for (const FileID &used : useFiles)
			{
				if (history.find(used) != history.end())
				{
					continue;
				}

				left.insert(used);
			}
		}

		out.insert(history.begin(), history.end());
	}

	// ���ļ�������ҵ���ָ���ļ�ʹ�õ������ļ�
	std::set<FileID> ParsingFile::get_depend_on_file(FileID ancestor, FileID child)
	{
		std::set<FileID> all;

		std::set<FileID> left;
		left.insert(child);

		std::set<FileID> history;

		// ѭ����ȡchild���õ��ļ��������ϻ�ȡ�������ļ����������ļ�
		while (!left.empty())
		{
			FileID cur = *left.begin();
			left.erase(left.begin());

			history.insert(cur);

			if (!is_ancestor(cur, ancestor))
			{
				continue;
			}

			all.insert(cur);

			bool has_child = (m_uses.find(cur) != m_uses.end());
			if (!has_child)
			{
				continue;
			}

			std::set<FileID> &use_list = m_uses[cur];

			for (const FileID &used : use_list)
			{
				if (history.find(used) != history.end())
				{
					continue;
				}

				left.insert(used);
			}
		}

		return all;
	}

	int ParsingFile::get_depth(FileID child)
	{
		int depth = 0;

		while (bool has_parent = (m_parentIDs.find(child) != m_parentIDs.end()))
		{
			child = m_parentIDs[child];
			++depth;
		}

		return depth;
	}

	// ��ȡ�뺢��������Ĺ�ͬ����
	FileID ParsingFile::get_common_ancestor(const std::set<FileID> &children)
	{
		FileID highest_child;
		int max_depth = 0;

		for (const FileID &child : children)
		{
			int depth = get_depth(child);

			if (max_depth == 0 || depth < max_depth)
			{
				highest_child = child;
				max_depth = depth;
			}
		}

		bool found = false;

		FileID ancestor = highest_child;
		while (!is_common_ancestor(children, ancestor))
		{
			bool has_parent = (m_parentIDs.find(ancestor) != m_parentIDs.end());
			if (!has_parent)
			{
				return m_srcMgr->getMainFileID();
			}

			ancestor = m_parentIDs[ancestor];
		}

		return ancestor;
	}

	// ��ȡ2������������Ĺ�ͬ����
	FileID ParsingFile::get_common_ancestor(FileID child_1, FileID child_2)
	{
		if (child_1 == child_2)
		{
			return child_1;
		}

		int deepth_1	= get_depth(child_1);
		int deepth_2	= get_depth(child_2);

		FileID old		= (deepth_1 < deepth_2 ? child_1 : child_2);
		FileID young	= (old == child_1 ? child_2 : child_1);

		// �ӽϸ߲���ļ����ϲ��Ҹ��ļ���ֱ���ø��ļ�ҲΪ����һ���ļ���ֱϵ����Ϊֹ
		while (!is_ancestor(young, old))
		{
			bool has_parent = (m_parentIDs.find(old) != m_parentIDs.end());
			if (!has_parent)
			{
				break;
			}

			old = m_parentIDs[old];
		}

		return old;
	}

	// �ҵ�Ŀ���ļ���λ��beforeǰ�����һ��#include��λ��
	SourceLocation ParsingFile::get_last_include_loc_before(FileID at, SourceLocation before)
	{
		if (at.isInvalid())
		{
			return SourceLocation();
		}

		SourceLocation lastLoc;

		for (SourceLocation loc : m_usedLocs)
		{
			if (m_srcMgr->getFileID(loc) != at)
			{
				continue;
			}

			if (!(loc < before))
			{
				continue;
			}

			if (lastLoc.isInvalid() || lastLoc < loc)
			{
				lastLoc = loc;
			}
		}

		return lastLoc;
	}

	// �ҵ�Ŀ���ļ������һ��#include��λ��
	SourceLocation ParsingFile::get_last_include_loc(FileID at)
	{
		if (at.isInvalid())
		{
			return SourceLocation();
		}

		SourceLocation lastLoc;

		for (SourceLocation loc : m_usedLocs)
		{
			if (m_srcMgr->getFileID(loc) != at)
			{
				continue;
			}

			if (lastLoc.isInvalid() || lastLoc < loc)
			{
				lastLoc = loc;
			}
		}

		return lastLoc;
	}

	// ��ȡtop�����б������ĺ����ļ�
	// ��ȡtop�����к����ļ���child�������Ĳ���
	std::set<FileID> ParsingFile::get_family_of(FileID top)
	{
		std::set<FileID> family;

		auto useItr = m_uses.find(top);
		if (useItr == m_uses.end())
		{
			return family;
		}

		const std::set<FileID> &useFiles		= useItr->second;		// ���õ��ļ��б�

		for (FileID ancestor : useFiles)
		{
			// 1. ��ȡancestor�ļ��ĺ���б����ļ�ʹ�õ��ǲ����ļ�
			const std::set<FileID> children = get_children(ancestor, useFiles);

			for (const FileID &child : children)
			{
				const std::set<FileID> depend_on_list = get_depend_on_file(ancestor, child);
				family.insert(depend_on_list.begin(), depend_on_list.end());
			}
		}

		return family;
	}

	void ParsingFile::generate_used_children()
	{
		for (FileID usedFile : m_usedFiles)
		{
			if (!has_parent(usedFile))
			{
				continue;
			}

			FileID parent = m_parentIDs[usedFile];
			if (!Vsproject::instance.has_file(get_absolute_file_name(parent)))
			{
				continue;
			}

			for (FileID child = usedFile, parent; has_parent(child); child = parent)
			{
				parent = m_parentIDs[child];
				m_usedChildren[parent].insert(usedFile);
			}
		}
	}

	void ParsingFile::generate_can_replace_include_in_root()
	{
		FileID mainFileID = m_srcMgr->getMainFileID();

		auto itr = m_uses.find(mainFileID);
		if (itr == m_uses.end())
		{
			return;
		}

		const std::set<FileID> &rootUseFiles = itr->second; // ���ļ����õ��ļ��б�
		const std::set<FileID> &rootDirectUseFiles = m_direct_uses[mainFileID]; // ���ļ�ֱ�����õ��ļ��б�

		// �������ļ���#include�����ļ�
		for (FileID ancestor : rootDirectUseFiles)
		{
			// ������ļ��Ƿ�ֱ��ʹ���˸��ļ�
			std::set<FileID>::iterator itr = rootUseFiles.find(ancestor);
			if (itr != rootUseFiles.end())
			{
				// ˵�����ļ�ֱ��ʹ���˸��ļ������ݣ�����Ҫ�ٴ���
				continue;
			}

			///////// ˵�����ļ���ʹ���˸��ļ�#include�������ļ������Գ���ʹ�ø��ļ�#include���ļ������ /////////

			// 1. ��ȡancestor�ļ��ĺ���б����ļ�ʹ�õ��ǲ����ļ�
			std::set<FileID> children = get_children(ancestor, rootUseFiles);

			std::set<FileID> all;

			for (const FileID &child : children)
			{
				std::set<FileID> depend_on_list = get_depend_on_file(ancestor, child);
				all.insert(depend_on_list.begin(), depend_on_list.end());
			}

			if (all.empty())
			{
				continue;
			}

			bool can_replace = false;

			if (all.size() == 1)
			{
				m_replaces[ancestor] = all;
			}
			else
			{
				FileID deepest_ancestor = get_common_ancestor(children);
				if (is_ancestor(deepest_ancestor, ancestor))
				{
					// llvm::outs() << "file = " << get_file_name(mainFileID) << "]\n";
					// llvm::outs() << "    is_ancestor(deepest_ancestor[" << get_file_name(deepest_ancestor) << "], ancestor[" << get_file_name(ancestor) << "])" << "\n";
					m_replaces[ancestor].insert(deepest_ancestor);
				}
				else if (4 * all.size() <= children.size())
				{
					// llvm::outs() << "4 * all.size()[" << all.size() << "] <= children.size()[" << children.size() << "]" << "\n";
					m_replaces[ancestor] = all;
				}
			}
		}
	}

	void ParsingFile::generate_can_replace()
	{
		// ����ÿ���ļ��������ļ������ú���ļ����д���
		for (auto itr : m_usedChildren)
		{
			FileID top					= itr.first;	// �����ļ�
			std::set<FileID> &children	= itr.second;	// ���õĺ���ļ�

			if (m_usedFiles.find(top) != m_usedFiles.end())
			{
				continue;
			}

			if (!is_cycly_used(top))
			{
				continue;
			}

			if (!is_loc_be_used(m_srcMgr->getIncludeLoc(top)))
			{
				continue;
			}

			// 1. �����õĺ���ļ� -> ֱ������������������ᷢ����
			if (children.empty())
			{
				continue;
			}
			// 2. ����һ�����õĺ���ļ� -> ��ǰ�ļ��ɱ��ú���滻
			else if (children.size() == 1)
			{
				m_replaces[top] = children;
			}
			// 3. �ж�����õĺ���ļ������ձ������� -> ���滻Ϊ����ļ��Ĺ�ͬ����
			else
			{
				// ��ȡ���к���ļ��������ͬ����
				FileID ancestor = get_common_ancestor(children);
				if (ancestor == top)
				{
					continue;
				}

				// ������ļ��ǵ������ͬ�����Ծ��ǵ�ǰ���ȵĺ�������滻Ϊ����ļ��ǵĹ�ͬ����
				if (is_ancestor(ancestor, top))
				{
					m_replaces[ancestor].insert(ancestor);
				}
			}
		}

		split_replace_by_file(m_splitReplaces);
	}

	void ParsingFile::generate_count()
	{
	}

	void ParsingFile::generate_can_forward_declaration()
	{
		// ����ʹ���ļ���ǰ��������¼ɾ��
		for (auto eachUse : m_uses)
		{
			const FileID file				= eachUse.first;
			const std::set<FileID> &useList = eachUse.second;

			auto findForwardItr = m_forwardDecls.find(file);
			if (findForwardItr == m_forwardDecls.end())
			{
				continue;
			}

			std::set<const CXXRecordDecl*> &old_forwards = findForwardItr->second;

			std::set<const CXXRecordDecl*> can_forwards;

			for (const CXXRecordDecl* cxxRecordDecl : old_forwards)
			{
				FileID in_file = m_srcMgr->getFileID(cxxRecordDecl->getLocStart());

				// �������ڵ��ļ������ã�����Ҫ�ټ�ǰ������
				if (useList.find(in_file) == useList.end())
				{
					can_forwards.insert(cxxRecordDecl);
				}
			}

			if (can_forwards.size() < old_forwards.size())
			{
				old_forwards = can_forwards;
			}
		}
	}

	std::string ParsingFile::get_source_of_range(SourceRange range)
	{
		SourceManager &srcMgr = m_rewriter->getSourceMgr();
		bool err1 = true;
		bool err2 = true;

		const char* beg = srcMgr.getCharacterData(range.getBegin(), &err1);
		const char* end = srcMgr.getCharacterData(range.getEnd(), &err2);

		if (err1 || err2)
		{
			return "";
		}

		return string(beg, end);
	}

	std::string ParsingFile::get_source_of_line(SourceLocation loc)
	{
		return get_source_of_range(get_cur_line(loc));
	}

	/*
		���ݴ���Ĵ���λ�÷��ظ��еķ�Χ��[���п�ͷ������ĩ���������з��� + 1]
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
	SourceRange ParsingFile::get_cur_line(SourceLocation loc)
	{
		SourceLocation fileBeginLoc = m_srcMgr->getLocForStartOfFile(m_srcMgr->getFileID(loc));
		SourceLocation fileEndLoc = m_srcMgr->getLocForEndOfFile(m_srcMgr->getFileID(loc));

		bool err1 = true;
		bool err2 = true;
		bool err3 = true;

		const char* character	= m_srcMgr->getCharacterData(loc, &err1);
		const char* fileStart	= m_srcMgr->getCharacterData(fileBeginLoc, &err2);
		const char* fileEnd		= m_srcMgr->getCharacterData(fileEndLoc, &err3);
		if (err1 || err2 || err3)
		{
			llvm::outs() << "get_cur_line error!" << "\n";
			return SourceRange();
		}

		int left = 0;
		int right = 0;

		for (const char* c = character - 1; c >= fileStart	&& *c && *c != '\n' && *c != '\r'; --c, ++left) {}
		for (const char* c = character;		c < fileEnd		&& *c && *c != '\n' && *c != '\r'; ++c, ++right) {}

		SourceLocation lineBeg = loc.getLocWithOffset(-left);
		SourceLocation lineEnd = loc.getLocWithOffset(right);

		return SourceRange(lineBeg, lineEnd);
	}

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
	SourceRange ParsingFile::get_cur_line_with_linefeed(SourceLocation loc)
	{
		SourceRange curLine		= get_cur_line(loc);
		SourceRange nextLine	= get_next_line(loc);

		return SourceRange(curLine.getBegin(), nextLine.getBegin());
	}

	// ���ݴ���Ĵ���λ�÷�����һ�еķ�Χ
	SourceRange ParsingFile::get_next_line(SourceLocation loc)
	{
		SourceRange curLine		= get_cur_line(loc);
		SourceLocation lineEnd	= curLine.getEnd();

		bool err1 = true;
		bool err2 = true;

		const char* c1			= m_srcMgr->getCharacterData(lineEnd, &err1);
		const char* c2			= m_srcMgr->getCharacterData(lineEnd.getLocWithOffset(1), &err2);

		if (err1 || err2)
		{
			llvm::outs() << "get_next_line error!" << "\n";

			SourceLocation fileEndLoc	= m_srcMgr->getLocForEndOfFile(m_srcMgr->getFileID(loc));
			return SourceRange(fileEndLoc, fileEndLoc);
		}

		int skip = 0;

		// windows���и�ʽ����
		if (*c1 == '\r' && *c2 == '\n')
		{
			skip = 2;
		}
		// ��Unix���и�ʽ����
		else if (*c1 == '\n')
		{
			skip = 1;
		}

		SourceRange nextLine	= get_cur_line(lineEnd.getLocWithOffset(skip));
		return nextLine;
	}

	bool ParsingFile::is_empty_line(SourceRange line)
	{
		string lineText = get_source_of_range(line);
		for (int i = 0, len = lineText.size(); i < len; ++i)
		{
			char c = lineText[i];

			if (c != '\n' && c != '\t' && c != ' ' && c != '\f' && c != '\v' && c != '\r')
			{
				return false;
			}
		}

		return true;
	}

	inline int ParsingFile::get_line_no(SourceLocation loc)
	{
		PresumedLoc presumed_loc	= m_srcMgr->getPresumedLoc(loc);
		return presumed_loc.isValid() ? presumed_loc.getLine() : 0;
	}

	// �Ƿ��ǻ��з�
	bool ParsingFile::is_new_line_word(SourceLocation loc)
	{
		string text = get_source_of_range(SourceRange(loc, loc.getLocWithOffset(1)));
		return text == "\r" || text == "\n";
	}

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
	std::string ParsingFile::get_raw_include_str(FileID file)
	{
		SourceManager &srcMgr = m_rewriter->getSourceMgr();
		SourceLocation loc = srcMgr.getIncludeLoc(file);

		if (m_locToRange.find(loc) == m_locToRange.end())
		{
			return "";
		}

		SourceRange range = m_locToRange[loc];
		return get_source_of_range(range);
	}

	// curλ�õĴ���ʹ��srcλ�õĴ���
	void ParsingFile::use(SourceLocation cur, SourceLocation src, const char* name /* = nullptr */)
	{
		FileID curFileID = m_srcMgr->getFileID(cur);
		FileID srcFileID = m_srcMgr->getFileID(src);

		use_include(curFileID, srcFileID, name);
	}

	void ParsingFile::use_name(FileID file, FileID beusedFile, const char* name /* = nullptr */)
	{
		if (nullptr == name)
		{
			return;
		}

		std::vector<UseNameInfo> &useNames = m_useNames[file];

		bool found = false;

		for (UseNameInfo &info : useNames)
		{
			if (info.file == beusedFile)
			{
				found = true;
				info.add_name(name);
				break;
			}
		}

		if (!found)
		{
			UseNameInfo info;
			info.file = beusedFile;
			info.add_name(name);

			useNames.push_back(info);
		}
	}

	// ���ݵ�ǰ�ļ������ҵ�2������ȣ���rootΪ��һ�㣩������ǰ�ļ������ڵ�2�㣬�򷵻ص�ǰ�ļ�
	FileID ParsingFile::get_lvl_2_ancestor(FileID file, FileID root)
	{
		FileID ancestor;

		while (bool has_parent = (m_parentIDs.find(file) != m_parentIDs.end()))
		{
			FileID parent = m_parentIDs[file];

			// Ҫ�󸸽����root
			if (parent == root)
			{
				ancestor = file;
				break;
			}

			file = parent;
		}

		return ancestor;
	}

	// ��ȡ���ļ����������ֵ�ͬ��������
	FileID ParsingFile::get_same_lvl_ancestor(FileID child, FileID ancestor_brother)
	{
		int ancestor_depth = get_depth(ancestor_brother);
		int child_depth = get_depth(child);

		FileID ancestor;

		if (ancestor_depth > child_depth)
		{
			return FileID();
		}
		else if (ancestor_depth == child_depth)
		{
			return child;
		}

		int now_depth = child_depth;

		while (has_parent(child))
		{
			FileID parent = m_parentIDs[child];

			// �ø�����Ѻ������ֵ�ͬ����˵��������
			if (--now_depth == ancestor_depth)
			{
				ancestor = child;
				break;
			}

			child = parent;
		}

		return ancestor;
	}

	// �ļ�2�Ƿ����ļ�1������
	bool ParsingFile::is_ancestor(FileID young, FileID old) const
	{
		auto parentItr = m_parentIDs.begin();

		while ((parentItr = m_parentIDs.find(young)) != m_parentIDs.end())
		{
			FileID parent = parentItr->second;
			if (parent == old)
			{
				return true;
			}

			young = parent;
		}

		return false;
	}

	// �ļ�2�Ƿ����ļ�1������
	bool ParsingFile::is_ancestor(FileID young, const char* old) const
	{
		auto parentItr = m_parentIDs.begin();

		while ((parentItr = m_parentIDs.find(young)) != m_parentIDs.end())
		{
			FileID parent = parentItr->second;
			if (get_absolute_file_name(parent) == old)
			{
				return true;
			}

			young = parent;
		}

		return false;
	}

	// �ļ�2�Ƿ����ļ�1������
	bool ParsingFile::is_ancestor(const char* young, FileID old) const
	{
		// �ڸ��ӹ�ϵ����������ļ�ͬ����FileID�������#includeͬһ�ļ������Ϊ���ļ���������ͬ��FileID��
		for (auto itr : m_parentIDs)
		{
			FileID child	= itr.first;

			if (get_absolute_file_name(child) == young)
			{
				if (is_ancestor(child, old))
				{
					return true;
				}
			}
		}

		return false;
	}

	bool ParsingFile::is_common_ancestor(const std::set<FileID> &children, FileID old)
	{
		for (const FileID &child : children)
		{
			if (child == old)
			{
				continue;
			}

			if (!is_ancestor(child, old))
			{
				return false;
			}
		}

		return true;
	}

	bool ParsingFile::has_parent(FileID child)
	{
		return m_parentIDs.find(child) != m_parentIDs.end();
	}

	inline FileID ParsingFile::get_parent(FileID child)
	{
		auto itr = m_parentIDs.find(child);
		if (itr == m_parentIDs.end())
		{
			return FileID();
		}

		return itr->second;
	}

	void ParsingFile::direct_use_include_up_by_up(FileID file, FileID beusedFile)
	{
		//			bool isMainFile = (file == m_srcMgr->getMainFileID());
		//
		//			std::set<FileID> direct_use_list;
		//
		//			// ���ļ����ø��ļ��ĵ�2������
		//			{
		//				FileID ancestor = get_lvl_2_ancestor(beusedFile, file);
		//				if (ancestor.isValid())
		//				{
		//					direct_use_list.insert(ancestor);
		//				}
		//			}
		//
		//			// ���ڱ�ʹ���ļ����õ�ÿ�������ļ������ļ������øú����ļ��ĵ�2������
		//			{
		//				if (m_uses.find(beusedFile) != m_uses.end())
		//				{
		//					for (auto child : m_uses[beusedFile])
		//					{
		//						FileID ancestor = get_lvl_2_ancestor(child, file);
		//						if (ancestor.isValid())
		//						{
		//							direct_use_list.insert(ancestor);
		//						}
		//					}
		//				}
		//			}
		//
		//			if (direct_use_list.empty())
		//			{
		//				return;
		//			}
		//
		//			m_direct_uses[file].insert(direct_use_list.begin(), direct_use_list.end());
		//
		//			{
		//				for (FileID used_file : direct_use_list)
		//				{
		//					m_usedLocs.insert(m_srcMgr->getIncludeLoc(used_file));
		//				}
		//			}
		//
		//			// �������ļ��У������¼use��ϵ
		//			if (isMainFile)
		//			{
		//				m_topUsedIncludes.insert(direct_use_list.begin(), direct_use_list.end());
		//			}
	}

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
	void ParsingFile::direct_use_include_by_family(FileID file, FileID beusedFile)
	{
		//			m_usedLocs.insert(m_srcMgr->getIncludeLoc(beusedFile));
		//
		//			// �ӱ�ʹ���ļ��ĸ��ڵ㣬������Ͻ������ø������ù�ϵ
		//			int now_depth	= get_depth(file);
		//			int beuse_depth	= get_depth(beusedFile);
		//
		//			if (now_depth < beuse_depth)
		//			{
		//				if (is_ancestor(beusedFile, file))
		//				{
		//					for (FileID child = beusedFile; has_parent(child) && child != file;)
		//					{
		//						FileID parent = m_parentIDs[child];
		//						direct_use_include_up_by_up(parent, beusedFile);
		//						child = parent;
		//					}
		//				}
		//			}
	}

	// ��ǰ�ļ�ʹ��Ŀ���ļ�
	void ParsingFile::use_include(FileID file, FileID beusedFile, const char* name /* = nullptr */)
	{
		if (file == beusedFile)
		{
			return;
		}

		if (file.isInvalid() || beusedFile.isInvalid())
		{
			return;
		}

		// �����ע�͵�
		const FileEntry *mainFileEntry = m_srcMgr->getFileEntryForID(file);
		const FileEntry *beusedFileEntry = m_srcMgr->getFileEntryForID(beusedFile);

		if (nullptr == mainFileEntry || nullptr == beusedFileEntry)
		{
			// llvm::outs() << "------->error: use_include() : m_srcMgr->getFileEntryForID(file) failed!" << m_srcMgr->getFilename(m_srcMgr->getLocForStartOfFile(file)) << ":" << m_srcMgr->getFilename(m_srcMgr->getLocForStartOfFile(beusedFile)) << "\n";
			// llvm::outs() << "------->error: use_include() : m_srcMgr->getFileEntryForID(file) failed!" << get_source_of_line(m_srcMgr->getLocForStartOfFile(file)) << ":" << get_source_of_line(m_srcMgr->getLocForStartOfFile(beusedFile)) << "\n";
			return;
		}

		m_uses[file].insert(beusedFile);
		// direct_use_include_by_family(file, beusedFile);

		use_name(file, beusedFile, name);
	}

	void ParsingFile::use_macro(SourceLocation loc, const MacroDefinition &macro, const Token &macroName)
	{
		MacroInfo *info = macro.getMacroInfo();
		if (nullptr == info)
		{
			return;
		}

		loc = m_srcMgr->getSpellingLoc(loc);

		// llvm::outs() << "macro id = " << macroName.getIdentifierInfo()->getName().str() << "\n";
		use(loc, info->getDefinitionLoc(), macroName.getIdentifierInfo()->getName().str().c_str());
	}

	// ��ǰλ��ʹ��Ŀ������
	void ParsingFile::use_type(SourceLocation loc, const QualType &t)
	{
		if (t.isNull())
		{
			return;
		}

		if (isa<TypedefType>(t))
		{
			const TypedefType *typedefType = cast<TypedefType>(t);
			const TypedefNameDecl *typedefNameDecl = typedefType->getDecl();

			on_use_decl(loc, typedefNameDecl);

			// ����typedef��ԭ����Ȼ����������typedef�������ɣ�������ֽ�
			/* �磺
					typedef int int32;
					typedef int32 time_t;
					�����time_t���int32ԭ�ͺ���Ҫ�ٸ���int32���intԭ��
					*/
			//				if (typedefType->isSugared())
			//				{
			//					use_type(typedefNameDecl->getLocStart(), typedefType->desugar());
			//				}
		}
		else if (isa<TemplateSpecializationType>(t))
		{
			const TemplateSpecializationType *templateType = cast<TemplateSpecializationType>(t);

			on_use_decl(loc, templateType->getTemplateName().getAsTemplateDecl());

			for (int i = 0, size = templateType->getNumArgs(); i < size; ++i)
			{
				const TemplateArgument &arg = templateType->getArg((unsigned)i);

				TemplateArgument::ArgKind argKind = arg.getKind();

				switch (argKind)
				{
				case TemplateArgument::Type:
					// arg.getAsType().dump();
					use_type(loc, arg.getAsType());
					break;

				case TemplateArgument::Declaration:
					on_use_decl(loc, arg.getAsDecl());
					break;

				case TemplateArgument::Expression:
					use(loc, arg.getAsExpr()->getLocStart());
					break;

				case TemplateArgument::Template:
					on_use_decl(loc, arg.getAsTemplate().getAsTemplateDecl());
					break;

				default:
					// t->dump();
					break;
				}
			}
		}
		else if (isa<ElaboratedType>(t))
		{
			const ElaboratedType *elaboratedType = cast<ElaboratedType>(t);
			use_type(loc, elaboratedType->getNamedType());
		}
		else if (isa<AttributedType>(t))
		{
			const AttributedType *attributedType = cast<AttributedType>(t);
			use_type(loc, attributedType->getModifiedType());
		}
		else if (isa<FunctionType>(t))
		{
			const FunctionType *functionType = cast<FunctionType>(t);

			// ʶ�𷵻�ֵ����
			{
				// �����ķ���ֵ
				QualType returnType = functionType->getReturnType();
				use_type(loc, returnType);
			}
		}
		else if (isa<MemberPointerType>(t))
		{
			const MemberPointerType *memberPointerType = cast<MemberPointerType>(t);
			use_type(loc, memberPointerType->getPointeeType());
		}
		else if (isa<TemplateTypeParmType>(t))
		{
			const TemplateTypeParmType *templateTypeParmType = cast<TemplateTypeParmType>(t);

			TemplateTypeParmDecl *decl = templateTypeParmType->getDecl();
			if (nullptr == decl)
			{
				// llvm::errs() << "------------ TemplateTypeParmType dump ------------:\n";
				// llvm::errs() << "------------ templateTypeParmType->getDecl()->dumpColor() ------------\n";
				return;
			}

			if (decl->hasDefaultArgument())
			{
				use_type(loc, decl->getDefaultArgument());
			}

			on_use_decl(loc, decl);
		}
		else if (isa<ParenType>(t))
		{
			const ParenType *parenType = cast<ParenType>(t);
			// llvm::errs() << "------------ ParenType dump ------------:\n";
			// llvm::errs() << "------------ parenType->getInnerType().dump() ------------:\n";
			use_type(loc, parenType->getInnerType());
		}
		else if (isa<InjectedClassNameType>(t))
		{
			const InjectedClassNameType *injectedClassNameType = cast<InjectedClassNameType>(t);
			// llvm::errs() << "------------InjectedClassNameType dump:\n";
			// llvm::errs() << "------------injectedClassNameType->getInjectedSpecializationType().dump():\n";
			use_type(loc, injectedClassNameType->getInjectedSpecializationType());
		}
		else if (isa<PackExpansionType>(t))
		{
			const PackExpansionType *packExpansionType = cast<PackExpansionType>(t);
			// llvm::errs() << "\n------------PackExpansionType------------\n";
			use_type(loc, packExpansionType->getPattern());
		}
		else if (isa<DecltypeType>(t))
		{
			const DecltypeType *decltypeType = cast<DecltypeType>(t);
			// llvm::errs() << "\n------------DecltypeType------------\n";
			use_type(loc, decltypeType->getUnderlyingType());
		}
		else if (isa<DependentNameType>(t))
		{
			//				const DependentNameType *dependentNameType = cast<DependentNameType>(t);
			//				llvm::errs() << "\n------------DependentNameType------------\n";
			//				llvm::errs() << "\n------------dependentNameType->getQualifier()->dump()------------\n";
			//				dependentNameType->getQualifier()->dump();
		}
		else if (isa<DependentTemplateSpecializationType>(t))
		{
		}
		else if (isa<AutoType>(t))
		{
		}
		else if (isa<UnaryTransformType>(t))
		{
			// t->dump();
		}
		else if (isa<RecordType>(t))
		{
			const RecordType *recordType = cast<RecordType>(t);

			const CXXRecordDecl *cxxRecordDecl = recordType->getAsCXXRecordDecl();
			if (nullptr == cxxRecordDecl)
			{
				llvm::errs() << "t->isRecordType() nullptr ==  t->getAsCXXRecordDecl():\n";
				t->dump();
				return;
			}

			// ��ֹ�����ļ���class A; �����ļ�ȴ������class A{}
			//{
			//	FileID file = m_srcMgr->getFileID(cxxRecordDecl->getLocStart());
			//	if (file.isValid() && nullptr != m_srcMgr->getFileEntryForID(file))
			//	{
			//		m_forwardDecls[file][get_cxxrecord_name(*cxxRecordDecl)] = false;
			//	}
			//}

			on_use_record(loc, cxxRecordDecl);
		}
		else if (t->isArrayType())
		{
			const ArrayType *arrayType = cast<ArrayType>(t);
			use_type(loc, arrayType->getElementType());
		}
		else if (t->isVectorType())
		{
			const VectorType *vectorType = cast<VectorType>(t);
			use_type(loc, vectorType->getElementType());
		}
		else if (t->isBuiltinType())
		{
		}
		else if (t->isPointerType() || t->isReferenceType())
		{
			QualType pointeeType = t->getPointeeType();

			// �����ָ�����;ͻ�ȡ��ָ������(PointeeType)
			while (pointeeType->isPointerType() || pointeeType->isReferenceType())
			{
				pointeeType = pointeeType->getPointeeType();
			}

			use_type(loc, pointeeType);
		}
		else if (t->isEnumeralType())
		{
			TagDecl *decl = t->getAsTagDecl();
			if (nullptr == decl)
			{
				llvm::errs() << "t->isEnumeralType() nullptr ==  t->getAsTagDecl():\n";
				t->dump();
				return;
			}

			on_use_decl(loc, decl);
		}
		else
		{
			//llvm::errs() << "-------------- haven't support type --------------\n";
			//t->dump();
		}
	}

	string ParsingFile::get_cxxrecord_name(const CXXRecordDecl &cxxRecordDecl)
	{
		string name;
		name += cxxRecordDecl.getKindName();
		name += " " + cxxRecordDecl.getNameAsString();
		name += ";";

		bool inNameSpace = false;

		const DeclContext *curDeclContext = cxxRecordDecl.getDeclContext();
		while (curDeclContext && curDeclContext->isNamespace())
		{
			const NamespaceDecl *namespaceDecl = cast<NamespaceDecl>(curDeclContext);
			if (nullptr == namespaceDecl)
			{
				break;
			}

			inNameSpace = true;

			string namespaceName = "namespace " + namespaceDecl->getNameAsString();
			name = namespaceName + "{" + name + "}";

			curDeclContext = curDeclContext->getParent();
		}

		if (inNameSpace)
		{
			name += ";";
		}

		return name;
	}

	bool ParsingFile::LexSkipComment(SourceLocation Loc, Token &Result)
	{
		const SourceManager &SM = *m_srcMgr;
		const LangOptions LangOpts;

		Loc = SM.getExpansionLoc(Loc);
		std::pair<FileID, unsigned> LocInfo = SM.getDecomposedLoc(Loc);
		bool Invalid = false;
		StringRef Buffer = SM.getBufferData(LocInfo.first, &Invalid);
		if (Invalid)
		{
			llvm::outs() << "LexSkipComment Invalid = true";
			return true;
		}

		const char *StrData = Buffer.data()+LocInfo.second;

		// Create a lexer starting at the beginning of this token.
		Lexer TheLexer(SM.getLocForStartOfFile(LocInfo.first), LangOpts,
		               Buffer.begin(), StrData, Buffer.end());
		TheLexer.SetCommentRetentionState(false);
		TheLexer.LexFromRawLexer(Result);
		return false;
	}

	// ���ز���ǰ�����������еĿ�ͷ
	SourceLocation ParsingFile::get_insert_forward_line(FileID at, const CXXRecordDecl &cxxRecord)
	{
		// �������ڵ��ļ�
		FileID recordAtFile	= m_srcMgr->getFileID(cxxRecord.getLocStart());

		/*
			�����Ӧ���ĸ��ļ��ĸ�������ǰ��������

			�����ļ�Bʹ�����ļ�A�е�class a������B��A�Ĺ�ϵ�ɷ������������

			1. B��A������
				�ҵ�B������A��#include�����ڸ���ĩ����ǰ������

			2. A��B������
				����Ҫ����

			3. ����˵��A��Bû��ֱϵ��ϵ
				����ͼΪ����->��ʾ#include����
					C -> D -> F -> A
					  -> E -> B

				��ʱ���ҵ�A��B�Ĺ�ͬ����C���ҵ�C������A��#include�����У���#include D�����ڸ���ĩ����ǰ������
		*/
		// 1. ��ǰ�ļ����������ļ�������
		if (is_ancestor(recordAtFile, at))
		{
			// �ҵ�A�����ȣ�Ҫ������ȱ�����B�Ķ���
			FileID ancestor = get_lvl_2_ancestor(recordAtFile, at);
			if (get_parent(ancestor) != at)
			{
				return SourceLocation();
			}

			// �ҵ������ļ�����Ӧ��#include����λ��
			SourceLocation includeLoc	= m_srcMgr->getIncludeLoc(ancestor);
			SourceRange line			= get_cur_line(includeLoc);
			return line.getBegin();
		}
		// 2. �������ļ��ǵ�ǰ�ļ�������
		else if (is_ancestor(at, recordAtFile))
		{
			// ����Ҫ����
		}
		// 3. �������ļ��͵�ǰ�ļ�û��ֱϵ��ϵ
		else
		{
			// �ҵ���2���ļ��Ĺ�ͬ����
			FileID common_ancestor = get_common_ancestor(recordAtFile, at);
			if (common_ancestor.isInvalid())
			{
				return SourceLocation();
			}

			// �ҵ��������ļ������ȣ�Ҫ������ȱ����ǹ�ͬ���ȵĶ���
			FileID ancestor = get_lvl_2_ancestor(recordAtFile, common_ancestor);
			if (get_parent(ancestor) != common_ancestor)
			{
				return SourceLocation();
			}

			// �ҵ������ļ�����Ӧ��#include����λ��
			SourceLocation includeLoc	= m_srcMgr->getIncludeLoc(ancestor);
			SourceRange line			= get_cur_line(includeLoc);
			return line.getBegin();
		}

		// ������ִ�е�����
		return SourceLocation();
	}

	void ParsingFile::use_forward(SourceLocation loc, const CXXRecordDecl *cxxRecordDecl)
	{
		FileID file = m_srcMgr->getFileID(loc);
		if (file.isInvalid())
		{
			return;
		}

		string cxxRecordName = get_cxxrecord_name(*cxxRecordDecl);
		m_forwardDecls[file].insert(cxxRecordDecl);

		// ������ü�¼
		{
			// �������ڵ��ļ�
			FileID recordAtFile	= m_srcMgr->getFileID(cxxRecordDecl->getLocStart());
			if (recordAtFile.isInvalid())
			{
				return;
			}

			// 1. ����ǰ�ļ����������ļ������ȣ�����ڵ�ǰ�ļ����ǰ��������������������ü�¼
			if (is_ancestor(recordAtFile, file))
			{
				return;
			}

			// 2. ����˵����ǰ�ļ����������ļ��ǲ�ͬ��֧�����������ļ��ǵ�ǰ�ļ�������

			// �ҵ���2���ļ��Ĺ�ͬ����
			FileID common_ancestor = get_common_ancestor(recordAtFile, file);
			if (common_ancestor.isInvalid())
			{
				return;
			}

			// ��ǰ�ļ����ù�ͬ����
			m_usedFiles.insert(common_ancestor);
		}
	}

	void ParsingFile::use_var(SourceLocation loc, const QualType &var)
	{
		if (!var->isPointerType() && !var->isReferenceType())
		{
			use_type(loc, var);
			return;
		}

		QualType pointeeType = var->getPointeeType();

		// �����ָ�����;ͻ�ȡ��ָ������(PointeeType)
		while (pointeeType->isPointerType() || pointeeType->isReferenceType())
		{
			pointeeType = pointeeType->getPointeeType();
		}

		if (!pointeeType->isRecordType())
		{
			use_type(loc, var);
			return;
		}

		const CXXRecordDecl *cxxRecordDecl = pointeeType->getAsCXXRecordDecl();
		if (nullptr == cxxRecordDecl)
		{
			use_type(loc, var);
			return;
		}

		if (isa<ClassTemplateSpecializationDecl>(cxxRecordDecl))
		{
			use_type(loc, var);
			return;
			// cxxRecord->dump(llvm::outs());
		}

		use_forward(loc, cxxRecordDecl);
	}

	void ParsingFile::on_use_decl(SourceLocation loc, const NamedDecl *nameDecl)
	{
		use(loc, nameDecl->getLocStart(), nameDecl->getNameAsString().c_str());
	}

	void ParsingFile::on_use_record(SourceLocation loc, const CXXRecordDecl *record)
	{
		use(loc, record->getLocStart(), get_cxxrecord_name(*record).c_str());
	}

	void ParsingFile::print_parents_by_id()
	{
		llvm::outs() << "    " << ++m_i << ". list of parent id:" << m_parentIDs.size() << "\n";

		for (auto childparent : m_parentIDs)
		{
			FileID childFileID = childparent.first;
			FileID parentFileID = childparent.second;

			// ����ӡ����Ŀ���ļ���ֱ�ӹ������ļ�
			if (!Vsproject::instance.has_file(get_absolute_file_name(childFileID)) && !Vsproject::instance.has_file(get_absolute_file_name(parentFileID)))
			{
				continue;
			}

			llvm::outs() << "        [" << get_file_name(childFileID) << "] parent = [" << get_file_name(parentFileID) << "]\n";
		}
	}

	string ParsingFile::debug_file_include_text(FileID file, bool is_absolute_name /* = false */)
	{
		SourceLocation loc = m_srcMgr->getIncludeLoc(file);

		PresumedLoc presumedLoc = m_srcMgr->getPresumedLoc(loc);

		std::stringstream text;
		string fileName;
		string parentFileName;
		std::string includeToken = "empty";

		std::map<SourceLocation, SourceRange>::iterator itr = m_locToRange.find(loc);
		if (m_locToRange.find(loc) != m_locToRange.end())
		{
			SourceRange range = itr->second;
			includeToken = get_source_of_range(range);
		}

		if (is_absolute_name)
		{
			fileName = get_file_name(file);
			parentFileName = presumedLoc.getFilename();
		}
		else
		{
			fileName = get_absolute_file_name(file);
			parentFileName = get_absolute_file_name(presumedLoc.getFilename());
		}

		text << "[" << fileName << "]";

		if (file != m_srcMgr->getMainFileID())
		{
			text << " in { [";
			text << parentFileName;
			text << "] -> [" << includeToken << "] line = " << (presumedLoc.isValid() ? presumedLoc.getLine() : 0);
			text << "}";
		}

		return text.str();
	}

	string ParsingFile::debug_file_direct_use_text(FileID file)
	{
		SourceLocation loc		= m_srcMgr->getIncludeLoc(file);
		PresumedLoc presumedLoc = m_srcMgr->getPresumedLoc(loc);

		std::stringstream text;
		string fileName;
		string includeToken = "empty";

		{
			std::map<SourceLocation, SourceRange>::iterator itr = m_locToRange.find(loc);
			if (m_locToRange.find(loc) != m_locToRange.end())
			{
				SourceRange range = itr->second;
				includeToken = get_source_of_range(range);
			}

			fileName = get_file_name(file);
		}

		text << "{[" << includeToken << "] line = " << (presumedLoc.isValid() ? presumedLoc.getLine() : 0);
		text << "} -> ";
		text << "[" << fileName << "]";

		return text.str();
	}

	string ParsingFile::debug_loc_text(SourceLocation loc)
	{
		PresumedLoc presumedLoc = m_srcMgr->getPresumedLoc(loc);
		if (presumedLoc.isInvalid())
		{
			return "";
		}

		string line = get_source_of_line(loc);
		std::stringstream text;
		text << "[" << line << "] in [";
		text << get_file_name(m_srcMgr->getFileID(loc));
		text << "] line = " << presumedLoc.getLine();
		return text.str();
	}

	string ParsingFile::debug_loc_include_text(SourceLocation loc)
	{
		PresumedLoc presumedLoc = m_srcMgr->getPresumedLoc(loc);
		if (presumedLoc.isInvalid())
		{
			return "";
		}

		std::stringstream text;
		std::string includeToken = "empty";

		std::map<SourceLocation, SourceRange>::iterator itr = m_locToRange.find(loc);
		if (m_locToRange.find(loc) != m_locToRange.end())
		{
			SourceRange range = itr->second;
			includeToken = get_source_of_range(range);
		}

		text << "[" + includeToken << "] line = " << presumedLoc.getLine();
		return text.str();
	}

	void ParsingFile::print_use()
	{
		llvm::outs() << "    " << ++m_i << ". list of include referenced: " << m_uses.size() << "\n";

		SourceManager &srcMgr = m_rewriter->getSourceMgr();

		for (auto use_list : m_uses)
		{
			if (!Vsproject::instance.has_file(get_absolute_file_name(use_list.first)))
			{
				continue;
			}

			llvm::outs() << "        file = " << get_absolute_file_name(use_list.first) << "\n";

			for (auto beuse : use_list.second)
			{
				// llvm::outs() << "            use = " << get_file_name(beuse) << "\n";
				// llvm::outs() << "            use = " << get_file_name(beuse) << " [" << token << "] in [" << presumedLoc.getFilename() << "] line = " << presumedLoc.getLine() << "\n";
				llvm::outs() << "            use = " << debug_file_include_text(beuse) << "\n";
			}

			llvm::outs() << "\n";
		}
	}

	void ParsingFile::print_used_names()
	{
		llvm::outs() << "    " << ++m_i << ". list of use name: " << m_uses.size() << "\n";

		for (auto use_list : m_useNames)
		{
			if (!Vsproject::instance.has_file(get_absolute_file_name(use_list.first)))
			{
				continue;
			}

			llvm::outs() << "        file = " << get_absolute_file_name(use_list.first) << "\n";

			for (const UseNameInfo &beuse : use_list.second)
			{
				llvm::outs() << "            use = " << debug_file_include_text(beuse.file) << "\n";

				for (const string& name : beuse.nameVec)
				{
					llvm::outs() << "                name = " << name << "\n";
				}

				llvm::outs() << "\n";
			}

			llvm::outs() << "\n";
		}
	}

	void ParsingFile::print_root_cycle_used_names()
	{
		llvm::outs() << "    " << ++m_i << ". list of root cycle use name: " << m_uses.size() << "\n";

		for (auto useItr : m_useNames)
		{
			FileID file									= useItr.first;
			const std::vector<UseNameInfo> &useNames	= useItr.second;

			if (!Vsproject::instance.has_file(get_absolute_file_name(file)))
			{
				continue;
			}

			if (m_allCycleUse.find(file) == m_allCycleUse.end() && file != m_srcMgr->getMainFileID())
			{
				continue;
			}

			llvm::outs() << "        file = " << get_absolute_file_name(file) << "\n";

			for (const UseNameInfo &beuse : useNames)
			{
				llvm::outs() << "            use = " << debug_file_include_text(beuse.file) << "\n";

				for (const string& name : beuse.nameVec)
				{
					llvm::outs() << "                name = " << name << "\n";
				}

				llvm::outs() << "\n";
			}

			llvm::outs() << "\n";
		}
	}

	void ParsingFile::print_used_children()
	{
		llvm::outs() << "\n    " << ++m_i << ". list of used children file: " << m_usedChildren.size();

		for (auto usedItr : m_usedChildren)
		{
			FileID file = usedItr.first;

			llvm::outs() << "\n        file = " << get_absolute_file_name(file) << "\n";

			for (auto usedChild : usedItr.second)
			{
				llvm::outs() << "            use children = " << debug_file_include_text(usedChild) << "\n";
			}

			llvm::outs() << "\n";
		}
	}

	void ParsingFile::print_all_file()
	{
		llvm::outs() << "\n    " << ++m_i << ". list of all file: " << m_files.size();

		for (auto itr : m_files)
		{
			const string &name = itr.first;

			if (!Vsproject::instance.has_file(name))
			{
				continue;
			}

			llvm::outs() << "\n        file = " << name;
		}

		llvm::outs() << "\n";
	}

	void ParsingFile::print_direct_used_include()
	{
		llvm::outs() << "\n    " << ++m_i << ". list of direct include referenced: " << m_direct_uses.size();

		for (auto direct_use_list : m_direct_uses)
		{
			FileID file = direct_use_list.first;

			if (!Vsproject::instance.has_file(get_absolute_file_name(file)))
			{
				continue;
			}

			llvm::outs() << "\n        file = " << get_absolute_file_name(file) << "\n";

			for (auto beuse_direct : direct_use_list.second)
			{
				llvm::outs() << "            direct use = " << debug_file_direct_use_text(beuse_direct) << "\n";
			}

			llvm::outs() << "\n";
		}
	}

	void ParsingFile::print_used_top_include()
	{
		llvm::outs() << "\n////////////////////////////////\n";
		llvm::outs() << ++m_i << ". list of top include referenced:" << m_topUsedIncludes.size() << "\n";
		llvm::outs() << "////////////////////////////////\n";

		SourceManager &srcMgr = m_rewriter->getSourceMgr();

		for (auto file : m_topUsedIncludes)
		{
			const FileEntry *curFile = srcMgr.getFileEntryForID(file);
			if (NULL == curFile)
			{
				continue;
			}

			llvm::outs() << "    top include = " << curFile->getName() << "\n";
		}
	}

	void ParsingFile::print_top_include_by_id()
	{
		llvm::outs() << "\n////////////////////////////////\n";
		llvm::outs() << ++m_i << ". list of top include by id:" << m_topIncludeIDs.size() << "\n";
		llvm::outs() << "////////////////////////////////\n";

		SourceManager &srcMgr = m_rewriter->getSourceMgr();

		for (auto file : m_topIncludeIDs)
		{
			llvm::outs() << "    top include = " << get_file_name(file) << "\n";
		}
	}

	void ParsingFile::print_unused_include_of_files(FileMap &files)
	{
		for (auto fileItr : files)
		{
			const string &file	= fileItr.first;
			EachFile &eachFile	= fileItr.second;

			if (!Vsproject::instance.has_file(file))
			{
				continue;
			}

			if (eachFile.m_unusedLines.empty())
			{
				continue;
			}

			llvm::outs() << "\n        file = {" << file << "}\n";

			for (auto unusedLineItr : eachFile.m_unusedLines)
			{
				int line = unusedLineItr.first;

				UselessLineInfo &unusedLine = unusedLineItr.second;

				llvm::outs() << "            unused [" << unusedLine.m_text << "] line = " << line << "\n";
			}
		}
	}

	void ParsingFile::print_can_forwarddecl_of_files(FileMap &files)
	{
		for (auto fileItr : files)
		{
			const string &file	= fileItr.first;
			EachFile &eachFile	= fileItr.second;

			if (!Vsproject::instance.has_file(file))
			{
				continue;
			}

			if (eachFile.m_forwards.empty())
			{
				continue;
			}

			llvm::outs() << "\n        file = {" << file << "}\n";

			for (auto forwardItr : eachFile.m_forwards)
			{
				int line = forwardItr.first;

				ForwardLine &forwardLine = forwardItr.second;

				llvm::outs() << "            [line = " << line << "] -> {old text = [" << forwardLine.m_oldText << "]}\n";

				for (const string &name : forwardLine.m_classes)
				{
					llvm::outs() << "                add forward [" << name << "]\n";
				}

				llvm::outs() << "\n";
			}
		}
	}

	void ParsingFile::print_can_replace_of_files(FileMap &files)
	{
		for (auto fileItr : files)
		{
			const string &file	= fileItr.first;
			EachFile &eachFile	= fileItr.second;

			if (!Vsproject::instance.has_file(file))
			{
				continue;
			}

			if (eachFile.m_replaces.empty())
			{
				continue;
			}

			llvm::outs() << "\n        file = {" << file << "}\n";

			for (auto replaceItr : eachFile.m_replaces)
			{
				int line = replaceItr.first;

				ReplaceLine &replaceLine = replaceItr.second;

				llvm::outs() << "            [line = " << line << "] -> {old text = [" << replaceLine.m_oldText << "]}";

				if (replaceLine.m_isSkip)
				{
					llvm::outs() << "  ==>  [warn: will skip this replacement, for it's force included]";
				}

				llvm::outs() << "\n";

				for (const ReplaceInfo &replaceInfo : replaceLine.m_newInclude)
				{
					// ��ӡ�滻ǰ���滻���#include����
					if (replaceInfo.m_newText == replaceInfo.m_oldText)
					{
						llvm::outs() << "                replace to = " << replaceInfo.m_oldText;
					}
					else
					{
						llvm::outs() << "                replace to = [old]" << replaceInfo.m_oldText << "-> [new]" << replaceInfo.m_newText;
					}

					// ����β���[in �������ļ� : line = xx]
					llvm::outs() << " in [" << replaceInfo.m_inFile << " : line = " << replaceInfo.m_line << "]\n";
				}

				llvm::outs() << "\n";
			}
		}
	}

	void ParsingFile::print_unused_include()
	{
		// ������ļ��ڼ����ļ��ڵ������ļ��е�#include���޷���ɾ�����򲻴�ӡ
		if (m_unusedLocs.empty())
		{
			return;
		}

		FileMap files;
		take_all_info_to(files);

		llvm::outs() << "\n    " << ++m_i << ". list of unused include :" << m_unusedLocs.size() << "";

		print_unused_include_of_files(files);
	}

	void ParsingFile::print_can_forwarddecl()
	{
		if (m_forwardDecls.empty())
		{
			return;
		}

		FileMap files;
		take_all_info_to(files);

		llvm::outs() << "\n    " << ++m_i << ". forward declaration list :" << m_forwardDecls.size() << "\n";

		print_can_forwarddecl_of_files(files);
	}

	void ParsingFile::print_unused_top_include()
	{
		// ������ļ��ڵ�#include���޷���ɾ�����򲻴�ӡ
		if (m_topUnusedIncludeLocs.empty())
		{
			return;
		}

		llvm::outs() << "\n    " << ++m_i << ". list of unused top include :" << m_topUnusedIncludeLocs.size() << "\n";

		SourceManager &srcMgr = m_rewriter->getSourceMgr();

		for (int i = 0, size = m_topUnusedIncludeLocs.size(); i < size; ++i)
		{
			SourceLocation loc	= m_topUnusedIncludeLocs[i];
			llvm::outs() << "        unused " << debug_loc_include_text(loc) << "\n";
			// llvm::outs() << "        unused whole line = [" << get_source_of_range(get_cur_line(loc)) << "] line = " << presumedLoc.getLine() << "\n";
		}
	}

	const char* ParsingFile::get_file_name(FileID file) const
	{
		const FileEntry *fileEntry = m_srcMgr->getFileEntryForID(file);
		if (nullptr == fileEntry)
		{
			return "";
		}

		return fileEntry->getName();

		//			SourceLocation loc = m_srcMgr->getLocForStartOfFile(file);
		//			PresumedLoc presumed_loc = m_srcMgr->getPresumedLoc(loc);
		//
		//			return presumed_loc.getFilename();
	}

	string ParsingFile::get_absolute_file_name(const char *raw_file_name)
	{
		if (nullptr == raw_file_name)
		{
			return "";
		}

		if (raw_file_name[0] == 0)
		{
			return "";
		}

		string absolute_path;

		if (llvm::sys::path::is_relative(raw_file_name))
		{
			absolute_path = get_absolute_path(raw_file_name);
		}
		else
		{
			absolute_path = raw_file_name;
		}

		absolute_path = simplify_path(absolute_path.c_str());
		return absolute_path;
	}

	string ParsingFile::get_absolute_file_name(FileID file) const
	{
		const char* raw_file_name = get_file_name(file);
		return get_absolute_file_name(raw_file_name);
	}

	const char* ParsingFile::get_include_value(const char* include_str)
	{
		include_str += sizeof("#include") / sizeof(char);

		while (is_blank(*include_str))
		{
			++include_str;
		}

		return include_str;
	}

	std::string ParsingFile::get_relative_include_str(FileID f1, FileID f2)
	{
		string raw_include2 = get_raw_include_str(f2);
		if (raw_include2.empty())
		{
			return "";
		}

		// �Ƿ񱻼����Ű������磺<stdio.h>
		bool isAngled = strtool::contain(raw_include2.c_str(), '<');
		if (isAngled)
		{
			return raw_include2;
		}

		string absolute_path2 = get_absolute_file_name(f2);

		string include2 = get_quoted_include_str(absolute_path2);
		if (!include2.empty())
		{
			return "#include " + include2;
		}
		else
		{
			string absolute_path1 = get_absolute_file_name(f1);
			include2 = "\"" + filetool::get_relative_path(absolute_path1.c_str(), absolute_path2.c_str()) + "\"";
		}

		return "#include " + include2;
	}

	bool ParsingFile::is_absolute_path(const string& path)
	{
		return llvm::sys::path::is_absolute(path);
	}

	// ��·��
	// d:/a/b/c/../../d/ -> d:/d/
	std::string ParsingFile::simplify_path(const char* path)
	{
		string native_path = to_linux_path(path);
		if (start_with(native_path, "../") || start_with(native_path, "./"))
		{
			return native_path;
		}

		strtool::replace(native_path, "/./", "/");

		string out(native_path.size(), '\0');

		int o = 0;

		const char up_dir[] = "/../";
		int up_dir_len = strlen(up_dir);

		for (int i = 0, len = native_path.size(); i < len;)
		{
			char c = native_path[i];

			if (c == '/')
			{
				if (i + up_dir_len - 1 >= len || i == 0)
				{
					out[o++] = c;
					++i;
					continue;
				}

				if(0 == strncmp(&native_path[i], "/../", up_dir_len))
				{
					if (out[o] == '/')
					{
						--o;
					}

					while (o >= 0)
					{
						if (out[o] == '/')
						{
							break;
						}
						else if (out[o] == ':')
						{
							++o;
							break;
						}

						--o;
					}

					if (o < 0)
					{
						o = 0;
					}

					i += up_dir_len - 1;
					continue;
				}
				else
				{
					out[o++] = c;
					++i;
				}
			}
			else
			{
				out[o++] = c;
				++i;
			}
		}

		out[o] = '\0';
		out.erase(out.begin() + o, out.end());
		return out;
	}

	string ParsingFile::get_absolute_path(const char *path)
	{
		llvm::SmallString<512> absolute_path(path);
		std::error_code error = llvm::sys::fs::make_absolute(absolute_path);
		if (error)
		{
			return "";
		}

		return absolute_path.str();
	}

	string ParsingFile::get_absolute_path(const char *base_path, const char* relative_path)
	{
		llvm::SmallString<512> absolute_path(base_path);
		llvm::sys::path::append(absolute_path, relative_path);

		return absolute_path.str();
	}

	string ParsingFile::get_parent_path(const char* path)
	{
		llvm::StringRef parent = llvm::sys::path::parent_path(path);
		return parent.str();
	}

	// ɾ��ָ��λ�����ڵ����д���
	void ParsingFile::erase_line_by_loc(SourceLocation loc)
	{
		Rewriter::RewriteOptions rewriteOption;
		rewriteOption.IncludeInsertsAtBeginOfRange = false;
		rewriteOption.IncludeInsertsAtEndOfRange = false;
		rewriteOption.RemoveLineIfEmpty = true;

		SourceRange lineRange	= get_cur_line(loc);
		bool err = m_rewriter->RemoveText(lineRange, rewriteOption);
		if (err)
		{
			string in_file = get_absolute_file_name(m_srcMgr->getFileID(loc));

			llvm::errs() << "\n------->error: erase_line_by_loc::remove text = [" << get_source_of_range(lineRange) << "] in [" << in_file << "] failed!\n";
		}
	}

	void ParsingFile::clean_unused_include_by_loc(SourceLocation unused_loc)
	{
		string in_file = get_absolute_file_name(m_srcMgr->getFileID(unused_loc));

		if (WholeProject::instance.has_cleaned(in_file.c_str()))
		{
			return;
		}

		if (!WholeProject::instance.need_clean(in_file.c_str()))
		{
			return;
		}

		if (!Vsproject::instance.has_file(in_file))
		{
			return;
		}

		PresumedLoc presumedLoc = m_srcMgr->getPresumedLoc(unused_loc);
		if (presumedLoc.isInvalid())
		{
			return;
		}

		int line = presumedLoc.getLine();

		EachFile &eachFile	= WholeProject::instance.m_files[in_file];
		if (!eachFile.is_line_unused(line))
		{
			return;
		}

		erase_line_by_loc(unused_loc);
	}

	void ParsingFile::clean_unused_include_in_all_file()
	{
		for (SourceLocation unused_loc : m_unusedLocs)
		{
			clean_unused_include_by_loc(unused_loc);
		}

		for (SourceLocation unused_loc : m_unusedLocs)
		{
			string in_file = get_absolute_file_name(m_srcMgr->getFileID(unused_loc));
			WholeProject::instance.on_cleaned(in_file.c_str());
		}
	}

	void ParsingFile::clean_unused_include_in_root()
	{
		Rewriter::RewriteOptions rewriteOption;
		rewriteOption.RemoveLineIfEmpty = true;

		for (auto unused_loc : m_topUnusedIncludeLocs)
		{
			bool found_range = (m_locToRange.find(unused_loc) != m_locToRange.end());
			if (!found_range)
			{
				continue;
			}

			erase_line_by_loc(unused_loc);
		}
	}

	void ParsingFile::clean_can_replace_in_root()
	{
		Rewriter::RewriteOptions rewriteOption;
		rewriteOption.RemoveLineIfEmpty = true;

		FileID mainFileID = m_srcMgr->getMainFileID();

		for (auto itr : m_replaces)
		{
			FileID be_replace_file			= itr.first;
			std::set<FileID> &replace_files	= itr.second;

			SourceLocation include_loc = m_srcMgr->getIncludeLoc(be_replace_file);

			bool found_range	= (m_locToRange.find(include_loc) != m_locToRange.end());
			if (!found_range)
			{
				continue;
			}

			if (nullptr == m_srcMgr->getFileEntryForID(m_srcMgr->getFileID(include_loc)))
			{
				continue;
			}

			std::string insert_text;

			int i	= 0;
			int len	= replace_files.size();

			for (const FileID &replace_file : replace_files)
			{
				insert_text.append(get_relative_include_str(mainFileID, replace_file));

				if (i + 1 < len)
				{
					insert_text.append("\n");
				}
			}

			// rewriter.InsertText(range.getEnd().getLocWithOffset(1), insert_text, true, true);

			// std::string text = rewriter.getRewrittenText(range);
			// llvm::outs() << "rewritten text = " << text << "\n";
			SourceRange line = get_cur_line(include_loc);

			// SourceRange spellLine(mainFile->m_srcMgr->getSpellingLoc(line.getBegin()), mainFile->m_srcMgr->getSpellingLoc(line.getEnd()));
			m_rewriter->ReplaceText(line, insert_text);
		}
	}

	void ParsingFile::add_forward_declaration(FileID file)
	{
		const std::set<const CXXRecordDecl*> &forwards = m_forwardDecls[file];
		if (forwards.empty())
		{
			return;
		}

		std::stringstream text;
		SourceLocation insertLoc;
		{
			for (const CXXRecordDecl* cxxRecord : forwards)
			{
				text << "\n" << get_cxxrecord_name(*cxxRecord) << ";";

				SourceLocation loc	= get_insert_forward_line(file, *cxxRecord);
				if (insertLoc.isInvalid() || insertLoc < loc)
				{
					insertLoc = loc;
				}
			}
		}

		if (insertLoc.isInvalid())
		{
			llvm::outs() << "insertLoc.isInvalid()\n";
			return;
		}

		SourceRange curLine		= get_cur_line(insertLoc);
		SourceRange nextLine	= get_next_line(insertLoc);

		{
			if (is_new_line_word(insertLoc))
			{
				std::stringstream old;
				old << "\n" << text.str();
				text.swap(old);
			}

			if (!is_empty_line(nextLine))
			{
				text << "\n";
			}
		}

		// llvm::outs() << "add_forward_declaration" << text.str() << " at " << mainFile->get_file_name(file) << "\n";
		m_rewriter->InsertText(insertLoc, text.str(), true, true);
	}

	void ParsingFile::clean_by_forwarddecl_in_all_file()
	{
		if (m_forwardDecls.empty())
		{
			return;
		}

		for (auto itr : m_forwardDecls)
		{
			FileID file	= itr.first;
			add_forward_declaration(file);
		}
	}

	void ParsingFile::clean()
	{
		if (WholeProject::instance.m_isOptimized)
		{
			clean_all_file();
		}
		else
		{
			clean_main_file();
		}

		if (ParsingFile::m_isOverWriteOption)
		{
			m_rewriter->overwriteChangedFiles();
		}
	}

	void ParsingFile::replace_text(FileID file, int beg, int end, string text)
	{
		SourceLocation fileBegLoc	= m_srcMgr->getLocForStartOfFile(file);
		SourceLocation begLoc		= fileBegLoc.getLocWithOffset(beg);
		SourceLocation endLoc		= fileBegLoc.getLocWithOffset(end);

		SourceRange range(begLoc, endLoc);

		// llvm::outs() << "\n------->replace text = [" << get_source_of_range(range) << "] in [" << get_absolute_file_name(file) << "]\n";

		m_rewriter->ReplaceText(begLoc, end - beg, text);

		//insert_text(file, beg, text);
		//remove_text(file, beg, end);
	}

	void ParsingFile::insert_text(FileID file, int loc, string text)
	{
		SourceLocation fileBegLoc	= m_srcMgr->getLocForStartOfFile(file);
		SourceLocation insertLoc	= fileBegLoc.getLocWithOffset(loc);

		m_rewriter->InsertText(insertLoc, text, true, true);
	}

	void ParsingFile::remove_text(FileID file, int beg, int end)
	{
		SourceLocation fileBegLoc	= m_srcMgr->getLocForStartOfFile(file);
		if (fileBegLoc.isInvalid())
		{
			llvm::errs() << "\n------->error: fileBegLoc.isInvalid(), remove text in [" << get_absolute_file_name(file) << "] failed!\n";
			return;
		}

		SourceLocation begLoc		= fileBegLoc.getLocWithOffset(beg);
		SourceLocation endLoc		= fileBegLoc.getLocWithOffset(end);

		SourceRange range(begLoc, endLoc);

		Rewriter::RewriteOptions rewriteOption;
		rewriteOption.IncludeInsertsAtBeginOfRange	= false;
		rewriteOption.IncludeInsertsAtEndOfRange	= false;
		rewriteOption.RemoveLineIfEmpty				= false;

		//			{
		//				string fileName = get_absolute_file_name(file);
		//
		//				if (fileName.find("server.cpp") != string::npos)
		//				{
		//					llvm::outs() << "\n------->remove text = [" << get_source_of_range(range) << "] in [" << fileName << "]\n";
		//				}
		//			}

		// llvm::outs() << "\n------->remove text = [" << get_source_of_range(range) << "] in [" << get_absolute_file_name(file) << "]\n";

		bool err = m_rewriter->RemoveText(range.getBegin(), end - beg, rewriteOption);
		if (err)
		{
			llvm::errs() << "\n------->error: remove text = [" << get_source_of_range(range) << "] in [" << get_absolute_file_name(file) << "] failed!\n";
		}
	}

	void ParsingFile::clean_by_unused_line(const EachFile &eachFile, FileID file)
	{
		if (eachFile.m_unusedLines.empty())
		{
			return;
		}

		for (auto unusedLineItr : eachFile.m_unusedLines)
		{
			int line				= unusedLineItr.first;
			UselessLineInfo &unusedLine	= unusedLineItr.second;

			remove_text(file, unusedLine.m_beg, unusedLine.m_end);
		}
	}

	void ParsingFile::clean_by_forward(const EachFile &eachFile, FileID file)
	{
		if (eachFile.m_forwards.empty())
		{
			return;
		}

		for (auto forwardItr : eachFile.m_forwards)
		{
			int line						= forwardItr.first;
			const ForwardLine &forwardLine	= forwardItr.second;

			std::stringstream text;

			for (const string &cxxRecord : forwardLine.m_classes)
			{
				text << cxxRecord;
				text << (eachFile.m_isWindowFormat ? "\r\n" : "\n");
			}

			insert_text(file, forwardLine.m_offsetAtFile, text.str());
		}
	}

	void ParsingFile::clean_by_replace(const EachFile &eachFile, FileID file)
	{
		if (eachFile.m_replaces.empty())
		{
			return;
		}

		for (auto replaceItr : eachFile.m_replaces)
		{
			int line						= replaceItr.first;
			const ReplaceLine &replaceLine	= replaceItr.second;

			// ���Ǳ�-include����ǿ�����룬����������Ϊ�滻��û��Ч��
			if (replaceLine.m_isSkip)
			{
				continue;
			}

			std::stringstream text;

			for (const ReplaceInfo &replaceInfo : replaceLine.m_newInclude)
			{
				text << replaceInfo.m_newText;
				text << (eachFile.m_isWindowFormat ? "\r\n" : "\n");
			}

			replace_text(file, replaceLine.m_beg, replaceLine.m_end, text.str());
		}
	}

	void ParsingFile::clean_by(const FileMap &files)
	{
		for (auto itr : files)
		{
			const string &fileName		= itr.first;

			if (!WholeProject::instance.has_file(fileName))
			{
				continue;
			}

			if (WholeProject::instance.has_cleaned(fileName))
			{
				continue;
			}

			if (!Vsproject::instance.has_file(fileName))
			{
				continue;
			}

			auto findItr = m_files.find(fileName);
			if (findItr == m_files.end())
			{
				continue;
			}

			FileID file					= findItr->second;
			const EachFile &eachFile	= WholeProject::instance.m_files[fileName];

			clean_by_replace(eachFile, file);
			clean_by_forward(eachFile, file);
			clean_by_unused_line(eachFile, file);

			WholeProject::instance.on_cleaned(fileName);
		}
	}

	void ParsingFile::clean_all_file()
	{
		clean_by(WholeProject::instance.m_files);

		//			clean_can_replace_include();
		//			clean_unused_include_in_all_file();
		//			clean_by_forwarddecl_in_all_file();
	}

	void ParsingFile::clean_main_file()
	{
		FileMap root;

		{
			string rootFileName = get_absolute_file_name(m_srcMgr->getMainFileID());

			auto rootItr = WholeProject::instance.m_files.find(rootFileName);
			if (rootItr != WholeProject::instance.m_files.end())
			{
				root.insert(*rootItr);
			}
		}

		clean_by(root);
	}

	void ParsingFile::print_can_replace()
	{
		if (m_replaces.empty())
		{
			// llvm::outs() << "        print_can_replace_list empty\n";
			return;
		}

		llvm::outs() << "\n    " << ++m_i << ". can replace include list :" << m_replaces.size() << "\n";

		for (auto itr : m_replaces)
		{
			FileID file = itr.first;

			if (m_parentIDs.find(file) == m_parentIDs.end())
			{
				continue;
			}

			FileID parent = m_parentIDs[file];

			std::set<FileID> &to_replaces = itr.second;

			llvm::outs() << "        file = " << get_file_name(parent) << "\n";

			// �����"old = #include <xxx.h> [line = xx]"
			{
				llvm::outs() << "            old = " << get_raw_include_str(file);

				SourceLocation include_loc	= m_srcMgr->getIncludeLoc(file);
				PresumedLoc presumed_loc	= m_srcMgr->getPresumedLoc(include_loc);

				llvm::outs() << " [line = " << presumed_loc.getLine() << "]\n";
			}

			// �����"replace = #include <xxx.h> in [../../x.h : line = xx]"
			for (FileID replace_file : to_replaces)
			{
				string raw_include		= get_raw_include_str(replace_file);
				string relative_include	= get_relative_include_str(parent, replace_file);

				// ��ӡ�滻ǰ���滻���#include����
				if (raw_include == relative_include)
				{
					llvm::outs() << "                replace = " << raw_include;
				}
				else
				{
					llvm::outs() << "                replace = [old]" << raw_include << "-> [new]" << relative_include;
				}

				// ����β���[in �������ļ� : line = xx]
				{
					SourceLocation include_loc	= m_srcMgr->getIncludeLoc(replace_file);
					PresumedLoc presumed_loc	= m_srcMgr->getPresumedLoc(include_loc);
					StringRef fileName			= m_srcMgr->getFilename(include_loc);

					llvm::outs() << " in [" << fileName << " : line = " << presumed_loc.getLine() << "]\n";
				}
			}

			llvm::outs() << "\n";
		}
	}

	void ParsingFile::print_header_search_path()
	{
		if (m_headerSearchPaths.empty())
		{
			return;
		}

		llvm::outs() << "\n    ////////////////////////////////";
		llvm::outs() << "\n    " << ++m_i << ". header search path list :" << m_headerSearchPaths.size();
		llvm::outs() << "\n    ////////////////////////////////\n";

		for (HeaderSearchPath &path : m_headerSearchPaths)
		{
			llvm::outs() << "        search path = " << path.path << "\n";
		}

		llvm::outs() << "\n        find main.cpp in search path = " << FindFileInSearchPath(m_headerSearchPaths, "main.cpp");
		llvm::outs() << "\n        absolute path of main.cpp  = " << get_absolute_path("main.cpp");
		llvm::outs() << "\n        relative path of D:/proj/dummygit\\server\\src\\server/net/connector.cpp  = " << llvm::sys::path::relative_path("D:/proj/dummygit\\server\\src\\server/net/connector.cpp");
		llvm::outs() << "\n        convert to quoted path  [D:/proj/dummygit\\server\\src\\server/net/connector.cpp] -> " << get_quoted_include_str(string("D:/proj/dummygit\\server\\src\\server/net/connector.cpp"));
	}

	void ParsingFile::print_relative_include()
	{
		llvm::outs() << "\n    ////////////////////////////////";
		llvm::outs() << "\n    " << ++m_i << ". relative include list :" << m_topIncludeIDs.size();
		llvm::outs() << "\n    ////////////////////////////////\n";

		FileID mainFileID = m_srcMgr->getMainFileID();

		for (auto itr : m_uses)
		{
			FileID file = itr.first;
			std::set<FileID> &be_uses = itr.second;

			llvm::outs() << "    file = [" << simplify_path(get_absolute_path(get_file_name(file)).c_str()) << "]\n";
			for (FileID be_used_file : be_uses)
			{
				llvm::outs() << "        use relative include = [" << get_relative_include_str(file, be_used_file) << "]\n";
			}

			llvm::outs() << "\n";
		}
	}

	void ParsingFile::print_range_to_filename()
	{
		llvm::outs() << "    " << ++m_i << ". include location to file name list:" << m_locToFileName.size() << "\n";

		SourceManager &srcMgr = m_rewriter->getSourceMgr();

		for (auto file : m_locToFileName)
		{
			SourceLocation loc		= file.first;
			const char* file_path	= file.second;

			const FileID fileID = srcMgr.getFileID(loc);
			if (fileID.isInvalid())
			{
				continue;
			}

			StringRef fileName = srcMgr.getFilename(loc);

			SourceRange range = m_locToRange[loc];

			PresumedLoc start_loc	= srcMgr.getPresumedLoc(range.getBegin());

			llvm::outs() << "        " << fileName << " line = " << start_loc.getLine()
			             << "[" << get_source_of_range(range) << "]" << " -> " << file_path << "\n";
		}
	}

	// ���ڵ��Ը���
	void ParsingFile::print_not_found_include_loc()
	{
		llvm::outs() << "    " << ++m_i << ". not found include loc: " << "" << "\n";

		for (auto itr : m_uses)
		{
			const std::set<FileID> &beuse_files = itr.second;

			for (FileID beuse_file : beuse_files)
			{
				SourceLocation loc = m_srcMgr->getIncludeLoc(beuse_file);
				if (m_locToRange.find(loc) == m_locToRange.end())
				{
					llvm::outs() << "        not found = " << get_absolute_file_name(beuse_file) << "\n";
					continue;
				}
			}
		}
	}

	// ���ڵ��Ը���
	void ParsingFile::print_same_line()
	{
		llvm::outs() << "    " << ++m_i << ". same line include loc: " << "" << "\n";

		std::map<string, std::set<int>> all_lines;

		for (auto itr : m_locToRange)
		{
			SourceLocation loc = itr.first;

			PresumedLoc presumedLoc = m_srcMgr->getPresumedLoc(loc);
			if (presumedLoc.isInvalid())
			{
				continue;
			}

			string fileName = get_absolute_file_name(presumedLoc.getFilename());
			int line = presumedLoc.getLine();
			SourceRange lineRange = get_cur_line(loc);

			std::set<int> &lines = all_lines[fileName];
			if (lines.find(line) != lines.end())
			{
				llvm::outs() << "        " << "found same line : " << line << "int [" << fileName << "]\n";
			}

			lines.insert(line);
		}
	}

	void ParsingFile::print()
	{
		//			if (m_unusedIncludeLocs.empty() && m_replaces.empty() && m_forwardDecls.empty())
		//			{
		//				return;
		//			}

		llvm::outs() << "\n\n////////////////////////////////////////////////////////////////\n";
		llvm::outs() << "[file = " << get_absolute_file_name(m_srcMgr->getMainFileID()) << "]";
		llvm::outs() << "\n////////////////////////////////////////////////////////////////\n";

		m_i = 0;

		//			print_direct_used_include();
		//			print_parents_by_id();
		// print_range_to_filename();
		// print_used_children();
		// print_unused_top_include();

		// print_used_names();
		print_root_cycle_used_names();

		//print_used_children();
		//print_all_file();
		print_unused_include();
		print_can_replace();
		print_can_forwarddecl();
	}

	bool ParsingFile::init_header_search_path(clang::SourceManager* srcMgr, clang::HeaderSearch &header_search)
	{
		m_srcMgr = srcMgr;
		m_headerSearchPaths = ComputeHeaderSearchPaths(header_search);

		return true;
	}

	void ParsingFile::merge_unused_line_to(const EachFile &newFile, EachFile &oldFile) const
	{
		EachFile::UnusedLineMap &oldLines = oldFile.m_unusedLines;

		for (EachFile::UnusedLineMap::iterator oldLine = oldLines.begin(), end = oldLines.end(); oldLine != end; )
		{
			int line = oldLine->first;

			if (newFile.is_line_unused(line))
			{
				++oldLine;
			}
			else
			{
				oldLines.erase(oldLine++);
			}
		}
	}

	void ParsingFile::merge_forward_line_to(const EachFile &newFile, EachFile &oldFile) const
	{
		for (auto newLineItr : newFile.m_forwards)
		{
			int line				= newLineItr.first;
			ForwardLine &newLine	= newLineItr.second;

			auto oldLineItr = oldFile.m_forwards.find(line);
			if (oldLineItr != oldFile.m_forwards.end())
			{
				ForwardLine &oldLine	= oldFile.m_forwards[line];
				oldLine.m_classes.insert(newLine.m_classes.begin(), newLine.m_classes.end());
			}
			else
			{
				oldFile.m_forwards[line] = newLine;
			}
		}
	}

	void ParsingFile::merge_replace_line_to(const EachFile &newFile, EachFile &oldFile) const
	{
		// ����������cpp�ļ�ʱ��δ�ڱ��ļ������滻��¼����˵�����ļ���û����Ҫ�滻��#include
		if (oldFile.m_replaces.empty())
		{
			return;
		}

		FileID newFileID;
		auto newFileItr = m_splitReplaces.begin();

		{
			for (auto itr = m_splitReplaces.begin(); itr != m_splitReplaces.end(); ++itr)
			{
				FileID top = itr->first;

				if (get_absolute_file_name(top) == oldFile.m_filename)
				{
					newFileID	= top;
					newFileItr	= itr;
					break;
				}
			}
		}

		if (newFileID.isInvalid())
		{
			return;
		}

		const ChildrenReplaceMap &newReplaceMap	= newFileItr->second;

		for (auto newLineItr : newFile.m_replaces)
		{
			int line				= newLineItr.first;
			ReplaceLine &newLine	= newLineItr.second;

			// �����Ƿ�����ͻ��
			bool conflict = oldFile.is_line_be_replaced(line);
			if (conflict)
			{
				ReplaceLine &oldLine	= oldFile.m_replaces[line];

				// �ҵ����оɵ�#include��Ӧ��FileID
				FileID beReplaceFileID;

				{
					for (auto childItr : newReplaceMap)
					{
						FileID child = childItr.first;
						if (get_absolute_file_name(child) == newLine.m_oldFile)
						{
							beReplaceFileID = child;
							break;
						}
					}
				}

				if (beReplaceFileID.isInvalid())
				{
					continue;
				}

				if (is_ancestor(beReplaceFileID, oldLine.m_oldFile.c_str()))
				{
					// �����ɵ��滻��Ϣ
					continue;
				}
				else if(is_ancestor(oldLine.m_oldFile.c_str(), beReplaceFileID))
				{
					// ��Ϊʹ���µ��滻��Ϣ
					oldLine.m_newInclude = newLine.m_newInclude;
				}
				else
				{
					// �����޷����滻��ɾ�����滻��¼
					oldFile.m_replaces.erase(line);
				}
			}
			// ������û�оɵ��滻��¼������к��������滻��¼
			else
			{
			}
		}
	}

	void ParsingFile::merge_count_to(FileMap &oldFiles) const
	{
		for (auto itr : m_parentIDs)
		{
			FileID child		= itr.first;
			string fileName		= get_absolute_file_name(child);
			EachFile &oldFile	= oldFiles[fileName];

			++oldFile.m_oldBeIncludeCount;

			if (m_unusedLocs.find(m_srcMgr->getIncludeLoc(child)) != m_unusedLocs.end())
			{
				++oldFile.m_newBeIncludeCount;
			}
		}

		for (FileID file : m_usedFiles)
		{
			string fileName		= get_absolute_file_name(file);
			EachFile &oldFile	= oldFiles[fileName];

			++oldFile.m_beUseCount;
		}
	}

	void ParsingFile::merge_to(FileMap &oldFiles)
	{
		FileMap newFiles;
		take_all_info_to(newFiles);

		for (auto fileItr : newFiles)
		{
			const string &fileName	= fileItr.first;
			const EachFile &newFile	= fileItr.second;

			auto findItr = oldFiles.find(fileName);

			bool found = (findItr != oldFiles.end());
			if (!found)
			{
				oldFiles[fileName] = newFile;
			}
			else
			{
				EachFile &oldFile = findItr->second;

				merge_unused_line_to(newFile, oldFile);
				merge_forward_line_to(newFile, oldFile);
				merge_replace_line_to(newFile, oldFile);
			}
		}

		merge_count_to(oldFiles);
	}

	// �ļ���ʽ�Ƿ���windows��ʽ�����з�Ϊ[\r\n]����Unix��Ϊ[\n]
	bool ParsingFile::IsWindowsFormat(FileID file)
	{
		SourceLocation fileStart = m_srcMgr->getLocForStartOfFile(file);
		if (fileStart.isInvalid())
		{
			return false;
		}

		// ��ȡ��һ�����ķ�Χ
		SourceRange firstLine		= get_cur_line(fileStart);
		SourceLocation firstLineEnd	= firstLine.getEnd();

		{
			bool err1 = true;
			bool err2 = true;

			const char* c1	= m_srcMgr->getCharacterData(firstLineEnd,						&err1);
			const char* c2	= m_srcMgr->getCharacterData(firstLineEnd.getLocWithOffset(1),	&err2);

			// ˵����һ��û�л��з������ߵ�һ�����Ľ�����ֻ����һ��\r��\n�ַ�
			if (err1 || err2)
			{
				return false;
			}

			// windows���и�ʽ
			if (*c1 == '\r' && *c2 == '\n')
			{
				return true;
			}
			// ��Unix���и�ʽ
			else if (*c1 == '\n')
			{
				return false;
			}
		}

		return false;
	}

	void ParsingFile::take_all_info_to(FileMap &out)
	{
		// �Ƚ���ǰcpp�ļ��漰�����ļ�ȫ����map��
		for (auto itr : m_locToRange)
		{
			SourceLocation loc	= itr.first;
			FileID file			= m_srcMgr->getFileID(loc);
			string fileName		= get_absolute_file_name(file);

			if (!Vsproject::instance.has_file(fileName))
			{
				continue;
			}

			// ���ɶ�Ӧ�ڸ��ļ��ĵļ�¼
			EachFile &eachFile = out[fileName];
			eachFile.m_isWindowFormat = IsWindowsFormat(file);
		}

		// 1. ����������а��ļ����д��
		take_unused_line_by_file(out);

		// 2. ��������ǰ���������ļ����д��
		take_new_forwarddecl_by_file(out);

		// 3. ���ļ��ڵ�#include�滻���ļ����д��
		take_replace_by_file(out);
	}

	// ����������а��ļ����д��
	void ParsingFile::take_unused_line_by_file(FileMap &out)
	{
		for (SourceLocation loc : m_unusedLocs)
		{
			PresumedLoc presumedLoc = m_srcMgr->getPresumedLoc(loc);
			if (presumedLoc.isInvalid())
			{
				llvm::outs() << "------->error: take_unused_line_by_file getPresumedLoc failed\n";
				continue;
			}

			string fileName			= get_absolute_file_name(presumedLoc.getFilename());
			if (!Vsproject::instance.has_file(fileName))
			{
				// llvm::outs() << "------->error: !Vsproject::instance.has_file(fileName) : " << fileName << "\n";
				continue;
			}

			SourceRange lineRange	= get_cur_line(loc);
			SourceRange nextLine	= get_next_line(loc);
			int line				= presumedLoc.getLine();

			EachFile &eachFile		= out[fileName];
			UselessLineInfo &unusedLine	= eachFile.m_unusedLines[line];

			unusedLine.m_beg	= m_srcMgr->getFileOffset(lineRange.getBegin());
			unusedLine.m_end	= m_srcMgr->getFileOffset(nextLine.getBegin());
			unusedLine.m_text	= get_source_of_range(lineRange);
		}
	}

	// ��������ǰ���������ļ����д��
	void ParsingFile::take_new_forwarddecl_by_file(FileMap &out)
	{
		if (m_forwardDecls.empty())
		{
			return;
		}

		for (auto itr : m_forwardDecls)
		{
			FileID file									= itr.first;
			std::set<const CXXRecordDecl*> &cxxRecords	= itr.second;

			if (!is_cycly_used(file))
			{
				continue;
			}

			for (const CXXRecordDecl *cxxRecord : cxxRecords)
			{
				SourceLocation insertLoc = get_insert_forward_line(file, *cxxRecord);
				if (insertLoc.isInvalid())
				{
					continue;
				}

				PresumedLoc presumedLoc = m_srcMgr->getPresumedLoc(insertLoc);
				if (presumedLoc.isInvalid())
				{
					llvm::outs() << "------->error: take_new_forwarddecl_by_file getPresumedLoc failed\n";
					continue;
				}

				string fileName			= get_absolute_file_name(presumedLoc.getFilename());

				if (!Vsproject::instance.has_file(fileName))
				{
					continue;
				}

				// ��ʼȡ������
				{
					int line					= presumedLoc.getLine();
					const string cxxRecordName	= get_cxxrecord_name(*cxxRecord);
					EachFile &eachFile			= out[fileName];
					ForwardLine &forwardLine	= eachFile.m_forwards[line];

					forwardLine.m_offsetAtFile	= m_srcMgr->getFileOffset(insertLoc);
					forwardLine.m_oldText		= get_source_of_line(insertLoc);
					forwardLine.m_classes.insert(cxxRecordName);

					{
						SourceLocation fileStart = m_srcMgr->getLocForStartOfFile(m_srcMgr->getFileID(insertLoc));
						if (fileStart.getLocWithOffset(forwardLine.m_offsetAtFile) != insertLoc)
						{
							llvm::outs() << "error: fileStart.getLocWithOffset(forwardLine.m_offsetAtFile) != insertLoc \n";
						}
					}
				}
			}
		}
	}

	// ���滻��Ϣ���ļ����д��
	void ParsingFile::split_replace_by_file(ReplaceFileMap &replaces)
	{
		for (auto itr : m_replaces)
		{
			FileID file						= itr.first;
			std::set<FileID> &to_replaces	= itr.second;

			auto parentItr = m_parentIDs.find(file);
			if (parentItr == m_parentIDs.end())
			{
				continue;
			}

			FileID parent = parentItr->second;
			replaces[parent].insert(itr);
		}
	}

	bool ParsingFile::is_force_included(FileID file)
	{
		return (nullptr == m_srcMgr->getFileEntryForID(m_srcMgr->getFileID(m_srcMgr->getIncludeLoc(file))));
	}

	void ParsingFile::take_be_replace_of_file(EachFile &eachFile, FileID top, const ChildrenReplaceMap &childernReplaces)
	{
		for (auto itr : childernReplaces)
		{
			FileID oldFile						= itr.first;
			const std::set<FileID> &to_replaces	= itr.second;

			// ȡ������#include���滻��Ϣ[�к� -> ���滻�ɵ�#include�б�]
			{
				bool is_be_force_included	= is_force_included(oldFile);

				SourceLocation include_loc	= m_srcMgr->getIncludeLoc(oldFile);
				SourceRange	lineRange		= get_cur_line_with_linefeed(include_loc);
				int line					= (is_be_force_included ? 0 : get_line_no(include_loc));

				ReplaceLine &replaceLine	= eachFile.m_replaces[line];
				replaceLine.m_oldFile		= get_absolute_file_name(oldFile);
				replaceLine.m_oldText		= get_raw_include_str(oldFile);
				replaceLine.m_beg			= m_srcMgr->getFileOffset(lineRange.getBegin());
				replaceLine.m_end			= m_srcMgr->getFileOffset(lineRange.getEnd());
				replaceLine.m_isSkip		= (is_be_force_included ? true : false);				// �����Ƿ���ǿ�ư���

				for (FileID replace_file : to_replaces)
				{
					SourceLocation include_loc	= m_srcMgr->getIncludeLoc(replace_file);

					ReplaceInfo replaceInfo;

					// ��¼[��#include����#include]
					replaceInfo.m_oldText	= get_raw_include_str(replace_file);
					replaceInfo.m_newText	= get_relative_include_str(m_parentIDs[oldFile], replace_file);

					// ��¼[�������ļ��������к�]
					replaceInfo.m_line			= get_line_no(include_loc);
					replaceInfo.m_newFile		= m_srcMgr->getFilename(include_loc);

					replaceLine.m_newInclude.push_back(replaceInfo);
				}
			}
		}
	}

	// ȡ�����ļ���#include�滻��Ϣ
	void ParsingFile::take_replace_by_file(FileMap &out)
	{
		if (m_replaces.empty())
		{
			return;
		}

		for (auto itr : m_splitReplaces)
		{
			FileID top									= itr.first;
			const ChildrenReplaceMap &childrenReplaces	= itr.second;

			// �����滻��¼
			string topFileName	= get_absolute_file_name(top);
			EachFile &newFile	= out[topFileName];

			take_be_replace_of_file(newFile, top, childrenReplaces);
		}
	}
}