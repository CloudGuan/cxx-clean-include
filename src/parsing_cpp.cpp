///<------------------------------------------------------------------------------
//< @file:   parsing_cpp.cpp
//< @author: ������
//< @date:   2016��2��22��
//< @brief:  �洢��ǰ���ڽ�����cpp�ļ����������ͷ�ļ�����Ϣ
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

#include "tool.h"
#include "project.h"
#include "html_log.h"

ParsingFile* ParsingFile::g_atFile = nullptr;

namespace cxxcleantool
{
	ParsingFile::ParsingFile(clang::Rewriter &rewriter, clang::CompilerInstance &compiler)
	{
		m_rewriter	= &rewriter;
		m_compiler	= &compiler;
		m_srcMgr	= &compiler.getSourceManager();
		m_printIdx	= 0;
		g_atFile	= this;

		clang::HeaderSearch &headerSearch = m_compiler->getPreprocessor().getHeaderSearchInfo();
		m_headerSearchPaths = TakeHeaderSearchPaths(headerSearch);
	}

	ParsingFile::~ParsingFile()
	{
		g_atFile = nullptr;
	}

	// ���#include��λ�ü�¼
	void ParsingFile::AddIncludeLoc(SourceLocation loc, SourceRange range)
	{
		SourceRange spellingRange(GetExpasionLoc(range.getBegin()), GetExpasionLoc(range.getEnd()));

		// cxx::log() << "ParsingFile::AddIncludeLoc = " << GetSourceOfLine(GetExpasionLoc(loc)) << "\n";

		m_includeLocs[GetExpasionLoc(loc)] = range;
	}

