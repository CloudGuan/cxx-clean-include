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
#include <clang/Lex/Preprocessor.h>
#include <clang/Rewrite/Core/Rewriter.h>
#include "clang/Frontend/CompilerInstance.h"
#include <llvm/Support/Path.h>

#include "tool.h"
#include "project.h"

namespace cxxcleantool
{
	ParsingFile::ParsingFile(clang::Rewriter &rewriter, clang::CompilerInstance &compiler)
	{
		m_rewriter	= &rewriter;
		m_compiler	= &compiler;
		m_srcMgr	= &compiler.getSourceManager();;
	}

	// ��ʼ��������
	bool ParsingFile::Init()
	{
		clang::HeaderSearch &headerSearch = m_compiler->getPreprocessor().getHeaderSearchInfo();
		m_headerSearchPaths = TakeHeaderSearchPaths(headerSearch);

		return true;
	}

	// ��ȡͷ�ļ�����·��
	vector<ParsingFile::HeaderSearchDir> ParsingFile::TakeHeaderSearchPaths(clang::HeaderSearch &headerSearch) const
	{
		IncludeDirMap dirs;

		// ��ȡϵͳͷ�ļ�����·��
		for (clang::HeaderSearch::search_dir_iterator
		        itr = headerSearch.system_dir_begin(), end = headerSearch.system_dir_end();
		        itr != end; ++itr)
		{
			if (const DirectoryEntry* entry = itr->getDir())
			{
				const string path = pathtool::fix_path(entry->getName());
				dirs.insert(make_pair(path, SrcMgr::C_System));
			}
		}

		// ��ȡ�û�ͷ�ļ�����·��
		for (clang::HeaderSearch::search_dir_iterator
		        itr = headerSearch.search_dir_begin(), end = headerSearch.search_dir_end();
		        itr != end; ++itr)
		{
			if (const DirectoryEntry* entry = itr->getDir())
			{
				const string path = pathtool::fix_path(entry->getName());
				dirs.insert(make_pair(path, SrcMgr::C_User));
			}
		}

		return SortHeaderSearchPath(dirs);
	}

	// ���ݳ����ɳ���������
	static bool SortByLongestLengthFirst(const ParsingFile::HeaderSearchDir& left, const ParsingFile::HeaderSearchDir& right)
	{
		return left.m_dir.length() > right.m_dir.length();
	}

	// ��ͷ�ļ�����·�����ݳ����ɳ���������
	std::vector<ParsingFile::HeaderSearchDir> ParsingFile::SortHeaderSearchPath(const IncludeDirMap& include_dirs_map) const
	{
		std::vector<HeaderSearchDir> dirs;

		for (auto itr : include_dirs_map)
		{
			const string &path					= itr.first;
			SrcMgr::CharacteristicKind pathType	= itr.second;

			string absolutePath = pathtool::get_absolute_path(path.c_str());

			dirs.push_back(HeaderSearchDir(absolutePath, pathType));
		}

		sort(dirs.begin(), dirs.end(), &SortByLongestLengthFirst);
		return dirs;
	}

