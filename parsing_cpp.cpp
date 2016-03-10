///<------------------------------------------------------------------------------
//< @file:   parsing_cpp.cpp
//< @author: ������
//< @date:   2016��2��22��
//< @brief:
//< Copyright (c) 2016. All rights reserved.
///<------------------------------------------------------------------------------

#include "parsing_cpp.h"

#include <sstream>

#include <clang/AST/DeclTemplate.h>
#include <clang/Lex/HeaderSearch.h>
#include <clang/Lex/MacroInfo.h>
#include <clang/Lex/Lexer.h>
#include <clang/Rewrite/Core/Rewriter.h>
#include <llvm/Support/Path.h>

#include "tool.h"
#include "project.h"

namespace cxxcleantool
{
	bool ParsingFile::m_isOverWriteOption = false;

	// ��·��ת����linux·����ʽ����·���е�ÿ��'\'�ַ����滻Ϊ'/'
	string ToLinuxPath(const char *path)
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

	string FixPath(const string& path)
	{
		string ret = ToLinuxPath(path.c_str());

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
				const string path = FixPath(entry->getName());
				search_path_map.insert(make_pair(path, SrcMgr::C_System));
			}
		}

		for (clang::HeaderSearch::search_dir_iterator
		        itr = headerSearch.search_dir_begin(), end = headerSearch.search_dir_end();
		        itr != end; ++itr)
		{
			if (const DirectoryEntry* entry = itr->getDir())
			{
				const string path = FixPath(entry->getName());
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

			string absolute_path = GetAbsoluteFileName(path.c_str());

			include_dirs.push_back(HeaderSearchPath(absolute_path, path_type));
		}

		sort(include_dirs.begin(), include_dirs.end(), &sort_by_longest_length_first);
		return include_dirs;
	}

	std::string ParsingFile::FindFileInSearchPath(const vector<HeaderSearchPath>& searchPaths, const std::string& fileName)
	{
		if (llvm::sys::fs::exists(fileName))
		{
			return GetAbsolutePath(fileName.c_str());
		}
		// ��Ϊ���·��
		else if (!IsAbsolutePath(fileName))
		{
			// ������·���в���
			for (const HeaderSearchPath &search_path : searchPaths)
			{
				string candidate = GetAbsolutePath(search_path.path.c_str(), fileName.c_str());
				if (llvm::sys::fs::exists(candidate))
				{
					return candidate;
				}
			}
		}

		return fileName;
	}

	string ParsingFile::GetQuotedIncludeStr(const string& absoluteFilePath)
	{
		string path = SimplifyPath(absoluteFilePath.c_str());

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

	void ParsingFile::GenerateUnusedTopInclude()
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

	inline bool ParsingFile::IsLocBeUsed(SourceLocation loc) const
	{
		return m_usedLocs.find(loc) != m_usedLocs.end();
	}

	void ParsingFile::GenerateResult()
	{
		GenerateRootCycleUse();
		GenerateAllCycleUse();

		GenerateUnusedInclude();
		GenerateCanForwardDeclaration();
		GenerateCanReplace();
	}

	void ParsingFile::GenerateRootCycleUse()
	{
		if (m_uses.empty())
		{
			return;
		}

		FileID mainFileID = m_srcMgr->getMainFileID();

		GetCycleUseFile(mainFileID, m_rootCycleUse);

		if (m_rootCycleUse.empty())
		{
			return;
		}
	}

	void ParsingFile::GenerateCycleUsedChildren()
	{
		for (FileID usedFile : m_allCycleUse)
		{
			for (FileID child = usedFile, parent; HasParent(child); child = parent)
			{
				//				if (!Project::instance.IsAllowedClean(GetAbsoluteFileName(child)))
				//				{
				//					continue;
				//				}

				parent = m_parentIDs[child];
				m_cycleUsedChildren[parent].insert(usedFile);
			}
		}
	}

	FileID ParsingFile::GetCanReplaceTo(FileID top)
	{
		// ���ļ�����Ҳ��ʹ�õ������޷����滻
		if (IsCyclyUsed(top))
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
			FileID ancestor = GetCommonAncestor(children);
			if (ancestor == top)
			{
				return FileID();
			}

			// ������ļ��ǵ������ͬ�����Ծ��ǵ�ǰ���ȵĺ�������滻Ϊ����ļ��ǵĹ�ͬ����
			if (IsAncestor(ancestor, top))
			{
				return ancestor;
			}
		}

		return FileID();
	}

	bool ParsingFile::TryAddAncestorOfCycleUse()
	{
		FileID mainFile = m_srcMgr->getMainFileID();

		for (auto itr = m_allCycleUse.rbegin(); itr != m_allCycleUse.rend(); ++itr)
		{
			FileID file = *itr;

			for (FileID top = mainFile, lv2Top; top != file; top = lv2Top)
			{
				lv2Top = GetLvl2Ancestor(file, top);

				if (lv2Top == file)
				{
					break;
				}

				if (IsCyclyUsed(lv2Top))
				{
					continue;
				}

				if (IsReplaced(lv2Top))
				{
					FileID oldReplaceTo = *(m_replaces[lv2Top].begin());
					FileID canReplaceTo = GetCanReplaceTo(lv2Top);

					if (canReplaceTo != oldReplaceTo)
					{
						m_replaces.erase(lv2Top);

						llvm::outs() << "fatal: there is replace cleared!\n";
						return true;
					}

					break;
				}

				FileID canReplaceTo = GetCanReplaceTo(lv2Top);

				bool expand = false;

				// �����ɱ��滻
				if (canReplaceTo.isInvalid())
				{
					expand = ExpandAllCycleUse(lv2Top);
				}
				// ���ɱ��滻
				else
				{
					expand = ExpandAllCycleUse(canReplaceTo);
					if (!expand)
					{
						m_replaces[lv2Top].insert(canReplaceTo);
					}

					expand = true;
				}

				if (expand)
				{
					return true;
				}
			}
		}

		return false;
	}

	void ParsingFile::GenerateAllCycleUse()
	{
		m_allCycleUse = m_rootCycleUse;

		GenerateCycleUsedChildren();

		while (TryAddAncestorOfCycleUse())
		{
			GenerateCycleUsedChildren();
		};

		{
			for (FileID beusedFile : m_allCycleUse)
			{
				// �ӱ�ʹ���ļ��ĸ��ڵ㣬������Ͻ������ø������ù�ϵ
				for (FileID child = beusedFile, parent; HasParent(child); child = parent)
				{
					parent = m_parentIDs[child];
					m_usedLocs.insert(m_srcMgr->getIncludeLoc(child));
				}
			}

			/*
			FileID mainFile = m_srcMgr->getMainFileID();

			for (FileID beusedFile : m_allCycleUse)
			{
				bool hasReplace = false;

				// �ӱ�ʹ���ļ��ĸ��ڵ㣬������Ͻ������ø������ù�ϵ
				for (FileID top = mainFile, lv2Top; top != beusedFile; top = lv2Top)
				{
					lv2Top = GetLvl2Ancestor(beusedFile, top);

					if (IsReplaced(lv2Top))
					{
						llvm::outs() << "GenerateAllCycleUse is replaced " << GetAbsoluteFileName(lv2Top) << "\n";
						hasReplace = true;
						break;
					}

					m_usedLocs.insert(m_srcMgr->getIncludeLoc(lv2Top));
				}

				if (!hasReplace)
				{
					m_usedLocs.insert(m_srcMgr->getIncludeLoc(beusedFile));
				}
			}
			*/
		}
	}

	bool ParsingFile::ExpandAllCycleUse(FileID newCycleUse)
	{
		std::set<FileID> cycleUseList;
		GetCycleUseFile(newCycleUse, cycleUseList);

		int oldSize = m_allCycleUse.size();

		m_allCycleUse.insert(cycleUseList.begin(), cycleUseList.end());

		int newSize = m_allCycleUse.size();

		return newSize > oldSize;
	}

	bool ParsingFile::IsCyclyUsed(FileID file)
	{
		if (file == m_srcMgr->getMainFileID())
		{
			return true;
		}

		return m_allCycleUse.find(file) != m_allCycleUse.end();
	}

	bool ParsingFile::IsReplaced(FileID file)
	{
		return m_replaces.find(file) != m_replaces.end();
	}

	void ParsingFile::GenerateUnusedInclude()
	{
		for (auto locItr : m_locToRange)
		{
			SourceLocation loc = locItr.first;
			FileID file = m_srcMgr->getFileID(loc);

			if (!IsCyclyUsed(file))
			{
				continue;
			}

			if (!IsLocBeUsed(loc))
			{
				m_unusedLocs.insert(loc);
			}
		}

		GenerateUnusedTopInclude();
	}

	// ��ָ�����ļ��б����ҵ����ڴ����ļ��ĺ��
	std::set<FileID> ParsingFile::GetChildren(FileID ancestor, std::set<FileID> all_children/* ������ancestor���ӵ��ļ� */)
	{
		// ����ancestor�ĺ���ļ��б�
		std::set<FileID> children;

		// llvm::outs() << "ancestor = " << get_file_name(ancestor) << "\n";

		// ��ȡ���ڸ��ļ��ĺ���б����ļ�ʹ�õ�һ����
		for (FileID child : all_children)
		{
			if (!IsAncestor(child, ancestor))
			{
				continue;
			}

			// llvm::outs() << "    child = " << get_file_name(child) << "\n";
			children.insert(child);
		}

		return children;
	}

	void ParsingFile::GetCycleUseFile(FileID top, std::set<FileID> &out)
	{
		auto topUseItr = m_uses.find(top);
		if (topUseItr == m_uses.end())
		{
			out.insert(top);
			return;
		}

		std::set<FileID> left;
		std::set<FileID> history;

		history.insert(top);

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
	std::set<FileID> ParsingFile::GetDependOnFile(FileID ancestor, FileID child)
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

			if (!IsAncestor(cur, ancestor))
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

	int ParsingFile::GetDepth(FileID child)
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
	FileID ParsingFile::GetCommonAncestor(const std::set<FileID> &children)
	{
		FileID highest_child;
		int min_depth = 0;

		for (const FileID &child : children)
		{
			int depth = GetDepth(child);

			if (min_depth == 0 || depth < min_depth)
			{
				highest_child = child;
				min_depth = depth;
			}
		}

		bool found = false;

		FileID ancestor = highest_child;
		while (!IsCommonAncestor(children, ancestor))
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
	FileID ParsingFile::GetCommonAncestor(FileID child_1, FileID child_2)
	{
		if (child_1 == child_2)
		{
			return child_1;
		}

		int deepth_1	= GetDepth(child_1);
		int deepth_2	= GetDepth(child_2);

		FileID old		= (deepth_1 < deepth_2 ? child_1 : child_2);
		FileID young	= (old == child_1 ? child_2 : child_1);

		// �ӽϸ߲���ļ����ϲ��Ҹ��ļ���ֱ���ø��ļ�ҲΪ����һ���ļ���ֱϵ����Ϊֹ
		while (!IsAncestor(young, old))
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

	void ParsingFile::GenerateCanReplace()
	{
		SplitReplaceByFile(m_splitReplaces);
	}

	void ParsingFile::GenerateCount()
	{
	}

	bool ParsingFile::HasRecordBefore(FileID cur, const CXXRecordDecl &cxxRecord)
	{
		FileID recordAtFile = m_srcMgr->getFileID(cxxRecord.getLocStart());

		// �������ڵ��ļ������ã�����Ҫ�ټ�ǰ������
		if (IsCyclyUsed(recordAtFile))
		{
			return true;
		}

		// ����˵�������ڵ��ļ�δ������
		// 1. ��ʱ��������ඨ��֮ǰ�������ļ��У��Ƿ����и����ǰ������
		for (const CXXRecordDecl *prev = cxxRecord.getPreviousDecl(); prev; prev = prev->getPreviousDecl())
		{
			FileID prevFileId = m_srcMgr->getFileID(prev->getLocation());
			if (!IsCyclyUsed(prevFileId))
			{
				continue;
			}

			bool hasFind = (prevFileId <= cur);
			hasFind |= IsAncestor(prevFileId, cur);

			if (hasFind)
			{
				return true;
			}
		}

		// 2. �������ʹ�ø�����ļ�ǰ���Ƿ�������ض���
		for (CXXRecordDecl::redecl_iterator redeclItr = cxxRecord.redecls_begin(), end = cxxRecord.redecls_end(); redeclItr != end; ++redeclItr)
		{
			const TagDecl *redecl = *redeclItr;

			FileID redeclFileID = m_srcMgr->getFileID(redecl->getLocation());
			if (!IsCyclyUsed(redeclFileID))
			{
				continue;
			}

			bool hasFind = (redeclFileID <= cur);
			hasFind |= IsAncestor(redeclFileID, cur);

			if (hasFind)
			{
				return true;
			}
		}

		return false;
	}

	void ParsingFile::GenerateCanForwardDeclaration()
	{
		// ����ʹ���ļ���ǰ��������¼ɾ��
		for (auto itr = m_forwardDecls.begin(), end = m_forwardDecls.end(); itr != end;)
		{
			FileID file										= itr->first;
			std::set<const CXXRecordDecl*> &old_forwards	= itr->second;

			if (IsCyclyUsed(file))
			{
				++itr;
			}
			else
			{
				m_forwardDecls.erase(itr++);
				continue;
			}

			std::set<const CXXRecordDecl*> can_forwards;

			for (const CXXRecordDecl* cxxRecordDecl : old_forwards)
			{
				if (!HasRecordBefore(file, *cxxRecordDecl))
				{
					can_forwards.insert(cxxRecordDecl);
				}
			}

			if (can_forwards.size() < old_forwards.size())
			{
				/*
				// ��ӡǰ��Ա�
				{
					llvm::outs() << "\nprocessing file: " << GetAbsoluteFileName(file) << "\n";
					llvm::outs() << "    old forwarddecl list: \n";

					for (const CXXRecordDecl* cxxRecordDecl : old_forwards)
					{
						llvm::outs() << "        forwarddecl: " << GetCxxrecordName(*cxxRecordDecl) << " in " << GetAbsoluteFileName(file) << "\n";
					}

					llvm::outs() << "\n";
					llvm::outs() << "    new forwarddecl list: \n";

					for (const CXXRecordDecl* cxxRecordDecl : can_forwards)
					{
						llvm::outs() << "        forwarddecl: " << GetCxxrecordName(*cxxRecordDecl) << "\n";
					}
				}
				*/

				old_forwards = can_forwards;
			}
		}
	}

	std::string ParsingFile::GetSourceOfRange(SourceRange range)
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

	std::string ParsingFile::GetSourceOfLine(SourceLocation loc)
	{
		return GetSourceOfRange(GetCurLine(loc));
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
	SourceRange ParsingFile::GetCurLine(SourceLocation loc)
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
	SourceRange ParsingFile::GetCurLineWithLinefeed(SourceLocation loc)
	{
		SourceRange curLine		= GetCurLine(loc);
		SourceRange nextLine	= GetNextLine(loc);

		return SourceRange(curLine.getBegin(), nextLine.getBegin());
	}

	// ���ݴ���Ĵ���λ�÷�����һ�еķ�Χ
	SourceRange ParsingFile::GetNextLine(SourceLocation loc)
	{
		SourceRange curLine		= GetCurLine(loc);
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

		SourceRange nextLine	= GetCurLine(lineEnd.getLocWithOffset(skip));
		return nextLine;
	}

	bool ParsingFile::IsEmptyLine(SourceRange line)
	{
		string lineText = GetSourceOfRange(line);
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

	inline int ParsingFile::GetLineNo(SourceLocation loc)
	{
		PresumedLoc presumed_loc	= m_srcMgr->getPresumedLoc(loc);
		return presumed_loc.isValid() ? presumed_loc.getLine() : 0;
	}

	// �Ƿ��ǻ��з�
	bool ParsingFile::IsNewLineWord(SourceLocation loc)
	{
		string text = GetSourceOfRange(SourceRange(loc, loc.getLocWithOffset(1)));
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
	std::string ParsingFile::GetRawIncludeStr(FileID file)
	{
		SourceManager &srcMgr = m_rewriter->getSourceMgr();
		SourceLocation loc = srcMgr.getIncludeLoc(file);

		if (m_locToRange.find(loc) == m_locToRange.end())
		{
			return "";
		}

		SourceRange range = m_locToRange[loc];
		return GetSourceOfRange(range);
	}

	// curλ�õĴ���ʹ��srcλ�õĴ���
	void ParsingFile::Use(SourceLocation cur, SourceLocation src, const char* name /* = nullptr */)
	{
		FileID curFileID = m_srcMgr->getFileID(cur);
		FileID srcFileID = m_srcMgr->getFileID(src);

		UseIinclude(curFileID, srcFileID, name, GetLineNo(src));
	}

	void ParsingFile::UseName(FileID file, FileID beusedFile, const char* name /* = nullptr */, int line)
	{
		if (nullptr == name)
		{
			return;
		}

		// �ҵ����ļ���ʹ��������ʷ��¼����Щ��¼���ļ�������
		std::vector<UseNameInfo> &useNames = m_useNames[file];

		bool found = false;

		// �����ļ����ҵ���Ӧ�ļ��ı�ʹ��������ʷ������������ʹ�ü�¼
		for (UseNameInfo &info : useNames)
		{
			if (info.file == beusedFile)
			{
				found = true;
				info.AddName(name, line);
				break;
			}
		}

		if (!found)
		{
			UseNameInfo info;
			info.file = beusedFile;
			info.AddName(name, line);

			useNames.push_back(info);
		}
	}

	// ���ݵ�ǰ�ļ������ҵ�2������ȣ���rootΪ��һ�㣩������ǰ�ļ������ڵ�2�㣬�򷵻ص�ǰ�ļ�
	FileID ParsingFile::GetLvl2Ancestor(FileID file, FileID root)
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

	// ��2���ļ��Ƿ��ǵ�1���ļ�������
	bool ParsingFile::IsAncestor(FileID young, FileID old) const
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

	// ��2���ļ��Ƿ��ǵ�1���ļ�������
	bool ParsingFile::IsAncestor(FileID young, const char* old) const
	{
		auto parentItr = m_parentIDs.begin();

		while ((parentItr = m_parentIDs.find(young)) != m_parentIDs.end())
		{
			FileID parent = parentItr->second;
			if (GetAbsoluteFileName(parent) == old)
			{
				return true;
			}

			young = parent;
		}

		return false;
	}

	// ��2���ļ��Ƿ��ǵ�1���ļ�������
	bool ParsingFile::IsAncestor(const char* young, FileID old) const
	{
		// �ڸ��ӹ�ϵ����������ļ�ͬ����FileID�������#includeͬһ�ļ������Ϊ���ļ���������ͬ��FileID��
		for (auto itr : m_parentIDs)
		{
			FileID child	= itr.first;

			if (GetAbsoluteFileName(child) == young)
			{
				if (IsAncestor(child, old))
				{
					return true;
				}
			}
		}

		return false;
	}

	bool ParsingFile::IsCommonAncestor(const std::set<FileID> &children, FileID old)
	{
		for (const FileID &child : children)
		{
			if (child == old)
			{
				continue;
			}

			if (!IsAncestor(child, old))
			{
				return false;
			}
		}

		return true;
	}

	bool ParsingFile::HasParent(FileID child)
	{
		return m_parentIDs.find(child) != m_parentIDs.end();
	}

	inline FileID ParsingFile::GetParent(FileID child)
	{
		auto itr = m_parentIDs.find(child);
		if (itr == m_parentIDs.end())
		{
			return FileID();
		}

		return itr->second;
	}

	// ��ǰ�ļ�ʹ��Ŀ���ļ�
	void ParsingFile::UseIinclude(FileID file, FileID beusedFile, const char* name /* = nullptr */, int line)
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

		UseName(file, beusedFile, name, line);
	}

	void ParsingFile::UseMacro(SourceLocation loc, const MacroDefinition &macro, const Token &macroNameTok)
	{
		MacroInfo *info = macro.getMacroInfo();
		if (nullptr == info)
		{
			return;
		}

		loc = m_srcMgr->getSpellingLoc(loc);

		string macroName = macroNameTok.getIdentifierInfo()->getName().str() + "[macro]";

		// llvm::outs() << "macro id = " << macroName.getIdentifierInfo()->getName().str() << "\n";
		Use(loc, info->getDefinitionLoc(), macroName.c_str());
	}

	// ��ǰλ��ʹ��Ŀ������
	void ParsingFile::UseType(SourceLocation loc, const QualType &t)
	{
		if (t.isNull())
		{
			return;
		}

		if (isa<TypedefType>(t))
		{
			const TypedefType *typedefType = cast<TypedefType>(t);
			const TypedefNameDecl *typedefNameDecl = typedefType->getDecl();

			OnUseDecl(loc, typedefNameDecl);

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

			OnUseDecl(loc, templateType->getTemplateName().getAsTemplateDecl());

			for (int i = 0, size = templateType->getNumArgs(); i < size; ++i)
			{
				const TemplateArgument &arg = templateType->getArg((unsigned)i);

				TemplateArgument::ArgKind argKind = arg.getKind();

				switch (argKind)
				{
				case TemplateArgument::Type:
					// arg.getAsType().dump();
					UseType(loc, arg.getAsType());
					break;

				case TemplateArgument::Declaration:
					OnUseDecl(loc, arg.getAsDecl());
					break;

				case TemplateArgument::Expression:
					Use(loc, arg.getAsExpr()->getLocStart());
					break;

				case TemplateArgument::Template:
					OnUseDecl(loc, arg.getAsTemplate().getAsTemplateDecl());
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
			UseType(loc, elaboratedType->getNamedType());
		}
		else if (isa<AttributedType>(t))
		{
			const AttributedType *attributedType = cast<AttributedType>(t);
			UseType(loc, attributedType->getModifiedType());
		}
		else if (isa<FunctionType>(t))
		{
			const FunctionType *functionType = cast<FunctionType>(t);

			// ʶ�𷵻�ֵ����
			{
				// �����ķ���ֵ
				QualType returnType = functionType->getReturnType();
				UseType(loc, returnType);
			}
		}
		else if (isa<MemberPointerType>(t))
		{
			const MemberPointerType *memberPointerType = cast<MemberPointerType>(t);
			UseType(loc, memberPointerType->getPointeeType());
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
				UseType(loc, decl->getDefaultArgument());
			}

			OnUseDecl(loc, decl);
		}
		else if (isa<ParenType>(t))
		{
			const ParenType *parenType = cast<ParenType>(t);
			// llvm::errs() << "------------ ParenType dump ------------:\n";
			// llvm::errs() << "------------ parenType->getInnerType().dump() ------------:\n";
			UseType(loc, parenType->getInnerType());
		}
		else if (isa<InjectedClassNameType>(t))
		{
			const InjectedClassNameType *injectedClassNameType = cast<InjectedClassNameType>(t);
			// llvm::errs() << "------------InjectedClassNameType dump:\n";
			// llvm::errs() << "------------injectedClassNameType->getInjectedSpecializationType().dump():\n";
			UseType(loc, injectedClassNameType->getInjectedSpecializationType());
		}
		else if (isa<PackExpansionType>(t))
		{
			const PackExpansionType *packExpansionType = cast<PackExpansionType>(t);
			// llvm::errs() << "\n------------PackExpansionType------------\n";
			UseType(loc, packExpansionType->getPattern());
		}
		else if (isa<DecltypeType>(t))
		{
			const DecltypeType *decltypeType = cast<DecltypeType>(t);
			// llvm::errs() << "\n------------DecltypeType------------\n";
			UseType(loc, decltypeType->getUnderlyingType());
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

			OnUseRecord(loc, cxxRecordDecl);
		}
		else if (t->isArrayType())
		{
			const ArrayType *arrayType = cast<ArrayType>(t);
			UseType(loc, arrayType->getElementType());
		}
		else if (t->isVectorType())
		{
			const VectorType *vectorType = cast<VectorType>(t);
			UseType(loc, vectorType->getElementType());
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

			UseType(loc, pointeeType);
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

			OnUseDecl(loc, decl);
		}
		else
		{
			//llvm::errs() << "-------------- haven't support type --------------\n";
			//t->dump();
		}
	}

	string ParsingFile::GetCxxrecordName(const CXXRecordDecl &cxxRecordDecl)
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

	// ��ָ��λ��������֮���ע�ͣ�ֱ�������һ��token
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
	SourceLocation ParsingFile::GetInsertForwardLine(FileID at, const CXXRecordDecl &cxxRecord)
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
		if (IsAncestor(recordAtFile, at))
		{
			// �ҵ�A�����ȣ�Ҫ������ȱ�����B�Ķ���
			FileID ancestor = GetLvl2Ancestor(recordAtFile, at);
			if (GetParent(ancestor) != at)
			{
				return SourceLocation();
			}

			// �ҵ������ļ�����Ӧ��#include����λ��
			SourceLocation includeLoc	= m_srcMgr->getIncludeLoc(ancestor);
			SourceRange line			= GetCurLine(includeLoc);
			return line.getBegin();
		}
		// 2. �������ļ��ǵ�ǰ�ļ�������
		else if (IsAncestor(at, recordAtFile))
		{
			// ����Ҫ����
		}
		// 3. �������ļ��͵�ǰ�ļ�û��ֱϵ��ϵ
		else
		{
			// �ҵ���2���ļ��Ĺ�ͬ����
			FileID common_ancestor = GetCommonAncestor(recordAtFile, at);
			if (common_ancestor.isInvalid())
			{
				return SourceLocation();
			}

			// �ҵ��������ļ������ȣ�Ҫ������ȱ����ǹ�ͬ���ȵĶ���
			FileID ancestor = GetLvl2Ancestor(recordAtFile, common_ancestor);
			if (GetParent(ancestor) != common_ancestor)
			{
				return SourceLocation();
			}

			// �ҵ������ļ�����Ӧ��#include����λ��
			SourceLocation includeLoc	= m_srcMgr->getIncludeLoc(ancestor);
			SourceRange line			= GetCurLine(includeLoc);
			return line.getBegin();
		}

		// ������ִ�е�����
		return SourceLocation();
	}

	void ParsingFile::UseForward(SourceLocation loc, const CXXRecordDecl *cxxRecordDecl)
	{
		FileID file = m_srcMgr->getFileID(loc);
		if (file.isInvalid())
		{
			return;
		}

		string cxxRecordName = GetCxxrecordName(*cxxRecordDecl);
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
			if (IsAncestor(recordAtFile, file))
			{
				return;
			}

			// 2. ����˵����ǰ�ļ����������ļ��ǲ�ͬ��֧�����������ļ��ǵ�ǰ�ļ�������

			// �ҵ���2���ļ��Ĺ�ͬ����
			FileID common_ancestor = GetCommonAncestor(recordAtFile, file);
			if (common_ancestor.isInvalid())
			{
				return;
			}

			// ��ǰ�ļ����ù�ͬ����
			m_usedFiles.insert(common_ancestor);
		}
	}

	void ParsingFile::UseVar(SourceLocation loc, const QualType &var)
	{
		if (!var->isPointerType() && !var->isReferenceType())
		{
			UseType(loc, var);
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
			UseType(loc, var);
			return;
		}

		const CXXRecordDecl *cxxRecordDecl = pointeeType->getAsCXXRecordDecl();
		if (nullptr == cxxRecordDecl)
		{
			UseType(loc, var);
			return;
		}

		if (isa<ClassTemplateSpecializationDecl>(cxxRecordDecl))
		{
			UseType(loc, var);
			return;
			// cxxRecord->dump(llvm::outs());
		}

		UseForward(loc, cxxRecordDecl);
	}

	void ParsingFile::OnUseDecl(SourceLocation loc, const NamedDecl *nameDecl)
	{
		std::stringstream name;
		name << nameDecl->getQualifiedNameAsString() << "[" << nameDecl->getDeclKindName() << "]";

		Use(loc, nameDecl->getLocStart(), name.str().c_str());
	}

	void ParsingFile::OnUseRecord(SourceLocation loc, const CXXRecordDecl *record)
	{
		Use(loc, record->getLocStart(), GetCxxrecordName(*record).c_str());
	}

	bool ParsingFile::CanClean(FileID file)
	{
		return Project::instance.CanClean(GetAbsoluteFileName(file));
	}

	void ParsingFile::PrintParentsById()
	{
		llvm::outs() << "    " << ++m_i << ". list of parent id:" << m_parentIDs.size() << "\n";

		for (auto childparent : m_parentIDs)
		{
			FileID childFileID = childparent.first;
			FileID parentFileID = childparent.second;

			// ����ӡ����Ŀ���ļ���ֱ�ӹ������ļ�
			if (!CanClean(childFileID) && !CanClean(parentFileID))
			{
				continue;
			}

			llvm::outs() << "        [" << GetFileName(childFileID) << "] parent = [" << GetFileName(parentFileID) << "]\n";
		}
	}

	string ParsingFile::DebugFileIncludeText(FileID file, bool is_absolute_name /* = false */)
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
			includeToken = GetSourceOfRange(range);
		}

		if (is_absolute_name)
		{
			fileName = GetFileName(file);
			parentFileName = presumedLoc.getFilename();
		}
		else
		{
			fileName = GetAbsoluteFileName(file);
			parentFileName = GetAbsoluteFileName(presumedLoc.getFilename());
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

	string ParsingFile::DebugFileDirectUseText(FileID file)
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
				includeToken = GetSourceOfRange(range);
			}

			fileName = GetFileName(file);
		}

		text << "{[" << includeToken << "] line = " << (presumedLoc.isValid() ? presumedLoc.getLine() : 0);
		text << "} -> ";
		text << "[" << fileName << "]";

		return text.str();
	}

	string ParsingFile::DebugLocText(SourceLocation loc)
	{
		PresumedLoc presumedLoc = m_srcMgr->getPresumedLoc(loc);
		if (presumedLoc.isInvalid())
		{
			return "";
		}

		string line = GetSourceOfLine(loc);
		std::stringstream text;
		text << "[" << line << "] in [";
		text << GetFileName(m_srcMgr->getFileID(loc));
		text << "] line = " << presumedLoc.getLine();
		return text.str();
	}

	string ParsingFile::DebugLocIncludeText(SourceLocation loc)
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
			includeToken = GetSourceOfRange(range);
		}

		text << "[" + includeToken << "] line = " << presumedLoc.getLine();
		return text.str();
	}

	void ParsingFile::PrintUse()
	{
		llvm::outs() << "    " << ++m_i << ". list of include referenced: " << m_uses.size() << "\n";

		SourceManager &srcMgr = m_rewriter->getSourceMgr();

		for (auto use_list : m_uses)
		{
			FileID file = use_list.first;

			if (!CanClean(file))
			{
				continue;
			}

			llvm::outs() << "        file = " << GetAbsoluteFileName(use_list.first) << "\n";

			for (FileID beuse : use_list.second)
			{
				// llvm::outs() << "            use = " << get_file_name(beuse) << "\n";
				// llvm::outs() << "            use = " << get_file_name(beuse) << " [" << token << "] in [" << presumedLoc.getFilename() << "] line = " << presumedLoc.getLine() << "\n";
				llvm::outs() << "            use = " << DebugFileIncludeText(beuse) << "\n";
			}

			llvm::outs() << "\n";
		}
	}

	void ParsingFile::PrintUsedNames()
	{
		llvm::outs() << "    " << ++m_i << ". list of use name: " << m_uses.size() << "\n";

		for (auto useItr : m_useNames)
		{
			FileID file									= useItr.first;
			const std::vector<UseNameInfo> &useNames	= useItr.second;

			if (!CanClean(file))
			{
				continue;
			}

			DebugUsedNames(file, useNames);
		}
	}

	void ParsingFile::DebugUsedNames(FileID file, const std::vector<UseNameInfo> &useNames)
	{
		llvm::outs() << "        file = " << GetAbsoluteFileName(file) << "\n";

		for (const UseNameInfo &beuse : useNames)
		{
			llvm::outs() << "            use = " << DebugFileIncludeText(beuse.file) << "\n";

			for (const string& name : beuse.nameVec)
			{
				std::stringstream linesStream;

				auto linesItr = beuse.nameMap.find(name);
				if (linesItr != beuse.nameMap.end())
				{
					for (int line : linesItr->second)
					{
						linesStream << line << ", ";
					}
				}

				std::string linesText = linesStream.str();

				strtool::try_strip_right(linesText, std::string(", "));

				llvm::outs() << "                name = " << name << " -> [line = " << linesText << "]" << "\n";
			}

			llvm::outs() << "\n";
		}
	}

	void ParsingFile::PrintRootCycleUsedNames()
	{
		llvm::outs() << "    " << ++m_i << ". list of root cycle use name: " << m_rootCycleUse.size() << "\n";

		for (auto useItr : m_useNames)
		{
			FileID file									= useItr.first;
			const std::vector<UseNameInfo> &useNames	= useItr.second;

			if (!CanClean(file))
			{
				continue;
			}

			if (m_rootCycleUse.find(file) == m_rootCycleUse.end() && file != m_srcMgr->getMainFileID())
			{
				continue;
			}

			DebugUsedNames(file, useNames);

			llvm::outs() << "\n";
		}
	}

	void ParsingFile::PrintAllCycleUsedNames()
	{
		llvm::outs() << "    " << ++m_i << ". list of all cycle use name: " << m_allCycleUse.size() << "\n";

		for (auto useItr : m_useNames)
		{
			FileID file									= useItr.first;
			const std::vector<UseNameInfo> &useNames	= useItr.second;

			if (!CanClean(file))
			{
				continue;
			}

			if (m_allCycleUse.find(file) == m_allCycleUse.end() && file != m_srcMgr->getMainFileID())
			{
				continue;
			}

			if (m_rootCycleUse.find(file) == m_rootCycleUse.end() && file != m_srcMgr->getMainFileID())
			{
				llvm::outs() << "        <--------- new add cycle use file --------->\n";
			}

			DebugUsedNames(file, useNames);
			llvm::outs() << "\n";
		}
	}

	bool ParsingFile::IsNeedPrintFile(FileID file)
	{
		if (CanClean(file))
		{
			return true;
		}

		FileID parent = GetParent(file);
		if (CanClean(parent))
		{
			return true;
		}

		return false;
	}

	void ParsingFile::PrintRootCycleUse()
	{
		llvm::outs() << "    " << ++m_i << ". list of root cycle use: " << m_rootCycleUse.size() << "\n";
		for (auto file : m_rootCycleUse)
		{
			if (!IsNeedPrintFile(file))
			{
				continue;
			}

			llvm::outs() << "        file = " << GetFileName(file) << "\n";
		}

		llvm::outs() << "\n";
	}

	void ParsingFile::PrintAllCycleUse()
	{
		llvm::outs() << "    " << ++m_i << ". list of all cycle use: " << m_allCycleUse.size() << "\n";
		for (auto file : m_allCycleUse)
		{
			if (!IsNeedPrintFile(file))
			{
				continue;
			}

			if (m_rootCycleUse.find(file) == m_rootCycleUse.end())
			{
				llvm::outs() << "        <--------- new add cycle use file --------->\n";
			}

			llvm::outs() << "        file = " << GetFileName(file) << "\n";
		}

		llvm::outs() << "\n";
	}

	void ParsingFile::PrintUsedChildren()
	{
		llvm::outs() << "\n    " << ++m_i << ". list of used children file: " << m_usedChildren.size();

		for (auto usedItr : m_usedChildren)
		{
			FileID file = usedItr.first;

			llvm::outs() << "\n        file = " << GetAbsoluteFileName(file) << "\n";

			for (auto usedChild : usedItr.second)
			{
				llvm::outs() << "            use children = " << DebugFileIncludeText(usedChild) << "\n";
			}

			llvm::outs() << "\n";
		}
	}

	void ParsingFile::PrintAllFile()
	{
		llvm::outs() << "\n    " << ++m_i << ". list of all file: " << m_files.size();

		for (auto itr : m_files)
		{
			const string &name = itr.first;

			if (!Project::instance.CanClean(name))
			{
				continue;
			}

			llvm::outs() << "\n        file = " << name;
		}

		llvm::outs() << "\n";
	}

	void ParsingFile::PrintUsedTopInclude()
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

	void ParsingFile::PrintTopIncludeById()
	{
		llvm::outs() << "\n////////////////////////////////\n";
		llvm::outs() << ++m_i << ". list of top include by id:" << m_topIncludeIDs.size() << "\n";
		llvm::outs() << "////////////////////////////////\n";

		SourceManager &srcMgr = m_rewriter->getSourceMgr();

		for (auto file : m_topIncludeIDs)
		{
			llvm::outs() << "    top include = " << GetFileName(file) << "\n";
		}
	}

	void ParsingFile::PrintUnusedIncludeOfFiles(FileHistoryMap &files)
	{
		for (auto fileItr : files)
		{
			const string &file	= fileItr.first;
			FileHistory &eachFile	= fileItr.second;

			if (!Project::instance.CanClean(file))
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

	void ParsingFile::PrintCanForwarddeclOfFiles(FileHistoryMap &files)
	{
		for (auto fileItr : files)
		{
			const string &file	= fileItr.first;
			FileHistory &eachFile	= fileItr.second;

			if (!Project::instance.CanClean(file))
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

	void ParsingFile::PrintCanReplaceOfFiles(FileHistoryMap &files)
	{
		for (auto fileItr : files)
		{
			const string &file	= fileItr.first;
			FileHistory &eachFile	= fileItr.second;

			if (!Project::instance.CanClean(file))
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

				// ���磬���: [line = 1] -> {old text = [#include "a.h"]}
				llvm::outs() << "            [line = " << line << "] -> {old text = [" << replaceLine.m_oldText << "]}";

				if (replaceLine.m_isSkip)
				{
					llvm::outs() << "  ==>  [warn: will skip this replacement, for it's force included]";
				}

				llvm::outs() << "\n";

				// ���磬���: replace to = #include <../1.h> in [./2.h : line = 3]
				for (const ReplaceInfo &replaceInfo : replaceLine.m_newInclude)
				{
					// ���滻�Ĵ����ݲ��䣬��ֻ��ӡһ��
					if (replaceInfo.m_newText == replaceInfo.m_oldText)
					{
						llvm::outs() << "                replace to = " << replaceInfo.m_oldText;
					}
					// ���򣬴�ӡ�滻ǰ���滻���#include����
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

	void ParsingFile::PrintUnusedInclude()
	{
		// ������ļ��ڼ����ļ��ڵ������ļ��е�#include���޷���ɾ�����򲻴�ӡ
		if (m_unusedLocs.empty())
		{
			return;
		}

		FileHistoryMap files;
		TakeAllInfoTo(files);

		int num = 0;

		for (auto itr : files)
		{
			const FileHistory &eachFile = itr.second;
			num += eachFile.m_unusedLines.empty() ? 0 : 1;
		}

		llvm::outs() << "\n    " << ++m_i << ". list of unused include : file count = " << num << "";

		PrintUnusedIncludeOfFiles(files);
	}

	void ParsingFile::PrintCanForwarddecl()
	{
		if (m_forwardDecls.empty())
		{
			return;
		}

		FileHistoryMap files;
		TakeAllInfoTo(files);

		int num = 0;

		for (auto itr : files)
		{
			const FileHistory &eachFile = itr.second;
			num += eachFile.m_forwards.empty() ? 0 : 1;
		}

		llvm::outs() << "\n    " << ++m_i << ". forward declaration list : file count = " << num << "\n";

		PrintCanForwarddeclOfFiles(files);
	}

	void ParsingFile::PrintUnusedTopInclude()
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
			llvm::outs() << "        unused " << DebugLocIncludeText(loc) << "\n";
			// llvm::outs() << "        unused whole line = [" << get_source_of_range(get_cur_line(loc)) << "] line = " << presumedLoc.getLine() << "\n";
		}
	}

	const char* ParsingFile::GetFileName(FileID file) const
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

	string ParsingFile::GetAbsoluteFileName(const char *raw_file_name)
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
			absolute_path = GetAbsolutePath(raw_file_name);
		}
		else
		{
			absolute_path = raw_file_name;
		}

		absolute_path = SimplifyPath(absolute_path.c_str());
		return absolute_path;
	}

	string ParsingFile::GetAbsoluteFileName(FileID file) const
	{
		const char* raw_file_name = GetFileName(file);
		return GetAbsoluteFileName(raw_file_name);
	}

	const char* ParsingFile::GetIncludeValue(const char* include_str)
	{
		include_str += sizeof("#include") / sizeof(char);

		while (IsBlank(*include_str))
		{
			++include_str;
		}

		return include_str;
	}

	std::string ParsingFile::GetRelativeIncludeStr(FileID f1, FileID f2)
	{
		string raw_include2 = GetRawIncludeStr(f2);
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

		string absolute_path2 = GetAbsoluteFileName(f2);

		string include2 = GetQuotedIncludeStr(absolute_path2);
		if (!include2.empty())
		{
			return "#include " + include2;
		}
		else
		{
			string absolute_path1 = GetAbsoluteFileName(f1);
			include2 = "\"" + filetool::get_relative_path(absolute_path1.c_str(), absolute_path2.c_str()) + "\"";
		}

		return "#include " + include2;
	}

	bool ParsingFile::IsAbsolutePath(const string& path)
	{
		return llvm::sys::path::is_absolute(path);
	}

	// ��·��
	// d:/a/b/c/../../d/ -> d:/d/
	std::string ParsingFile::SimplifyPath(const char* path)
	{
		string native_path = ToLinuxPath(path);
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

	string ParsingFile::GetAbsolutePath(const char *path)
	{
		llvm::SmallString<512> absolute_path(path);
		std::error_code error = llvm::sys::fs::make_absolute(absolute_path);
		if (error)
		{
			return "";
		}

		return absolute_path.str();
	}

	string ParsingFile::GetAbsolutePath(const char *base_path, const char* relative_path)
	{
		llvm::SmallString<512> absolute_path(base_path);
		llvm::sys::path::append(absolute_path, relative_path);

		return absolute_path.str();
	}

	string ParsingFile::GetParentPath(const char* path)
	{
		llvm::StringRef parent = llvm::sys::path::parent_path(path);
		return parent.str();
	}

	void ParsingFile::Clean()
	{
		if (Project::instance.m_isDeepClean)
		{
			CleanAllFile();
		}
		else
		{
			CleanMainFile();
		}

		if (ParsingFile::m_isOverWriteOption)
		{
			m_rewriter->overwriteChangedFiles();
		}
	}

	void ParsingFile::ReplaceText(FileID file, int beg, int end, string text)
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

	void ParsingFile::InsertText(FileID file, int loc, string text)
	{
		SourceLocation fileBegLoc	= m_srcMgr->getLocForStartOfFile(file);
		SourceLocation insertLoc	= fileBegLoc.getLocWithOffset(loc);

		m_rewriter->InsertText(insertLoc, text, true, true);
	}

	void ParsingFile::RemoveText(FileID file, int beg, int end)
	{
		SourceLocation fileBegLoc	= m_srcMgr->getLocForStartOfFile(file);
		if (fileBegLoc.isInvalid())
		{
			llvm::errs() << "\n------->error: fileBegLoc.isInvalid(), remove text in [" << GetAbsoluteFileName(file) << "] failed!\n";
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
			llvm::errs() << "\n------->error: remove text = [" << GetSourceOfRange(range) << "] in [" << GetAbsoluteFileName(file) << "] failed!\n";
		}
	}

	void ParsingFile::CleanByUnusedLine(const FileHistory &eachFile, FileID file)
	{
		if (eachFile.m_unusedLines.empty())
		{
			return;
		}

		for (auto unusedLineItr : eachFile.m_unusedLines)
		{
			int line				= unusedLineItr.first;
			UselessLineInfo &unusedLine	= unusedLineItr.second;

			RemoveText(file, unusedLine.m_beg, unusedLine.m_end);
		}
	}

	void ParsingFile::CleanByForward(const FileHistory &eachFile, FileID file)
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

			InsertText(file, forwardLine.m_offsetAtFile, text.str());
		}
	}

	void ParsingFile::CleanByReplace(const FileHistory &eachFile, FileID file)
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

			ReplaceText(file, replaceLine.m_beg, replaceLine.m_end, text.str());
		}
	}

	void ParsingFile::CleanBy(const FileHistoryMap &files)
	{
		for (auto itr : files)
		{
			const string &fileName		= itr.first;

			if (!ProjectHistory::instance.HasFile(fileName))
			{
				continue;
			}

			if (ProjectHistory::instance.HasCleaned(fileName))
			{
				continue;
			}

			if (!Project::instance.CanClean(fileName))
			{
				continue;
			}

			auto findItr = m_files.find(fileName);
			if (findItr == m_files.end())
			{
				continue;
			}

			FileID file					= findItr->second;
			const FileHistory &eachFile	= ProjectHistory::instance.m_files[fileName];

			CleanByReplace(eachFile, file);
			CleanByForward(eachFile, file);
			CleanByUnusedLine(eachFile, file);

			ProjectHistory::instance.OnCleaned(fileName);
		}
	}

	void ParsingFile::CleanAllFile()
	{
		CleanBy(ProjectHistory::instance.m_files);

		//			clean_can_replace_include();
		//			clean_unused_include_in_all_file();
		//			clean_by_forwarddecl_in_all_file();
	}

	void ParsingFile::CleanMainFile()
	{
		FileHistoryMap root;

		{
			string rootFileName = GetAbsoluteFileName(m_srcMgr->getMainFileID());

			auto rootItr = ProjectHistory::instance.m_files.find(rootFileName);
			if (rootItr != ProjectHistory::instance.m_files.end())
			{
				root.insert(*rootItr);
			}
		}

		CleanBy(root);
	}

	void ParsingFile::PrintCanReplace()
	{
		// ������ļ��ڼ����ļ��ڵ������ļ��е�#include���޷����滻���򲻴�ӡ
		if (m_replaces.empty())
		{
			return;
		}

		FileHistoryMap files;
		TakeAllInfoTo(files);

		if (files.empty())
		{
			return;
		}

		int num = 0;

		for (auto itr : files)
		{
			const FileHistory &eachFile = itr.second;
			num += eachFile.m_replaces.empty() ? 0 : 1;
		}

		llvm::outs() << "\n    " << ++m_i << ". list of can replace #include : file count = " << num << "";

		PrintCanReplaceOfFiles(files);
	}

	void ParsingFile::PrintHeaderSearchPath()
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
		llvm::outs() << "\n        absolute path of main.cpp  = " << GetAbsolutePath("main.cpp");
		llvm::outs() << "\n        relative path of D:/proj/dummygit\\server\\src\\server/net/connector.cpp  = " << llvm::sys::path::relative_path("D:/proj/dummygit\\server\\src\\server/net/connector.cpp");
		llvm::outs() << "\n        convert to quoted path  [D:/proj/dummygit\\server\\src\\server/net/connector.cpp] -> " << GetQuotedIncludeStr(string("D:/proj/dummygit\\server\\src\\server/net/connector.cpp"));
	}

	void ParsingFile::PrintRelativeInclude()
	{
		llvm::outs() << "\n    ////////////////////////////////";
		llvm::outs() << "\n    " << ++m_i << ". relative include list :" << m_topIncludeIDs.size();
		llvm::outs() << "\n    ////////////////////////////////\n";

		FileID mainFileID = m_srcMgr->getMainFileID();

		for (auto itr : m_uses)
		{
			FileID file = itr.first;
			std::set<FileID> &be_uses = itr.second;

			llvm::outs() << "    file = [" << SimplifyPath(GetAbsolutePath(GetFileName(file)).c_str()) << "]\n";
			for (FileID be_used_file : be_uses)
			{
				llvm::outs() << "        use relative include = [" << GetRelativeIncludeStr(file, be_used_file) << "]\n";
			}

			llvm::outs() << "\n";
		}
	}

	void ParsingFile::PrintRangeToFileName()
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
			             << "[" << GetSourceOfRange(range) << "]" << " -> " << file_path << "\n";
		}
	}

	// ���ڵ��Ը���
	void ParsingFile::PrintNotFoundIncludeLoc()
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
					llvm::outs() << "        not found = " << GetAbsoluteFileName(beuse_file) << "\n";
					continue;
				}
			}
		}
	}

	// ���ڵ��Ը���
	void ParsingFile::PrintSameLine()
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

			string fileName = GetAbsoluteFileName(presumedLoc.getFilename());
			int line = presumedLoc.getLine();
			SourceRange lineRange = GetCurLine(loc);

			std::set<int> &lines = all_lines[fileName];
			if (lines.find(line) != lines.end())
			{
				llvm::outs() << "        " << "found same line : " << line << "int [" << fileName << "]\n";
			}

			lines.insert(line);
		}
	}

	void ParsingFile::Print()
	{
		//			if (m_unusedIncludeLocs.empty() && m_replaces.empty() && m_forwardDecls.empty())
		//			{
		//				return;
		//			}

		llvm::outs() << "\n\n////////////////////////////////////////////////////////////////\n";
		llvm::outs() << "[file = " << GetAbsoluteFileName(m_srcMgr->getMainFileID()) << "]";
		llvm::outs() << "\n////////////////////////////////////////////////////////////////\n";

		m_i = 0;

		//PrintUsedNames();
		//PrintAllCycleUsedNames();
		// PrintRootCycleUse();
		// PrintAllCycleUse();

		PrintUnusedInclude();
		PrintCanReplace();
		PrintCanForwarddecl();
	}

	bool ParsingFile::InitHeaderSearchPath(clang::SourceManager* srcMgr, clang::HeaderSearch &header_search)
	{
		m_srcMgr = srcMgr;
		m_headerSearchPaths = ComputeHeaderSearchPaths(header_search);

		return true;
	}

	void ParsingFile::MergeUnusedLineTo(const FileHistory &newFile, FileHistory &oldFile) const
	{
		FileHistory::UnusedLineMap &oldLines = oldFile.m_unusedLines;

		for (FileHistory::UnusedLineMap::iterator oldLineItr = oldLines.begin(), end = oldLines.end(); oldLineItr != end; )
		{
			int line = oldLineItr->first;

			if (newFile.IsLineUnused(line))
			{
				++oldLineItr;
			}
			else
			{
				// llvm::outs() << oldFile.m_filename << ": conflict at line " << line << " -> " << oldLineItr->second.m_text << "\n";
				oldLines.erase(oldLineItr++);
			}
		}
	}

	void ParsingFile::MergeForwardLineTo(const FileHistory &newFile, FileHistory &oldFile) const
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

	void ParsingFile::MergeReplaceLineTo(const FileHistory &newFile, FileHistory &oldFile) const
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

				if (GetAbsoluteFileName(top) == oldFile.m_filename)
				{
					newFileID	= top;
					newFileItr	= itr;
					break;
				}
			}
		}

		// ˵�����ļ�δ�������κ��滻��¼����ʱӦ�����ļ������о��滻��¼ȫ��ɾ��
		if (newFileID.isInvalid())
		{
			// llvm::outs() << "error, merge_replace_line_to not found = " << oldFile.m_filename << "\n";
			oldFile.m_replaces.clear();
			return;
		}

		const ChildrenReplaceMap &newReplaceMap	= newFileItr->second;

		// �ϲ������滻��¼����ʷ��¼��
		for (auto oldLineItr = oldFile.m_replaces.begin(), end = oldFile.m_replaces.end(); oldLineItr != end; )
		{
			int line				= oldLineItr->first;
			ReplaceLine &oldLine	= oldLineItr->second;

			// �����Ƿ�����ͻ��
			auto conflictItr = newFile.m_replaces.find(line);
			bool conflict = (conflictItr != newFile.m_replaces.end());

			// ��������ͻ����������µ�ȡ���ļ���ɵ�ȡ���ļ��Ƿ���ֱϵ�����ϵ
			if (conflict)
			{
				const ReplaceLine &newLine	= conflictItr->second;

				// �ҵ����оɵ�#include��Ӧ��FileID
				FileID beReplaceFileID;

				{
					for (auto childItr : newReplaceMap)
					{
						FileID child = childItr.first;
						if (GetAbsoluteFileName(child) == newLine.m_oldFile)
						{
							beReplaceFileID = child;
							break;
						}
					}
				}

				if (beReplaceFileID.isInvalid())
				{
					++oldLineItr;
					continue;
				}

				// ���ɵ�ȡ���ļ����µ�ȡ���ļ������ȣ������ɵ��滻��Ϣ
				if (IsAncestor(beReplaceFileID, oldLine.m_oldFile.c_str()))
				{
					++oldLineItr;
					continue;
				}
				// ���µ�ȡ���ļ��Ǿɵ�ȡ���ļ������ȣ����Ϊʹ���µ��滻��Ϣ
				else if(IsAncestor(oldLine.m_oldFile.c_str(), beReplaceFileID))
				{
					oldLine.m_newInclude = newLine.m_newInclude;
				}
				// ������û��ֱϵ��ϵ��������޷����滻��ɾ�������ĸ����滻��¼
				else
				{
					// llvm::outs() << "merge_replace_line_to: " << oldFile.m_filename << " should remove conflict old line = " << line << " -> " << oldLine.m_oldText << "\n";
					oldFile.m_replaces.erase(oldLineItr++);
				}
			}
			// ������û���µ��滻��¼��˵�������޷����滻��ɾ�����оɵ��滻��¼
			else
			{
				// llvm::outs() << "merge_replace_line_to: " << oldFile.m_filename << " should remove old line = " << line << " -> " << oldLine.m_oldText << "\n";
				oldFile.m_replaces.erase(oldLineItr++);
			}
		}
	}

	void ParsingFile::MergeCountTo(FileHistoryMap &oldFiles) const
	{
		for (auto itr : m_parentIDs)
		{
			FileID child		= itr.first;
			string fileName		= GetAbsoluteFileName(child);
			FileHistory &oldFile	= oldFiles[fileName];

			++oldFile.m_oldBeIncludeCount;

			if (m_unusedLocs.find(m_srcMgr->getIncludeLoc(child)) != m_unusedLocs.end())
			{
				++oldFile.m_newBeIncludeCount;
			}
		}

		for (FileID file : m_usedFiles)
		{
			string fileName		= GetAbsoluteFileName(file);
			FileHistory &oldFile	= oldFiles[fileName];

			++oldFile.m_beUseCount;
		}
	}

	void ParsingFile::MergeTo(FileHistoryMap &oldFiles)
	{
		FileHistoryMap newFiles;
		TakeAllInfoTo(newFiles);

		for (auto fileItr : newFiles)
		{
			const string &fileName	= fileItr.first;
			const FileHistory &newFile	= fileItr.second;

			auto findItr = oldFiles.find(fileName);

			bool found = (findItr != oldFiles.end());
			if (!found)
			{
				oldFiles[fileName] = newFile;
			}
			else
			{
				FileHistory &oldFile = findItr->second;

				MergeUnusedLineTo(newFile, oldFile);
				MergeForwardLineTo(newFile, oldFile);
				MergeReplaceLineTo(newFile, oldFile);
			}
		}

		MergeCountTo(oldFiles);
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
		SourceRange firstLine		= GetCurLine(fileStart);
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

	void ParsingFile::TakeAllInfoTo(FileHistoryMap &out)
	{
		// �Ƚ���ǰcpp�ļ�ʹ�õ����ļ�ȫ����map��
		for (FileID file : m_allCycleUse)
		{
			string fileName		= GetAbsoluteFileName(file);

			if (!Project::instance.CanClean(fileName))
			{
				continue;
			}

			// ���ɶ�Ӧ�ڸ��ļ��ĵļ�¼
			FileHistory &eachFile		= out[fileName];
			eachFile.m_isWindowFormat	= IsWindowsFormat(file);
			eachFile.m_filename			= fileName;
		}

		// 1. ����������а��ļ����д��
		TakeUnusedLineByFile(out);

		// 2. ��������ǰ���������ļ����д��
		TakeNewForwarddeclByFile(out);

		// 3. ���ļ��ڵ�#include�滻���ļ����д��
		TakeReplaceByFile(out);
	}

	// ����������а��ļ����д��
	void ParsingFile::TakeUnusedLineByFile(FileHistoryMap &out)
	{
		for (SourceLocation loc : m_unusedLocs)
		{
			PresumedLoc presumedLoc = m_srcMgr->getPresumedLoc(loc);
			if (presumedLoc.isInvalid())
			{
				llvm::outs() << "------->error: take_unused_line_by_file getPresumedLoc failed\n";
				continue;
			}

			string fileName			= GetAbsoluteFileName(presumedLoc.getFilename());
			if (!Project::instance.CanClean(fileName))
			{
				// llvm::outs() << "------->error: !Vsproject::instance.has_file(fileName) : " << fileName << "\n";
				continue;
			}

			if (out.find(fileName) == out.end())
			{
				continue;
			}

			SourceRange lineRange	= GetCurLine(loc);
			SourceRange nextLine	= GetNextLine(loc);
			int line				= presumedLoc.getLine();

			FileHistory &eachFile		= out[fileName];
			UselessLineInfo &unusedLine	= eachFile.m_unusedLines[line];

			unusedLine.m_beg	= m_srcMgr->getFileOffset(lineRange.getBegin());
			unusedLine.m_end	= m_srcMgr->getFileOffset(nextLine.getBegin());
			unusedLine.m_text	= GetSourceOfRange(lineRange);
		}
	}

	// ��������ǰ���������ļ����д��
	void ParsingFile::TakeNewForwarddeclByFile(FileHistoryMap &out)
	{
		if (m_forwardDecls.empty())
		{
			return;
		}

		for (auto itr : m_forwardDecls)
		{
			FileID file									= itr.first;
			std::set<const CXXRecordDecl*> &cxxRecords	= itr.second;

			string fileName			= GetAbsoluteFileName(file);

			if (out.find(fileName) == out.end())
			{
				continue;
			}

			for (const CXXRecordDecl *cxxRecord : cxxRecords)
			{
				SourceLocation insertLoc = GetInsertForwardLine(file, *cxxRecord);
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

				string fileName			= GetAbsoluteFileName(presumedLoc.getFilename());

				if (!Project::instance.CanClean(fileName))
				{
					continue;
				}

				// ��ʼȡ������
				{
					int line					= presumedLoc.getLine();
					const string cxxRecordName	= GetCxxrecordName(*cxxRecord);
					FileHistory &eachFile		= out[fileName];
					ForwardLine &forwardLine	= eachFile.m_forwards[line];

					forwardLine.m_offsetAtFile	= m_srcMgr->getFileOffset(insertLoc);
					forwardLine.m_oldText		= GetSourceOfLine(insertLoc);
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
	void ParsingFile::SplitReplaceByFile(ReplaceFileMap &replaces)
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

	bool ParsingFile::IsForceIncluded(FileID file)
	{
		return (nullptr == m_srcMgr->getFileEntryForID(m_srcMgr->getFileID(m_srcMgr->getIncludeLoc(file))));
	}

	void ParsingFile::TakeBeReplaceOfFile(FileHistory &eachFile, FileID top, const ChildrenReplaceMap &childernReplaces)
	{
		for (auto itr : childernReplaces)
		{
			FileID oldFile						= itr.first;
			const std::set<FileID> &to_replaces	= itr.second;

			// ȡ������#include���滻��Ϣ[�к� -> ���滻�ɵ�#include�б�]
			{
				bool is_be_force_included	= IsForceIncluded(oldFile);

				SourceLocation include_loc	= m_srcMgr->getIncludeLoc(oldFile);
				SourceRange	lineRange		= GetCurLineWithLinefeed(include_loc);
				int line					= (is_be_force_included ? 0 : GetLineNo(include_loc));

				ReplaceLine &replaceLine	= eachFile.m_replaces[line];
				replaceLine.m_oldFile		= GetAbsoluteFileName(oldFile);
				replaceLine.m_oldText		= GetRawIncludeStr(oldFile);
				replaceLine.m_beg			= m_srcMgr->getFileOffset(lineRange.getBegin());
				replaceLine.m_end			= m_srcMgr->getFileOffset(lineRange.getEnd());
				replaceLine.m_isSkip		= (is_be_force_included ? true : false);				// �����Ƿ���ǿ�ư���

				for (FileID replace_file : to_replaces)
				{
					SourceLocation include_loc	= m_srcMgr->getIncludeLoc(replace_file);

					ReplaceInfo replaceInfo;

					// ��¼[��#include����#include]
					replaceInfo.m_oldText	= GetRawIncludeStr(replace_file);
					replaceInfo.m_newText	= GetRelativeIncludeStr(m_parentIDs[oldFile], replace_file);

					// ��¼[�������ļ��������к�]
					replaceInfo.m_line			= GetLineNo(include_loc);
					replaceInfo.m_fileName		= GetAbsoluteFileName(replace_file);
					replaceInfo.m_inFile		= m_srcMgr->getFilename(include_loc);

					replaceLine.m_newInclude.push_back(replaceInfo);
				}
			}
		}
	}

	// ȡ�����ļ���#include�滻��Ϣ
	void ParsingFile::TakeReplaceByFile(FileHistoryMap &out)
	{
		if (m_replaces.empty())
		{
			return;
		}

		for (auto itr : m_splitReplaces)
		{
			FileID top									= itr.first;
			const ChildrenReplaceMap &childrenReplaces	= itr.second;

			string fileName			= GetAbsoluteFileName(top);

			if (out.find(fileName) == out.end())
			{
				continue;
			}

			// �����滻��¼
			string topFileName	= GetAbsoluteFileName(top);
			FileHistory &newFile	= out[topFileName];

			TakeBeReplaceOfFile(newFile, top, childrenReplaces);
		}
	}
}