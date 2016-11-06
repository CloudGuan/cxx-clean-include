//------------------------------------------------------------------------------
// �ļ�: parser.cpp
// ����: ������
// ˵��: ������ǰcpp�ļ�
// Copyright (c) 2016 game. All rights reserved.
//------------------------------------------------------------------------------

#include "parser.h"

#include <sstream>
#include <fstream>

// ����3��#include��_chmod������Ҫ�õ�
#include <io.h>
#include <sys/stat.h>
#include <sys/types.h>

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

ParsingFile* ParsingFile::g_nowFile = nullptr;

namespace cxxclean
{
	// set��ȥset
	template <typename T>
	inline void Del(std::set<T> &a, const std::set<T> &b)
	{
		for (const T &t : b)
		{
			a.erase(t);
		}
	}

	// set��ȥmap
	template <typename Key, typename Val>
	inline void Del(std::set<Key> &a, const std::map<Key, Val> &b)
	{
		for (const auto &itr : b)
		{
			a.erase(itr.first);
		}
	}

	// map��ȥset
	template <typename Key, typename Val>
	inline void Del(std::map<Key, Val> &a, const std::set<Key> &b)
	{
		for (const Key &t : b)
		{
			a.erase(t);
		}
	}

	// set��ȥset
	template <typename T>
	inline void Add(std::set<T> &a, const std::set<T> &b)
	{
		a.insert(b.begin(), b.end());
	}

	// set����map
	template <typename Key, typename Val>
	inline void Add(std::set<Key> &a, const std::map<Key, Val> &b)
	{
		for (const auto &itr : b)
		{
			a.insert(itr.first);
		}
	}

	// set����set�з��������ĳ�Ա
	template <typename Key, typename Op>
	inline void AddIf(std::set<Key> &a, const std::set<Key> &b, const Op& op)
	{
		for (const Key &key : b)
		{
			if (op(key))
			{
				a.insert(key);
			}
		}
	}

	// set����map��ָ������Ӧ��ֵ
	template <typename Key, typename Val>
	inline void AddByKey(std::set<Val> &a, const std::map<Key, std::set<Val>> &b, const Key &key)
	{
		auto &itr = b.find(key);
		if (itr != b.end())
		{
			Add(a, itr->second);
		}
	}

	template <typename Container, typename Op>
	inline void EraseIf(Container& container, const Op& op)
	{
		for (auto it = container.begin(); it != container.end(); )
		{
			if (op(*it)) container.erase(it++);
			else ++it;
		}
	}

	template <typename Container, typename Op>
	inline void MapEraseIf(Container& container, const Op& op)
	{
		for (auto it = container.begin(); it != container.end(); )
		{
			if (op(it->first, it->second)) container.erase(it++);
			else ++it;
		}
	}

	template <typename Container, typename Key>
	inline bool Has(Container& container, const Key &key)
	{
		return container.find(key) != container.end();
	}

	template <typename Key>
	inline bool HasInMap(const std::map<Key, std::set<Key>> &container, const Key &by, const Key &kid)
	{
		auto &itr = container.find(by);
		if (itr == container.end())
		{
			return false;
		}

		return Has(itr->second, kid);
	}

	template <typename Expand>
	FileSet GetChain(FileID top, const Expand& expand)
	{
		FileSet todo;
		FileSet done;

		todo.insert(top);

		while (!todo.empty())
		{
			FileID cur = *todo.begin();
			todo.erase(todo.begin());

			if (done.find(cur) == done.end())
			{
				done.insert(cur);
				expand(done, todo, cur);
			}
		}

		return done;
	}

	ParsingFile::ParsingFile(clang::CompilerInstance &compiler)
	{
		m_compiler	= &compiler;
		m_srcMgr	= &compiler.getSourceManager();
		m_printIdx	= 0;
		g_nowFile	= this;
		m_root		= m_srcMgr->getMainFileID();

		m_rewriter.setSourceMgr(*m_srcMgr, compiler.getLangOpts());
		m_headerSearchPaths = TakeHeaderSearchPaths(m_compiler->getPreprocessor().getHeaderSearchInfo());
	}

	ParsingFile::~ParsingFile()
	{
		g_nowFile = nullptr;
	}

	// ��Ӹ��ļ���ϵ
	inline void ParsingFile::AddParent(FileID child, FileID parent)
	{
		if (child != parent && child.isValid() && parent.isValid())
		{
			m_parents[child] = parent;
		}
	}

	// ��Ӱ����ļ���¼
	inline void ParsingFile::AddInclude(FileID file, FileID beInclude)
	{
		if (file != beInclude && file.isValid() && beInclude.isValid())
		{
			m_includes[file].insert(beInclude);
		}
	}

	// ��ӳ�Ա�ļ�
	void ParsingFile::AddFile(FileID file)
	{
		if (file.isInvalid())
		{
			return;
		}

		m_files.insert(file);

		const std::string fileName = GetAbsoluteFileName(file);
		if (!fileName.empty())
		{
			const std::string lowerFileName = tolower(fileName);

			m_fileNames.insert(std::make_pair(file, fileName));
			m_lowerFileNames.insert(std::make_pair(file, lowerFileName));
			m_sameFiles[lowerFileName].insert(file);
		}

		FileID parent = m_srcMgr->getFileID(m_srcMgr->getIncludeLoc(file));
		if (parent.isValid())
		{
			if (IsForceIncluded(file))
			{
				parent = m_root;
			}

			AddParent(file, parent);
			AddInclude(parent, file);
		}
	}

	// ��ȡͷ�ļ�����·��
	vector<ParsingFile::HeaderSearchDir> ParsingFile::TakeHeaderSearchPaths(const clang::HeaderSearch &headerSearch) const
	{
		typedef clang::HeaderSearch::search_dir_iterator search_iterator;

		IncludeDirMap dirs;

		auto AddIncludeDir = [&](search_iterator beg, search_iterator end, SrcMgr::CharacteristicKind includeKind)
		{
			// ��ȡϵͳͷ�ļ�����·��
			for (auto itr = beg; itr != end; ++itr)
			{
				if (const DirectoryEntry* entry = itr->getDir())
				{
					const string path = pathtool::fix_path(entry->getName());
					dirs.insert(make_pair(path, includeKind));
				}
			}
		};

		// ��ȡϵͳͷ�ļ�����·��
		AddIncludeDir(headerSearch.system_dir_begin(), headerSearch.system_dir_end(), SrcMgr::C_System);
		AddIncludeDir(headerSearch.search_dir_begin(), headerSearch.search_dir_end(), SrcMgr::C_User);

		return SortHeaderSearchPath(dirs);
	}

	// ��ͷ�ļ�����·�����ݳ����ɳ���������
	std::vector<ParsingFile::HeaderSearchDir> ParsingFile::SortHeaderSearchPath(const IncludeDirMap& includeDirsMap) const
	{
		std::vector<HeaderSearchDir> dirs;

		for (const auto &itr : includeDirsMap)
		{
			const string &path						= itr.first;
			SrcMgr::CharacteristicKind includeKind	= itr.second;

			string absolutePath = pathtool::get_absolute_path(path.c_str());

			dirs.push_back(HeaderSearchDir(absolutePath, includeKind));
		}

		// ���ݳ����ɳ���������
		sort(dirs.begin(), dirs.end(), [](const ParsingFile::HeaderSearchDir& left, const ParsingFile::HeaderSearchDir& right)
		{
			return left.m_dir.length() > right.m_dir.length();
		});

		return dirs;
	}