	// ����ͷ�ļ�����·����������·��ת��Ϊ˫���Ű�Χ���ı�����
	// ���磺������ͷ�ļ�����·��"d:/a/b/c" ��"d:/a/b/c/d/e.h" -> "d/e.h"
	string ParsingFile::GetQuotedIncludeStr(const string& absoluteFilePath) const
	{
		string path = pathtool::simplify_path(absoluteFilePath.c_str());

		for (const HeaderSearchDir &itr :m_headerSearchPaths)
		{
			if (strtool::try_strip_left(path, itr.m_dir))
			{
				if (itr.m_dirType == SrcMgr::C_System)
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

	// ָ��λ�õĶ�Ӧ��#include�Ƿ��õ�
	inline bool ParsingFile::IsLocBeUsed(SourceLocation loc) const
	{
		return m_usedLocs.find(loc) != m_usedLocs.end();
	}

	// ���ɸ��ļ��Ĵ������¼
	void ParsingFile::GenerateResult()
	{
		GenerateRootCycleUse();
		GenerateAllCycleUse();

		GenerateUnusedInclude();
		GenerateCanForwardDeclaration();
		GenerateCanReplace();
	}

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
	void ParsingFile::GenerateRootCycleUse()
	{
		if (m_uses.empty())
		{
			return;
		}

		FileID mainFileID = m_srcMgr->getMainFileID();

		GetCycleUseFile(mainFileID, m_rootCycleUse);
	}

	// ��¼���ļ��ı���������ļ�
	void ParsingFile::GenerateCycleUsedChildren()
	{
		for (FileID usedFile : m_allCycleUse)
		{
			for (FileID child = usedFile, parent; (parent = GetParent(child)).isValid(); child = parent)
			{
				m_cycleUsedChildren[parent].insert(usedFile);
			}
		}
	}

	// ��ȡ���ļ��ɱ��滻�����ļ������޷����滻���򷵻ؿ��ļ�id
	FileID ParsingFile::GetCanReplaceTo(FileID top) const
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

	// ������Ӹ����������ļ��������ļ�������ֵ��true��ʾ�����ļ��������š�false��ʾ�����ļ��в���
	bool ParsingFile::TryAddAncestor()
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

	// �������ļ���������ϵ����������ļ��������ļ���
	void ParsingFile::GenerateAllCycleUse()
	{
		/*
			������δ�����cxx-clean-include���ߵ���Ҫ����˼·
		*/
		{
			// 1. ��ȡ���ļ���ѭ�������ļ���
			m_allCycleUse = m_rootCycleUse;

			// 2. ��¼���ļ��ĺ���ļ��б������Ĳ���
			GenerateCycleUsedChildren();

			// 3. ������Ӹ����������ļ��������ļ�
			while (TryAddAncestor())
			{
				// 4. �������ļ��������ţ����������ɺ�������ļ���
				GenerateCycleUsedChildren();
			};
		}

		for (FileID beusedFile : m_allCycleUse)
		{
			// �ӱ�ʹ���ļ��ĸ��ڵ㣬������Ͻ������ø������ù�ϵ
			for (FileID child = beusedFile, parent; (parent = GetParent(child)).isValid(); child = parent)
			{
				m_usedLocs.insert(m_srcMgr->getIncludeLoc(child));
			}
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

	// ���ļ��Ƿ�ѭ�����õ�
	bool ParsingFile::IsCyclyUsed(FileID file) const
	{
		return m_allCycleUse.find(file) != m_allCycleUse.end();
	}

	// ���ļ��Ƿ����ļ�ѭ�����õ�
	bool ParsingFile::IsRootCyclyUsed(FileID file) const
	{
		return m_rootCycleUse.find(file) != m_rootCycleUse.end();
	}

	// ���ļ��Ƿ�ɱ��滻
	bool ParsingFile::IsReplaced(FileID file) const
	{
		return m_replaces.find(file) != m_replaces.end();
	}

	// ��������#include�ļ�¼
	void ParsingFile::GenerateUnusedInclude()
	{
		for (auto locItr : m_includeLocs)
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

	// ��ȡָ���ļ�ֱ�������ͼ���������ļ���
	// ��������ǣ�
	//     ��ȡtop���������ļ�����ѭ����ȡ��Щ�����ļ��������������ļ���ֱ�����еı������ļ����ѱ�����
	void ParsingFile::GetCycleUseFile(FileID top, std::set<FileID> &out) const
	{
		// �������ļ������ü�¼
		auto topUseItr = m_uses.find(top);
		if (topUseItr == m_uses.end())
		{
			out.insert(top);
			return;
		}

		std::set<FileID> left;
		std::set<FileID> history;

		history.insert(top);

		// ��ȡ���ļ��������ļ���
		const std::set<FileID> &topUseFiles = topUseItr->second;
		left.insert(topUseFiles.begin(), topUseFiles.end());

		// ѭ����ȡ�������ļ��������������ļ�
		while (!left.empty())
		{
			FileID cur = *left.begin();
			left.erase(left.begin());

			history.insert(cur);

			// ����ǰ�ļ��������������ļ�������Բ�����չ��ǰ�ļ�
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

	// ��ȡ�ļ�����ȣ������ļ������Ϊ0��
	int ParsingFile::GetDepth(FileID child) const
	{
		int depth = 0;

		for (FileID parent; (parent = GetParent(child)).isValid(); child = parent)
		{
			++depth;
		}

		return depth;
	}

	// ��ȡ�뺢��������Ĺ�ͬ����
	FileID ParsingFile::GetCommonAncestor(const std::set<FileID> &children) const
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
			FileID parent = GetParent(ancestor);
			if (parent.isInvalid())
			{
				return m_srcMgr->getMainFileID();
			}

			ancestor = parent;
		}

		return ancestor;
	}

	// ��ȡ2������������Ĺ�ͬ����
	FileID ParsingFile::GetCommonAncestor(FileID child_1, FileID child_2) const
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
			FileID parent = GetParent(old);
			if (parent.isInvalid())
			{
				break;
			}

			old = parent;
		}

		return old;
	}

	// �����ļ��滻�б�
	void ParsingFile::GenerateCanReplace()
	{
		SplitReplaceByFile(m_splitReplaces);
	}

	bool ParsingFile::HasRecordBefore(FileID cur, const CXXRecordDecl &cxxRecord) const
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

	// ��������ǰ�������б�
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

	// ��ȡָ����Χ���ı�
	std::string ParsingFile::GetSourceOfRange(SourceRange range) const
	{
		bool err1 = true;
		bool err2 = true;

		const char* beg = m_srcMgr->getCharacterData(range.getBegin(), &err1);
		const char* end = m_srcMgr->getCharacterData(range.getEnd(), &err2);

		if (err1 || err2)
		{
			return "";
		}

		return string(beg, end);
	}

	// ��ȡָ��λ�������е��ı�
	std::string ParsingFile::GetSourceOfLine(SourceLocation loc) const
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
	SourceRange ParsingFile::GetCurLine(SourceLocation loc) const
	{
		SourceLocation fileBeginLoc = m_srcMgr->getLocForStartOfFile(m_srcMgr->getFileID(loc));
		SourceLocation fileEndLoc	= m_srcMgr->getLocForEndOfFile(m_srcMgr->getFileID(loc));

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
	SourceRange ParsingFile::GetCurLineWithLinefeed(SourceLocation loc) const
	{
		SourceRange curLine		= GetCurLine(loc);
		SourceRange nextLine	= GetNextLine(loc);

		return SourceRange(curLine.getBegin(), nextLine.getBegin());
	}

	// ���ݴ���Ĵ���λ�÷�����һ�еķ�Χ
	SourceRange ParsingFile::GetNextLine(SourceLocation loc) const
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

	// �Ƿ�Ϊ����
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

	// ��ȡ�к�
	inline int ParsingFile::GetLineNo(SourceLocation loc) const
	{
		PresumedLoc presumed_loc	= m_srcMgr->getPresumedLoc(loc);
		return presumed_loc.isValid() ? presumed_loc.getLine() : 0;
	}

	// �Ƿ��ǻ��з�
	bool ParsingFile::IsNewLineWord(SourceLocation loc) const
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
	std::string ParsingFile::GetRawIncludeStr(FileID file) const
	{
		SourceLocation loc = m_srcMgr->getIncludeLoc(file);

		auto itr = m_includeLocs.find(loc);
		if (itr == m_includeLocs.end())
		{
			return "";
		}

		SourceRange range = itr->second;
		return GetSourceOfRange(range);
	}

	// curλ�õĴ���ʹ��srcλ�õĴ���
	void ParsingFile::Use(SourceLocation cur, SourceLocation src, const char* name /* = nullptr */)
	{
		FileID curFileID = m_srcMgr->getFileID(cur);
		FileID srcFileID = m_srcMgr->getFileID(src);

		UseInclude(curFileID, srcFileID, name, GetLineNo(src));
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

	// ���ݵ�ǰ�ļ������ҵ�2������ȣ���rootΪ��һ�㣩������ǰ�ļ��ĸ��ļ���Ϊ���ļ����򷵻ص�ǰ�ļ�
	FileID ParsingFile::GetLvl2Ancestor(FileID file, FileID root) const
	{
		FileID ancestor;

		for (FileID parent; (parent = GetParent(file)).isValid(); file = parent)
		{
			// Ҫ�󸸽����root
			if (parent == root)
			{
				ancestor = file;
				break;
			}
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

	// �Ƿ�Ϊ�����ļ��Ĺ�ͬ����
	bool ParsingFile::IsCommonAncestor(const std::set<FileID> &children, FileID old) const
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

	// ��ȡ���ļ������ļ�û�и��ļ���
	inline FileID ParsingFile::GetParent(FileID child) const
	{
		if (child == m_srcMgr->getMainFileID())
		{
			return FileID();
		}

		auto itr = m_parentIDs.find(child);
		if (itr == m_parentIDs.end())
		{
			return FileID();
		}

		return itr->second;
	}

	// ��ǰ�ļ�ʹ��Ŀ���ļ�
	void ParsingFile::UseInclude(FileID file, FileID beusedFile, const char* name /* = nullptr */, int line)
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

	// ��ǰλ��ʹ��ָ���ĺ�
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

	// ��ȡc++��class��struct��union��ȫ������������������ռ�
	// ���磺
	//     ������C��C���������ռ�A�е������ռ�B�����������أ�namespace A{ namespace B{ class C; }}
	string ParsingFile::GetCxxrecordName(const CXXRecordDecl &cxxRecordDecl) const
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
	SourceLocation ParsingFile::GetInsertForwardLine(FileID at, const CXXRecordDecl &cxxRecord) const
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

	// ����ʹ��ǰ��������¼
	void ParsingFile::UseForward(SourceLocation loc, const CXXRecordDecl *cxxRecordDecl)
	{
		FileID file = m_srcMgr->getFileID(loc);
		if (file.isInvalid())
		{
			return;
		}

		string cxxRecordName = GetCxxrecordName(*cxxRecordDecl);

		// ����ļ���ʹ�õ�ǰ��������¼�����ڲ���Ҫ��ӵ�ǰ����������֮���������
		m_forwardDecls[file].insert(cxxRecordDecl);
	}

	// ����ʹ�ñ�����¼
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

	// ����ʹ��������¼
	void ParsingFile::OnUseDecl(SourceLocation loc, const NamedDecl *nameDecl)
	{
		std::stringstream name;
		name << nameDecl->getQualifiedNameAsString() << "[" << nameDecl->getDeclKindName() << "]";

		Use(loc, nameDecl->getLocStart(), name.str().c_str());
	}

	// ����ʹ��class��struct��union��¼
	void ParsingFile::OnUseRecord(SourceLocation loc, const CXXRecordDecl *record)
	{
		Use(loc, record->getLocStart(), GetCxxrecordName(*record).c_str());
	}

	// �Ƿ����������c++�ļ������������������ļ����ݲ������κα仯��
	bool ParsingFile::CanClean(FileID file) const
	{
		return Project::instance.CanClean(GetAbsoluteFileName(file));
	}

	// ��ӡ���ļ��ĸ��ļ�
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

		llvm::outs() << "\n";
	}

	// ��ȡ���ļ��ı�������Ϣ���������ݰ��������ļ��������ļ����������ļ�#include���кš������ļ�#include��ԭʼ�ı���
	string ParsingFile::DebugBeIncludeText(FileID file, bool isAbsoluteName /* = false */) const
	{
		SourceLocation loc = m_srcMgr->getIncludeLoc(file);

		PresumedLoc parentPresumedLoc = m_srcMgr->getPresumedLoc(loc);

		std::stringstream text;
		string fileName;
		string parentFileName;
		std::string includeToken = "empty";

		auto itr = m_includeLocs.find(loc);
		if (m_includeLocs.find(loc) != m_includeLocs.end())
		{
			SourceRange range = itr->second;
			includeToken = GetSourceOfRange(range);
		}

		if (isAbsoluteName)
		{
			fileName = GetFileName(file);
			parentFileName = parentPresumedLoc.getFilename();
		}
		else
		{
			fileName = GetAbsoluteFileName(file);
			parentFileName = pathtool::get_absolute_path(parentPresumedLoc.getFilename());
		}

		text << "[" << fileName << "]";

		if (file != m_srcMgr->getMainFileID())
		{
			text << " in { [";
			text << parentFileName;
			text << "] -> [" << includeToken << "] line = " << (parentPresumedLoc.isValid() ? parentPresumedLoc.getLine() : 0);
			text << "}";
		}

		return text.str();
	}

	// ��ȡ���ļ��ı�ֱ�Ӱ�����Ϣ���������ݰ��������ļ����������ļ�#include���кš������ļ�#include��ԭʼ�ı���
	string ParsingFile::DebugBeDirectIncludeText(FileID file) const
	{
		SourceLocation loc		= m_srcMgr->getIncludeLoc(file);
		PresumedLoc presumedLoc = m_srcMgr->getPresumedLoc(loc);

		std::stringstream text;
		string fileName;
		string includeToken = "empty";

		{
			auto itr = m_includeLocs.find(loc);
			if (m_includeLocs.find(loc) != m_includeLocs.end())
			{
				SourceRange range = itr->second;
				includeToken = GetSourceOfRange(range);
			}

			fileName = GetFileName(file);
		}

		text << "{line = " << (presumedLoc.isValid() ? presumedLoc.getLine() : 0) << " [" << includeToken << "]";
		text << "} -> ";
		text << "[" << fileName << "]";

		return text.str();
	}

	// ��ȡ��λ�������е���Ϣ�������е��ı��������ļ������к�
	string ParsingFile::DebugLocText(SourceLocation loc) const
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

	// ��ȡ��λ���������#include��Ϣ
	string ParsingFile::DebugLocIncludeText(SourceLocation loc) const
	{
		PresumedLoc presumedLoc = m_srcMgr->getPresumedLoc(loc);
		if (presumedLoc.isInvalid())
		{
			return "";
		}

		std::stringstream text;
		std::string includeToken = "empty";

		auto itr = m_includeLocs.find(loc);
		if (m_includeLocs.find(loc) != m_includeLocs.end())
		{
			SourceRange range = itr->second;
			includeToken = GetSourceOfRange(range);
		}

		text << "[" + includeToken << "] line = " << presumedLoc.getLine();
		return text.str();
	}

	// ��ӡ���ü�¼
	void ParsingFile::PrintUse() const
	{
		llvm::outs() << "    " << ++m_i << ". list of include referenced: " << m_uses.size() << "\n";

		for (auto use_list : m_uses)
		{
			FileID file = use_list.first;

			if (!CanClean(file))
			{
				continue;
			}

			llvm::outs() << "        file = " << GetAbsoluteFileName(file) << "\n";

			for (FileID beuse : use_list.second)
			{
				// llvm::outs() << "            use = " << get_file_name(beuse) << "\n";
				// llvm::outs() << "            use = " << get_file_name(beuse) << " [" << token << "] in [" << presumedLoc.getFilename() << "] line = " << presumedLoc.getLine() << "\n";
				llvm::outs() << "            use = " << DebugBeIncludeText(beuse) << "\n";
			}

			llvm::outs() << "\n";
		}
	}

	// ��ӡ#include��¼
	void ParsingFile::PrintInclude() const
	{
		llvm::outs() << "    " << ++m_i << ". list of include: " << m_includes.size() << "\n";

		for (auto includeList : m_includes)
		{
			FileID file = includeList.first;

			if (!CanClean(file))
			{
				continue;
			}

			llvm::outs() << "        file = " << GetAbsoluteFileName(file) << "\n";

			for (FileID beinclude : includeList.second)
			{
				llvm::outs() << "            include = " << DebugBeDirectIncludeText(beinclude) << "\n";
			}

			llvm::outs() << "\n";
		}
	}

	// ��ӡ�����������������������ȵļ�¼
	void ParsingFile::PrintUsedNames() const
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

	// ��ȡ�ļ���ʹ��������Ϣ���ļ�������ʹ�õ����������������������Լ���Ӧ�к�
	void ParsingFile::DebugUsedNames(FileID file, const std::vector<UseNameInfo> &useNames) const
	{
		llvm::outs() << "        file = " << GetAbsoluteFileName(file) << "\n";

		for (const UseNameInfo &beuse : useNames)
		{
			llvm::outs() << "            use = " << DebugBeIncludeText(beuse.file) << "\n";

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

	// ��ӡ���ļ�ѭ�����õ����Ƽ�¼
	void ParsingFile::PrintRootCycleUsedNames() const
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

			if (!IsRootCyclyUsed(file))
			{
				continue;
			}

			DebugUsedNames(file, useNames);
		}
	}

	// ��ӡѭ�����õ����Ƽ�¼
	void ParsingFile::PrintAllCycleUsedNames() const
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

			if (!IsCyclyUsed(file))
			{
				continue;
			}

			if (!IsRootCyclyUsed(file))
			{
				llvm::outs() << "        <--------- new add cycle use file --------->\n";
			}

			DebugUsedNames(file, useNames);
		}
	}

	// �Ƿ��б�Ҫ��ӡ���ļ�
	bool ParsingFile::IsNeedPrintFile(FileID file) const
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

	// ��ӡ���ļ�ѭ�����õ��ļ��б�
	void ParsingFile::PrintRootCycleUse() const
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

	// ��ӡѭ�����õ��ļ��б�
	void ParsingFile::PrintAllCycleUse() const
	{
		llvm::outs() << "    " << ++m_i << ". list of all cycle use: " << m_allCycleUse.size() << "\n";
		for (auto file : m_allCycleUse)
		{
			if (!IsNeedPrintFile(file))
			{
				continue;
			}

			if (!IsRootCyclyUsed(file))
			{
				llvm::outs() << "        <--------- new add cycle use file --------->\n";
			}

			llvm::outs() << "        file = " << GetFileName(file) << "\n";
		}

		llvm::outs() << "\n";
	}

	// ��ӡ���ļ���Ӧ�����ú����ļ���¼
	void ParsingFile::PrintCycleUsedChildren() const
	{
		llvm::outs() << "    " << ++m_i << ". list of cycle used children file: " << m_cycleUsedChildren.size() << "\n";

		for (auto usedItr : m_cycleUsedChildren)
		{
			FileID file = usedItr.first;

			if (!CanClean(file))
			{
				continue;
			}

			llvm::outs() << "        file = " << GetAbsoluteFileName(file) << "\n";

			for (auto usedChild : usedItr.second)
			{
				llvm::outs() << "            use children = " << DebugBeIncludeText(usedChild) << "\n";
			}

			llvm::outs() << "\n";
		}
	}

	// ��ӡ��������������ļ��б�
	void ParsingFile::PrintAllFile() const
	{
		llvm::outs() << "\n    " << ++m_i << ". list of all file: " << m_files.size() << "\n";

		for (FileID file : m_files)
		{
			const string &name = GetAbsoluteFileName(file);

			if (!Project::instance.CanClean(name))
			{
				continue;
			}

			llvm::outs() << "        file id = " << file.getHashValue() << ", filename = " << name << "\n";
		}

		llvm::outs() << "\n";
	}

	// ��ӡ���ļ��ڵĿɱ�ɾ#include��¼
	void ParsingFile::PrintUnusedIncludeOfFiles(const FileHistoryMap &files)
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

				llvm::outs() << "            unused: [line = " << line << "] -> [" << unusedLine.m_text << "]\n";
			}
		}
	}

	// ��ӡ���ļ��ڵĿ�����ǰ��������¼
	void ParsingFile::PrintCanForwarddeclOfFiles(const FileHistoryMap &files)
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

	// ��ӡ���ļ��ڵĿɱ��滻#include��¼
	void ParsingFile::PrintCanReplaceOfFiles(const FileHistoryMap &files)
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

	// ��ӡ��Ŀ�ܵĿɱ�ɾ#include��¼
	void ParsingFile::PrintUnusedInclude() const
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

	// ��ӡ��Ŀ�ܵĿ�����ǰ��������¼
	void ParsingFile::PrintCanForwarddecl() const
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

	// ��ȡ�ļ�����ͨ��clang��ӿڣ��ļ���δ�������������Ǿ���·����Ҳ���������·����
	// ���磺
	//     ���ܷ��ؾ���·����d:/a/b/hello.h
	//     Ҳ���ܷ��أ�./../hello.h
	const char* ParsingFile::GetFileName(FileID file) const
	{
		const FileEntry *fileEntry = m_srcMgr->getFileEntryForID(file);
		if (nullptr == fileEntry)
		{
			return "";
		}

		return fileEntry->getName();
	}

	// ��ȡ�ļ��ľ���·��
	string ParsingFile::GetAbsoluteFileName(FileID file) const
	{
		const char* raw_file_name = GetFileName(file);
		return pathtool::get_absolute_path(raw_file_name);
	}

	// ��ȡ��1���ļ�#include��2���ļ����ı���
	std::string ParsingFile::GetRelativeIncludeStr(FileID f1, FileID f2) const
	{
		// ����2���ļ��ı�������ԭ������#include <xxx>�ĸ�ʽ���򷵻�ԭ����#include��
		{
			string rawInclude2 = GetRawIncludeStr(f2);
			if (rawInclude2.empty())
			{
				return "";
			}

			// �Ƿ񱻼����Ű������磺<stdio.h>
			bool isAngled = strtool::contain(rawInclude2.c_str(), '<');
			if (isAngled)
			{
				return rawInclude2;
			}
		}

		string absolutePath1 = GetAbsoluteFileName(f1);
		string absolutePath2 = GetAbsoluteFileName(f2);

		string include2 = GetQuotedIncludeStr(absolutePath2);

		// �����ж�2���ļ��Ƿ���ͬһ�ļ�����
		if (get_dir(absolutePath1) == get_dir(absolutePath2))
		{
			include2 = "\"" + strip_dir(absolutePath2) + "\"";
		}
		else
		{
			// ��ͷ�ļ�����·������Ѱ��2���ļ������ɹ��ҵ����򷵻ػ���ͷ�ļ�����·�������·��
			string include2 = GetQuotedIncludeStr(absolutePath2);

			// ��δ��ͷ�ļ�����·����Ѱ����2���ļ����򷵻ػ��ڵ�1���ļ������·��
			if (include2.empty())
			{
				include2 = "\"" + pathtool::get_relative_path(absolutePath1.c_str(), absolutePath2.c_str()) + "\"";
			}
		}

		return "#include " + include2;
	}

	// ��ʼ�����ļ������Ķ�c++Դ�ļ���
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

		// ������������ѡ��ʱ���Ž��䶯��д��c++�ļ�
		if (Project::instance.m_isOverWrite)
		{
			m_rewriter->overwriteChangedFiles();
		}
	}

	// �滻ָ����Χ�ı�
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

	// ���ı����뵽ָ��λ��֮ǰ
	// ���磺������"abcdefg"�ı�������c������123�Ľ�����ǣ�"ab123cdefg"
	void ParsingFile::InsertText(FileID file, int loc, string text)
	{
		SourceLocation fileBegLoc	= m_srcMgr->getLocForStartOfFile(file);
		SourceLocation insertLoc	= fileBegLoc.getLocWithOffset(loc);

		m_rewriter->InsertText(insertLoc, text, true, true);
	}

	// �Ƴ�ָ����Χ�ı������Ƴ��ı�����б�Ϊ���У��򽫸ÿ���һ���Ƴ�
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

		// llvm::outs() << "\n------->remove text = [" << get_source_of_range(range) << "] in [" << get_absolute_file_name(file) << "]\n";

		bool err = m_rewriter->RemoveText(range.getBegin(), end - beg, rewriteOption);
		if (err)
		{
			llvm::errs() << "\n------->error: remove text = [" << GetSourceOfRange(range) << "] in [" << GetAbsoluteFileName(file) << "] failed!\n";
		}
	}