	// ��ӳ�Ա�ļ�
	void ParsingFile::AddFile(FileID file)
	{
		if (file.isValid())
		{
			m_files.insert(file);
		}
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

	// ���ɽ��ǰ��׼��
	void ParsingFile::PrepareResult()
	{
		// 1. ��������ÿ���ļ��ĺ���ļ�
		for (FileID file : m_files)
		{
			for (FileID child = file, parent; (parent = GetParent(child)).isValid(); child = parent)
			{
				m_children[parent].insert(file);
			}
		}

		// 2. ���������ļ�������Ҫ��Ϊ�˴ﵽ���Ŀ�꣺ÿ���ļ����ȱ����Լ��ļ��ڵ�#include���
		for (auto itr : m_uses)
		{
			FileID file							= itr.first;
			const std::set<FileID> &beuseList	= itr.second;

			if (!CanClean(file))
			{
				continue;
			}

			for (FileID beuse : beuseList)
			{
				if (!IsIncludedBy(beuse, file))
				{
					UseByFileName(file, GetAbsoluteFileName(beuse).c_str());
				}
			}
		}
	}

	// ���ɸ��ļ��Ĵ������¼
	void ParsingFile::GenerateResult()
	{
		PrepareResult();

		GenerateRootCycleUse();
		GenerateAllCycleUse();

		GenerateUnusedInclude();
		GenerateCanForwardDeclaration();
		GenerateCanReplace();

		GenerateUsefulUsingNamespace();

		ProjectHistory::instance.AddFile(this);
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
					FileID oldReplaceTo = m_replaces[lv2Top];
					FileID canReplaceTo = GetCanReplaceTo(lv2Top);

					if (canReplaceTo != oldReplaceTo)
					{
						m_replaces.erase(lv2Top);

						// cxx::log() << "fatal: there is replace cleared!\n";
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
						m_replaces[lv2Top] = canReplaceTo;
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
			������δ����Ǳ����ߵ���Ҫ����˼·
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
			FileID file = GetFileID(loc);

			if (!IsCyclyUsed(file))
			{
				continue;
			}

			if (!IsLocBeUsed(loc))
			{
				m_unusedLocs.insert(loc);
			}
		}

		for (FileID beusedFile : m_files)
		{
			if (IsPrecompileHeader(beusedFile))
			{
				m_unusedLocs.erase(m_srcMgr->getIncludeLoc(beusedFile));
			}
		}
	}

	// ��ָ�����ļ��б����ҵ����ڴ����ļ��ĺ��
	std::set<FileID> ParsingFile::GetChildren(FileID ancestor, std::set<FileID> all_children/* ������ancestor���ӵ��ļ� */)
	{
		// ����ancestor�ĺ���ļ��б�
		std::set<FileID> children;

		// cxx::log() << "ancestor = " << get_file_name(ancestor) << "\n";

		// ��ȡ���ڸ��ļ��ĺ���б����ļ�ʹ�õ�һ����
		for (FileID child : all_children)
		{
			if (!IsAncestor(child, ancestor))
			{
				continue;
			}

			// cxx::log() << "    child = " << get_file_name(child) << "\n";
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

		//------------------------------- ѭ����ȡ�������ļ��������������ļ� -------------------------------//
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

	// ����Ӧ������using namespace
	void ParsingFile::GenerateUsefulUsingNamespace()
	{
		// 1. ȡ���������ļ��е������ռ�
		std::set<string> usedNamespaces;
		for (auto itr : m_namespaces)
		{
			FileID file		= itr.first;
			auto namespaces	= itr.second;
			if (!IsCyclyUsed(file))
			{
				continue;
			}

			usedNamespaces.insert(namespaces.begin(), namespaces.end());
		}

		// 2. ɾ�����ڱ������ļ��е�using namespace��������namespace��using
		for (auto itr = m_usefulUsingNamespaces.begin(); itr != m_usefulUsingNamespaces.end();)
		{
			SourceLocation loc		= itr->first;
			const NamespaceInfo &ns	= itr->second;

			bool need = true;
			need &= !IsCyclyUsed(GetFileID(loc));
			need &= (usedNamespaces.find(ns.ns_name) != usedNamespaces.end());

			if (!need)
			{
				m_usefulUsingNamespaces.erase(itr++);
			}
			else
			{
				++itr;
			}
		}

		// 3. ɾ���ظ���using namespace
		{
			std::set<std::string> namespaces;

			for (auto itr = m_usefulUsingNamespaces.begin(); itr != m_usefulUsingNamespaces.end();)
			{
				const NamespaceInfo &ns	= itr->second;

				if (namespaces.find(ns.ns_name) != namespaces.end())
				{
					m_usefulUsingNamespaces.erase(itr++);
				}
				else
				{
					namespaces.insert(ns.ns_name);
					++itr;
				}
			}
		}
	}

	// ��ǰ�ļ�֮ǰ�Ƿ������ļ������˸�class��struct��union
	bool ParsingFile::HasRecordBefore(FileID cur, const CXXRecordDecl &cxxRecord) const
	{
		FileID recordAtFile = GetFileID(cxxRecord.getLocStart());

		// �������ڵ��ļ������ã�����Ҫ�ټ�ǰ������
		if (IsCyclyUsed(recordAtFile))
		{
			return true;
		}

		// ����˵�������ڵ��ļ�δ������
		// 1. ��ʱ��������ඨ��֮ǰ�������ļ��У��Ƿ����и����ǰ������
		for (const CXXRecordDecl *prev = cxxRecord.getPreviousDecl(); prev; prev = prev->getPreviousDecl())
		{
			FileID prevFileId = GetFileID(prev->getLocation());
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

			FileID redeclFileID = GetFileID(redecl->getLocation());
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
					cxx::log() << "\nprocessing file: " << GetAbsoluteFileName(file) << "\n";
					cxx::log() << "    old forwarddecl list: \n";

					for (const CXXRecordDecl* cxxRecordDecl : old_forwards)
					{
						cxx::log() << "        forwarddecl: " << GetCxxrecordName(*cxxRecordDecl) << " in " << GetAbsoluteFileName(file) << "\n";
					}

					cxx::log() << "\n";
					cxx::log() << "    new forwarddecl list: \n";

					for (const CXXRecordDecl* cxxRecordDecl : can_forwards)
					{
						cxx::log() << "        forwarddecl: " << GetCxxrecordName(*cxxRecordDecl) << "\n";
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
		if (range.isInvalid())
		{
			return "";
		}

		range.setBegin(GetExpasionLoc(range.getBegin()));
		range.setEnd(GetExpasionLoc(range.getEnd()));

		if (range.getEnd() < range.getBegin())
		{
			llvm::errs() << "[error][ParsingFile::GetSourceOfRange] if (range.getEnd() < range.getBegin())\n";
			return "";
		}

		if (!m_srcMgr->isWrittenInSameFile(range.getBegin(), range.getEnd()))
		{
			llvm::errs() << "[error][ParsingFile::GetSourceOfRange] if (!m_srcMgr->isWrittenInSameFile(range.getBegin(), range.getEnd()))\n";
			return "";
		}

		const char* beg = GetSourceAtLoc(range.getBegin());
		const char* end = GetSourceAtLoc(range.getEnd());

		if (nullptr == beg || nullptr == end)
		{
			return "";
		}

		if (end < beg)
		{
			// ע�⣺������������ж���������ܻ������Ϊ�п���ĩβ��������ʼ�ַ�ǰ�棬�����ں�֮��
			return "";
		}

		return string(beg, end);
	}

	// ��ȡָ��λ�õ��ı�
	const char* ParsingFile::GetSourceAtLoc(SourceLocation loc) const
	{
		bool err = true;

		const char* beg = m_srcMgr->getCharacterData(loc,	&err);
		if (err)
		{
			/*
			PresumedLoc presumedLoc = m_srcMgr->getPresumedLoc(loc);
			if (presumedLoc.isValid())
			{
				llvm::errs() << "[error][ParsingFile::GetSourceAtLoc]! err = " << err << "at file = " << presumedLoc.getFilename() << ", line = " << GetLineNo(loc) << "\n";
			}
			else
			{
				llvm::errs() << "[error][ParsingFile::GetSourceAtLoc]! err = " << err << "\n";
			}
			*/

			return nullptr;
		}

		return beg;
	}

	// ��ȡ�÷�ΧԴ�����Ϣ���ı��������ļ������к�
	std::string ParsingFile::DebugRangeText(SourceRange range) const
	{
		string rangeText = GetSourceOfRange(range);
		std::stringstream text;
		text << "[" << htmltool::get_include_html(rangeText) << "] in [";
		text << htmltool::get_file_html(GetAbsoluteFileName(GetFileID(range.getBegin())));
		text << "] line = " << htmltool::get_number_html(GetLineNo(range.getBegin()));
		return text.str();
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
		SourceLocation fileBeginLoc = m_srcMgr->getLocForStartOfFile(GetFileID(loc));
		SourceLocation fileEndLoc	= m_srcMgr->getLocForEndOfFile(GetFileID(loc));

		const char* character	= GetSourceAtLoc(loc);
		const char* fileStart	= GetSourceAtLoc(fileBeginLoc);
		const char* fileEnd		= GetSourceAtLoc(fileEndLoc);

		if (nullptr == character || nullptr == fileStart || nullptr == fileEnd)
		{
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

		const char* c1			= GetSourceAtLoc(lineEnd);
		const char* c2			= GetSourceAtLoc(lineEnd.getLocWithOffset(1));

		if (nullptr == c1 || nullptr == c2)
		{
			SourceLocation fileEndLoc	= m_srcMgr->getLocForEndOfFile(GetFileID(loc));
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
		bool invalid = false;

		int line = m_srcMgr->getSpellingLineNumber(loc, &invalid);
		if (invalid)
		{
			line = m_srcMgr->getExpansionLineNumber(loc, &invalid);
		}

		return invalid ? 0 : line;
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
		std::string text = GetSourceOfRange(range);

		if (text.empty())
		{
			text = GetSourceOfLine(range.getBegin());
		}

		return text;
	}

	// curλ�õĴ���ʹ��srcλ�õĴ���
	void ParsingFile::Use(SourceLocation cur, SourceLocation src, const char* name /* = nullptr */)
	{
		cur = GetExpasionLoc(cur);
		src = GetExpasionLoc(src);

		FileID curFileID = GetFileID(cur);
		FileID srcFileID = GetFileID(src);

		UseInclude(curFileID, srcFileID, name, GetLineNo(cur));
	}

	void ParsingFile::UseName(FileID file, FileID beusedFile, const char* name /* = nullptr */, int line)
	{
		if (Project::instance.m_verboseLvl < VerboseLvl_2)
		{
			return;
		}

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
		const FileEntry *rootFileEntry		= m_srcMgr->getFileEntryForID(file);
		const FileEntry *beusedFileEntry	= m_srcMgr->getFileEntryForID(beusedFile);

		if (nullptr == rootFileEntry || nullptr == beusedFileEntry)
		{
			// cxx::log() << "------->error: use_include() : m_srcMgr->getFileEntryForID(file) failed!" << m_srcMgr->getFilename(m_srcMgr->getLocForStartOfFile(file)) << ":" << m_srcMgr->getFilename(m_srcMgr->getLocForStartOfFile(beusedFile)) << "\n";
			// cxx::log() << "------->error: use_include() : m_srcMgr->getFileEntryForID(file) failed!" << get_source_of_line(m_srcMgr->getLocForStartOfFile(file)) << ":" << get_source_of_line(m_srcMgr->getLocForStartOfFile(beusedFile)) << "\n";
			return;
		}

		if (GetAbsoluteFileName(file) == GetAbsoluteFileName(beusedFile))
		{
			return;
		}

		m_uses[file].insert(beusedFile);
		UseName(file, beusedFile, name, line);
	}

	// �ļ�aʹ��ָ�����Ƶ�Ŀ���ļ�
	void ParsingFile::UseByFileName(FileID a, const char* filename)
	{
		auto itr = m_uses.find(a);
		if (itr == m_uses.end())
		{
			llvm::errs() << "[error][ParsingFile::UseByFileName] if (itr == m_uses.end())";
			return;
		}

		FileID child = GetDirectChildByName(a, filename);
		if (child.isInvalid())
		{
			child = GetChildByName(a, filename);

			if (child.isInvalid())
			{
				// ���������������ֱ�ӷ���
				return;
			}
		}

		std::set<FileID> &useList = itr->second;
		if (useList.find(child) == useList.end())
		{
			useList.insert(child);
			m_newUse[a].insert(child);
		}
	}

	// ��ǰλ��ʹ��ָ���ĺ�
	void ParsingFile::UseMacro(SourceLocation loc, const MacroDefinition &macro, const Token &macroNameTok, const MacroArgs *args /* = nullptr */)
	{
		MacroInfo *info = macro.getMacroInfo();
		if (nullptr == info)
		{
			return;
		}

		string macroName = macroNameTok.getIdentifierInfo()->getName().str() + "[macro]";

		// cxx::log() << "macro id = " << macroName.getIdentifierInfo()->getName().str() << "\n";
		Use(loc, info->getDefinitionLoc(), macroName.c_str());
	}

	// ��ǰλ��ʹ�ö�λ
	void ParsingFile::UseQualifier(SourceLocation loc, const NestedNameSpecifier *specifier)
	{
		while (specifier)
		{
			const Type *pType = specifier->getAsType();
			UseType(loc, pType);

			specifier = specifier->getPrefix();
		}
	}

	// �����������ռ�
	void ParsingFile::DeclareNamespace(const NamespaceDecl *d)
	{
		SourceLocation loc = GetSpellingLoc(d->getLocation());

		FileID file = GetFileID(loc);
		if (file.isInvalid())
		{
			return;
		}

		m_namespaces[file].insert(d->getQualifiedNameAsString());
	}

	// using�������ռ䣬���磺using namespace std;
	void ParsingFile::UsingNamespace(const UsingDirectiveDecl *d)
	{
		SourceLocation loc = GetSpellingLoc(d->getLocation());

		FileID file = GetFileID(loc);
		if (file.isInvalid())
		{
			return;
		}

		const NamespaceDecl *nsDecl	= d->getNominatedNamespace();
		std::string ns				= nsDecl->getQualifiedNameAsString();
		m_usingNamespaces[file].insert(ns);

		NamespaceInfo nsInfo;
		nsInfo.ns_decl = GetNestedNamespace(nsDecl);
		nsInfo.ns_name = ns;
		m_usefulUsingNamespaces[loc] = nsInfo;

		// ���������ռ����ڵ��ļ���ע�⣺using namespaceʱ�������ҵ���Ӧ��namespace���������磬using namespace Aǰһ��Ҫ��namespace A{}�������ᱨ��
		Use(loc, nsDecl->getLocation(), GetNestedNamespace(nsDecl).c_str());
	}

	// using�������ռ��µ�ĳ�࣬���磺using std::string;
	void ParsingFile::UsingXXX(const UsingDecl *d)
	{
		SourceLocation usingLoc		= d->getUsingLoc();

		for (auto itr = d->shadow_begin(); itr != d->shadow_end(); ++itr)
		{
			UsingShadowDecl *shadowDecl = *itr;

			NamedDecl *nameDecl = shadowDecl->getTargetDecl();
			if (nullptr == nameDecl)
			{
				continue;
			}

			std::stringstream name;
			name << "using " << shadowDecl->getQualifiedNameAsString() << "[" << nameDecl->getDeclKindName() << "]";

			Use(usingLoc, nameDecl->getLocEnd(), name.str().c_str());

			// ע�⣺����Ҫ�������ã���Ϊ����������a�ļ���#include <string>��Ȼ����b�ļ�using std::string����ôb�ļ�Ҳ�����õ�
			Use(nameDecl->getLocEnd(), usingLoc);
		}
	}

	// ��ȡ����ȱʧ��using namespace
	bool ParsingFile::GetMissingNamespace(SourceLocation loc, std::map<std::string, std::string> &miss) const
	{
		loc = GetSpellingLoc(loc);

		FileID file = GetFileID(loc);

		bool is_need_add = true;

		for (auto itr : m_usefulUsingNamespaces)
		{
			SourceLocation usingNsLoc	= itr.first;
			const NamespaceInfo &ns		= itr.second;

			FileID usingNsfile	= GetFileID(usingNsLoc);

			if (!IsAncestor(usingNsfile, file))
			{
				continue;
			}

			FileID lv2 = GetLvl2Ancestor(usingNsfile, file);
			if (m_srcMgr->getIncludeLoc(lv2) == loc)
			{
				std::string addNs = "using namespace " + ns.ns_name + ";";
				miss[addNs] = ns.ns_decl;
				is_need_add = true;
			}
		}

		return is_need_add;
	}

	// ��ȡ����ȱʧ��using namespace
	bool ParsingFile::GetMissingNamespace(SourceLocation topLoc, SourceLocation oldLoc,
	                                      std::map<std::string, std::string> &frontMiss, std::map<std::string, std::string> &backMiss) const
	{
		FileID file = GetFileID(topLoc);

		bool is_need_add = true;

		for (auto itr : m_usefulUsingNamespaces)
		{
			SourceLocation usingNsLoc	= itr.first;
			const NamespaceInfo &ns		= itr.second;
			FileID usingNsfile	= GetFileID(usingNsLoc);

			if (!IsAncestor(usingNsfile, file))
			{
				continue;
			}

			FileID lv2 = GetLvl2Ancestor(usingNsfile, file);

			SourceLocation lv2BeIncludeLoc = m_srcMgr->getIncludeLoc(lv2);
			if (lv2BeIncludeLoc != topLoc)
			{
				continue;
			}

			is_need_add = true;

			std::string addNs = "using namespace " + ns.ns_name + ";";
			if (oldLoc < usingNsLoc)
			{
				backMiss[addNs] = ns.ns_decl;
			}
			else
			{
				frontMiss[addNs] = ns.ns_decl;
			}
		}

		return is_need_add;
	}

	// ��ȡ�����ռ��ȫ��·�������磬����namespace A{ namespace B{ class C; }}
	std::string ParsingFile::GetNestedNamespace(const NamespaceDecl *d)
	{
		if (nullptr == d)
		{
			return "";
		}

		string name;// = "namespace " + d->getNameAsString() + "{}";

		while (d)
		{
			string namespaceName = "namespace " + d->getNameAsString();
			name = namespaceName + "{" + name + "}";

			const DeclContext *parent = d->getParent();
			if (parent && parent->isNamespace())
			{
				d = cast<NamespaceDecl>(parent);
			}
			else
			{
				break;
			}
		}

		return name;
	}

	// �ļ�b�Ƿ�ֱ��#include�ļ�a
	bool ParsingFile::IsIncludedBy(FileID a, FileID b)
	{
		auto itr = m_includes.find(b);
		if (itr == m_includes.end())
		{
			return false;
		}

		const std::set<FileID> &includes = itr->second;
		return includes.find(a) != includes.end();
	}

	// ��ȡ�ļ�a��ָ�����Ƶ�ֱ�Ӻ���ļ�
	FileID ParsingFile::GetDirectChildByName(FileID a, const char* childFileName)
	{
		auto itr = m_includes.find(a);
		if (itr == m_includes.end())
		{
			return FileID();
		}

		const std::set<FileID> &includes = itr->second;
		for (FileID child : includes)
		{
			if (GetAbsoluteFileName(child) == childFileName)
			{
				return child;
			}
		}

		return FileID();
	}

	// ��ȡ�ļ�a��ָ�����Ƶĺ���ļ�
	FileID ParsingFile::GetChildByName(FileID a, const char* childFileName)
	{
		auto itr = m_children.find(a);
		if (itr == m_children.end())
		{
			return FileID();
		}

		const std::set<FileID> &children = itr->second;
		for (FileID child : children)
		{
			if (GetAbsoluteFileName(child) == childFileName)
			{
				return child;
			}
		}

		return FileID();
	}


	// ��ǰλ��ʹ��Ŀ�����ͣ�ע��Type����ĳ�����ͣ�������const��volatile��static�ȵ����Σ�
	void ParsingFile::UseType(SourceLocation loc, const Type *t)
	{
		if (nullptr == t)
		{
			return;
		}

		// ʹ�õ�typedef���ͣ����磺typedef int dword����dword����TypedefType
		if (isa<TypedefType>(t))
		{
			const TypedefType *typedefType = cast<TypedefType>(t);
			const TypedefNameDecl *typedefNameDecl = typedefType->getDecl();

			UseNameDecl(loc, typedefNameDecl);

			// ע������typedef��ԭ����Ȼ����������typedef�������ɣ�����Ҫ�����ֽ�
		}
		// ĳ�����͵Ĵ��ţ��磺struct S��N::M::type
		else if (isa<ElaboratedType>(t))
		{
			const ElaboratedType *elaboratedType = cast<ElaboratedType>(t);
			UseQualType(loc, elaboratedType->getNamedType());

			// Ƕ����������
			if (elaboratedType->getQualifier())
			{
				UseQualifier(loc, elaboratedType->getQualifier());
			}
		}
		else if (isa<TemplateSpecializationType>(t))
		{
			const TemplateSpecializationType *templateType = cast<TemplateSpecializationType>(t);

			UseNameDecl(loc, templateType->getTemplateName().getAsTemplateDecl());

			for (int i = 0, size = templateType->getNumArgs(); i < size; ++i)
			{
				const TemplateArgument &arg = templateType->getArg((unsigned)i);
				UseTemplateArgument(loc, arg);
			}
		}
		// ����ȡ�����ģ�����Ͳ���
		else if (isa<SubstTemplateTypeParmType>(t))
		{
			const SubstTemplateTypeParmType *substTemplateTypeParmType = cast<SubstTemplateTypeParmType>(t);
			UseType(loc, substTemplateTypeParmType->getReplacedParameter());
		}
		else if (isa<ElaboratedType>(t))
		{
			const ElaboratedType *elaboratedType = cast<ElaboratedType>(t);
			UseQualType(loc, elaboratedType->getNamedType());
		}
		else if (isa<AttributedType>(t))
		{
			const AttributedType *attributedType = cast<AttributedType>(t);
			UseQualType(loc, attributedType->getModifiedType());
		}
		else if (isa<FunctionType>(t))
		{
			const FunctionType *functionType = cast<FunctionType>(t);

			// ʶ�𷵻�ֵ����
			{
				// �����ķ���ֵ
				QualType returnType = functionType->getReturnType();
				UseQualType(loc, returnType);
			}
		}
		else if (isa<MemberPointerType>(t))
		{
			const MemberPointerType *memberPointerType = cast<MemberPointerType>(t);
			UseQualType(loc, memberPointerType->getPointeeType());
		}
		// ģ��������ͣ����磺template <typename T>�����T
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

			// ��ģ�������Ĭ�ϲ���
			if (decl->hasDefaultArgument())
			{
				UseQualType(loc, decl->getDefaultArgument());
			}

			UseNameDecl(loc, decl);
		}
		else if (isa<ParenType>(t))
		{
			const ParenType *parenType = cast<ParenType>(t);
			// llvm::errs() << "------------ ParenType dump ------------:\n";
			// llvm::errs() << "------------ parenType->getInnerType().dump() ------------:\n";
			UseQualType(loc, parenType->getInnerType());
		}
		else if (isa<InjectedClassNameType>(t))
		{
			const InjectedClassNameType *injectedClassNameType = cast<InjectedClassNameType>(t);
			// llvm::errs() << "------------InjectedClassNameType dump:\n";
			// llvm::errs() << "------------injectedClassNameType->getInjectedSpecializationType().dump():\n";
			UseQualType(loc, injectedClassNameType->getInjectedSpecializationType());
		}
		else if (isa<PackExpansionType>(t))
		{
			const PackExpansionType *packExpansionType = cast<PackExpansionType>(t);
			// llvm::errs() << "\n------------PackExpansionType------------\n";
			UseQualType(loc, packExpansionType->getPattern());
		}
		else if (isa<DecltypeType>(t))
		{
			const DecltypeType *decltypeType = cast<DecltypeType>(t);
			// llvm::errs() << "\n------------DecltypeType------------\n";
			UseQualType(loc, decltypeType->getUnderlyingType());
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
		// �ࡢstruct��union
		else if (isa<RecordType>(t))
		{
			const RecordType *recordType = cast<RecordType>(t);

			const RecordDecl *recordDecl = recordType->getDecl();
			if (nullptr == recordDecl)
			{
				return;
			}

			UseRecord(loc, recordDecl);
		}
		else if (t->isArrayType())
		{
			const ArrayType *arrayType = cast<ArrayType>(t);
			UseQualType(loc, arrayType->getElementType());
		}
		else if (t->isVectorType())
		{
			const VectorType *vectorType = cast<VectorType>(t);
			UseQualType(loc, vectorType->getElementType());
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

			UseQualType(loc, pointeeType);
		}
		else if (t->isEnumeralType())
		{
			TagDecl *decl = t->getAsTagDecl();
			if (nullptr == decl)
			{
				return;
			}

			UseNameDecl(loc, decl);
		}
		else
		{
			// llvm::errs() << "-------------- haven't support type --------------\n";
			// t->dump();
		}
	}

	// ��ǰλ��ʹ��Ŀ�����ͣ�ע��QualType������ĳ�����͵�const��volatile��static�ȵ����Σ�
	void ParsingFile::UseQualType(SourceLocation loc, const QualType &t)
	{
		if (t.isNull())
		{
			return;
		}

		const Type *pType = t.getTypePtr();
		UseType(loc, pType);
	}

	// ��ȡc++��class��struct��union��ȫ������������������ռ�
	// ���磺
	//     ������C��C���������ռ�A�е������ռ�B�����������أ�namespace A{ namespace B{ class C; }}
	string ParsingFile::GetRecordName(const RecordDecl &recordDecl) const
	{
		string name;
		name += recordDecl.getKindName();
		name += " " + recordDecl.getNameAsString();
		name += ";";

		bool inNameSpace = false;

		const DeclContext *curDeclContext = recordDecl.getDeclContext();
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
			cxx::log() << "LexSkipComment Invalid = true";
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
		FileID recordAtFile	= GetFileID(cxxRecord.getLocStart());

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
		if (nullptr == cxxRecordDecl)
		{
			return;
		}

		FileID file = GetFileID(loc);
		if (file.isInvalid())
		{
			return;
		}

		// ����ļ���ʹ�õ�ǰ��������¼�����ڲ���Ҫ��ӵ�ǰ����������֮���������
		m_forwardDecls[file].insert(cxxRecordDecl);
	}

	// �Ƿ�Ϊ��ǰ������������
	bool ParsingFile::IsForwardType(const QualType &var)
	{
		if (!var->isPointerType() && !var->isReferenceType())
		{
			return false;
		}

		QualType pointeeType = var->getPointeeType();

		// �����ָ�����;ͻ�ȡ��ָ������(PointeeType)
		while (pointeeType->isPointerType() || pointeeType->isReferenceType())
		{
			pointeeType = pointeeType->getPointeeType();
		}

		if (!isa<RecordType>(pointeeType))
		{
			return false;
		}

		if (!pointeeType->isRecordType())
		{
			return false;
		}

		const CXXRecordDecl *cxxRecordDecl = pointeeType->getAsCXXRecordDecl();
		if (nullptr == cxxRecordDecl)
		{
			return false;
		}

		// ģ���ػ�
		if (isa<ClassTemplateSpecializationDecl>(cxxRecordDecl))
		{
			return false;
		}

		return true;
	}

	// ����ʹ�ñ�����¼
	void ParsingFile::UseVarType(SourceLocation loc, const QualType &var)
	{
		if (!IsForwardType(var))
		{
			UseQualType(loc, var);
			return;
		}

		QualType pointeeType = var->getPointeeType();
		while (pointeeType->isPointerType() || pointeeType->isReferenceType())
		{
			pointeeType = pointeeType->getPointeeType();
		}

		const CXXRecordDecl *cxxRecordDecl = pointeeType->getAsCXXRecordDecl();
		UseForward(loc, cxxRecordDecl);
	}

	// ���ñ�������
	void ParsingFile::UseVarDecl(SourceLocation loc, const VarDecl *var)
	{
		if (nullptr == var)
		{
			return;
		}

		if (!IsForwardType(var->getType()))
		{
			UseValueDecl(loc, var);
			return;
		}

		UseVarType(loc, var->getType());
	}

	// ���ã�����������Ϊ��ֵ����������ʶ��enum����
	void ParsingFile::UseValueDecl(SourceLocation loc, const ValueDecl *valueDecl)
	{
		if (nullptr == valueDecl)
		{
			return;
		}

		if (isa<FunctionDecl>(valueDecl))
		{
			const FunctionDecl *f = cast<FunctionDecl>(valueDecl);
			if (nullptr == f)
			{
				return;
			}

			UseFuncDecl(loc, f);
			return;
		}

		std::stringstream name;
		name << valueDecl->getQualifiedNameAsString() << "[" << valueDecl->getDeclKindName() << "]";

		Use(loc, valueDecl->getLocEnd(), name.str().c_str());
		UseQualType(loc, valueDecl->getType());
	}

	// ���ô������Ƶ�����
	void ParsingFile::UseNameDecl(SourceLocation loc, const NamedDecl *nameDecl)
	{
		if (nullptr == nameDecl)
		{
			return;
		}

		std::stringstream name;
		name << nameDecl->getQualifiedNameAsString() << "[" << nameDecl->getDeclKindName() << "]";

		Use(loc, nameDecl->getLocEnd(), name.str().c_str());
	}

	// ����ʹ�ú���������¼
	void ParsingFile::UseFuncDecl(SourceLocation loc, const FunctionDecl *f)
	{
		if (nullptr == f)
		{
			return;
		}

		// Ƕ����������
		if (f->getQualifier())
		{
			UseQualifier(loc, f->getQualifier());
		}

		// ʶ�𷵻�ֵ����
		{
			// �����ķ���ֵ
			QualType returnType = f->getReturnType();
			UseVarType(loc, returnType);
		}

		// ʶ��������
		{
			// ���α����������������ù�ϵ
			for (FunctionDecl::param_const_iterator itr = f->param_begin(), end = f->param_end(); itr != end; ++itr)
			{
				ParmVarDecl *vardecl = *itr;
				QualType vartype = vardecl->getType();
				UseVarType(loc, vartype);
			}
		}

		if (f->getTemplateSpecializationArgs())
		{
			const TemplateArgumentList *args = f->getTemplateSpecializationArgs();
			UseTemplateArgumentList(loc, args);
		}

		std::stringstream name;
		name << f->getQualifiedNameAsString() << "[" << f->clang::Decl::getDeclKindName() << "]";

		Use(loc, f->getTypeSpecStartLoc(), name.str().c_str());
	}

	// ����ģ�����
	void ParsingFile::UseTemplateArgument(SourceLocation loc, const TemplateArgument &arg)
	{
		TemplateArgument::ArgKind argKind = arg.getKind();
		switch (argKind)
		{
		case TemplateArgument::Type:
			UseQualType(loc, arg.getAsType());
			break;

		case TemplateArgument::Declaration:
			UseValueDecl(loc, arg.getAsDecl());
			break;

		case TemplateArgument::Expression:
			Use(loc, arg.getAsExpr()->getLocStart());
			break;

		case TemplateArgument::Template:
			UseNameDecl(loc, arg.getAsTemplate().getAsTemplateDecl());
			break;

		default:
			break;
		}
	}

	// ����ģ������б�
	void ParsingFile::UseTemplateArgumentList(SourceLocation loc, const TemplateArgumentList *args)
	{
		if (nullptr == args)
		{
			return;
		}

		for (unsigned i = 0; i < args->size(); ++i)
		{
			const TemplateArgument &arg = args->get(i);
			UseTemplateArgument(loc, arg);
		}
	}

	// ����ʹ��class��struct��union��¼
	void ParsingFile::UseRecord(SourceLocation loc, const RecordDecl *record)
	{
		if (isa<ClassTemplateSpecializationDecl>(record))
		{
			const ClassTemplateSpecializationDecl *d = cast<ClassTemplateSpecializationDecl>(record);
			UseTemplateArgumentList(loc, &d->getTemplateArgs());
		}

		Use(loc, record->getLocStart(), GetRecordName(*record).c_str());
	}

	// �Ƿ����������c++�ļ������������������ļ����ݲ������κα仯��
	bool ParsingFile::CanClean(FileID file) const
	{
		return Project::instance.CanClean(GetAbsoluteFileName(file));
	}

	// ��ӡ���ļ��ĸ��ļ�
	void ParsingFile::PrintParent()
	{
		int num = 0;
		for (auto childparent : m_parentIDs)
		{
			FileID childFileID = childparent.first;
			FileID parentFileID = childparent.second;

			// ����ӡ����Ŀ���ļ���ֱ�ӹ������ļ�
			if (!CanClean(childFileID) && !CanClean(parentFileID))
			{
				continue;
			}

			++num;
		}

		HtmlDiv &div = HtmlLog::instance.m_newDiv;
		div.AddRow(AddPrintIdx() + ". list of parent id: has parent file count = " + htmltool::get_number_html(num), 1);

		for (auto childparent : m_parentIDs)
		{
			FileID childFileID = childparent.first;
			FileID parentFileID = childparent.second;

			// ����ӡ����Ŀ���ļ���ֱ�ӹ������ļ�
			if (!CanClean(childFileID) && !CanClean(parentFileID))
			{
				continue;
			}

			div.AddRow(htmltool::get_file_html(GetAbsoluteFileName(childFileID)), 2, 50);
			div.AddGrid("parent = ", 10);
			div.AddGrid(htmltool::get_file_html(GetAbsoluteFileName(parentFileID)), 39);
		}

		div.AddRow("");
	}

	// ��ӡ���ļ��ĺ����ļ�
	void ParsingFile::PrintChildren()
	{
		int num = 0;
		for (auto itr : m_children)
		{
			FileID parent = itr.first;

			// ����ӡ����Ŀ���ļ���ֱ�ӹ������ļ�
			if (!CanClean(parent))
			{
				continue;
			}

			++num;
		}

		HtmlDiv &div = HtmlLog::instance.m_newDiv;
		div.AddRow(AddPrintIdx() + ". list of children : file count = " + strtool::itoa(num), 1);

		for (auto itr : m_children)
		{
			FileID parent = itr.first;

			if (!CanClean(parent))
			{
				continue;
			}

			div.AddRow("file = " + htmltool::get_file_html(GetAbsoluteFileName(parent)), 2);

			for (FileID child : itr.second)
			{
				if (!CanClean(child))
				{
					continue;
				}

				div.AddRow("child = " + DebugBeIncludeText(child), 3);
			}

			div.AddRow("");
		}
	}

	// ��ӡ���������ù�ϵ
	void ParsingFile::PrintNewUse()
	{
		int num = 0;
		for (auto itr : m_newUse)
		{
			FileID file = itr.first;

			if (!CanClean(file))
			{
				continue;
			}

			++num;
		}

		HtmlDiv &div = HtmlLog::instance.m_newDiv;
		div.AddRow(AddPrintIdx() + ". list of new use : use count = " + strtool::itoa(num), 1);

		for (auto itr : m_newUse)
		{
			FileID file = itr.first;

			if (!CanClean(file))
			{
				continue;
			}

			div.AddRow("file = " + htmltool::get_file_html(GetAbsoluteFileName(file)), 2);

			for (FileID newuse : itr.second)
			{
				div.AddRow("new use = " + DebugBeIncludeText(newuse), 3);
			}

			div.AddRow("");
		}
	}

	// ��ȡ���ļ��ı�������Ϣ���������ݰ��������ļ��������ļ����������ļ�#include���кš������ļ�#include��ԭʼ�ı���
	string ParsingFile::DebugBeIncludeText(FileID file) const
	{
		string fileName = GetAbsoluteFileName(file);
		std::string text;

		if (file == m_srcMgr->getMainFileID())
		{
			text = strtool::get_text(cn_main_file_debug_text, htmltool::get_file_html(fileName).c_str(), file.getHashValue());
		}
		else
		{
			SourceLocation loc				= m_srcMgr->getIncludeLoc(file);
			PresumedLoc parentPresumedLoc	= m_srcMgr->getPresumedLoc(loc);
			string includeToken				= GetRawIncludeStr(file);
			string parentFileName			= pathtool::get_absolute_path(parentPresumedLoc.getFilename());

			if (includeToken.empty())
			{
				includeToken = "empty";
			}

			text = strtool::get_text(cn_file_debug_text,
			                         htmltool::get_file_html(fileName).c_str(), file.getHashValue(),
			                         htmltool::get_file_html(parentFileName).c_str(),
			                         htmltool::get_number_html(GetLineNo(loc)).c_str(), htmltool::get_include_html(includeToken).c_str());
		}

		return text;
	}

	// ��ȡ���ļ��ı�ֱ�Ӱ�����Ϣ���������ݰ��������ļ����������ļ�#include���кš������ļ�#include��ԭʼ�ı���
	string ParsingFile::DebugBeDirectIncludeText(FileID file) const
	{
		SourceLocation loc		= m_srcMgr->getIncludeLoc(file);
		string fileName			= GetFileName(file);
		string includeToken		= GetRawIncludeStr(file);

		if (includeToken.empty())
		{
			includeToken = "empty";
		}

		std::stringstream text;
		text << "{line = " << htmltool::get_number_html(GetLineNo(loc)) << " [" << htmltool::get_include_html(includeToken) << "]";
		text << "} -> ";
		text << "[" << htmltool::get_file_html(fileName) << "]";

		return text.str();
	}

	// ��ȡ��λ�������е���Ϣ�������е��ı��������ļ������к�
	string ParsingFile::DebugLocText(SourceLocation loc) const
	{
		string lineText = GetSourceOfLine(loc);
		std::stringstream text;
		text << "[" << htmltool::get_include_html(lineText) << "] in [";
		text << htmltool::get_file_html(GetAbsoluteFileName(GetFileID(loc)));
		text << "] line = " << htmltool::get_number_html(GetLineNo(loc));
		return text.str();
	}

	// ��ӡ���ü�¼
	void ParsingFile::PrintUse() const
	{
		int num = 0;
		for (auto use_list : m_uses)
		{
			FileID file = use_list.first;

			if (!CanClean(file))
			{
				continue;
			}

			++num;
		}

		HtmlDiv &div = HtmlLog::instance.m_newDiv;
		div.AddRow(AddPrintIdx() + ". list of use : use count = " + strtool::itoa(num), 1);

		for (auto use_list : m_uses)
		{
			FileID file = use_list.first;

			if (!CanClean(file))
			{
				continue;
			}

			div.AddRow("file = " + htmltool::get_file_html(GetAbsoluteFileName(file)), 2);

			for (FileID beuse : use_list.second)
			{
				div.AddRow("use = " + DebugBeIncludeText(beuse), 3);
			}

			div.AddRow("");
		}
	}

	// ��ӡ#include��¼
	void ParsingFile::PrintInclude() const
	{
		int num = 0;
		for (auto includeList : m_includes)
		{
			FileID file = includeList.first;

			if (!CanClean(file))
			{
				continue;
			}

			++num;
		}

		HtmlDiv &div = HtmlLog::instance.m_newDiv;
		div.AddRow(AddPrintIdx() + ". list of include : include count = " + htmltool::get_number_html(num), 1);

		for (auto includeList : m_includes)
		{
			FileID file = includeList.first;

			if (!CanClean(file))
			{
				continue;
			}

			div.AddRow("file = " + htmltool::get_file_html(GetAbsoluteFileName(file)), 2);

			for (FileID beinclude : includeList.second)
			{
				div.AddRow("include = " + DebugBeDirectIncludeText(beinclude), 3);
			}

			div.AddRow("");
		}
	}

	// ��ӡ�����������������������ȵļ�¼
	void ParsingFile::PrintUsedNames() const
	{
		int num = 0;
		for (auto useItr : m_useNames)
		{
			FileID file = useItr.first;

			if (!CanClean(file))
			{
				continue;
			}

			++num;
		}

		HtmlDiv &div = HtmlLog::instance.m_newDiv;
		div.AddRow(AddPrintIdx() + ". list of use name : use count = " + htmltool::get_number_html(num), 1);

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
		HtmlDiv &div = HtmlLog::instance.m_newDiv;
		div.AddRow("file = " + htmltool::get_file_html(GetAbsoluteFileName(file)), 2);

		for (const UseNameInfo &beuse : useNames)
		{
			div.AddRow("use = " + DebugBeIncludeText(beuse.file), 3);

			for (const string& name : beuse.nameVec)
			{
				std::stringstream linesStream;

				auto linesItr = beuse.nameMap.find(name);
				if (linesItr != beuse.nameMap.end())
				{
					for (int line : linesItr->second)
					{
						linesStream << htmltool::get_number_html(line) << ", ";
					}
				}

				std::string linesText = linesStream.str();

				strtool::try_strip_right(linesText, std::string(", "));

				div.AddRow("name = " + name, 4, 70);
				div.AddGrid("lines = " + linesText, 29);
			}

			div.AddRow("");
		}
	}

	// ��ӡ���ļ�ѭ�����õ����Ƽ�¼
	void ParsingFile::PrintRootCycleUsedNames() const
	{
		int num = 0;
		for (auto useItr : m_useNames)
		{
			FileID file = useItr.first;

			if (!CanClean(file) || !IsRootCyclyUsed(file))
			{
				continue;
			}

			++num;
		}

		HtmlDiv &div = HtmlLog::instance.m_newDiv;
		div.AddRow(AddPrintIdx() + ". list of root cycle use name : file count = " + strtool::itoa(num), 1);

		for (auto useItr : m_useNames)
		{
			FileID file									= useItr.first;
			const std::vector<UseNameInfo> &useNames	= useItr.second;

			if (!CanClean(file) || !IsRootCyclyUsed(file))
			{
				continue;
			}

			DebugUsedNames(file, useNames);
		}
	}

	// ��ӡѭ�����õ����Ƽ�¼
	void ParsingFile::PrintAllCycleUsedNames() const
	{
		HtmlDiv &div = HtmlLog::instance.m_newDiv;
		div.AddRow(AddPrintIdx() + ". list of all cycle use name : file count = " + htmltool::get_number_html(GetCanCleanFileCount()), 1);

		for (auto useItr : m_useNames)
		{
			FileID file									= useItr.first;
			const std::vector<UseNameInfo> &useNames	= useItr.second;

			if (!CanClean(file) || !IsCyclyUsed(file))
			{
				continue;
			}

			if (!IsRootCyclyUsed(file))
			{
				div.AddRow("<--------- new add cycle use file --------->", 2, 100, true);
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

	// ��ȡ���ڱ���Ŀ������������ļ���
	int ParsingFile::GetCanCleanFileCount() const
	{
		int num = 0;
		for (FileID file : m_allCycleUse)
		{
			if (!CanClean(file))
			{
				continue;
			}

			++num;
		}

		return num;
	}

	// ��ӡ���ļ�ѭ�����õ��ļ��б�
	void ParsingFile::PrintRootCycleUse() const
	{
		int num = 0;

		for (auto file : m_rootCycleUse)
		{
			if (!IsNeedPrintFile(file))
			{
				continue;
			}

			++num;
		}

		HtmlDiv &div = HtmlLog::instance.m_newDiv;
		div.AddRow(AddPrintIdx() + ". list of root cycle use : file count = " + htmltool::get_number_html(num), 1);

		for (auto file : m_rootCycleUse)
		{
			if (!IsNeedPrintFile(file))
			{
				continue;
			}

			div.AddRow("file = " + htmltool::get_file_html(GetFileName(file)), 2);
		}

		div.AddRow("");
	}

	// ��ӡѭ�����õ��ļ��б�
	void ParsingFile::PrintAllCycleUse() const
	{
		int num = 0;
		for (auto file : m_allCycleUse)
		{
			if (!IsNeedPrintFile(file))
			{
				continue;
			}

			++num;
		}

		HtmlDiv &div = HtmlLog::instance.m_newDiv;
		div.AddRow(AddPrintIdx() + ". list of all cycle use : file count = " + htmltool::get_number_html(num), 1);

		for (auto file : m_allCycleUse)
		{
			if (!IsNeedPrintFile(file))
			{
				continue;
			}

			if (!IsRootCyclyUsed(file))
			{
				div.AddRow("<--------- new add cycle use file --------->", 2, 100, true);
			}

			div.AddRow("file = " + htmltool::get_file_html(GetFileName(file)), 2);
		}

		div.AddRow("");
	}

	// ��ӡ���ļ���Ӧ�����ú����ļ���¼
	void ParsingFile::PrintCycleUsedChildren() const
	{
		int num = 0;
		for (auto usedItr : m_cycleUsedChildren)
		{
			FileID file = usedItr.first;

			if (!CanClean(file))
			{
				continue;
			}

			++num;
		}

		HtmlDiv &div = HtmlLog::instance.m_newDiv;
		div.AddRow(AddPrintIdx() + ". list of cycle used children file: file count = " + htmltool::get_number_html(num), 1);

		for (auto usedItr : m_cycleUsedChildren)
		{
			FileID file = usedItr.first;

			if (!CanClean(file))
			{
				continue;
			}

			div.AddRow("file = " + htmltool::get_file_html(GetAbsoluteFileName(file)), 2);

			for (auto usedChild : usedItr.second)
			{
				div.AddRow("use children " + DebugBeIncludeText(usedChild), 3);
			}

			div.AddRow("");
		}
	}

	// ��ӡ��������������ļ��б�
	void ParsingFile::PrintAllFile() const
	{
		int num = 0;
		for (FileID file : m_files)
		{
			if (!CanClean(file))
			{
				continue;
			}

			++num;
		}

		HtmlDiv &div = HtmlLog::instance.m_newDiv;
		div.AddRow(AddPrintIdx() + ". list of all file: file count = " + htmltool::get_number_html(num), 1);

		for (FileID file : m_files)
		{
			const string &name = GetAbsoluteFileName(file);

			if (!Project::instance.CanClean(name))
			{
				continue;
			}

			div.AddRow("file id = " + strtool::itoa(file.getHashValue()), 2, 20);
			div.AddGrid("filename = " + htmltool::get_file_html(name), 79);
		}

		div.AddRow("");
	}

	// ��ӡ�����ļ���������ʷ
	void ParsingFile::PrintFileHistory(const FileHistory &history)
	{
		if (!Project::instance.CanClean(history.m_filename))
		{
			return;
		}

		if (history.m_isSkip)
		{
			HtmlDiv &div = HtmlLog::instance.m_newDiv;

			// ��ӡ�����ļ�ΪԤ����ͷ�ļ�����ֹ���Ķ�
			div.AddRow(htmltool::get_warn_html(cn_file_skip), 1);
		}

		PrintUnusedIncludeOfFile(history);
		PrintCanForwarddeclOfFile(history);
		PrintCanReplaceOfFile(history);
	}

	// ��ӡ�����ļ��ڵĿɱ�ɾ#include��¼
	void ParsingFile::PrintUnusedIncludeOfFile(const FileHistory &history)
	{
		HtmlDiv &div = HtmlLog::instance.m_newDiv;
		if (history.m_unusedLines.empty())
		{
			return;
		}

		// ��ӡ��url���ļ���
		div.AddRow(strtool::get_text(cn_file_unused_count, htmltool::get_number_html(history.m_unusedLines.size()).c_str()), 2);

		for (auto unusedLineItr : history.m_unusedLines)
		{
			int line = unusedLineItr.first;

			UselessLine &unusedLine = unusedLineItr.second;

			div.AddRow(strtool::get_text(cn_file_unused_line, htmltool::get_number_html(line).c_str()), 3, 25);
			div.AddGrid(strtool::get_text(cn_file_unused_include, htmltool::get_include_html(unusedLine.m_text).c_str()), 74, true);

			for (auto itr : unusedLine.m_usingNamespace)
			{
				const std::string &ns_name = itr.first;
				const std::string &ns_decl = itr.second;

				div.AddRow(strtool::get_text(cn_file_add_using_namespace, htmltool::get_include_html(ns_decl).c_str()), 4);
				div.AddRow(strtool::get_text(cn_file_add_using_namespace, htmltool::get_include_html(ns_name).c_str()), 4);
			}
		}

		div.AddRow("");
	}

	// ��ӡ�����ļ��ڵĿ�����ǰ��������¼
	void ParsingFile::PrintCanForwarddeclOfFile(const FileHistory &history)
	{
		HtmlDiv &div = HtmlLog::instance.m_newDiv;
		if (history.m_forwards.empty())
		{
			return;
		}

		// ��ӡ��url���ļ���
		div.AddRow(strtool::get_text(cn_file_add_forward_num, htmltool::get_number_html(history.m_forwards.size()).c_str()), 2);

		for (auto forwardItr : history.m_forwards)
		{
			int line = forwardItr.first;

			ForwardLine &forwardLine = forwardItr.second;

			div.AddRow(strtool::get_text(cn_file_add_forward_line, htmltool::get_number_html(line).c_str(), htmltool::get_include_html(forwardLine.m_oldText).c_str()), 3);
			// div.AddGrid(strtool::get_text(cn_file_add_forward_old_text, ), 69, true);

			for (const string &name : forwardLine.m_classes)
			{
				div.AddRow(strtool::get_text(cn_file_add_forward_new_text, htmltool::get_include_html(name).c_str()), 4);
			}

			div.AddRow("");
		}
	}

	// ��ӡ�����ļ��ڵĿɱ��滻#include��¼
	void ParsingFile::PrintCanReplaceOfFile(const FileHistory &history)
	{
		HtmlDiv &div = HtmlLog::instance.m_newDiv;
		if (history.m_replaces.empty())
		{
			return;
		}

		// ��ӡ��url���ļ���
		div.AddRow(strtool::get_text(cn_file_can_replace_num, htmltool::get_number_html(history.m_replaces.size()).c_str()), 2);

		for (auto replaceItr : history.m_replaces)
		{
			int line = replaceItr.first;

			ReplaceLine &replaceLine = replaceItr.second;

			// ���磬���: [line = 1] -> {old text = [#include "a.h"]}

			std::string oldText = strtool::get_text(cn_file_can_replace_line, htmltool::get_number_html(line).c_str(), htmltool::get_include_html(replaceLine.m_oldText).c_str());
			if (replaceLine.m_isSkip)
			{
				oldText += cn_file_force_include_text;
			}

			div.AddRow(oldText.c_str(), 3);
			// div.AddGrid(oldText, 69);

			// ����滻�б����磬���: replace to = #include <../1.h> in [./2.h : line = 3]
			{
				const ReplaceTo &replaceInfo = replaceLine.m_newInclude;

				// ���滻�Ĵ����ݲ��䣬��ֻ��ӡһ��
				if (replaceInfo.m_newText == replaceInfo.m_oldText)
				{
					div.AddRow(strtool::get_text(cn_file_replace_same_text, htmltool::get_include_html(replaceInfo.m_oldText).c_str()), 4, 40, true);
				}
				// ���򣬴�ӡ�滻ǰ���滻���#include����
				else
				{
					div.AddRow(strtool::get_text(cn_file_replace_old_text, htmltool::get_include_html(replaceInfo.m_oldText).c_str()), 4, 40, true);
					div.AddGrid(strtool::get_text(cn_file_replace_new_text, htmltool::get_include_html(replaceInfo.m_newText).c_str()), 59);
				}

				// ����β���[in �������ļ� : line = xx]
				div.AddRow(strtool::get_text(cn_file_replace_in_file, htmltool::get_file_html(replaceInfo.m_inFile).c_str(), htmltool::get_number_html(replaceInfo.m_line).c_str()), 5);
			}

			for (auto itr : replaceLine.m_frontNamespace)
			{
				const std::string &ns_name = itr.first;
				const std::string &ns_decl = itr.second;

				div.AddRow(strtool::get_text(cn_file_add_front_using_ns, htmltool::get_include_html(ns_decl).c_str()), 5);
				div.AddRow(strtool::get_text(cn_file_add_front_using_ns, htmltool::get_include_html(ns_name).c_str()), 5);
			}

			for (auto itr : replaceLine.m_backNamespace)
			{
				const std::string &ns_name = itr.first;
				const std::string &ns_decl = itr.second;

				div.AddRow(strtool::get_text(cn_file_add_back_using_ns, htmltool::get_include_html(ns_decl).c_str()), 5);
				div.AddRow(strtool::get_text(cn_file_add_back_using_ns, htmltool::get_include_html(ns_name).c_str()), 5);
			}

			div.AddRow("");
		}
	}

	// ��ӡ���ļ������ı������
	void ParsingFile::PrintCompileError(const CompileErrorHistory &errHistory)
	{
		if (errHistory.m_errNum <= 0)
		{
			return;
		}

		HtmlDiv &div = HtmlLog::instance.m_newDiv;
		div.AddRow(" ", 1, 100, false, true);

		div.AddRow(cn_error, 1, 100, false, true);

		for (const std::string& errTip : errHistory.m_errTips)
		{
			div.AddRow(errTip, 2, 100, true, true);
			div.AddRow(" ", 1, 100, false, true);
		}

		// div.AddRow(" ", 1, 100, false, true);

		if (!errHistory.m_fatalErrors.empty())
		{
			std::string errTexts;

			int i		= 0;
			int size	= errHistory.m_fatalErrors.size();

			for (int err : errHistory.m_fatalErrors)
			{
				errTexts += htmltool::get_number_html(err);

				if (++i < size)
				{
					errTexts += ",";
				}
			}

			std::string tip = get_text(cn_error_fatal, errTexts.c_str());
			div.AddRow(tip, 1, 100, true, true);
		}

		if (errHistory.m_hasTooManyError)
		{
			std::string tip = get_text(cn_error_too_many, htmltool::get_number_html(errHistory.m_errNum).c_str());
			div.AddRow(tip, 1, 100, true, true);
		}
		else if (errHistory.m_fatalErrors.empty())
		{
			if (errHistory.m_errNum > 0)
			{
				std::string tip = get_text(cn_error_ignore, htmltool::get_number_html(errHistory.m_errNum).c_str());
				div.AddRow(tip, 1, 100, true, true);
			}
		}

		div.AddRow("");
	}

	// ��ӡ������־
	void ParsingFile::PrintCleanLog() const
	{
		HtmlDiv &div = HtmlLog::instance.m_newDiv;

		PrintCompileError(m_compileErrorHistory);

		FileHistoryMap files;
		TakeAllInfoTo(files);

		int i = 0;

		for (auto itr : files)
		{
			const FileHistory &history = itr.second;
			if (!history.IsNeedClean())
			{
				continue;
			}

			if (Project::instance.m_verboseLvl < VerboseLvl_2)
			{
				string ext = strtool::get_ext(history.m_filename);
				if (!cpptool::is_cpp(ext))
				{
					continue;
				}
			}

			const char *tip = (history.m_compileErrorHistory.HaveFatalError() ? cn_file_history_compile_error : cn_file_history);

			div.AddRow(strtool::get_text(tip, htmltool::get_number_html(++i).c_str(), htmltool::get_file_html(history.m_filename).c_str()), 1);

			PrintFileHistory(history);
		}
	}

	// ��ӡ���ļ��ڵ������ռ�
	void ParsingFile::PrintNamespace() const
	{
		int num = 0;
		for (auto itr : m_namespaces)
		{
			FileID file = itr.first;

			if (!IsNeedPrintFile(file))
			{
				continue;
			}

			++num;
		}

		HtmlDiv &div = HtmlLog::instance.m_newDiv;
		div.AddRow(AddPrintIdx() + ". each file's namespace: file count = " + htmltool::get_number_html(num), 1);

		for (auto itr : m_namespaces)
		{
			FileID file = itr.first;

			if (!IsNeedPrintFile(file))
			{
				continue;
			}

			div.AddRow("file = " + htmltool::get_file_html(GetAbsoluteFileName(file)), 2);

			std::set<std::string> &namespaces = itr.second;

			for (const std::string &ns : namespaces)
			{
				div.AddRow("declare namespace = " + htmltool::get_include_html(ns), 3);
			}

			div.AddRow("");
		}
	}

	// ��ӡ���ļ��ڵ�using namespace
	void ParsingFile::PrintUsingNamespace() const
	{
		int num = 0;
		for (auto itr : m_usingNamespaces)
		{
			FileID file = itr.first;

			if (!IsNeedPrintFile(file))
			{
				continue;
			}

			++num;
		}

		HtmlDiv &div = HtmlLog::instance.m_newDiv;
		div.AddRow(AddPrintIdx() + ". each file's using namespace: file count = " + htmltool::get_number_html(num), 1);

		for (auto itr : m_usingNamespaces)
		{
			FileID file = itr.first;

			if (!IsNeedPrintFile(file))
			{
				continue;
			}

			div.AddRow("file = " + htmltool::get_file_html(GetAbsoluteFileName(file)), 2);

			std::set<std::string> &namespaces = itr.second;

			for (const std::string &ns : namespaces)
			{
				div.AddRow("using namespace = " + htmltool::get_include_html(ns), 3);
			}

			div.AddRow("");
		}
	}

	// ��ӡ���ļ���Ӧ������using namespace
	void ParsingFile::PrintUsefulUsingNamespace() const
	{
		std::map<FileID, std::set<std::string>>	remainNamespaces;
		for (auto itr : m_usefulUsingNamespaces)
		{
			SourceLocation loc		= itr.first;
			const NamespaceInfo &ns	= itr.second;

			remainNamespaces[GetFileID(loc)].insert(ns.ns_name);
		}

		int num = 0;
		for (auto itr : remainNamespaces)
		{
			FileID file = itr.first;

			if (!IsNeedPrintFile(file))
			{
				continue;
			}

			++num;
		}

		HtmlDiv &div = HtmlLog::instance.m_newDiv;
		div.AddRow(AddPrintIdx() + ". each file's useful using namespace: file count = " + htmltool::get_number_html(num), 1);

		for (auto itr : remainNamespaces)
		{
			FileID file = itr.first;

			if (!IsNeedPrintFile(file))
			{
				continue;
			}

			div.AddRow("file = " + htmltool::get_file_html(GetAbsoluteFileName(file)), 2);

			std::set<std::string> &namespaces = itr.second;

			for (const std::string &ns : namespaces)
			{
				div.AddRow("remain using namespace = " + htmltool::get_include_html(ns), 3);
			}

			div.AddRow("");
		}
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

	// ��ȡƴдλ��
	inline SourceLocation ParsingFile::GetSpellingLoc(SourceLocation loc) const
	{
		return m_srcMgr->getSpellingLoc(loc);
	}

	// ��ȡ��������չ���λ��
	inline SourceLocation ParsingFile::GetExpasionLoc(SourceLocation loc) const
	{
		return m_srcMgr->getExpansionLoc(loc);
	}

	// ��ȡ�ļ�ID
	inline FileID ParsingFile::GetFileID(SourceLocation loc) const
	{
		FileID fileID = m_srcMgr->getFileID(loc);
		if (fileID.isInvalid())
		{
			fileID = m_srcMgr->getFileID(GetSpellingLoc(loc));

			if (fileID.isInvalid())
			{
				fileID = m_srcMgr->getFileID(GetExpasionLoc(loc));
			}
		}

		return fileID;
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

		string include2;

		// �����ж�2���ļ��Ƿ���ͬһ�ļ�����
		if (get_dir(absolutePath1) == get_dir(absolutePath2))
		{
			include2 = "\"" + strip_dir(absolutePath2) + "\"";
		}
		else
		{
			// ��ͷ�ļ�����·������Ѱ��2���ļ������ɹ��ҵ����򷵻ػ���ͷ�ļ�����·�������·��
			include2 = GetQuotedIncludeStr(absolutePath2);

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
			// ��������c++�ļ�
			CleanAllFile();
		}
		else
		{
			// ������ǰcpp�ļ�
			CleanMainFile();
		}

		// ����c++Դ�ļ�
		if (Project::instance.m_isOverWrite)
		{
			// ������������ѡ��ʱ���Ž��䶯��д��c++�ļ�
			bool err = Overwrite();
			if (err)
			{
				llvm::errs() << "[Error]overwrite some changed files failed!\n";
			}
		}
	}

	// ����������д��c++Դ�ļ������أ�true��д�ļ�ʱ�������� false��д�ɹ�
	// �����ӿڿ�����Rewriter::overwriteChangedFiles��Ψһ�Ĳ�ͬ�ǻ�д�ɹ�ʱ��ɾ���ļ����棩
	bool ParsingFile::Overwrite()
	{
		// ����������Ǵ�clang\lib\Rewrite\Rewriter.cpp�ļ�����������
		// A wrapper for a file stream that atomically overwrites the target.
		//
		// Creates a file output stream for a temporary file in the constructor,
		// which is later accessible via getStream() if ok() return true.
		// Flushes the stream and moves the temporary file to the target location
		// in the destructor.
		class AtomicallyMovedFile
		{
		public:
			AtomicallyMovedFile(DiagnosticsEngine &Diagnostics, StringRef Filename,
			                    bool &AllWritten)
				: Diagnostics(Diagnostics), Filename(Filename), AllWritten(AllWritten)
			{
				TempFilename = Filename;
				TempFilename += "-%%%%%%%%";
				int FD;
				if (llvm::sys::fs::createUniqueFile(TempFilename, FD, TempFilename))
				{
					AllWritten = false;
					Diagnostics.Report(clang::diag::err_unable_to_make_temp)
					        << TempFilename;
				}
				else
				{
					FileStream.reset(new llvm::raw_fd_ostream(FD, /*shouldClose=*/true));
				}
			}

			~AtomicallyMovedFile()
			{
				if (!ok()) return;

				// Close (will also flush) theFileStream.
				FileStream->close();
				if (std::error_code ec = llvm::sys::fs::rename(TempFilename, Filename))
				{
					AllWritten = false;
					Diagnostics.Report(clang::diag::err_unable_to_rename_temp)
					        << TempFilename << Filename << ec.message();
					// If the remove fails, there's not a lot we can do - this is already an
					// error.
					llvm::sys::fs::remove(TempFilename);
				}
			}

			bool ok() { return (bool)FileStream; }
			raw_ostream &getStream() { return *FileStream; }

		private:
			DiagnosticsEngine &Diagnostics;
			StringRef Filename;
			SmallString<128> TempFilename;
			std::unique_ptr<llvm::raw_fd_ostream> FileStream;
			bool &AllWritten;
		};

		SourceManager &srcMgr	= m_rewriter->getSourceMgr();
		FileManager &fileMgr	= srcMgr.getFileManager();

		bool AllWritten = true;
		for (Rewriter::buffer_iterator I = m_rewriter->buffer_begin(), E = m_rewriter->buffer_end(); I != E; ++I)
		{
			const FileEntry *Entry				= srcMgr.getFileEntryForID(I->first);
			const RewriteBuffer &rewriteBuffer	= I->second;

			AtomicallyMovedFile File(srcMgr.getDiagnostics(), Entry->getName(), AllWritten);
			if (File.ok())
			{
				rewriteBuffer.write(File.getStream());
				fileMgr.modifyFileEntry(const_cast<FileEntry*>(Entry), rewriteBuffer.size(), Entry->getModificationTime());
			}

			//fileMgr.invalidateCache(Entry);
		}

		return !AllWritten;
	}

	// �滻ָ����Χ�ı�
	void ParsingFile::ReplaceText(FileID file, int beg, int end, string text)
	{
		SourceLocation fileBegLoc	= m_srcMgr->getLocForStartOfFile(file);
		SourceLocation begLoc		= fileBegLoc.getLocWithOffset(beg);
		SourceLocation endLoc		= fileBegLoc.getLocWithOffset(end);

		if (nullptr == GetSourceAtLoc(begLoc) || nullptr == GetSourceAtLoc(endLoc))
		{
			llvm::errs() << "[error][ParsingFile::RemoveText] nullptr == GetSourceAtLoc(begLoc) || nullptr == GetSourceAtLoc(endLoc)\n";
			return;
		}

		SourceRange range(begLoc, endLoc);

		// cxx::log() << "\n------->replace text = [" << get_source_of_range(range) << "] in [" << get_absolute_file_name(file) << "]\n";

		m_rewriter->ReplaceText(begLoc, end - beg, text);
	}

	// ���ı����뵽ָ��λ��֮ǰ
	// ���磺������"abcdefg"�ı�������c������123�Ľ�����ǣ�"ab123cdefg"
	void ParsingFile::InsertText(FileID file, int loc, string text)
	{
		SourceLocation fileBegLoc	= m_srcMgr->getLocForStartOfFile(file);
		SourceLocation insertLoc	= fileBegLoc.getLocWithOffset(loc);

		if (nullptr == GetSourceAtLoc(insertLoc))
		{
			llvm::errs() << "[error][ParsingFile::RemoveText] nullptr == GetSourceAtLoc(insertLoc)\n";
			return;
		}

		m_rewriter->InsertText(insertLoc, text, true, true);
	}

	// �Ƴ�ָ����Χ�ı������Ƴ��ı�����б�Ϊ���У��򽫸ÿ���һ���Ƴ�
	void ParsingFile::RemoveText(FileID file, int beg, int end)
	{
		SourceLocation fileBegLoc	= m_srcMgr->getLocForStartOfFile(file);
		if (fileBegLoc.isInvalid())
		{
			llvm::errs() << "[error][ParsingFile::RemoveText] if (fileBegLoc.isInvalid()), remove text in [" << GetAbsoluteFileName(file) << "] failed!\n";
			return;
		}

		SourceLocation begLoc		= fileBegLoc.getLocWithOffset(beg);
		SourceLocation endLoc		= fileBegLoc.getLocWithOffset(end);

		if (nullptr == GetSourceAtLoc(begLoc) || nullptr == GetSourceAtLoc(endLoc))
		{
			llvm::errs() << "[error][ParsingFile::RemoveText] nullptr == GetSourceAtLoc(begLoc) || nullptr == GetSourceAtLoc(endLoc)\n";
			return;
		}

		SourceRange range(begLoc, endLoc);

		Rewriter::RewriteOptions rewriteOption;
		rewriteOption.IncludeInsertsAtBeginOfRange	= false;
		rewriteOption.IncludeInsertsAtEndOfRange	= false;
		rewriteOption.RemoveLineIfEmpty				= false;

		// cxx::log() << "\n------->remove text = [" << get_source_of_range(range) << "] in [" << get_absolute_file_name(file) << "]\n";

		bool err = m_rewriter->RemoveText(range.getBegin(), end - beg, rewriteOption);
		if (err)
		{
			llvm::errs() << "[error][ParsingFile::RemoveText]: remove text = [" << GetSourceOfRange(range) << "] in [" << GetAbsoluteFileName(file) << "] failed!\n";
		}
	}

	// �Ƴ�ָ���ļ��ڵ�����#include
	void ParsingFile::CleanByUnusedLine(const FileHistory &history, FileID file)
	{
		if (history.m_unusedLines.empty())
		{
			return;
		}

		for (auto unusedLineItr : history.m_unusedLines)
		{
			UselessLine &unusedLine	= unusedLineItr.second;

			RemoveText(file, unusedLine.m_beg, unusedLine.m_end);

			for (auto itr : unusedLine.m_usingNamespace)
			{
				const std::string &ns_name = itr.first;
				const std::string &ns_decl = itr.second;

				InsertText(file, unusedLine.m_beg, ns_decl + history.GetNewLineWord());
				InsertText(file, unusedLine.m_beg, ns_name + history.GetNewLineWord());
			}
		}
	}

	// ��ָ���ļ������ǰ������
	void ParsingFile::CleanByForward(const FileHistory &history, FileID file)
	{
		if (history.m_forwards.empty())
		{
			return;
		}

		for (auto forwardItr : history.m_forwards)
		{
			const ForwardLine &forwardLine	= forwardItr.second;

			std::stringstream text;

			for (const string &cxxRecord : forwardLine.m_classes)
			{
				text << cxxRecord;
				text << history.GetNewLineWord();
			}

			InsertText(file, forwardLine.m_offsetAtFile, text.str());
		}
	}

	// ��ָ���ļ����滻#include
	void ParsingFile::CleanByReplace(const FileHistory &history, FileID file)
	{
		if (history.m_replaces.empty())
		{
			return;
		}

		for (auto replaceItr : history.m_replaces)
		{
			const ReplaceLine &replaceLine	= replaceItr.second;

			// ���Ǳ�-include����ǿ�����룬����������Ϊ�滻��û��Ч��
			if (replaceLine.m_isSkip)
			{
				continue;
			}

			const char* newLineWord = history.GetNewLineWord();

			// 1. ���滻#include
			{
				std::stringstream text;

				const ReplaceTo &replaceInfo = replaceLine.m_newInclude;
				text << replaceInfo.m_newText;
				text << newLineWord;

				ReplaceText(file, replaceLine.m_beg, replaceLine.m_end, text.str());
			}

			// 2. ��ԭ����#includeǰ���namespace������using namespace
			{
				std::stringstream text;

				for (auto itr : replaceLine.m_frontNamespace)
				{
					const std::string &ns_name = itr.first;
					const std::string &ns_decl = itr.second;

					text << ns_decl << newLineWord << ns_name << newLineWord;
				}

				InsertText(file, replaceLine.m_beg, text.str());
			}

			// 3. ��ԭ����#include�����namespace������using namespace
			{
				std::stringstream text;

				for (auto itr : replaceLine.m_backNamespace)
				{
					const std::string &ns_name = itr.first;
					const std::string &ns_decl = itr.second;

					text << ns_decl << newLineWord << ns_name << newLineWord;
				}

				InsertText(file, replaceLine.m_end, text.str());
			}
		}
	}

	// ������ʷ����ָ���ļ�
	void ParsingFile::CleanByHistory(const FileHistoryMap &historys)
	{
		std::map<std::string, FileID> nowFiles;

		// ������ǰcpp���ļ������ļ�FileID��mapӳ�䣨ע�⣺ͬһ���ļ����ܱ�������Σ�FileID�ǲ�һ���ģ�����ֻ������С��FileID��
		{
			for (FileID file : m_files)
			{
				const string &name = GetAbsoluteFileName(file);

				if (!Project::instance.CanClean(name))
				{
					continue;
				}

				if (nowFiles.find(name) == nowFiles.end())
				{
					nowFiles.insert(std::make_pair(name, file));
				}
			}
		}

		for (auto itr : historys)
		{
			const string &fileName		= itr.first;
			const FileHistory &history	= itr.second;

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

			if (history.m_isSkip || history.HaveFatalError())
			{
				continue;
			}

			// ���������ڵ�ǰcpp���ļ����ҵ���Ӧ���ļ�ID��ע�⣺ͬһ���ļ����ܱ�������Σ�FileID�ǲ�һ���ģ�����ȡ����������С��FileID��
			auto findItr = nowFiles.find(fileName);
			if (findItr == nowFiles.end())
			{
				// llvm::errs() << "[error][ParsingFile::CleanBy] if (findItr == allFiles.end()) filename = " << fileName << "\n";
				continue;
			}

			FileID file	= findItr->second;

			CleanByReplace(history, file);
			CleanByForward(history, file);
			CleanByUnusedLine(history, file);

			ProjectHistory::instance.OnCleaned(fileName);
		}
	}

	// ���������б�Ҫ������ļ�
	void ParsingFile::CleanAllFile()
	{
		CleanByHistory(ProjectHistory::instance.m_files);
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

		CleanByHistory(root);
	}

	// ��ӡͷ�ļ�����·��
	void ParsingFile::PrintHeaderSearchPath() const
	{
		if (m_headerSearchPaths.empty())
		{
			return;
		}

		HtmlDiv &div = HtmlLog::instance.m_newDiv;
		div.AddRow(AddPrintIdx() + ". header search path list : path count = " + htmltool::get_number_html(m_headerSearchPaths.size()), 1);

		for (const HeaderSearchDir &path : m_headerSearchPaths)
		{
			div.AddRow("search path = " + htmltool::get_file_html(path.m_dir), 2);
		}

		div.AddRow("");
	}

	// ���ڵ��ԣ���ӡ���ļ����õ��ļ�������ڸ��ļ���#include�ı�
	void ParsingFile::PrintRelativeInclude() const
	{
		int num = 0;
		for (auto itr : m_uses)
		{
			FileID file = itr.first;

			if (!CanClean(file))
			{
				continue;
			}

			++num;
		}

		HtmlDiv &div = HtmlLog::instance.m_newDiv;
		div.AddRow(AddPrintIdx() + ". relative include list : use = " + htmltool::get_number_html(num), 1);

		for (auto itr : m_uses)
		{
			FileID file = itr.first;

			if (!CanClean(file))
			{
				continue;
			}

			std::set<FileID> &be_uses = itr.second;

			div.AddRow("file = " + htmltool::get_file_html(GetAbsoluteFileName(file)), 2);

			for (FileID be_used_file : be_uses)
			{
				div.AddRow("old include = " + htmltool::get_include_html(GetRawIncludeStr(be_used_file)), 3, 45);
				div.AddGrid("-> relative include = " + htmltool::get_include_html(GetRelativeIncludeStr(file, be_used_file)), 54);
			}

			div.AddRow("");
		}
	}

	// ���ڵ��Ը��٣���ӡ�Ƿ����ļ��ı�����������©
	void ParsingFile::PrintNotFoundIncludeLocForDebug()
	{
		HtmlDiv &div = HtmlLog::instance.m_newDiv;
		div.AddRow(AddPrintIdx() + ". not found include loc:", 1);

		for (auto itr : m_uses)
		{
			const std::set<FileID> &beuse_files = itr.second;

			for (FileID beuse_file : beuse_files)
			{
				if (beuse_file == m_srcMgr->getMainFileID())
				{
					continue;
				}

				SourceLocation loc = m_srcMgr->getIncludeLoc(beuse_file);
				if (m_includeLocs.find(loc) == m_includeLocs.end())
				{
					div.AddRow("not found = " + htmltool::get_file_html(GetAbsoluteFileName(beuse_file)), 2);
					div.AddRow("old text = " + DebugLocText(loc), 2);
					continue;
				}
			}
		}

		div.AddRow("");
	}

	// ��ӡ���� + 1
	std::string ParsingFile::AddPrintIdx() const
	{
		return strtool::itoa(++m_printIdx);
	}

	// ��ӡ��Ϣ
	void ParsingFile::Print()
	{
		int verbose = Project::instance.m_verboseLvl;
		if (verbose <= VerboseLvl_0)
		{
			return;
		}

		HtmlDiv &div = HtmlLog::instance.m_newDiv;

		std::string rootFileName	= GetAbsoluteFileName(m_srcMgr->getMainFileID());
		div.AddTitle(strtool::get_text(cn_file_history_title,
		                               htmltool::get_number_html(ProjectHistory::instance.g_printFileNo).c_str(),
		                               htmltool::get_number_html(Project::instance.m_cpps.size()).c_str(),
		                               htmltool::get_file_html(rootFileName).c_str()));

		m_printIdx = 0;

		if (verbose >= VerboseLvl_1)
		{
			PrintCleanLog();
		}

		if (verbose >= VerboseLvl_3)
		{
			PrintUsedNames();
			PrintUse();
			PrintNewUse();

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
			PrintParent();
			PrintChildren();
			PrintNamespace();
			PrintUsingNamespace();
			PrintUsefulUsingNamespace();
		}

		if (verbose >= VerboseLvl_5)
		{
			PrintNotFoundIncludeLocForDebug();
		}

		HtmlLog::instance.AddDiv(div);
	}

	// �ϲ��ɱ��Ƴ���#include��
	void ParsingFile::MergeUnusedLineTo(const FileHistory &newFile, FileHistory &oldFile) const
	{
		FileHistory::UnusedLineMap &oldLines = oldFile.m_unusedLines;

		for (FileHistory::UnusedLineMap::iterator oldLineItr = oldLines.begin(), end = oldLines.end(); oldLineItr != end; )
		{
			int line = oldLineItr->first;
			UselessLine &unusedLine = oldLineItr->second;

			if (newFile.IsLineUnused(line))
			{
				auto newLineItr = newFile.m_unusedLines.find(line);
				const UselessLine &newUnusedLine = newLineItr->second;

				unusedLine.m_usingNamespace.insert(newUnusedLine.m_usingNamespace.begin(), newUnusedLine.m_usingNamespace.end());
				++oldLineItr;
			}
			else
			{
				// �������ļ��и���Ӧ���滻�����ھ��ļ��и���Ӧ��ɾ�������ھ��ļ��������滻��¼
				if (newFile.IsLineBeReplaced(line))
				{
					auto newReplaceLineItr = newFile.m_replaces.find(line);
					const ReplaceLine &newReplaceLine = newReplaceLineItr->second;

					oldFile.m_replaces[line] = newReplaceLine;
				}

				// cxx::log() << oldFile.m_filename << ": conflict at line " << line << " -> " << oldLineItr->second.m_text << "\n";
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

				// ��2���滻һ������
				if (newLine.m_newInclude.m_newText == oldLine.m_newInclude.m_newText)
				{
					++oldLineItr;
					continue;
				}

				// ˵�����ļ�δ�������κ��滻��¼����ʱӦ�����ļ������о��滻��¼ȫ��ɾ��
				if (newFileID.isInvalid())
				{
					// cxx::log() << "error, merge_replace_line_to not found = " << oldFile.m_filename << "\n";
					// oldFile.m_replaces.clear();
					++oldLineItr;
					continue;
				}

				const ChildrenReplaceMap &newReplaceMap	= newFileItr->second;

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
				}
				// ���µ�ȡ���ļ��Ǿɵ�ȡ���ļ������ȣ����Ϊʹ���µ��滻��Ϣ
				else if(IsAncestor(oldLine.m_oldFile.c_str(), beReplaceFileID))
				{
					oldLine.m_newInclude = newLine.m_newInclude;
					oldLine.m_frontNamespace.insert(newLine.m_frontNamespace.begin(), newLine.m_frontNamespace.end());
					oldLine.m_backNamespace.insert(newLine.m_backNamespace.begin(), newLine.m_backNamespace.end());
					++oldLineItr;
				}
				// ������û��ֱϵ��ϵ��������޷����滻��ɾ������ԭ�е��滻��¼
				else
				{
					// cxx::log() << "merge_replace_line_to: " << oldFile.m_filename << " should remove conflict old line = " << line << " -> " << oldLine.m_oldText << "\n";
					oldFile.m_replaces.erase(oldLineItr++);
				}
			}
			else
			{
				// �������ļ��и���Ӧ��ɾ�������ھ��ļ��и���Ӧ���滻���������ļ����滻��¼
				if (newFile.IsLineUnused(line))
				{
					++oldLineItr;
					continue;
				}
				// ������û���µ��滻��¼��˵�������޷����滻��ɾ�����оɵ��滻��¼
				else
				{
					oldFile.m_replaces.erase(oldLineItr++);
				}
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

		// �����ļ������ر������������������࣬����ϲ����������ʷ�����ڴ�ӡ
		if (m_compileErrorHistory.HaveFatalError())
		{
			std::string rootFile	= GetAbsoluteFileName(m_srcMgr->getMainFileID());
			oldFiles[rootFile]		= newFiles[rootFile];
			return;
		}

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

		// MergeCountTo(oldFiles);
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
			const char* c1	= GetSourceAtLoc(firstLineEnd);
			const char* c2	= GetSourceAtLoc(firstLineEnd.getLocWithOffset(1));

			// ˵����һ��û�л��з������ߵ�һ�����Ľ�����ֻ����һ��\r��\n�ַ�
			if (nullptr == c1 || nullptr == c2)
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
			eachFile.m_isSkip			= IsPrecompileHeader(file);
		}

		// 1. ����������а��ļ����д��
		TakeUnusedLineByFile(out);

		// 2. ��������ǰ���������ļ����д��
		TakeNewForwarddeclByFile(out);

		// 3. ���ļ��ڵ�#include�滻���ļ����д��
		TakeReplaceByFile(out);

		// 4. ȡ�����ļ��ı��������ʷ
		TakeCompileErrorHistory(out);
	}

	// ����������а��ļ����д��
	void ParsingFile::TakeUnusedLineByFile(FileHistoryMap &out) const
	{
		for (SourceLocation loc : m_unusedLocs)
		{
			PresumedLoc presumedLoc = m_srcMgr->getPresumedLoc(loc);
			if (presumedLoc.isInvalid())
			{
				cxx::log() << "------->error: take_unused_line_by_file getPresumedLoc failed\n";
				continue;
			}

			string fileName			= pathtool::get_absolute_path(presumedLoc.getFilename());
			if (!Project::instance.CanClean(fileName))
			{
				// cxx::log() << "------->error: !Vsproject::instance.has_file(fileName) : " << fileName << "\n";
				continue;
			}

			if (out.find(fileName) == out.end())
			{
				continue;
			}

			SourceRange lineRange	= GetCurLine(loc);
			SourceRange nextLine	= GetNextLine(loc);
			int line				= presumedLoc.getLine();

			FileHistory &eachFile	= out[fileName];
			UselessLine &unusedLine	= eachFile.m_unusedLines[line];

			unusedLine.m_beg	= m_srcMgr->getFileOffset(lineRange.getBegin());
			unusedLine.m_end	= m_srcMgr->getFileOffset(nextLine.getBegin());
			unusedLine.m_text	= GetSourceOfRange(lineRange);

			{
				GetMissingNamespace(loc, unusedLine.m_usingNamespace);
			}
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

				PresumedLoc insertPresumedLoc = m_srcMgr->getPresumedLoc(insertLoc);
				if (insertPresumedLoc.isInvalid())
				{
					cxx::log() << "------->error: take_new_forwarddecl_by_file getPresumedLoc failed\n";
					continue;
				}

				string insertFileName			= pathtool::get_absolute_path(insertPresumedLoc.getFilename());

				if (!Project::instance.CanClean(insertFileName))
				{
					continue;
				}

				// ��ʼȡ������
				{
					int line					= insertPresumedLoc.getLine();
					const string cxxRecordName	= GetRecordName(*cxxRecord);
					FileHistory &eachFile		= out[insertFileName];
					ForwardLine &forwardLine	= eachFile.m_forwards[line];

					forwardLine.m_offsetAtFile	= m_srcMgr->getFileOffset(insertLoc);
					forwardLine.m_oldText		= GetSourceOfLine(insertLoc);
					forwardLine.m_classes.insert(cxxRecordName);

					{
						SourceLocation fileStart = m_srcMgr->getLocForStartOfFile(GetFileID(insertLoc));
						if (fileStart.getLocWithOffset(forwardLine.m_offsetAtFile) != insertLoc)
						{
							cxx::log() << "error: fileStart.getLocWithOffset(forwardLine.m_offsetAtFile) != insertLoc \n";
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
			FileID file			= itr.first;
			FileID to_replaces	= itr.second;

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
		FileID parent = GetFileID(m_srcMgr->getIncludeLoc(file));
		return (m_srcMgr->getFileEntryForID(parent) == nullptr);
	}

	// ���ļ��Ƿ���Ԥ����ͷ�ļ�
	bool ParsingFile::IsPrecompileHeader(FileID file) const
	{
		std::string filePath = GetAbsoluteFileName(file);

		std::string fileName = pathtool::get_file_name(filePath);

		bool is = !strnicmp(fileName.c_str(), "stdafx.h", fileName.size());
		is |= !strnicmp(fileName.c_str(), "stdafx.cpp", fileName.size());
		return is;
	}

	// ȡ��ָ���ļ���#include�滻��Ϣ
	void ParsingFile::TakeBeReplaceOfFile(FileHistory &history, FileID top, const ChildrenReplaceMap &childernReplaces) const
	{
		for (auto itr : childernReplaces)
		{
			FileID oldFile		= itr.first;
			FileID replaceFile	= itr.second;

			// ȡ������#include���滻��Ϣ[�к� -> ���滻�ɵ�#include�б�]
			{
				bool isBeForceIncluded		= IsForceIncluded(oldFile);

				SourceLocation include_loc	= m_srcMgr->getIncludeLoc(oldFile);
				SourceRange	lineRange		= GetCurLineWithLinefeed(include_loc);
				int line					= (isBeForceIncluded ? 0 : GetLineNo(include_loc));

				ReplaceLine &replaceLine	= history.m_replaces[line];
				replaceLine.m_oldFile		= GetAbsoluteFileName(oldFile);
				replaceLine.m_oldText		= GetRawIncludeStr(oldFile);
				replaceLine.m_beg			= m_srcMgr->getFileOffset(lineRange.getBegin());
				replaceLine.m_end			= m_srcMgr->getFileOffset(lineRange.getEnd());
				replaceLine.m_isSkip		= isBeForceIncluded || IsPrecompileHeader(oldFile);	// �����Ƿ���ǿ�ư���

				{
					SourceLocation deep_include_loc	= m_srcMgr->getIncludeLoc(replaceFile);

					ReplaceTo replaceTo;

					// ��¼[��#include����#include]
					replaceTo.m_oldText	= GetRawIncludeStr(replaceFile);
					replaceTo.m_newText	= GetRelativeIncludeStr(GetParent(oldFile), replaceFile);

					// ��¼[�������ļ��������к�]
					replaceTo.m_line		= GetLineNo(deep_include_loc);
					replaceTo.m_fileName	= GetAbsoluteFileName(replaceFile);
					replaceTo.m_inFile	= GetAbsoluteFileName(GetFileID(deep_include_loc));

					replaceLine.m_newInclude = replaceTo;

					GetMissingNamespace(include_loc, deep_include_loc, replaceLine.m_frontNamespace, replaceLine.m_backNamespace);
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

	// ȡ�����ļ��ı��������ʷ
	void ParsingFile::TakeCompileErrorHistory(FileHistoryMap &out) const
	{
		std::string rootFile			= GetAbsoluteFileName(m_srcMgr->getMainFileID());
		FileHistory &history			= out[rootFile];
		history.m_compileErrorHistory	= m_compileErrorHistory;
	}
}