	// ����ͷ�ļ�����·����������·��ת��Ϊ˫���Ű�Χ���ı�����
	// ���磺������ͷ�ļ�����·��"d:/a/b/c" ��"d:/a/b/c/d/e.h" -> "d/e.h"
	string ParsingFile::GetQuotedIncludeStr(const char *absoluteFilePath) const
	{
		string path = pathtool::simplify_path(absoluteFilePath);

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

	// ��ȡͬ���ļ��б�
	FileSet ParsingFile::GetAllSameFiles(FileID file) const
	{
		auto sameItr = m_sameFiles.find(GetLowerFileNameInCache(file));
		if (sameItr != m_sameFiles.end())
		{
			const FileSet &sames = sameItr->second;
			return std::move(sames);
		}
		else
		{
			FileSet sames;
			sames.insert(file);
			return sames;
		}
	}

	// 2���ļ��Ƿ��ļ���һ��
	inline bool ParsingFile::IsSameName(FileID a, FileID b) const
	{
		return (std::string(GetLowerFileNameInCache(a)) == GetLowerFileNameInCache(b));
	}

	// Ϊ��ǰcpp�ļ���������ǰ��׼��
	void ParsingFile::InitCpp()
	{
		// 1. ����ÿ���ļ��ĺ���ļ�����������������Ҫ�õ���
		for (FileID file : m_files)
		{
			for (FileID child = file, parent; (parent = GetParent(child)).isValid(); child = parent)
			{
				m_kids[parent].insert(file);
			}
		}

		// 2. �����������������ļ�
		MapEraseIf(m_sameFiles, [&](const std::string&, const FileSet &sameFiles)
		{
			return sameFiles.size() <= 1;
		});
	}

	// ��ʼ����
	void ParsingFile::Analyze()
	{
		/*
			������δ����Ǳ����ߵ���Ҫ����˼·
		*/
		if (Project::IsCleanModeOpen(CleanMode_Need))
		{
			GenerateForceIncludes();
			GenerateOutFileAncestor();
			GenerateKidBySame();
			GenerateUserUse();
			GenerateMinUse();
			GenerateForwardClass();
		}
		else
		{
			GenerateRely();

			GenerateUnusedInclude();
			GenerateReplace();
		}

		TakeHistorys(m_historys);
		MergeTo(ProjectHistory::instance.m_files);
	}

	FileSet ParsingFile::GetUseChain(FileID top) const
	{
		FileSet chain = GetChain(top, [&](const FileSet &done, FileSet &todo, FileID cur)
		{
			// 1. ����ǰ�ļ������������ļ���������
			auto & useItr = m_uses.find(cur);
			if (useItr != m_uses.end())
			{
				// 2. todo���� += ��ǰ�ļ������������ļ�
				const FileSet &useFiles = useItr->second;
				AddIf(todo, useFiles, [&top](FileID beuse)
				{
					// ֻ��չ����ļ�
					return g_nowFile->IsAncestorBySame(beuse, top);
				});
			}
		});

		chain.erase(top);
		return chain;
	}

	bool ParsingFile::MergeMin()
	{
		bool smaller = false;

		// �ϲ�
		for (auto &itr : m_min)
		{
			FileID top		= itr.first;
			FileSet &kids	= itr.second;

			if (kids.empty())
			{
				m_min.erase(top);
				return true;
			}

			FileSet minKids = kids;
			FileSet eraseList;

			for (FileID kid : kids)
			{
				for (FileID forceInclude : m_forceIncludes)
				{
					FileID same = GetBestKid(forceInclude, kid);
					if (same != kid)
					{
						LogInfoByLvl(LogLvl_3, "force includes: erase [kid](top = " << GetDebugFileName(top) << ", forceInclude = " << GetDebugFileName(forceInclude) << ", kid = " << GetDebugFileName(kid) << ")");

						eraseList.insert(kid);
						break;
					}
				}

				minKids.erase(kid);

				for (FileID other : minKids)
				{
					if (IsSameName(kid, other))
					{
						LogInfoByLvl(LogLvl_3, "[kid]'name = [other]'name: erase [other](top = " << GetDebugFileName(top) << ", kid = " << GetDebugFileName(kid) << ", other = " << GetDebugFileName(other) << ")");

						eraseList.insert(other);
						break;
					}

					if (HasMinKidBySameName(kid, other))
					{
						LogInfoByLvl(LogLvl_3, "[kid]'s child contains [other]: erase [other](top = " << GetDebugFileName(top) << ", kid = " << GetDebugFileName(kid) << ", other = " << GetDebugFileName(other) << ")");

						eraseList.insert(other);
						break;
					}

					if (HasMinKidBySameName(other, kid))
					{
						LogInfoByLvl(LogLvl_3, "[other]'s child contains [kid]: erase [kid](top = " << GetDebugFileName(top) << ", other = " << GetDebugFileName(other) << ", kid = " << GetDebugFileName(kid) << ")");

						eraseList.insert(kid);
						break;
					}
				}
			}

			if (!eraseList.empty())
			{
				Del(kids, eraseList);
				smaller = true;
			}
		}

		return smaller;
	}

	inline bool ParsingFile::IsUserFile(FileID file) const
	{
		return CanClean(file);
	}

	inline bool ParsingFile::IsOuterFile(FileID file) const
	{
		return file.isValid() && (IsAncestorForceInclude(file) || !IsUserFile(file));
	}

	inline FileID ParsingFile::GetOuterFileAncestor(FileID file) const
	{
		auto itr = m_outFileAncestor.find(file);
		return itr != m_outFileAncestor.end() ? itr->second : file;
	}

	inline FileID ParsingFile::SearchOuterFileAncestor(FileID file) const
	{
		FileID topSysAncestor = file;

		for (FileID parent = file; IsOuterFile(parent); parent = GetParent(parent))
		{
			topSysAncestor = parent;
		}

		return topSysAncestor;
	}

	void ParsingFile::GenerateForceIncludes()
	{
		for (FileID file : m_files)
		{
			if (IsForceIncluded(file))
			{
				m_forceIncludes.insert(file);
			}
		}
	}

	void ParsingFile::GenerateOutFileAncestor()
	{
		for (FileID file : m_files)
		{
			FileID outerFileAncestor;

			for (FileID parent = file; IsOuterFile(parent); parent = GetParent(parent))
			{
				outerFileAncestor = parent;
			}

			if (outerFileAncestor.isValid() && outerFileAncestor != file)
			{
				m_outFileAncestor[file] = outerFileAncestor;
			}
		}

		FileSet dels;

		for (auto itr : m_outFileAncestor)
		{
			FileID kid = itr.first;

			auto sameItr = m_sameFiles.find(GetLowerFileNameInCache(kid));
			if (sameItr != m_sameFiles.end())
			{
				const FileSet &sames = sameItr->second;
				for (FileID same :sames)
				{
					if (!Has(m_outFileAncestor, same))
					{
						Add(dels, sames);
						break;
					}
				}
			}
		}

		Del(m_outFileAncestor, dels);
	}

	void ParsingFile::GenerateKidBySame()
	{
		m_userKids = m_kids;

		for (const auto &itr : m_userKids)
		{
			FileID top = itr.first;

			FileSet &kids = m_kidsBySame[GetLowerFileNameInCache(top)];
			GetKidsBySame(top, kids);
		}
	}

	void ParsingFile::GenerateUserUse()
	{
		for (const auto &itr : m_uses)
		{
			FileID by				= itr.first;
			const FileSet &useList	= itr.second;

			if (IsForceIncluded(by))
			{
				continue;
			}

			bool isByOuter = IsOuterFile(by);

			FileID byAncestor = GetOuterFileAncestor(by);

			FileSet userUseList;
			for (FileID beUse : useList)
			{
				if (isByOuter)
				{
					if (IsAncestorBySame(beUse, by))
					{
						continue;
					}
				}

				FileID a = GetBestKidBySame(by, beUse);

				/*
				FileID b = GetBestKid(by, beUse);

				if (a != b)
				{
					LogInfo("a != b: by = " << GetDebugFileName(by));
					LogInfo("------: beuse = " << GetDebugFileName(beUse));
					LogInfo("------: a = " << GetDebugFileName(a));
					LogInfo("------: b = " << GetDebugFileName(b));
				}
				*/

				FileID beUseAncestor = GetOuterFileAncestor(a);
				userUseList.insert(beUseAncestor);
			}

			userUseList.erase(byAncestor);
			if (!userUseList.empty())
			{
				Add(m_userUses[byAncestor], userUseList);
			}
		}
	}

	void ParsingFile::GenerateMinUse()
	{
		for (const auto &itr : m_uses)
		{
			FileID top = itr.first;
			const FileSet chain = GetUseChain(top);
			if (!chain.empty())
			{
				//FileSet &better = m_min[top];
				for (FileID f : chain)
				{
					// m_min[GetOuterFileAncestor(top)].insert(GetOuterFileAncestor(GetBestKidBySame(top, f)));
					m_min[GetOuterFileAncestor(top)].insert(f);
				}

				// m_min.insert(std::make_pair(top, chain));
			}
		}

		m_minKids = m_min;

		// 2. �ϲ�
		while (MergeMin()) {}
	}

	void ParsingFile::GetKidsBySame(FileID top, FileSet &kids) const
	{
		kids = GetChain(top, [&](const FileSet &done, FileSet &todo, FileID cur)
		{
			// �ڲ��������������ļ�����todo����
			auto AddTodo = [&] (FileID file)
			{
				if (!Has(done, file))
				{
					todo.insert(file);
				}

				auto kidItr = m_userKids.find(file);
				if (kidItr != m_userKids.end())
				{
					const FileSet &kids = kidItr->second;
					AddIf(todo, kids, [&done](FileID kid)
					{
						return !Has(done, kid);
					});
				}
			};

			const FileSet sames = GetAllSameFiles(cur);
			for (FileID same : sames)
			{
				AddTodo(same);
			}
		});

		const FileSet sames = GetAllSameFiles(top);
		Del(kids, sames);
	}

	int ParsingFile::GetDeepth(FileID file) const
	{
		int deepth = 0;

		for (FileID parent; (parent = GetParent(file)).isValid(); file = parent)
		{
			++deepth;
		}

		return deepth;
	}

	// ��¼���ļ��ı���������ļ�
	void ParsingFile::GenerateRelyChildren()
	{
		for (FileID usedFile : m_relys)
		{
			for (FileID child = usedFile, parent; (parent = GetParent(child)).isValid(); child = parent)
			{
				m_relyKids[parent].insert(usedFile);
			}
		}
	}

	// ��ȡ���ļ��ɱ��滻�����ļ������޷����滻���򷵻ؿ��ļ�id
	FileID ParsingFile::GetCanReplaceTo(FileID top) const
	{
		// ��[�滻#include]����ѡ���ʱ���ų����滻
		if (!Project::IsCleanModeOpen(CleanMode_Replace))
		{
			return FileID();
		}

		// ���ļ�����Ҳ��ʹ�õ������޷����滻
		if (IsRely(top))
		{
			return FileID();
		}

		auto &itr = m_relyKids.find(top);
		if (itr == m_relyKids.end())
		{
			return FileID();
		}

		const FileSet &children = itr->second;		// ���õĺ���ļ�

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

	// ������Ӹ����������ļ��������ļ�������ֵ��true��ʾ�����ļ��������š�false��ʾ�����ļ�������
	bool ParsingFile::TryAddAncestor()
	{
		for (auto &itr = m_relys.rbegin(); itr != m_relys.rend(); ++itr)
		{
			FileID file = *itr;

			for (FileID top = m_root, lv2Top; top != file; top = lv2Top)
			{
				lv2Top = GetLvl2Ancestor(file, top);
				if (lv2Top == file)
				{
					break;
				}

				if (IsRely(lv2Top))
				{
					continue;
				}

				FileID canReplaceTo = GetCanReplaceTo(lv2Top);

				auto &replaceItr = m_replaces.find(lv2Top);
				if (replaceItr != m_replaces.end())
				{
					FileID oldReplaceTo = replaceItr->second;

					if (canReplaceTo != oldReplaceTo)
					{
						m_replaces.erase(lv2Top);
						return true;
					}
				}
				else
				{
					bool expand = true;

					// �����ɱ��滻
					if (canReplaceTo.isInvalid())
					{
						expand = ExpandRely(lv2Top);
					}
					// ���ɱ��滻
					else if (!ExpandRely(canReplaceTo))
					{
						m_replaces[lv2Top] = canReplaceTo;
					}

					if (expand)
					{
						return true;
					}
				}
			}
		}

		return false;
	}

	// �������ļ���������ϵ����������ļ��������ļ���
	void ParsingFile::GenerateRely()
	{
		// 1. ��ȡ���ļ��������ļ���
		GetTopRelys(m_root, m_topRelys);

		m_relys = m_topRelys;

		// 2. ��¼���ļ��ĺ���ļ��б������Ĳ���
		GenerateRelyChildren();

		// 3. ������Ӹ����������ļ��������ļ�
		while (TryAddAncestor())
		{
			// 4. �������ļ��������ţ����������ɺ�������ļ���
			GenerateRelyChildren();
		};

		// 5. �ӱ�ʹ���ļ��ĸ��ڵ㣬������Ͻ������ø������ù�ϵ
		for (FileID beusedFile : m_relys)
		{
			for (FileID child = beusedFile, parent; (parent = GetParent(child)).isValid(); child = parent)
			{
				m_usedFiles.insert(child);
			}
		}
	}

	bool ParsingFile::ExpandRely(FileID top)
	{
		FileSet topRelys;
		GetTopRelys(top, topRelys);

		int oldSize = m_relys.size();
		Add(m_relys, topRelys);
		int newSize = m_relys.size();

		return newSize > oldSize;
	}

	// �Ƿ��ֹ�Ķ�ĳ�ļ�
	bool ParsingFile::IsSkip(FileID file) const
	{
		return IsForceIncluded(file) || IsPrecompileHeader(file);
	}

	// ���ļ��Ƿ�����
	inline bool ParsingFile::IsRely(FileID file) const
	{
		return Has(m_relys, file);
	}

	// ���ļ�������ͬ���ļ��Ƿ�������ͬһ�ļ��ɱ�������Σ�
	inline bool ParsingFile::HasMinKidBySameName(FileID top, FileID kid) const
	{
		const FileSet &sames = GetAllSameFiles(kid);
		for (FileID same : sames)
		{
			if (HasInMap(m_minKids, top, same))
			{
				return true;
			}
		}

		return false;
	}

	// a�ļ��Ƿ���bλ��֮ǰ
	bool ParsingFile::IsFileBeforeLoc(FileID a, SourceLocation b) const
	{
		SourceLocation aBeg = m_srcMgr->getLocForStartOfFile(a);
		return m_srcMgr->isBeforeInTranslationUnit(aBeg, b);
	}

	// �����ļ��Ƿ�ǿ�ư���
	inline bool ParsingFile::IsAncestorForceInclude(FileID file) const
	{
		return GetAncestorForceInclude(file).isValid();
	}

	// ��ȡ��ǿ�ư��������ļ�
	inline FileID ParsingFile::GetAncestorForceInclude(FileID file) const
	{
		for (FileID forceInclude : m_forceIncludes)
		{
			if (file == forceInclude || IsAncestor(file, forceInclude))
			{
				return forceInclude;
			}
		}

		return FileID();
	}

	// ��������#include�ļ�¼
	void ParsingFile::GenerateUnusedInclude()
	{
		// ��[ɾ������#include]����ѡ���ʱ�����������
		if (!Project::IsCleanModeOpen(CleanMode_Unused))
		{
			return;
		}

		// ���������ļ��б�
		for (FileID file : m_files)
		{
			FileID parent = GetParent(file);
			if (!IsRely(parent))
			{
				continue;
			}

			// �ų����õ����ļ�
			if (Has(m_usedFiles, file))
			{
				continue;
			}

			// ��Щ#include��Ӧ��ɾ�����޷���ɾ������ǿ�ư�����Ԥ����ͷ�ļ�
			if (IsSkip(file))
			{
				continue;
			}

			m_unusedFiles.insert(file);
		}
	}

	void ParsingFile::GetTopRelys(FileID top, FileSet &out) const
	{
		out = GetChain(top, [&](const FileSet &done, FileSet &todo, FileID cur)
		{
			// 1. ����ǰ�ļ��������������ļ�������Բ�����չ��ǰ�ļ�
			auto &useItr = m_uses.find(cur);
			if (useItr != m_uses.end())
			{
				// 2. ����ǰ�ļ������������ļ��������������
				const FileSet &useFiles = useItr->second;
				AddIf(todo, useFiles, [&](FileID beuse)
				{
					return done.find(beuse) == done.end();
				});
			}
		});
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
	FileID ParsingFile::GetCommonAncestor(const FileSet &kids) const
	{
		FileID highestKid;
		int minDepth = 0;

		for (FileID kid : kids)
		{
			int depth = GetDepth(kid);

			if (minDepth == 0 || depth < minDepth)
			{
				highestKid = kid;
				minDepth = depth;
			}
		}

		FileID ancestor = highestKid;
		while (!IsCommonAncestor(kids, ancestor))
		{
			FileID parent = GetParent(ancestor);
			if (parent.isInvalid())
			{
				return m_root;
			}

			ancestor = parent;
		}

		return ancestor;
	}

	// �����ļ��滻�б�
	void ParsingFile::GenerateReplace()
	{
		for (auto &itr : m_replaces)
		{
			FileID from		= itr.first;
			FileID parent	= GetParent(from);

			m_splitReplaces[parent].insert(itr);
		}

		m_splitReplaces.erase(FileID());
	}

	// �Ƿ�Ӧ������λ�����õ�class��struct��union��ǰ������
	bool ParsingFile::IsNeedMinClass(FileID by, const CXXRecordDecl &cxxRecord) const
	{
		auto IsAnyKidHasRecord = [&](FileID byFile, FileID recordAt) -> bool
		{
			if (IsAncestorForceInclude(recordAt))
			{
				LogInfoByLvl(LogLvl_3, "record is force included: skip record = [" <<  GetRecordName(cxxRecord) << "], by = " << GetDebugFileName(byFile) << ", record file = " << GetDebugFileName(recordAt));
				return true;
			}

			if (IsSameName(byFile, recordAt))
			{
				LogInfoByLvl(LogLvl_3, "record is at same file: skip record = [" <<  GetRecordName(cxxRecord) << "], by = " << GetDebugFileName(byFile) << ", record file = " << GetDebugFileName(recordAt));
				return true;
			}

			FileID best = GetBestKidBySame(recordAt, byFile);
			FileID outerAncestor = GetOuterFileAncestor(best);
			return HasMinKidBySameName(byFile, outerAncestor);
		};

		auto IsAnyRecordBeInclude = [&]() -> bool
		{
			// ������ļ��ڸ�λ��֮ǰ����ǰ���������ٴ���
			const TagDecl *first = cxxRecord.getFirstDecl();
			for (const TagDecl *next : first->redecls())
			{
				FileID recordAtFile = GetFileID(next->getLocation());
				if (IsAnyKidHasRecord(by, recordAtFile))
				{
					return true;
				}

				LogErrorByLvl(LogLvl_3, "[IsAnyKidHasRecord = false]: by = " << GetDebugFileName(by) <<  ", file = " << GetDebugFileName(recordAtFile) << ", record = " << next->getNameAsString());
			}

			return false;
		};

		if (IsAnyRecordBeInclude())
		{
			return false;
		}

		return true;
	}

	// ��������ǰ�������б�
	void ParsingFile::GenerateForwardClass()
	{
		// 1. ���һЩ����Ҫ������ǰ������
		for (auto &itr : m_fileUseRecordPointers)
		{
			FileID by			= itr.first;
			RecordSet &records	= itr.second;

			auto &beUseItr = m_fileUseRecords.find(by);
			if (beUseItr != m_fileUseRecords.end())
			{
				const RecordSet &beUseRecords = beUseItr->second;
				Del(records, beUseRecords);
			}
		}

		m_fowardClass = m_fileUseRecordPointers;

		MapEraseIf(m_fowardClass, [&](FileID by, RecordSet &records)
		{
			EraseIf(records, [&](const CXXRecordDecl* record)
			{
				bool need = g_nowFile->IsNeedMinClass(by, *record);
				if (need)
				{
					LogErrorByLvl(LogLvl_3, "IsNeedMinClass = true: " << GetDebugFileName(by) << "," << GetRecordName(*record));
				}

				return !need;
			});

			return records.empty();
		});

		// 2.
		MinimizeForwardClass();
	}

	// �ü�ǰ�������б�
	void ParsingFile::MinimizeForwardClass()
	{
		FileSet all;
		Add(all, m_fileUseRecordPointers);
		Add(all, m_min);

		FileUseRecordsMap bigRecordMap;

		for (FileID by : all)
		{
			GetUseRecordsInKids(by, m_fowardClass, bigRecordMap[by]);
		}

		for (auto &itr : bigRecordMap)
		{
			FileID by = itr.first;
			RecordSet small = itr.second;	// ����������

			auto &useItr = m_min.find(by);
			if (useItr != m_min.end())
			{
				const FileSet &useList = useItr->second;

				for (FileID beUse : useList)
				{
					auto &recordItr = bigRecordMap.find(beUse);
					if (recordItr != bigRecordMap.end())
					{
						const RecordSet &records = recordItr->second;
						Del(small, records);
					}
				}
			}

			if (!small.empty())
			{
				m_fowardClass[by] = small;
			}
		}
	}

	void ParsingFile::GetUseRecordsInKids(FileID top, const FileUseRecordsMap &recordMap, RecordSet &records)
	{
		const FileSet done = GetChain(top, [&](const FileSet &done, FileSet &todo, FileID cur)
		{
			// 1. ����ǰ�ļ������������ļ���������
			auto &useItr = m_min.find(cur);
			if (useItr != m_min.end())
			{
				// 2. todo���� += ��ǰ�ļ������������ļ�
				const FileSet &useFiles = useItr->second;
				AddIf(todo, useFiles, [&done](FileID beuse)
				{
					return done.find(beuse) == done.end();
				});
			}
		});

		for (FileID file : done)
		{
			AddByKey(records, recordMap, file);
		}
	}

	// ��ȡָ����Χ���ı�
	std::string ParsingFile::GetSourceOfRange(SourceRange range) const
	{
		if (range.isInvalid())
		{
			return "";
		}

		range = m_srcMgr->getExpansionRange(range);

		if (range.getEnd() < range.getBegin())
		{
			LogError("if (range.getEnd() < range.getBegin())");
			return "";
		}

		if (!m_srcMgr->isWrittenInSameFile(range.getBegin(), range.getEnd()))
		{
			LogError("if (!m_srcMgr->isWrittenInSameFile(range.getBegin(), range.getEnd()))");
			return "";
		}

		const char* beg = GetSourceAtLoc(range.getBegin());
		const char* end = GetSourceAtLoc(range.getEnd());

		if (nullptr == beg || nullptr == end || end < beg)
		{
			// ע�⣺����������ж�end < beg��������ܻ������Ϊ�п���ĩβ��������ʼ�ַ�ǰ�棬�����ں�֮��
			return "";
		}

		return string(beg, end);
	}

	// ��ȡָ��λ�õ��ı�
	const char* ParsingFile::GetSourceAtLoc(SourceLocation loc) const
	{
		bool err = true;

		const char* str = m_srcMgr->getCharacterData(loc, &err);
		return err ? nullptr : str;
	}

	// ��ȡָ��λ�������е��ı�
	std::string ParsingFile::GetSourceOfLine(SourceLocation loc) const
	{
		return GetSourceOfRange(GetCurLine(loc));
	}

	SourceRange ParsingFile::GetCurLine(SourceLocation loc) const
	{
		if (loc.isInvalid())
		{
			return SourceRange();
		}

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

	SourceRange ParsingFile::GetCurFullLine(SourceLocation loc) const
	{
		SourceRange curLine		= GetCurLine(loc);
		SourceRange nextLine	= GetNextLine(loc);

		return SourceRange(curLine.getBegin(), nextLine.getBegin());
	}

	// ���ݴ���Ĵ���λ�÷�����һ�еķ�Χ
	SourceRange ParsingFile::GetNextLine(SourceLocation loc) const
	{
		SourceRange curLine			= GetCurLine(loc);
		SourceLocation lineEnd		= curLine.getEnd();
		SourceLocation fileEndLoc	= m_srcMgr->getLocForEndOfFile(GetFileID(loc));

		if (m_srcMgr->isBeforeInTranslationUnit(fileEndLoc, lineEnd) || fileEndLoc == lineEnd)
		{
			return SourceRange(fileEndLoc, fileEndLoc);
		}

		const char* c1			= GetSourceAtLoc(lineEnd);
		const char* c2			= GetSourceAtLoc(lineEnd.getLocWithOffset(1));

		if (nullptr == c1 || nullptr == c2)
		{
			LogErrorByLvl(LogLvl_2, "GetNextLine = null");
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

	// ��ȡ�ļ���Ӧ�ı������к�
	int ParsingFile::GetIncludeLineNo(FileID file) const
	{
		if (IsForceIncluded(file))
		{
			return 0;
		}

		return GetLineNo(m_srcMgr->getIncludeLoc(file));
	}

	// ��ȡ�ļ���Ӧ��#include����Χ
	SourceRange ParsingFile::GetIncludeRange(FileID file) const
	{
		SourceLocation includeLoc = m_srcMgr->getIncludeLoc(file);
		return GetCurFullLine(includeLoc);
	}

	// �Ƿ��ǻ��з�
	bool ParsingFile::IsNewLineWord(SourceLocation loc) const
	{
		string text = GetSourceOfRange(SourceRange(loc, loc.getLocWithOffset(1)));
		return text == "\r" || text == "";
	}

	// ��ȡ�ļ���Ӧ��#include���ڵ��У��������з���
	std::string ParsingFile::GetBeIncludeLineText(FileID file) const
	{
		SourceLocation loc = m_srcMgr->getIncludeLoc(file);
		return GetSourceOfLine(loc);
	}

	// aλ�õĴ���ʹ��bλ�õĴ���
	inline void ParsingFile::Use(SourceLocation a, SourceLocation b, const char* name /* = nullptr */)
	{
		a = GetExpasionLoc(a);
		b = GetExpasionLoc(b);

		UseInclude(GetFileID(a), GetFileID(b), name, GetLineNo(a));
	}

	inline void ParsingFile::UseName(FileID file, FileID beusedFile, const char* name /* = nullptr */, int line)
	{
		if (Project::instance.m_logLvl < LogLvl_3)
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

	// ���ݵ�ǰ�ļ������ҵ�2������ȣ���rootΪ��һ�㣩������ǰ�ļ��ĸ��ļ���Ϊ���ļ����򷵻ص�ǰ�ļ�
	inline FileID ParsingFile::GetLvl2AncestorBySame(FileID kid, FileID top) const
	{
		auto itr = m_includes.find(top);
		if (itr == m_includes.end())
		{
			return FileID();
		}

		const std::string kidFileName = GetLowerFileNameInCache(kid);

		const FileSet &includes = itr->second;
		for (FileID beInclude : includes)
		{
			if (kidFileName == GetLowerFileNameInCache(beInclude))
			{
				return beInclude;
			}

			const FileSet &sames = GetAllSameFiles(beInclude);
			for (FileID same : sames)
			{
				if (IsAncestorBySame(kid, same))
				{
					return beInclude;
				}
			}
		}

		return FileID();
	}

	// ��2���ļ��Ƿ��ǵ�1���ļ�������
	inline bool ParsingFile::IsAncestor(FileID young, FileID old) const
	{
		// ��������ļ�
		auto &itr = m_kids.find(old);
		if (itr != m_kids.end())
		{
			const FileSet &children = itr->second;
			return children.find(young) != children.end();
		}

		return false;
	}

	// ��2���ļ��Ƿ��ǵ�1���ļ�������
	inline bool ParsingFile::IsAncestor(FileID young, const char* old) const
	{
		const std::string strOld = tolower(old);

		for (FileID parent = GetParent(young); parent.isValid(); parent = GetParent(parent))
		{
			if (GetLowerFileNameInCache(parent) == strOld)
			{
				return true;
			}
		}

		return false;
	}

	// ��2���ļ��Ƿ��ǵ�1���ļ�������
	inline bool ParsingFile::IsAncestor(const char* young, FileID old) const
	{
		// ��������ļ�
		auto &itr = m_kids.find(old);
		if (itr != m_kids.end())
		{
			const FileSet &kids = itr->second;

			const std::string strKid = tolower(young);
			for (FileID kid : kids)
			{
				if (strKid == GetLowerFileNameInCache(kid))
				{
					return true;
				}
			}
		}

		return false;
	}

	// ��2���ļ��Ƿ��ǵ�1���ļ������ȣ�����ͬ���ļ���
	inline bool ParsingFile::IsAncestorBySame(FileID young, FileID old) const
	{
		auto itr = m_kidsBySame.find(GetLowerFileNameInCache(old));
		if (itr != m_kidsBySame.end())
		{
			const FileSet &kids = itr->second;
			return Has(kids, young);
		}

		return false;
	}

	// �Ƿ�Ϊ�����ļ��Ĺ�ͬ����
	bool ParsingFile::IsCommonAncestor(const FileSet &children, FileID old) const
	{
		for (FileID child : children)
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
		auto &itr = m_parents.find(child);
		if (itr != m_parents.end())
		{
			if (child == m_root)
			{
				return FileID();
			}

			return itr->second;
		}

		return FileID();
	}

	// a�ļ�ʹ��b�ļ�
	inline void ParsingFile::UseInclude(FileID a, FileID b, const char* name /* = nullptr */, int line)
	{
		if (a == b || a.isInvalid() || b.isInvalid())
		{
			return;
		}

		if (nullptr == m_srcMgr->getFileEntryForID(a) || nullptr == m_srcMgr->getFileEntryForID(b))
		{
			LogErrorByLvl(LogLvl_5, "m_srcMgr->getFileEntryForID(a) failed!" << m_srcMgr->getFilename(m_srcMgr->getLocForStartOfFile(a)) << ":" << m_srcMgr->getFilename(m_srcMgr->getLocForStartOfFile(b)));
			LogErrorByLvl(LogLvl_5, "m_srcMgr->getFileEntryForID(b) failed!" << GetSourceOfLine(m_srcMgr->getLocForStartOfFile(a)) << ":" << GetSourceOfLine(m_srcMgr->getLocForStartOfFile(b)));

			return;
		}

		if (IsSameName(a, b))
		{
			return;
		}

		b = GetBestKid(a, b);

		m_uses[a].insert(b);
		UseName(a, b, name, line);
	}

	// ��ǰλ��ʹ��ָ���ĺ�
	void ParsingFile::UseMacro(SourceLocation loc, const MacroDefinition &macro, const Token &macroNameTok, const MacroArgs *args /* = nullptr */)
	{
		MacroInfo *info = macro.getMacroInfo();
		if (info)
		{
			string macroName = macroNameTok.getIdentifierInfo()->getName().str() + "[macro]";
			Use(loc, info->getDefinitionLoc(), macroName.c_str());
		}
	}

	void ParsingFile::UseContext(SourceLocation loc, const DeclContext *context)
	{
		while (context && context->isNamespace())
		{
			const NamespaceDecl *ns = cast<NamespaceDecl>(context);

			UseNamespaceDecl(loc, ns);
			context = context->getParent();
		}
	}

	// ����Ƕ���������η�
	void ParsingFile::UseQualifier(SourceLocation loc, const NestedNameSpecifier *specifier)
	{
		while (specifier)
		{
			NestedNameSpecifier::SpecifierKind kind = specifier->getKind();
			switch (kind)
			{
			case NestedNameSpecifier::Namespace:
				UseNamespaceDecl(loc, specifier->getAsNamespace());
				break;

			case NestedNameSpecifier::NamespaceAlias:
				UseNamespaceAliasDecl(loc, specifier->getAsNamespaceAlias());
				break;

			default:
				UseType(loc, specifier->getAsType());
				break;
			}

			specifier = specifier->getPrefix();
		}
	}

	// ���������ռ�����
	void ParsingFile::UseNamespaceDecl(SourceLocation loc, const NamespaceDecl *ns)
	{
		UseNameDecl(loc, ns);

		for (auto itr : m_usingNamespaces)
		{
			SourceLocation usingLoc		= itr.first;
			const NamespaceDecl	*ns		= itr.second;

			if (m_srcMgr->isBeforeInTranslationUnit(usingLoc, loc))
			{
				if (ns->getQualifiedNameAsString() == ns->getQualifiedNameAsString())
				{
					Use(loc, usingLoc, GetNestedNamespace(ns).c_str());
				}
			}
		}
	}

	// ���������ռ����
	void ParsingFile::UseNamespaceAliasDecl(SourceLocation loc, const NamespaceAliasDecl *ns)
	{
		UseNameDecl(loc, ns);
		UseNamespaceDecl(ns->getAliasLoc(), ns->getNamespace());
	}

	// �����������ռ�
	void ParsingFile::DeclareNamespace(const NamespaceDecl *d)
	{
		if (Project::instance.m_logLvl >= LogLvl_3)
		{
			SourceLocation loc = GetSpellingLoc(d->getLocation());

			FileID file = GetFileID(loc);
			if (file.isInvalid())
			{
				return;
			}

			m_namespaces[file].insert(d->getQualifiedNameAsString());
		}
	}

	// using�������ռ䣬���磺using namespace std;
	void ParsingFile::UsingNamespace(const UsingDirectiveDecl *d)
	{
		const NamespaceDecl *nominatedNs = d->getNominatedNamespace();
		if (nullptr == nominatedNs)
		{
			return;
		}

		const NamespaceDecl *firstNs = nominatedNs->getOriginalNamespace();
		if (nullptr == firstNs)
		{
			return;
		}

		SourceLocation usingLoc = GetSpellingLoc(d->getUsingLoc());
		if (usingLoc.isInvalid())
		{
			return;
		}

		m_usingNamespaces[usingLoc] = firstNs;

		for (const NamespaceDecl *ns : firstNs->redecls())
		{
			SourceLocation nsLoc	= GetSpellingLoc(ns->getLocStart());
			if (nsLoc.isInvalid())
			{
				continue;
			}

			if (m_srcMgr->isBeforeInTranslationUnit(nsLoc, usingLoc))
			{
				// ���������ռ����ڵ��ļ���ע�⣺using namespaceʱ�������ҵ���Ӧ��namespace���������磬using namespace Aǰһ��Ҫ��namespace A{}�������ᱨ��
				Use(usingLoc, nsLoc, GetNestedNamespace(ns).c_str());
				break;
			}
		}
	}

	// using�������ռ��µ�ĳ�࣬���磺using std::string;
	void ParsingFile::UsingXXX(const UsingDecl *d)
	{
		SourceLocation usingLoc		= d->getUsingLoc();

		for (auto &itr = d->shadow_begin(); itr != d->shadow_end(); ++itr)
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

	// ��ȡ�����ռ��ȫ��·�������磬����namespace A{ namespace B{ class C; }}
	std::string ParsingFile::GetNestedNamespace(const NamespaceDecl *d)
	{
		if (nullptr == d)
		{
			return "";
		}

		string name;

		while (d)
		{
			string namespaceName = "namespace " + d->getNameAsString();
			name = namespaceName + "{" + name + "}";

			const DeclContext *parent = d->getParent();
			if (parent && parent->isNamespace())
			{
				d = cast<NamespaceDecl>(parent);
				continue;
			}

			break;
		}

		return name;
	}

	// ��aʹ��bʱ�����b��Ӧ���ļ���������Σ���b��ͬ���ļ���ѡȡһ����õ��ļ�
	inline FileID ParsingFile::GetBestKid(FileID a, FileID b) const
	{
		// ���b�ļ���������Σ���������ѡ��һ���Ϻ��ʵģ�ע�⣺ÿ���ļ����ȱ����Լ�������ļ��е�#include��䣩
		auto &sameItr = m_sameFiles.find(GetLowerFileNameInCache(b));
		if (sameItr != m_sameFiles.end())
		{
			// ���ҵ�ͬ���ļ��б�
			const FileSet &sames = sameItr->second;

			// 1. ���ȷ���ֱ��ֱ�Ӱ������ļ�
			auto &includeItr = m_includes.find(a);
			if (includeItr != m_includes.end())
			{
				const FileSet &includes = includeItr->second;

				for (FileID same :sames)
				{
					if (Has(includes, same))
					{
						return same;
					}
				}
			}

			// 2. ������������ļ�
			auto &kidItr = m_kids.find(a);
			if (kidItr != m_kids.end())
			{
				const FileSet &children = kidItr->second;
				for (FileID same :sames)
				{
					if (Has(children, same))
					{
						return same;
					}
				}
			}
		}

		return b;
	}

	inline FileID ParsingFile::GetBestKidBySame(FileID a, FileID b) const
	{
		if (IsAncestor(b, a))
		{
			return b;
		}

		if (!IsAncestorBySame(b, a))
		{
			return b;
		}

		FileSet todo;
		FileSet done;

		todo.insert(b);

		while (!todo.empty())
		{
			todo.erase(FileID());
			Del(todo, done);

			FileSet doing = todo;
			todo.clear();

			for (FileID f : doing)
			{
				const FileSet sames = GetAllSameFiles(f);
				for (FileID same : sames)
				{
					if (IsAncestor(same, a))
					{
						return same;
					}

					todo.insert(GetParent(same));
				}
			}

			Add(done, doing);
		}

		return b;
	}

	// ��ǰλ��ʹ��Ŀ�����ͣ�ע��Type����ĳ�����ͣ�������const��volatile��static�ȵ����Σ�
	void ParsingFile::UseType(SourceLocation loc, const Type *t)
	{
		if (nullptr == t || loc.isInvalid())
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
			UseTemplateDecl(loc, templateType->getTemplateName().getAsTemplateDecl());

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
			UseQualType(loc, parenType->getInnerType());
		}
		else if (isa<InjectedClassNameType>(t))
		{
			const InjectedClassNameType *injectedClassNameType = cast<InjectedClassNameType>(t);
			UseQualType(loc, injectedClassNameType->getInjectedSpecializationType());
		}
		else if (isa<PackExpansionType>(t))
		{
			const PackExpansionType *packExpansionType = cast<PackExpansionType>(t);
			UseQualType(loc, packExpansionType->getPattern());
		}
		else if (isa<DecltypeType>(t))
		{
			const DecltypeType *decltypeType = cast<DecltypeType>(t);
			UseQualType(loc, decltypeType->getUnderlyingType());
		}
		else if (isa<DependentNameType>(t))
		{
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
			// LogInfo(""-------------- haven't support type --------------");
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

	// ����ʹ��ǰ��������¼�����ڲ���Ҫ��ӵ�ǰ����������֮���������
	inline void ParsingFile::UseForward(SourceLocation loc, const CXXRecordDecl *cxxRecord)
	{
		if (nullptr == cxxRecord)
		{
			return;
		}

		FileID by = GetFileID(loc);
		if (by.isInvalid())
		{
			return;
		}

		if (Project::IsCleanModeOpen(CleanMode_Need))
		{
			// ����ļ���ʹ�õ�ǰ��������¼
			m_fileUseRecordPointers[by].insert(cxxRecord);

			if (Project::instance.m_logLvl >= LogLvl_3)
			{
				m_locUseRecordPointers[loc].insert(cxxRecord);
			}
		}
		else
		{
			// �������ø�λ��֮ǰ��ǰ�������򣬾�����������������class����
			const TagDecl *first = cxxRecord->getFirstDecl();
			for (const TagDecl *next : first->redecls())
			{
				if (m_srcMgr->isBeforeInTranslationUnit(loc, next->getLocation()))
				{
					break;
				}

				if (!next->isThisDeclarationADefinition())
				{
					UseNameDecl(loc, next);
					return;
				}
			}

			UseRecord(loc, cxxRecord);
		}
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
		while (isa<PointerType>(pointeeType) || isa<ReferenceType>(pointeeType))
		{
			pointeeType = pointeeType->getPointeeType();
		}

		if (!isa<RecordType>(pointeeType))
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

		if (IsOuterFile(GetFileID(loc)))
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

	// ���ù��캯��
	void ParsingFile::UseConstructor(SourceLocation loc, const CXXConstructorDecl *constructor)
	{
		for (const CXXCtorInitializer *initializer : constructor->inits())
		{
			if (initializer->isAnyMemberInitializer())
			{
				UseValueDecl(initializer->getSourceLocation(), initializer->getAnyMember());
			}
			else if (initializer->isBaseInitializer())
			{
				UseType(initializer->getSourceLocation(), initializer->getBaseClass());
			}
			else if (initializer->isDelegatingInitializer())
			{
				if (initializer->getTypeSourceInfo())
				{
					UseQualType(initializer->getSourceLocation(), initializer->getTypeSourceInfo()->getType());
				}
			}
			else
			{
				// decl->dump();
			}
		}

		UseValueDecl(loc, constructor);
	}

	// ���ñ�������
	void ParsingFile::UseVarDecl(SourceLocation loc, const VarDecl *var)
	{
		if (nullptr == var)
		{
			return;
		}

		UseValueDecl(loc, var);
	}

	// ���ã�����������Ϊ��ֵ����������ʶ��enum����
	void ParsingFile::UseValueDecl(SourceLocation loc, const ValueDecl *valueDecl)
	{
		if (nullptr == valueDecl)
		{
			return;
		}

		UseContext(loc, valueDecl->getDeclContext());

		if (isa<TemplateDecl>(valueDecl))
		{
			const TemplateDecl *t = cast<TemplateDecl>(valueDecl);
			if (nullptr == t)
			{
				return;
			}

			UseTemplateDecl(loc, t);
		}

		if (isa<FunctionDecl>(valueDecl))
		{
			const FunctionDecl *f = cast<FunctionDecl>(valueDecl);
			if (nullptr == f)
			{
				return;
			}

			UseFuncDecl(loc, f);
		}

		if (!IsForwardType(valueDecl->getType()))
		{
			std::stringstream name;
			name << valueDecl->getQualifiedNameAsString() << "[" << valueDecl->getDeclKindName() << "]";

			Use(loc, valueDecl->getLocEnd(), name.str().c_str());
		}

		UseVarType(loc, valueDecl->getType());
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
		UseContext(loc, nameDecl->getDeclContext());
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

		// �����ķ���ֵ
		QualType returnType = f->getReturnType();
		UseVarType(loc, returnType);

		// ���α����������������ù�ϵ
		for (FunctionDecl::param_const_iterator itr = f->param_begin(), end = f->param_end(); itr != end; ++itr)
		{
			ParmVarDecl *vardecl = *itr;
			UseVarDecl(loc, vardecl);
		}

		if (f->getTemplateSpecializationArgs())
		{
			const TemplateArgumentList *args = f->getTemplateSpecializationArgs();
			UseTemplateArgumentList(loc, args);
		}

		std::stringstream name;
		name << f->getQualifiedNameAsString() << "[" << f->clang::Decl::getDeclKindName() << "]";

		Use(loc, f->getTypeSpecStartLoc(), name.str().c_str());

		// ����������ģ���й�
		FunctionDecl::TemplatedKind templatedKind = f->getTemplatedKind();
		if (templatedKind != FunctionDecl::TK_NonTemplate)
		{
			// [����ģ�崦] ���� [ģ�嶨�崦]
			Use(f->getLocStart(), f->getLocation(), name.str().c_str());
		}
	}

	// ����ģ�����
	void ParsingFile::UseTemplateArgument(SourceLocation loc, const TemplateArgument &arg)
	{
		TemplateArgument::ArgKind argKind = arg.getKind();
		switch (argKind)
		{
		case TemplateArgument::Type:
			UseVarType(loc, arg.getAsType());
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

	// ����ģ�嶨��
	void ParsingFile::UseTemplateDecl(SourceLocation loc, const TemplateDecl *decl)
	{
		if (nullptr == decl)
		{
			return;
		}

		UseNameDecl(loc, decl);

		TemplateParameterList *params = decl->getTemplateParameters();

		for (int i = 0, n = params->size(); i < n; ++i)
		{
			NamedDecl *param = params->getParam(i);
			UseNameDecl(loc, param);
		}
	}

	// ����ʹ��class��struct��union��¼
	void ParsingFile::UseRecord(SourceLocation loc, const RecordDecl *record)
	{
		if (nullptr == record)
		{
			return;
		}

		FileID by = GetFileID(loc);
		if (by.isInvalid())
		{
			return;
		}

		if (isa<ClassTemplateSpecializationDecl>(record))
		{
			const ClassTemplateSpecializationDecl *d = cast<ClassTemplateSpecializationDecl>(record);
			UseTemplateArgumentList(loc, &d->getTemplateArgs());
		}

		if (Project::IsCleanModeOpen(CleanMode_Need))
		{
			if (isa<CXXRecordDecl>(record))
			{
				const CXXRecordDecl *cxxRecord = cast<CXXRecordDecl>(record);
				m_fileUseRecords[by].insert(cxxRecord);
			}
		}

		Use(loc, record->getLocStart(), GetRecordName(*record).c_str());
		UseContext(loc, record->getDeclContext());
	}

	// �Ƿ����������c++�ļ������������������ļ����ݲ������κα仯��
	inline bool ParsingFile::CanClean(FileID file) const
	{
		return Project::CanClean(GetLowerFileNameInCache(file));
	}

	// ��ӡ���ļ��ĸ��ļ�
	void ParsingFile::PrintParent()
	{
		HtmlDiv &div = HtmlLog::instance.m_newDiv;
		div.AddRow(AddPrintIdx() + ". list of parent id: has parent file count = " + htmltool::get_number_html(m_parents.size()), 1);

		for (auto &itr : m_parents)
		{
			FileID child	= itr.first;
			FileID parent	= itr.second;

			// ����ӡ����Ŀ���ļ���ֱ�ӹ������ļ�
			if (!CanClean(child) && !CanClean(parent))
			{
				continue;
			}

			div.AddRow("kid = " + DebugBeIncludeText(child), 2);
			div.AddRow("parent = " + DebugBeIncludeText(parent), 3);
			div.AddRow("");
		}

		div.AddRow("");
	}

	// ��ӡ���ļ��ĺ����ļ�
	void ParsingFile::PrintKids()
	{
		HtmlDiv &div = HtmlLog::instance.m_newDiv;
		div.AddRow(AddPrintIdx() + ". list of kids : file count = " + strtool::itoa(m_kids.size()), 1);

		for (auto &itr : m_kids)
		{
			FileID parent = itr.first;
			const FileSet &kids = itr.second;

			div.AddRow(DebugParentFileText(parent, kids.size()), 2);

			for (FileID kid : kids)
			{
				div.AddRow("kid = " + DebugBeIncludeText(kid), 3);
			}

			div.AddRow("");
		}
	}

	// ��ӡ���ļ��ĺ����ļ�
	void ParsingFile::PrintUserKids()
	{
		HtmlDiv &div = HtmlLog::instance.m_newDiv;
		div.AddRow(AddPrintIdx() + ". list of user kids : file count = " + strtool::itoa(m_userKids.size()), 1);

		for (auto &itr : m_userKids)
		{
			FileID parent = itr.first;
			const FileSet &kids = itr.second;

			div.AddRow(DebugParentFileText(parent, kids.size()), 2);

			for (FileID child : kids)
			{
				div.AddRow("user kid = " + DebugBeIncludeText(child), 3);
			}

			div.AddRow("");
		}
	}

	void ParsingFile::PrintKidsBySame()
	{
		HtmlDiv &div = HtmlLog::instance.m_newDiv;
		div.AddRow(AddPrintIdx() + ". list of kids by same name : file count = " + strtool::itoa(m_kidsBySame.size()), 1);

		for (auto &itr : m_kidsBySame)
		{
			const std::string &parent = itr.first;
			const FileSet &kids = itr.second;

			div.AddRow("file = " + htmltool::get_file_html(parent.c_str()) + ", kid num = " + htmltool::get_number_html(kids.size()), 2);

			for (FileID kid : kids)
			{
				div.AddRow("kid by same = " + DebugBeIncludeText(kid), 3);
			}

			div.AddRow("");
		}
	}

	// ��ȡ���ļ��ı�������Ϣ���������ݰ��������ļ��������ļ����������ļ�#include���кš������ļ�#include��ԭʼ�ı���
	string ParsingFile::DebugBeIncludeText(FileID file) const
	{
		const char *fileName = GetFileNameInCache(file);
		if (file == m_root)
		{
			return strtool::get_text(cn_main_file_debug_text, htmltool::get_file_html(fileName).c_str(),
			                         file.getHashValue(), htmltool::get_number_html(GetDeepth(file)).c_str());
		}
		else
		{
			stringstream ancestors;

			for (FileID parent = GetParent(file), child = file; parent.isValid();)
			{
				string includeLineText = strtool::get_text(cn_file_include_line, htmltool::get_min_file_name_html(GetFileNameInCache(parent)).c_str(),
				                         strtool::itoa(GetIncludeLineNo(child)).c_str(), htmltool::get_include_html(GetBeIncludeLineText(child)).c_str());

				ancestors << includeLineText;
				child = parent;

				parent = GetParent(parent);
				if (parent.isValid())
				{
					ancestors << "<-";
				}
			}

			return strtool::get_text(cn_file_debug_text, IsOuterFile(file) ? cn_outer_file_flag : "", htmltool::get_file_html(fileName).c_str(),
			                         file.getHashValue(), htmltool::get_number_html(GetDeepth(file)).c_str(), ancestors.str().c_str());
		}

		return "";
	}

	// ��ȡ�ļ���Ϣ
	std::string ParsingFile::DebugParentFileText(FileID file, int n) const
	{
		return strtool::get_text(cn_parent_file_debug_text, DebugBeIncludeText(file).c_str(), htmltool::get_number_html(n).c_str());
	}

	// ��ȡ��λ�������е���Ϣ�������е��ı��������ļ������к�
	string ParsingFile::DebugLocText(SourceLocation loc) const
	{
		string lineText = GetSourceOfLine(loc);
		std::stringstream text;
		text << "[" << htmltool::get_include_html(lineText) << "] in [";
		text << htmltool::get_file_html(GetFileNameInCache(GetFileID(loc)));
		text << "] line = " << htmltool::get_number_html(GetLineNo(loc)) << " col = " << htmltool::get_number_html(m_srcMgr->getSpellingColumnNumber(loc));
		return text.str();
	}

	// ��ȡ�ļ�����ͨ��clang��ӿڣ��ļ���δ�������������Ǿ���·����Ҳ���������·����
	// ���磺
	//     ���ܷ��ؾ���·����d:/a/b/hello.h
	//     Ҳ���ܷ��أ�./../hello.h
	inline const char* ParsingFile::GetFileName(FileID file) const
	{
		const FileEntry *fileEntry = m_srcMgr->getFileEntryForID(file);
		return fileEntry ? fileEntry->getName() : "";
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
	inline string ParsingFile::GetAbsoluteFileName(FileID file) const
	{
		const char* raw_file_name = GetFileName(file);
		return pathtool::get_absolute_path(raw_file_name);
	}

	// ��ȡ�ļ��ľ���·�����ӻ����л�ȡ���ٶȱ�ÿ�����¼����30����
	inline const char* ParsingFile::GetFileNameInCache(FileID file) const
	{
		auto itr = m_fileNames.find(file);
		return itr != m_fileNames.end() ? itr->second.c_str() : "";
	}

	// ��ȡ�ļ���Сд����·��
	inline const char* ParsingFile::GetLowerFileNameInCache(FileID file) const
	{
		auto itr = m_lowerFileNames.find(file);
		return itr != m_lowerFileNames.end() ? itr->second.c_str() : "";
	}

	// ���ڵ��ԣ���ȡ�ļ��ľ���·���������Ϣ
	string ParsingFile::GetDebugFileName(FileID file) const
	{
		stringstream name;
		stringstream ancestors;

		const char *fileNameInCache = GetFileNameInCache(file);
		string fileName = (IsOuterFile(file) ? fileNameInCache : pathtool::get_file_name(fileNameInCache));

		name << "[" << fileName << "]";

		for (FileID parent = GetParent(file), child = file; parent.isValid(); )
		{
			ancestors << pathtool::get_file_name(GetFileNameInCache(parent));
			ancestors << "=" << GetIncludeLineNo(child);

			child = parent;
			parent = GetParent(child);
			if (parent.isValid())
			{
				ancestors << ",";
			}
		}

		if (!ancestors.str().empty())
		{
			name << "(" << ancestors.str() << ")";
		}

		return name.str();
	}

	// ��ȡ��1���ļ�#include��2���ļ����ı���
	std::string ParsingFile::GetRelativeIncludeStr(FileID f1, FileID f2) const
	{
		// ����2���ļ��ı�������ԭ������#include <xxx>�ĸ�ʽ���򷵻�ԭ����#include��
		{
			string rawInclude2 = GetBeIncludeLineText(f2);
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

		string absolutePath1 = GetFileNameInCache(f1);
		string absolutePath2 = GetFileNameInCache(f2);

		string include2;

		// �����ж�2���ļ��Ƿ���ͬһ�ļ�����
		if (tolower(get_dir(absolutePath1)) == tolower(get_dir(absolutePath2)))
		{
			include2 = "\"" + strip_dir(absolutePath2) + "\"";
		}
		else
		{
			// ��ͷ�ļ�����·������Ѱ��2���ļ������ɹ��ҵ����򷵻ػ���ͷ�ļ�����·�������·��
			include2 = GetQuotedIncludeStr(absolutePath2.c_str());

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
		// �Ƿ񸲸�c++Դ�ļ�
		if (!Project::instance.m_isOverWrite)
		{
			return;
		}

		if (Project::instance.m_canCleanAll)
		{
			// ��������c++�ļ�
			CleanAllFile();
		}
		else
		{
			// ������ǰcpp�ļ�
			CleanMainFile();
		}

		// ���䶯��д��c++�ļ�
		bool err = Overwrite();
		if (err)
		{
			LogError("overwrite some changed files failed!");
		}
	}

	// ����������д��c++Դ�ļ������أ�true��д�ļ�ʱ�������� false��д�ɹ�
	// �����ӿڲο���Rewriter::overwriteChangedFiles��
	bool ParsingFile::Overwrite()
	{
		class CxxCleanReWriter
		{
		public:
			// ���ļ���ӿ�дȨ��
			static bool EnableWrite(const char *file_name)
			{
				struct stat s;
				int err = stat(file_name, &s);
				if (err > 0)
				{
					return false;
				}

				err = _chmod(file_name, s.st_mode|S_IWRITE);
				return err == 0;
			}

			// �����ļ�
			static bool WriteToFile(const RewriteBuffer &rewriteBuffer, const char *fileName)
			{
				if (!EnableWrite(fileName))
				{
					LogError("overwrite file [" << fileName << "] failed: has no permission");
					return false;
				}

				std::ofstream fout;
				fout.open(fileName, ios_base::out | ios_base::binary);

				if (!fout.is_open())
				{
					LogError("overwrite file [" << fileName << "] failed: can not open file, error code = " << errno <<" "<<strerror(errno));
					return false;
				}

				std::stringstream ss;
				for (RopePieceBTreeIterator itr = rewriteBuffer.begin(), end = rewriteBuffer.end(); itr != end; itr.MoveToNextPiece())
				{
					ss << itr.piece().str();
				}

				fout << ss.str();
				fout.close();
				return true;
			}
		};

		bool isAllOk = true;
		for (Rewriter::buffer_iterator itr = m_rewriter.buffer_begin(), end = m_rewriter.buffer_end(); itr != end; ++itr)
		{
			FileID fileID = itr->first;

			const RewriteBuffer &rewriteBuffer	= itr->second;

			LogInfoByLvl(LogLvl_2, "overwriting " << GetDebugFileName(fileID) << " ...");

			bool ok = CxxCleanReWriter::WriteToFile(rewriteBuffer, GetFileNameInCache(fileID));
			if (!ok)
			{
				LogError("overwrite file" << GetDebugFileName(fileID) << " failed!");
				isAllOk = false;
			}
			else
			{
				LogInfoByLvl(LogLvl_2, "overwriting " << GetDebugFileName(fileID) << " success!");
			}
		}

		return !isAllOk;
	}

	// �滻ָ����Χ�ı�
	void ParsingFile::ReplaceText(FileID file, int beg, int end, const char* text)
	{
		if (strtool::is_empty(text))
		{
			return;
		}

		SourceLocation fileBegLoc	= m_srcMgr->getLocForStartOfFile(file);
		SourceLocation begLoc		= fileBegLoc.getLocWithOffset(beg);
		SourceLocation endLoc		= fileBegLoc.getLocWithOffset(end);

		if (nullptr == GetSourceAtLoc(begLoc) || nullptr == GetSourceAtLoc(endLoc))
		{
			LogError("nullptr == GetSourceAtLoc(begLoc) || nullptr == GetSourceAtLoc(endLoc)");
			return;
		}

		LogInfoByLvl(LogLvl_2, "replace [" << GetFileNameInCache(file) << "]: [" << beg << "," << end << "] to text = [" << text << "]");

		bool err = m_rewriter.ReplaceText(begLoc, end - beg, text);
		if (err)
		{
			LogError("replace [" << GetDebugFileName(file) << "]: [" << beg << "," << end << "] to text = [" << text << "] failed");
		}
	}

	// ���ı����뵽ָ��λ��֮ǰ
	// ���磺������"abcdefg"�ı�������c������123�Ľ�����ǣ�"ab123cdefg"
	void ParsingFile::InsertText(FileID file, int loc, const char* text)
	{
		if (strtool::is_empty(text))
		{
			return;
		}

		SourceLocation fileBegLoc	= m_srcMgr->getLocForStartOfFile(file);
		SourceLocation insertLoc	= fileBegLoc.getLocWithOffset(loc);

		if (nullptr == GetSourceAtLoc(insertLoc))
		{
			LogError("nullptr == GetSourceAtLoc(insertLoc)");
			return;
		}

		LogInfoByLvl(LogLvl_2, "insert [" << GetFileNameInCache(file) << "]: [" << loc << "] to text = [" << text << "]");

		bool err = m_rewriter.InsertText(insertLoc, text, false, false);
		if (err)
		{
			LogError("insert [" << GetDebugFileName(file) << "]: [" << loc << "] to text = [" << text << "] failed");
		}
	}

	// �Ƴ�ָ����Χ�ı������Ƴ��ı�����б�Ϊ���У��򽫸ÿ���һ���Ƴ�
	void ParsingFile::RemoveText(FileID file, int beg, int end)
	{
		SourceLocation fileBegLoc	= m_srcMgr->getLocForStartOfFile(file);
		if (fileBegLoc.isInvalid())
		{
			LogError("if (fileBegLoc.isInvalid()), remove text in [" << GetDebugFileName(file) << "] failed!");
			return;
		}

		SourceLocation begLoc	= fileBegLoc.getLocWithOffset(beg);
		SourceLocation endLoc	= fileBegLoc.getLocWithOffset(end);

		if (nullptr == GetSourceAtLoc(begLoc) || nullptr == GetSourceAtLoc(endLoc))
		{
			LogError("nullptr == GetSourceAtLoc(begLoc) || nullptr == GetSourceAtLoc(endLoc)");
			return;
		}

		SourceRange range(begLoc, endLoc);

		Rewriter::RewriteOptions rewriteOption;
		rewriteOption.IncludeInsertsAtBeginOfRange	= false;
		rewriteOption.IncludeInsertsAtEndOfRange	= false;
		rewriteOption.RemoveLineIfEmpty				= false;

		LogInfoByLvl(LogLvl_2, "remove [" << GetFileNameInCache(file) << "]: [" << beg << "," << end << "], text = [" << GetSourceOfRange(range) << "]");

		bool err = m_rewriter.RemoveText(range.getBegin(), end - beg, rewriteOption);
		if (err)
		{
			LogError("remove [" << GetDebugFileName(file) << "]: [" << beg << "," << end << "], text = [" << GetSourceOfRange(range) << "] failed");
		}
	}

	// �Ƴ�ָ���ļ��ڵ�������
	void ParsingFile::CleanByDelLine(const FileHistory &history, FileID file)
	{
		for (auto &itr : history.m_delLines)
		{
			int line = itr.first;
			const DelLine &delLine = itr.second;

			if (line > 0)
			{
				RemoveText(file, delLine.beg, delLine.end);
			}
		}
	}

	// ��ָ���ļ������ǰ������
	void ParsingFile::CleanByForward(const FileHistory &history, FileID file)
	{
		for (auto &itr : history.m_forwards)
		{
			int line = itr.first;
			const ForwardLine &forwardLine	= itr.second;

			if (line > 0)
			{
				std::stringstream text;

				for (const string &cxxRecord : forwardLine.classes)
				{
					text << cxxRecord;
					text << history.GetNewLineWord();
				}

				InsertText(file, forwardLine.offset, text.str().c_str());
			}
		}
	}

	// ��ָ���ļ����滻#include
	void ParsingFile::CleanByReplace(const FileHistory &history, FileID file)
	{
		const char *newLineWord = history.GetNewLineWord();

		for (auto &itr : history.m_replaces)
		{
			int line = itr.first;
			const ReplaceLine &replaceLine	= itr.second;

			// ���Ǳ�-include����ǿ�����룬����������Ϊ�滻��û��Ч��
			if (replaceLine.isSkip || line <= 0)
			{
				continue;
			}

			// �滻#include
			std::stringstream text;

			const ReplaceTo &replaceInfo = replaceLine.replaceTo;
			text << replaceInfo.newText;
			text << newLineWord;

			ReplaceText(file, replaceLine.beg, replaceLine.end, text.str().c_str());
		}
	}

	// ��ָ���ļ���������
	void ParsingFile::CleanByAdd(const FileHistory &history, FileID file)
	{
		for (auto &addItr : history.m_adds)
		{
			int line				= addItr.first;
			const AddLine &addLine	= addItr.second;

			if (line > 0)
			{
				std::stringstream text;

				for (const BeAdd &beAdd : addLine.adds)
				{
					text << beAdd.text;
					text << history.GetNewLineWord();
				}

				InsertText(file, addLine.offset, text.str().c_str());
			}
		}
	}

	// ������ʷ����ָ���ļ�
	void ParsingFile::CleanByHistory(const FileHistoryMap &historys)
	{
		std::map<std::string, FileID> nameToFileIDMap;

		// ������ǰcpp���ļ������ļ�FileID��mapӳ�䣨ע�⣺ͬһ���ļ����ܱ�������Σ�FileID�ǲ�һ���ģ�����ֻ������С��FileID��
		for (FileID file : m_files)
		{
			const char *name = GetLowerFileNameInCache(file);

			if (!Project::CanClean(name))
			{
				continue;
			}

			if (nameToFileIDMap.find(name) == nameToFileIDMap.end())
			{
				nameToFileIDMap.insert(std::make_pair(name, file));
			}
		}

		for (auto &itr : historys)
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

			if (!Project::CanClean(fileName))
			{
				continue;
			}

			if (history.m_isSkip || history.HaveFatalError())
			{
				continue;
			}

			// ���������ڵ�ǰcpp���ļ����ҵ���Ӧ���ļ�ID��ע�⣺ͬһ���ļ����ܱ�������Σ�FileID�ǲ�һ���ģ�����ȡ����������С��FileID��
			auto & findItr = nameToFileIDMap.find(fileName);
			if (findItr == nameToFileIDMap.end())
			{
				continue;
			}

			FileID file	= findItr->second;

			CleanByReplace(history, file);
			CleanByForward(history, file);
			CleanByDelLine(history, file);
			CleanByAdd(history, file);

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
		// ��ȡ�����ļ��Ĵ������¼
		auto &rootItr = ProjectHistory::instance.m_files.find(GetLowerFileNameInCache(m_root));
		if (rootItr != ProjectHistory::instance.m_files.end())
		{
			FileHistoryMap historys;
			historys.insert(*rootItr);

			CleanByHistory(historys);
		}
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
			div.AddRow("search path = " + htmltool::get_file_html(path.m_dir.c_str()), 2);
		}

		div.AddRow("");
	}

	// ���ڵ��ԣ���ӡ���ļ����õ��ļ�������ڸ��ļ���#include�ı�
	void ParsingFile::PrintRelativeInclude() const
	{
		HtmlDiv &div = HtmlLog::instance.m_newDiv;
		div.AddRow(AddPrintIdx() + ". relative include list : use = " + htmltool::get_number_html(m_uses.size()), 1);

		for (auto &itr : m_uses)
		{
			FileID file = itr.first;

			if (!CanClean(file))
			{
				continue;
			}

			const FileSet &be_uses = itr.second;

			div.AddRow("file = " + DebugBeIncludeText(file), 2);

			for (FileID be_used_file : be_uses)
			{
				div.AddRow("old include = " + htmltool::get_include_html(GetBeIncludeLineText(be_used_file)), 3, 45);
				div.AddGrid("-> relative include = " + htmltool::get_include_html(GetRelativeIncludeStr(file, be_used_file)));
			}

			div.AddRow("");
		}
	}

	// ��ӡ���� + 1
	std::string ParsingFile::AddPrintIdx() const
	{
		return strtool::itoa(++m_printIdx);
	}

	// ��ӡ��Ϣ
	void ParsingFile::Print()
	{
		LogLvl verbose = Project::instance.m_logLvl;
		if (verbose <= LogLvl_0)
		{
			return;
		}

		HtmlDiv &div = HtmlLog::instance.m_newDiv;

		div.AddTitle(strtool::get_text(cn_file_history_title,
		                               htmltool::get_number_html(ProjectHistory::instance.g_fileNum).c_str(),
		                               htmltool::get_number_html(Project::instance.m_cpps.size()).c_str(),
		                               htmltool::get_file_html(GetFileNameInCache(m_root)).c_str()));

		m_printIdx = 0;

		if (verbose >= LogLvl_1)
		{
			PrintCleanLog();
		}

		if (verbose >= LogLvl_3)
		{
			if (Project::IsCleanModeOpen(CleanMode_Need))
			{
				PrintMinUse();
				PrintMinKid();
				PrintUserUse();
				PrintForwardClass();
			}
			else
			{
				PrintTopRely();
				PrintRely();
				PrintRelyChildren();
			}

			PrintUse();
			PrintUseName();
			PrintSameFile();
		}

		if (verbose >= LogLvl_4)
		{
			if (Project::IsCleanModeOpen(CleanMode_Need))
			{
				PrintOutFileAncestor();
				PrintUserKids();
				PrintKidsBySame();
				PrintUseRecord();
			}

			PrintAllFile();
			PrintInclude();
			PrintHeaderSearchPath();
			PrintRelativeInclude();
			PrintParent();
			PrintKids();
			PrintNamespace();
			PrintUsingNamespace();
		}

		HtmlLog::instance.AddDiv(div);
	}

	// �ϲ��ɱ��Ƴ���#include��
	void ParsingFile::MergeUnusedLine(const FileHistory &newFile, FileHistory &oldFile) const
	{
		FileHistory::DelLineMap &oldLines = oldFile.m_delLines;

		for (FileHistory::DelLineMap::iterator oldLineItr = oldLines.begin(); oldLineItr != oldLines.end(); )
		{
			int line			= oldLineItr->first;
			DelLine &oldLine	= oldLineItr->second;

			if (newFile.IsLineUnused(line))
			{
				LogInfoByLvl(LogLvl_3, "merge unused at [" << oldFile.m_filename << "]: new line unused, old line unused at line = " << line << " -> " << oldLine.text );
				++oldLineItr;
			}
			else
			{
				// �������ļ��и���Ӧ���滻�����ھ��ļ��и���Ӧ��ɾ�������ھ��ļ��������滻��¼
				if (newFile.IsLineBeReplaced(line))
				{
					auto & newReplaceLineItr = newFile.m_replaces.find(line);
					const ReplaceLine &newReplaceLine = newReplaceLineItr->second;

					oldFile.m_replaces[line] = newReplaceLine;

					LogInfoByLvl(LogLvl_3, "merge unused at [" << oldFile.m_filename << "]: new line replace, old line unused at line = " << line << " -> " << oldLine.text );
				}
				else
				{
					LogInfoByLvl(LogLvl_3, "merge unused at [" << oldFile.m_filename << "]: new line do nothing, old line unused at line = " << line << " -> " << oldLine.text );
				}

				oldLines.erase(oldLineItr++);
			}
		}
	}

	// �ϲ����滻��#include
	void ParsingFile::MergeReplaceLine(const FileHistory &newFile, FileHistory &oldFile) const
	{
		if (oldFile.m_replaces.empty() && newFile.m_replaces.empty())
		{
			// ���ļ���û����Ҫ�滻��#include
			return;
		}

		FileID newFileID;
		auto & newFileItr = m_splitReplaces.begin();

		for (auto &itr = m_splitReplaces.begin(); itr != m_splitReplaces.end(); ++itr)
		{
			FileID top = itr->first;

			if (is_same_ignore_case(oldFile.m_filename, GetLowerFileNameInCache(top)))
			{
				newFileID	= top;
				newFileItr	= itr;
				break;
			}
		}

		// 1. �ϲ������滻��¼����ʷ��¼��
		for (auto & oldLineItr = oldFile.m_replaces.begin(), end = oldFile.m_replaces.end(); oldLineItr != end; )
		{
			int line				= oldLineItr->first;
			ReplaceLine &oldLine	= oldLineItr->second;

			// �����Ƿ�����ͻ��
			auto &conflictItr	= newFile.m_replaces.find(line);
			bool is_conflict	= (conflictItr != newFile.m_replaces.end());

			// ��������ͻ����������µ�ȡ���ļ���ɵ�ȡ���ļ��Ƿ���ֱϵ�����ϵ
			if (is_conflict)
			{
				const ReplaceLine &newLine	= conflictItr->second;

				// ��2���滻һ������
				if (newLine.replaceTo.newText == oldLine.replaceTo.newText)
				{
					++oldLineItr;
					continue;
				}

				// ˵�����ļ�δ�������κ��滻��¼����ʱӦ�����ļ������о��滻��¼ȫ��ɾ��
				if (newFileID.isInvalid())
				{
					LogInfoByLvl(LogLvl_3, "merge repalce at [" << oldFile.m_filename << "]: error, not found newFileID at line = " << line << " -> " << oldLine.oldText );

					++oldLineItr;
					continue;
				}

				const ReplaceMap &newReplaceMap	= newFileItr->second;

				// �ҵ����оɵ�#include��Ӧ��FileID
				FileID beReplaceFileID;

				for (auto & childItr : newReplaceMap)
				{
					FileID child = childItr.first;
					if (GetLowerFileNameInCache(child) == newLine.oldFile)
					{
						beReplaceFileID = child;
						break;
					}
				}

				if (beReplaceFileID.isInvalid())
				{
					++oldLineItr;
					continue;
				}

				// ���ɵ�ȡ���ļ����µ�ȡ���ļ������ȣ������ɵ��滻��Ϣ
				if (IsAncestor(beReplaceFileID, oldLine.oldFile.c_str()))
				{
					LogInfoByLvl(LogLvl_3, "merge repalce at [" << oldFile.m_filename << "]: old line replace is new line replace ancestorat line = " << line << " -> " << oldLine.oldText );
					++oldLineItr;
				}
				// ���µ�ȡ���ļ��Ǿɵ�ȡ���ļ������ȣ����Ϊʹ���µ��滻��Ϣ
				else if(IsAncestor(oldLine.oldFile.c_str(), beReplaceFileID))
				{
					LogInfoByLvl(LogLvl_3, "merge repalce at [" << oldFile.m_filename << "]: new line replace is old line replace ancestor at line = " << line << " -> " << oldLine.oldText );

					oldLine.replaceTo = newLine.replaceTo;
					++oldLineItr;
				}
				// ������û��ֱϵ��ϵ��������޷����滻��ɾ������ԭ�е��滻��¼
				else
				{
					LogInfoByLvl(LogLvl_3, "merge repalce at [" << oldFile.m_filename << "]: old line replace, new line replace at line = " << line << " -> " << oldLine.oldText );

					SkipRelyLines(oldLine.replaceTo.m_rely);
					oldFile.m_replaces.erase(oldLineItr++);
				}
			}
			else
			{
				// �������ļ��и���Ӧ��ɾ�������ھ��ļ��и���Ӧ���滻���������ļ����滻��¼
				if (newFile.IsLineUnused(line))
				{
					LogInfoByLvl(LogLvl_3, "merge repalce at [" << oldFile.m_filename << "]: old line replace, new line delete at line = " << line << " -> " << oldLine.oldText );

					++oldLineItr;
					continue;
				}
				// ������û���µ��滻��¼��˵�������޷����滻��ɾ�����оɵ��滻��¼
				else
				{
					LogInfoByLvl(LogLvl_3, "merge repalce at [" << oldFile.m_filename << "]: old line replace, new line do nothing at line = " << line << " -> " << oldLine.oldText );

					SkipRelyLines(oldLine.replaceTo.m_rely);
					oldFile.m_replaces.erase(oldLineItr++);
				}
			}
		}

		// 2. �������滻��¼����ֱ�ӱ�ɾ��Ҫ�����
		for (auto &newLineItr : newFile.m_replaces)
		{
			int line					= newLineItr.first;
			const ReplaceLine &newLine	= newLineItr.second;

			if (!oldFile.IsLineBeReplaced(line))
			{
				SkipRelyLines(newLine.replaceTo.m_rely);
			}
		}
	}

	// ��ĳЩ�ļ��е�һЩ�б��Ϊ�����޸�
	void ParsingFile::SkipRelyLines(const FileSkipLineMap &skips) const
	{
		for (auto &itr : skips)
		{
			ProjectHistory::instance.m_skips.insert(itr);
		}
	}

	// ����ǰcpp�ļ������Ĵ������¼��֮ǰ����cpp�ļ������Ĵ������¼�ϲ�
	void ParsingFile::MergeTo(FileHistoryMap &oldFiles) const
	{
		const FileHistoryMap &newFiles = m_historys;

		// �����ļ������ر������������������࣬����ϲ����������ʷ�����ڴ�ӡ
		if (m_compileErrorHistory.HaveFatalError())
		{
			const char *rootFile = GetLowerFileNameInCache(m_root);

			auto itr = newFiles.find(rootFile);
			if (itr == newFiles.end())
			{
				return;
			}

			oldFiles[rootFile] = itr->second;
			return;
		}

		for (auto & fileItr : newFiles)
		{
			const string &fileName	= fileItr.first;
			const FileHistory &newFile	= fileItr.second;

			auto &findItr = oldFiles.find(fileName);

			bool found = (findItr != oldFiles.end());
			if (found)
			{
				if (Project::IsCleanModeOpen(CleanMode_Need))
				{
					continue;
				}

				FileHistory &oldFile = findItr->second;

				MergeUnusedLine(newFile, oldFile);
				MergeReplaceLine(newFile, oldFile);

				oldFile.Fix();
			}
			else
			{
				oldFiles[fileName] = newFile;
			}
		}
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

	void ParsingFile::InitHistory(FileID file, FileHistory &history) const
	{
		history.m_isWindowFormat	= IsWindowsFormat(file);
		history.m_filename			= GetFileNameInCache(file);
		history.m_isSkip			= IsPrecompileHeader(file);
	}

	// ȡ�������ļ��Ŀ�ɾ��#include��
	void ParsingFile::TakeDel(FileHistory &history, const FileSet &dels) const
	{
		for (FileID del : dels)
		{
			int line = GetIncludeLineNo(del);
			if (line <= 0)
			{
				continue;
			}

			SourceRange lineRange	= GetIncludeRange(del);

			DelLine &delLine	= history.m_delLines[line];

			delLine.beg		= m_srcMgr->getFileOffset(lineRange.getBegin());
			delLine.end		= m_srcMgr->getFileOffset(lineRange.getEnd());
			delLine.text	= GetSourceOfLine(lineRange.getBegin());

			if (Project::instance.m_logLvl >= LogLvl_2)
			{
				SourceRange nextLine = GetNextLine(m_srcMgr->getIncludeLoc(del));
				LogInfo("TakeDel [" << history.m_filename << "]: line = " << line << "[" << delLine.beg << "," << m_srcMgr->getFileOffset(nextLine.getBegin())
				        << "," << delLine.end << "," << m_srcMgr->getFileOffset(nextLine.getEnd()) << "], text = [" << delLine.text << "]");
			}
		}
	}

	void ParsingFile::TakeReplaceLine(ReplaceLine &replaceLine, FileID from, FileID to) const
	{
		// 1. ���еľ��ı�
		SourceRange	includeRange	= GetIncludeRange(from);
		replaceLine.oldFile			= GetLowerFileNameInCache(from);
		replaceLine.oldText			= GetBeIncludeLineText(from);
		replaceLine.beg				= m_srcMgr->getFileOffset(includeRange.getBegin());
		replaceLine.end				= m_srcMgr->getFileOffset(includeRange.getEnd());
		replaceLine.isSkip			= IsSkip(from);	// �����Ƿ���ǿ�ư���

		// 2. ���пɱ��滻��ʲô
		ReplaceTo &replaceTo	= replaceLine.replaceTo;

		// ��¼[��#include����#include]
		replaceTo.oldText		= GetBeIncludeLineText(to);
		replaceTo.newText		= GetRelativeIncludeStr(GetParent(from), to);

		// ��¼[�������ļ��������к�]
		replaceTo.line			= GetIncludeLineNo(to);
		replaceTo.fileName		= GetFileNameInCache(to);
		replaceTo.inFile		= GetFileNameInCache(GetParent(to));
	}

	void ParsingFile::TakeForwardClass(FileHistory &history, FileID insertAfter, FileID top) const
	{
		auto &useRecordItr = m_fowardClass.find(top);
		if (useRecordItr == m_fowardClass.end())
		{
			return;
		}

		const RecordSet &cxxRecords = useRecordItr->second;
		for (const CXXRecordDecl *cxxRecord : cxxRecords)
		{
			SourceRange includeRange = GetIncludeRange(insertAfter);
			SourceLocation insertLoc = includeRange.getEnd();
			if (insertLoc.isInvalid())
			{
				LogErrorByLvl(LogLvl_2, "insertLoc.isInvalid(), " << GetDebugFileName(top) << ", record = " << GetRecordName(*cxxRecord));
				continue;
			}

			// ��ʼȡ������
			int line = GetLineNo(insertLoc);

			ForwardLine &forwardLine = history.m_forwards[line];
			forwardLine.offset	= m_srcMgr->getFileOffset(insertLoc);
			forwardLine.oldText	= GetSourceOfLine(includeRange.getBegin());

			forwardLine.classes.insert(GetRecordName(*cxxRecord));
		}
	}

	void ParsingFile::TakeAdd(FileHistory &history, FileID top, FileID insertAfter, const FileSet &adds) const
	{
		for (FileID add : adds)
		{
			FileID lv2 = GetLvl2AncestorBySame(add, top);
			if (lv2.isInvalid())
			{
				LogErrorByLvl(LogLvl_3, "lv2.isInvalid(): " << GetDebugFileName(add) << ", " << GetDebugFileName(top));
				continue;
			}

			if (IsForceIncluded(lv2))
			{
				lv2 = insertAfter;
			}

			int line = GetIncludeLineNo(lv2);

			AddLine &addLine = history.m_adds[line];
			if (addLine.offset <= 0)
			{
				addLine.offset	= m_srcMgr->getFileOffset(GetIncludeRange(lv2).getEnd());
				addLine.oldText	= GetBeIncludeLineText(lv2);
			}

			BeAdd beAdd;
			beAdd.fileName	= GetFileNameInCache(add);
			beAdd.text		= GetRelativeIncludeStr(top, add);

			addLine.adds.push_back(beAdd);
		}
	}

	// �����Ӧ���ĸ��ļ���Ӧ��#include�������ı�
	FileID ParsingFile::CalcInsertLoc(const FileSet &includes, const FileSet &dels, const std::map<FileID, FileID> &replaces) const
	{
		// �ҵ����ļ���һ�����޸ĵ�#include��λ��
		FileID firstInclude;
		int firstIncludeLineNo = 0;

		auto SearchFirstInclude = [&](FileID a)
		{
			int line = GetIncludeLineNo(a);
			if ((firstIncludeLineNo == 0 || line < firstIncludeLineNo) && line > 0)
			{
				firstIncludeLineNo	= line;
				firstInclude		= a;
			}
		};

		for (FileID del : dels)
		{
			SearchFirstInclude(del);
		}

		for (auto &itr : replaces)
		{
			SearchFirstInclude(itr.first);
		}

		if (firstIncludeLineNo <= 0)
		{
			for (FileID a : includes)
			{
				SearchFirstInclude(a);
			}
		}

		return firstInclude;
	}

	// ȡ����¼��ʹ�ø��ļ��������Լ�����Ҫ��ͷ�ļ�
	void ParsingFile::TakeNeed(FileID top, FileHistory &history) const
	{
		InitHistory(top, history);
		history.m_isSkip = false;

		auto includeItr = m_includes.find(top);
		if (includeItr == m_includes.end())
		{
			return;
		}

		//--------------- һ���ȷ����ļ�����Щ#include����Ҫ��ɾ�����滻������Ҫ������Щ#include ---------------//

		// ����Ӧ�������ļ��б�
		FileSet kids;

		auto itr = m_min.find(top);
		if (itr != m_min.end())
		{
			kids = std::move(itr->second);
		}
		else
		{
			LogInfoByLvl(LogLvl_3, "not in m_min, don't need any include [top] = " << GetDebugFileName(top));
		}

		// �ɵİ����ļ��б�
		FileSet oldIncludes	= includeItr->second;

		// �µİ����ļ��б�
		FileSet newIncludes	= kids;

		// Ӧ���
		FileSet adds;

		// Ӧɾ��
		FileSet dels;

		// Ӧ�滻��<���滻��Ӧ�滻��>
		std::map<FileID, FileID> replaces;

		// 1. �ҵ��¡����ļ������б���[id��ͬ�����ļ�����ͬ���ļ�]��һ�Զ�����
		for (FileID kid : kids)
		{
			bool isSame = false;

			if (oldIncludes.find(kid) != oldIncludes.end())
			{
				oldIncludes.erase(kid);
				newIncludes.erase(kid);
			}
			else
			{
				const std::string kidName = GetLowerFileNameInCache(kid);

				for (FileID beInclude : oldIncludes)
				{
					if (kidName == GetLowerFileNameInCache(beInclude))
					{
						oldIncludes.erase(beInclude);
						newIncludes.erase(kid);
						break;
					}
				}
			}
		}

		// 2. �ҵ��¡����ļ������б���[�������Ⱥ����ϵ���ļ�]��һ�ԶԼ����滻����
		for (FileID beInclude : oldIncludes)
		{
			for (FileID kid : newIncludes)
			{
				FileID lv2Ancestor = GetLvl2AncestorBySame(kid, beInclude);
				if (lv2Ancestor.isValid())
				{
					replaces[beInclude] = kid;
					newIncludes.erase(kid);

					break;
				}
			}
		}

		Del(oldIncludes, replaces);

		// 3. ����¾��ļ��б���ܻ�ʣ��һЩ�ļ����������ǣ�ֱ��ɾ���ɵġ�����µ�
		dels = std::move(oldIncludes);
		adds = std::move(newIncludes);

		//--------------- ������ʼȡ��������� ---------------//

		// 1. ȡ��ɾ��#include��¼
		TakeDel(history, dels);

		// 2. ȡ���滻#include��¼
		for (auto &itr : replaces)
		{
			FileID from	= itr.first;
			FileID to	= itr.second;

			int line	= GetIncludeLineNo(from);

			ReplaceLine &replaceLine = history.m_replaces[line];
			TakeReplaceLine(replaceLine, from, to);
		}

		// 3. ȡ������ǰ��������¼
		FileID insertAfter = CalcInsertLoc(includeItr->second, dels, replaces);

		TakeForwardClass(history, insertAfter, top);

		// 4. ȡ������#include��¼
		TakeAdd(history, top, insertAfter, adds);
	}

	// ȡ���Ե�ǰcpp�ļ��ķ������
	void ParsingFile::TakeHistorys(FileHistoryMap &out) const
	{
		if (Project::IsCleanModeOpen(CleanMode_Need))
		{
			for (FileID top : m_files)
			{
				if (IsOuterFile(top))
				{
					continue;
				}

				const char *fileName = GetLowerFileNameInCache(top);
				if (!Has(out, fileName))
				{
					TakeNeed(top, out[fileName]);
				}
			}

			TakeCompileErrorHistory(out);
		}
		else
		{
			// �Ƚ���ǰcpp�ļ�ʹ�õ����ļ�ȫ����map��
			for (FileID file : m_relys)
			{
				const char *fileName = GetLowerFileNameInCache(file);
				if (Project::CanClean(fileName))
				{
					InitHistory(file, out[fileName]);
				}
			}

			// 1. ����������а��ļ����д��
			TakeUnusedLine(out);

			// 2. �����滻��#include���ļ����д��
			TakeReplace(out);

			// 3. ȡ�����ļ��ı��������ʷ
			TakeCompileErrorHistory(out);

			// 4. �޸�ÿ���ļ�����ʷ����ֹ��ͬһ���޸Ķ�ε��±���
			for (auto &itr : out)
			{
				FileHistory &history = itr.second;
				history.Fix();
			}
		}
	}

	// ����������а��ļ����д��
	void ParsingFile::TakeUnusedLine(FileHistoryMap &out) const
	{
		// 1. ����ɾ���ļ������ļ�����
		std::map<FileID, FileSet> delsByFile;

		for (FileID unusedFile : m_unusedFiles)
		{
			FileID atFile = GetParent(unusedFile);
			delsByFile[atFile].insert(unusedFile);
		}

		// 2. ȡ�����ļ��еĿ�ɾ����
		for (auto &itr : delsByFile)
		{
			FileID atFile = itr.first;
			const FileSet &dels = itr.second;

			const char *atFileName = GetLowerFileNameInCache(atFile);
			if (!Project::CanClean(atFileName))
			{
				continue;
			}

			auto &historyItr = out.find(atFileName);
			if (historyItr == out.end())
			{
				continue;
			}

			FileHistory &history = historyItr->second;
			TakeDel(history, dels);
		}
	}

	// ���ļ��Ƿ��Ǳ�-includeǿ�ư���
	inline bool ParsingFile::IsForceIncluded(FileID file) const
	{
		if (file == m_root)
		{
			return false;
		}

		FileID parent = GetFileID(m_srcMgr->getIncludeLoc(file));
		return (m_srcMgr->getFileEntryForID(parent) == nullptr);
	}

	// ���ļ��Ƿ���Ԥ����ͷ�ļ�
	bool ParsingFile::IsPrecompileHeader(FileID file) const
	{
		const std::string fileName = pathtool::get_file_name(GetLowerFileNameInCache(file));
		return fileName == "stdafx.h";
	}

	// ȡ��ָ���ļ���#include�滻��Ϣ
	void ParsingFile::TakeBeReplaceOfFile(FileHistory &history, FileID top, const ReplaceMap &childernReplaces) const
	{
		// ����ȡ��ÿ��#include���滻��Ϣ[�к� -> ���滻�ɵ�#include�б�]
		for (auto &itr : childernReplaces)
		{
			FileID from	= itr.first;
			FileID to	= itr.second;

			int line					= GetIncludeLineNo(from);
			ReplaceLine &replaceLine	= history.m_replaces[line];
			TakeReplaceLine(replaceLine, from, to);

			// 2. ���������������ļ�����Щ��
			ReplaceTo &replaceTo			= replaceLine.replaceTo;

			const char *replaceParentName	= GetLowerFileNameInCache(GetParent(to));
			int replaceLineNo				= GetIncludeLineNo(to);

			if (Project::CanClean(replaceParentName))
			{
				replaceTo.m_rely[replaceParentName].insert(replaceLineNo);
			}

			auto childItr = m_relyKids.find(to);
			if (childItr != m_relyKids.end())
			{
				const FileSet &relys = childItr->second;

				for (FileID rely : relys)
				{
					const char *relyParentName	= GetLowerFileNameInCache(GetParent(rely));
					int relyLineNo				= GetIncludeLineNo(rely);

					if (Project::CanClean(relyParentName))
					{
						replaceTo.m_rely[relyParentName].insert(relyLineNo);
					}
				}
			}
		}
	}

	// ȡ�����ļ���#include�滻��Ϣ
	void ParsingFile::TakeReplace(FileHistoryMap &out) const
	{
		if (m_replaces.empty())
		{
			return;
		}

		for (auto &itr : m_splitReplaces)
		{
			FileID top							= itr.first;
			const ReplaceMap &childrenReplaces	= itr.second;

			auto outItr = out.find(GetLowerFileNameInCache(top));
			if (outItr == out.end())
			{
				continue;
			}

			// �����滻��¼
			FileHistory &newFile = outItr->second;
			TakeBeReplaceOfFile(newFile, top, childrenReplaces);
		}
	}

	// ȡ�����ļ��ı��������ʷ
	void ParsingFile::TakeCompileErrorHistory(FileHistoryMap &out) const
	{
		FileHistory &history			= out[GetLowerFileNameInCache(m_root)];
		history.m_compileErrorHistory	= m_compileErrorHistory;
	}

	// ��ӡ���ü�¼
	void ParsingFile::PrintUse() const
	{
		HtmlDiv &div = HtmlLog::instance.m_newDiv;
		div.AddRow(AddPrintIdx() + ". list of use : use count = " + htmltool::get_number_html(m_uses.size()), 1);

		for (const auto &itr : m_uses)
		{
			FileID file = itr.first;

			if (!IsNeedPrintFile(file))
			{
				continue;
			}

			div.AddRow(DebugParentFileText(file, itr.second.size()), 2);

			for (FileID beuse : itr.second)
			{
				div.AddRow("use = " + DebugBeIncludeText(beuse), 3);
			}

			div.AddRow("");
		}
	}

	// ��ӡ#include��¼
	void ParsingFile::PrintInclude() const
	{
		HtmlDiv &div = HtmlLog::instance.m_newDiv;
		div.AddRow(AddPrintIdx() + ". list of include : include count = " + htmltool::get_number_html(m_includes.size()), 1);

		for (auto & includeList : m_includes)
		{
			FileID file = includeList.first;

			if (!CanClean(file))
			{
				continue;
			}

			div.AddRow(DebugParentFileText(file, includeList.second.size()), 2);

			for (FileID beinclude : includeList.second)
			{
				div.AddRow("include = " + DebugBeIncludeText(beinclude), 3);
			}

			div.AddRow("");
		}
	}

	// ��ӡ�����������������������ȵļ�¼
	void ParsingFile::PrintUseName() const
	{
		HtmlDiv &div = HtmlLog::instance.m_newDiv;
		div.AddRow(AddPrintIdx() + ". list of use name : use count = " + htmltool::get_number_html(m_useNames.size()), 1);

		for (auto & useItr : m_useNames)
		{
			FileID file									= useItr.first;
			const std::vector<UseNameInfo> &useNames	= useItr.second;

			if (!IsNeedPrintFile(file))
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
		div.AddRow(DebugParentFileText(file, useNames.size()), 2);

		for (const UseNameInfo &beuse : useNames)
		{
			div.AddRow("use = " + DebugBeIncludeText(beuse.file), 3);

			for (const string& name : beuse.nameVec)
			{
				std::stringstream linesStream;

				auto & linesItr = beuse.nameMap.find(name);
				if (linesItr != beuse.nameMap.end())
				{
					const std::set<int> &lines = linesItr->second;
					int n = (int)lines.size();
					int i = 0;
					for (int line : lines)
					{
						linesStream << htmltool::get_number_html(line);
						if (++i < n)
						{
							linesStream << ", ";
						}
					}
				}

				std::string linesText = linesStream.str();
				div.AddRow("name = " + name, 4, 70);
				div.AddGrid("lines = " + linesText, 30);
			}

			div.AddRow("");
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

	// ��ӡ���ļ��������ļ���
	void ParsingFile::PrintTopRely() const
	{
		HtmlDiv &div = HtmlLog::instance.m_newDiv;
		div.AddRow(AddPrintIdx() + ". list of top rely: file count = " + htmltool::get_number_html(m_topRelys.size()), 1);

		for (auto &file : m_topRelys)
		{
			if (!IsNeedPrintFile(file))
			{
				continue;
			}

			div.AddRow("file = " + DebugBeIncludeText(file), 2);
		}

		div.AddRow("");
	}

	// ��ӡ�����ļ���
	void ParsingFile::PrintRely() const
	{
		HtmlDiv &div = HtmlLog::instance.m_newDiv;
		div.AddRow(AddPrintIdx() + ". list of rely : file count = " + htmltool::get_number_html(m_relys.size()), 1);

		for (auto & file : m_relys)
		{
			if (!IsNeedPrintFile(file))
			{
				continue;
			}

			if (m_topRelys.find(file) == m_topRelys.end())
			{
				div.AddRow("<--------- new add rely file --------->", 2, 100, true);
			}

			div.AddRow("file = " + DebugBeIncludeText(file), 2);
		}

		div.AddRow("");
	}

	// ��ӡ���ļ���Ӧ�����ú����ļ���¼
	void ParsingFile::PrintRelyChildren() const
	{
		HtmlDiv &div = HtmlLog::instance.m_newDiv;
		div.AddRow(AddPrintIdx() + ". list of rely children file: file count = " + htmltool::get_number_html(m_relyKids.size()), 1);

		for (auto & usedItr : m_relyKids)
		{
			FileID file = usedItr.first;

			if (!CanClean(file))
			{
				continue;
			}

			div.AddRow(DebugParentFileText(file, usedItr.second.size()), 2);

			for (auto & usedChild : usedItr.second)
			{
				div.AddRow("use children " + DebugBeIncludeText(usedChild), 3);
			}

			div.AddRow("");
		}
	}

	// ��ӡ��תΪǰ����������ָ������ü�¼
	void ParsingFile::PrintUseRecord() const
	{
		// 1. ���ļ�ǰ��������¼���ļ����й���
		UseRecordsByFileMap recordMap;

		for (auto &itr : m_locUseRecordPointers)
		{
			SourceLocation loc	= itr.first;
			FileID file	= GetFileID(loc);
			recordMap[file].insert(itr);
		}
		recordMap.erase(FileID());

		// 2. ��ӡ
		HtmlDiv &div = HtmlLog::instance.m_newDiv;
		div.AddRow(AddPrintIdx() + ". use records list: file count = " + htmltool::get_number_html(recordMap.size()), 1);

		for (auto &itr : recordMap)
		{
			FileID file = itr.first;
			const LocUseRecordsMap &locToRecords = itr.second;

			div.AddRow(DebugParentFileText(file, locToRecords.size()), 2);

			for (auto &recordItr : locToRecords)
			{
				SourceLocation loc = recordItr.first;
				const std::set<const CXXRecordDecl*> &cxxRecords = recordItr.second;

				div.AddRow("at loc = " + DebugLocText(loc), 3);

				for (const CXXRecordDecl *record : cxxRecords)
				{
					div.AddRow("use record = " + GetRecordName(*record), 4);
				}

				div.AddRow("");
			}
		}
	}

	// ��ӡ���յ�ǰ��������¼
	void ParsingFile::PrintForwardClass() const
	{
		if (m_fowardClass.empty())
		{
			return;
		}

		HtmlDiv &div = HtmlLog::instance.m_newDiv;
		div.AddRow(AddPrintIdx() + ". final forward class list: file count = " + htmltool::get_number_html(m_fowardClass.size()), 1);

		for (auto &itr : m_fowardClass)
		{
			FileID by = itr.first;
			const RecordSet &records = itr.second;

			div.AddRow(DebugParentFileText(by, records.size()), 2);

			for (const CXXRecordDecl *record : records)
			{
				div.AddRow("add forward class = " + GetRecordName(*record), 3);
			}

			div.AddRow("");
		}
	}

	// ��ӡ��������������ļ��б�
	void ParsingFile::PrintAllFile() const
	{
		HtmlDiv &div = HtmlLog::instance.m_newDiv;
		div.AddRow(AddPrintIdx() + ". list of all file: file count = " + htmltool::get_number_html(m_files.size()), 1);

		for (FileID file : m_files)
		{
			if (!IsNeedPrintFile(file))
			{
				continue;
			}

			div.AddRow("file = " + DebugBeIncludeText(file), 2);
		}

		div.AddRow("");
	}

	// ��ӡ������־
	void ParsingFile::PrintCleanLog() const
	{
		HtmlDiv &div = HtmlLog::instance.m_newDiv;

		m_compileErrorHistory.Print();

		int i = 0;

		for (auto &itr : m_historys)
		{
			const FileHistory &history = itr.second;
			if (!history.IsNeedClean())
			{
				continue;
			}

			if (Project::instance.m_logLvl < LogLvl_3)
			{
				string ext = strtool::get_ext(history.m_filename);
				if (!cpptool::is_cpp(ext))
				{
					continue;
				}
			}

			history.Print(++i, false);
		}
	}

	// ��ӡ���ļ��ڵ������ռ�
	void ParsingFile::PrintNamespace() const
	{
		HtmlDiv &div = HtmlLog::instance.m_newDiv;
		div.AddRow(AddPrintIdx() + ". each file's namespace: file count = " + htmltool::get_number_html(m_namespaces.size()), 1);

		for (auto &itr : m_namespaces)
		{
			FileID file = itr.first;
			const std::set<std::string> &namespaces = itr.second;

			if (!IsNeedPrintFile(file))
			{
				continue;
			}

			div.AddRow(DebugParentFileText(file, namespaces.size()), 2);

			for (const std::string &ns : namespaces)
			{
				div.AddRow("declare namespace = " + htmltool::get_include_html(ns), 3);
			}

			div.AddRow("");
		}
	}

	// ��ӡ���ļ���Ӧ������using namespace
	void ParsingFile::PrintUsingNamespace() const
	{
		std::map<FileID, std::set<std::string>>	nsByFile;
		for (auto &itr : m_usingNamespaces)
		{
			SourceLocation loc		= itr.first;
			const NamespaceDecl	*ns	= itr.second;

			nsByFile[GetFileID(loc)].insert(ns->getQualifiedNameAsString());
		}

		HtmlDiv &div = HtmlLog::instance.m_newDiv;
		div.AddRow(AddPrintIdx() + ". each file's using namespace: file count = " + htmltool::get_number_html(nsByFile.size()), 1);

		for (auto &itr : nsByFile)
		{
			FileID file = itr.first;
			const std::set<std::string> &namespaces = itr.second;

			if (!IsNeedPrintFile(file))
			{
				continue;
			}

			div.AddRow(DebugParentFileText(file, namespaces.size()), 2);

			for (const std::string &ns : namespaces)
			{
				div.AddRow("using namespace = " + htmltool::get_include_html(ns), 3);
			}

			div.AddRow("");
		}
	}

	// ��ӡ��������ε��ļ�
	void ParsingFile::PrintSameFile() const
	{
		HtmlDiv &div = HtmlLog::instance.m_newDiv;
		div.AddRow(AddPrintIdx() + ". same file list: file count = " + htmltool::get_number_html(m_sameFiles.size()), 1);

		for (auto &itr : m_sameFiles)
		{
			const std::string &fileName			= itr.first;
			const FileSet sameFiles	= itr.second;

			div.AddRow("fileName = " + fileName, 2);

			for (FileID sameFile : sameFiles)
			{
				div.AddRow("same file = " + DebugBeIncludeText(sameFile), 3);
			}

			div.AddRow("");
		}
	}

	// ��ӡ
	void ParsingFile::PrintMinUse() const
	{
		HtmlDiv &div = HtmlLog::instance.m_newDiv;
		div.AddRow(strtool::get_text(cn_file_min_use, htmltool::get_number_html(++m_printIdx).c_str(), htmltool::get_number_html(m_min.size()).c_str()), 1);

		for (auto & kidItr : m_min)
		{
			FileID by = kidItr.first;

			if (!CanClean(by))
			{
				continue;
			}

			div.AddRow(DebugParentFileText(by, kidItr.second.size()), 2);

			for (FileID kid : kidItr.second)
			{
				div.AddRow("min use = " + DebugBeIncludeText(kid), 3);
			}

			div.AddRow("");
		}
	}

	void ParsingFile::PrintMinKid() const
	{
		HtmlDiv &div = HtmlLog::instance.m_newDiv;
		div.AddRow(strtool::get_text(cn_file_min_kid, htmltool::get_number_html(++m_printIdx).c_str(), htmltool::get_number_html(m_minKids.size()).c_str()), 1);

		for (auto & kidItr : m_minKids)
		{
			FileID by = kidItr.first;

			div.AddRow(DebugParentFileText(by, kidItr.second.size()), 2);

			for (FileID kid : kidItr.second)
			{
				div.AddRow("min kid = " + DebugBeIncludeText(kid), 3);
			}

			div.AddRow("");
		}
	}

	// ��ӡ
	void ParsingFile::PrintOutFileAncestor() const
	{
		HtmlDiv &div = HtmlLog::instance.m_newDiv;
		div.AddRow(strtool::get_text(cn_file_sys_ancestor, htmltool::get_number_html(++m_printIdx).c_str(), htmltool::get_number_html(m_outFileAncestor.size()).c_str()), 1);

		for (auto &itr : m_outFileAncestor)
		{
			FileID kid		= itr.first;
			FileID ancestor	= itr.second;

			div.AddRow("kid  = " + DebugBeIncludeText(kid), 2);
			div.AddRow("out file ancestor = " + DebugBeIncludeText(ancestor), 3);
			div.AddRow("");
		}
	}

	void ParsingFile::PrintUserUse() const
	{
		HtmlDiv &div = HtmlLog::instance.m_newDiv;
		div.AddRow(strtool::get_text(cn_file_user_use, htmltool::get_number_html(++m_printIdx).c_str(), htmltool::get_number_html(m_userUses.size()).c_str()), 1);

		for (auto &itr : m_userUses)
		{
			FileID file = itr.first;

			div.AddRow(DebugParentFileText(file, itr.second.size()), 2);

			for (FileID beuse : itr.second)
			{
				div.AddRow("be user use = " + DebugBeIncludeText(beuse), 3);
			}

			div.AddRow("");
		}
	}
}