	// �Ƴ�ָ���ļ��ڵ�����#include
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

	// ��ָ���ļ������ǰ������
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

	// ��ָ���ļ����滻#include
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

	// ����ָ���ļ�
	void ParsingFile::CleanBy(const FileHistoryMap &files)
	{
		std::map<std::string, FileID> allFiles;

		// �����ļ������ļ�FileID��mapӳ�䣨ע�⣺ͬһ���ļ����ܱ�������Σ�FileID�ǲ�һ���ģ�����ֻ������С��FileID��
		{
			for (FileID file : m_files)
			{
				const string &name = GetAbsoluteFileName(file);

				if (!Project::instance.CanClean(name))
				{
					continue;
				}

				if (allFiles.find(name) == allFiles.end())
				{
					allFiles.insert(std::make_pair(name, file));
				}
			}
		}

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

			// �ҵ��ļ�����Ӧ���ļ�ID��ע�⣺ͬһ���ļ����ܱ�������Σ�FileID�ǲ�һ���ģ�����ȡ����������С��FileID��
			auto findItr = allFiles.find(fileName);
			if (findItr == allFiles.end())
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

	// ���������б�Ҫ������ļ�
	void ParsingFile::CleanAllFile()
	{
		CleanBy(ProjectHistory::instance.m_files);
	}

	// �������ļ�
	void ParsingFile::CleanMainFile()
	{
		FileHistoryMap root;

		// ��ȡ�����ļ��Ĵ������¼
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

	// ��ӡ���滻��¼
	void ParsingFile::PrintCanReplace() const
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

	// ��ӡͷ�ļ�����·��
	void ParsingFile::PrintHeaderSearchPath() const
	{
		if (m_headerSearchPaths.empty())
		{
			return;
		}

		llvm::outs() << "    " << ++m_i << ". header search path list :" << m_headerSearchPaths.size() << "\n";

		for (const HeaderSearchDir &path : m_headerSearchPaths)
		{
			llvm::outs() << "        search path = " << path.m_dir << "\n";
		}

		llvm::outs() << "\n";
	}

	// ���ڵ��ԣ���ӡ���ļ����õ��ļ�������ڸ��ļ���#include�ı�
	void ParsingFile::PrintRelativeInclude() const
	{
		llvm::outs() << "    " << ++m_i << ". relative include list :" << m_uses.size() << "\n";

		FileID mainFileID = m_srcMgr->getMainFileID();

		for (auto itr : m_uses)
		{
			FileID file = itr.first;

			if (!CanClean(file))
			{
				continue;
			}

			std::set<FileID> &be_uses = itr.second;

			llvm::outs() << "        file = [" << pathtool::simplify_path(pathtool::get_absolute_path(GetFileName(file)).c_str()) << "]\n";
			for (FileID be_used_file : be_uses)
			{
				llvm::outs() << "            use relative include = [" << GetRelativeIncludeStr(file, be_used_file) << "]\n";
			}

			llvm::outs() << "\n";
		}
	}

	// ���ڵ��Ը��٣���ӡ�Ƿ����ļ��ı�����������©
	void ParsingFile::PrintNotFoundIncludeLocForDebug()
	{
		llvm::outs() << "    " << ++m_i << ". not found include loc: " << "" << "\n";

		for (auto itr : m_uses)
		{
			const std::set<FileID> &beuse_files = itr.second;

			for (FileID beuse_file : beuse_files)
			{
				SourceLocation loc = m_srcMgr->getIncludeLoc(beuse_file);
				if (m_includeLocs.find(loc) == m_includeLocs.end())
				{
					llvm::outs() << "        not found = " << GetAbsoluteFileName(beuse_file) << "\n";
					continue;
				}
			}
		}
	}

	// ���ڵ��Ը��٣���ӡ���ļ����Ƿ�ͬһ�г�����2��#include
	void ParsingFile::PrintSameLineForDebug()
	{
		llvm::outs() << "    " << ++m_i << ". same line include loc: " << "" << "\n";

		std::map<string, std::set<int>> all_lines;

		for (auto itr : m_includeLocs)
		{
			SourceLocation loc = itr.first;

			PresumedLoc presumedLoc = m_srcMgr->getPresumedLoc(loc);
			if (presumedLoc.isInvalid())
			{
				continue;
			}

			string fileName = pathtool::get_absolute_path(presumedLoc.getFilename());
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

	// ��ӡ��Ϣ
	void ParsingFile::Print()
	{
		llvm::outs() << "\n\n////////////////////////////////////////////////////////////////\n";
		llvm::outs() << "[file = " << GetAbsoluteFileName(m_srcMgr->getMainFileID()) << "]";
		llvm::outs() << "\n////////////////////////////////////////////////////////////////\n";

		m_i = 0;

		int verbose = Project::instance.m_verboseLvl;

		if (verbose >= VerboseLvl_1)
		{
			PrintUnusedInclude();
			PrintCanReplace();
			PrintCanForwarddecl();
		}

		if (verbose >= VerboseLvl_2)
		{
			PrintUsedNames();
			PrintUse();
		}

		if (verbose >= VerboseLvl_3)
		{
			PrintAllCycleUsedNames();
			PrintRootCycleUsedNames();

			PrintRootCycleUse();
			PrintAllCycleUse();

			PrintCycleUsedChildren();
		}

		if (verbose >= VerboseLvl_4)
		{
			PrintAllFile();
			PrintInclude();

			PrintHeaderSearchPath();
			PrintRelativeInclude();
			PrintParentsById();
		}

		if (verbose >= VerboseLvl_5)
		{
			PrintNotFoundIncludeLocForDebug();
			PrintSameLineForDebug();
		}
	}

	// �ϲ��ɱ��Ƴ���#include��
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

	// �ϲ���������ǰ������
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

	// �ϲ����滻��#include
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

		for (FileID file : m_allCycleUse)
		{
			string fileName		= GetAbsoluteFileName(file);
			FileHistory &oldFile	= oldFiles[fileName];

			++oldFile.m_beUseCount;
		}
	}

	// ����ǰcpp�ļ������Ĵ������¼��֮ǰ����cpp�ļ������Ĵ������¼�ϲ�
	void ParsingFile::MergeTo(FileHistoryMap &oldFiles) const
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
	bool ParsingFile::IsWindowsFormat(FileID file) const
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

	// ȡ����ǰcpp�ļ������Ĵ������¼
	void ParsingFile::TakeAllInfoTo(FileHistoryMap &out) const
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
	void ParsingFile::TakeUnusedLineByFile(FileHistoryMap &out) const
	{
		for (SourceLocation loc : m_unusedLocs)
		{
			PresumedLoc presumedLoc = m_srcMgr->getPresumedLoc(loc);
			if (presumedLoc.isInvalid())
			{
				llvm::outs() << "------->error: take_unused_line_by_file getPresumedLoc failed\n";
				continue;
			}

			string fileName			= pathtool::get_absolute_path(presumedLoc.getFilename());
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
	void ParsingFile::TakeNewForwarddeclByFile(FileHistoryMap &out) const
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

				string fileName			= pathtool::get_absolute_path(presumedLoc.getFilename());

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

	// ���ļ��滻��¼�����ļ����й���
	void ParsingFile::SplitReplaceByFile(ReplaceFileMap &replaces) const
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

	// ���ļ��Ƿ��Ǳ�-includeǿ�ư���
	bool ParsingFile::IsForceIncluded(FileID file) const
	{
		return (nullptr == m_srcMgr->getFileEntryForID(m_srcMgr->getFileID(m_srcMgr->getIncludeLoc(file))));
	}

	// ȡ��ָ���ļ���#include�滻��Ϣ
	void ParsingFile::TakeBeReplaceOfFile(FileHistory &eachFile, FileID top, const ChildrenReplaceMap &childernReplaces) const
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
					replaceInfo.m_newText	= GetRelativeIncludeStr(GetParent(oldFile), replace_file);

					// ��¼[�������ļ��������к�]
					replaceInfo.m_line		= GetLineNo(include_loc);
					replaceInfo.m_fileName	= GetAbsoluteFileName(replace_file);
					replaceInfo.m_inFile	= m_srcMgr->getFilename(include_loc);

					replaceLine.m_newInclude.push_back(replaceInfo);
				}
			}
		}
	}

	// ȡ�����ļ���#include�滻��Ϣ
	void ParsingFile::TakeReplaceByFile(FileHistoryMap &out) const
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