///<------------------------------------------------------------------------------
//< @file:   whole_project.h
//< @author: ������
//< @date:   2016��2��22��
//< @brief:
//< Copyright (c) 2015 game. All rights reserved.
///<------------------------------------------------------------------------------

#ifndef _whole_project_h_
#define _whole_project_h_

#include <string>
#include <vector>
#include <set>
#include <map>

using namespace std;

namespace cxxcleantool
{
	class ParsingFile;

	struct UselessLineInfo
	{
		int		m_beg;
		int		m_end;
		string	m_text;
	};

	struct ForwardLine
	{
		int					m_offsetAtFile; // ����ĩ���ļ��ڵ�ƫ����
		string				m_oldText;
		std::set<string>	m_classes;

	};

	struct ReplaceInfo
	{
		string				m_newFile;		// ��#include��Ӧ���ļ�
		string				m_inFile;		// ��#includeԭ�����ڵ��ļ�
		int					m_line;			// ��#include���ڵ���
		string				m_oldText;		// �滻ǰ��#include������: #include "../b/../b/../a.h��
		string				m_newText;		// �滻���#include������: #include "../a.h��
	};

	struct ReplaceLine
	{
		bool						m_isSkip;		// ��¼�����滻�Ƿ�Ӧ����������Ϊ��Щ#include�Ǳ�-include����������ģ����޷����滻������Ȼ�д�ӡ�ı�Ҫ
		int							m_beg;
		int							m_end;
		string						m_oldText;		// �滻ǰ��#include�ı�����: #include "../b/../b/../a.h��
		string						m_oldFile;		// �滻ǰ��#include��Ӧ���ļ�
		std::vector<ReplaceInfo>	m_newInclude;	// �滻���#include���б�
	};

	struct EachFile
	{
		EachFile()
			: m_oldBeIncludeCount(0)
			, m_newBeIncludeCount(0)
			, m_beUseCount(0)
			, m_isWindowFormat(false)
		{
		}

		bool is_line_unused(int line) const
		{
			return m_unusedLines.find(line) != m_unusedLines.end();
		}

		bool is_line_be_replaced(int line) const
		{
			return m_replaces.find(line) != m_replaces.end();
		}

		bool is_line_add_forward(int line) const
		{
			return m_forwards.find(line) != m_forwards.end();
		}

		typedef std::map<int, UselessLineInfo> UnusedLineMap;
		typedef std::map<int, ForwardLine> ForwardLineMap;
		typedef std::map<int, ReplaceLine> ReplaceLineMap;

		bool				m_isWindowFormat;	// ���ļ��Ƿ���Windows��ʽ�Ļ��з�[\r\n]������Ϊ��Unix��ʽ[\n]��ͨ���ļ���һ�л��з����жϣ�

		UnusedLineMap		m_unusedLines;
		ForwardLineMap		m_forwards;
		ReplaceLineMap		m_replaces;
		std::string			m_filename;

		int					m_newBeIncludeCount;
		int					m_oldBeIncludeCount;
		int					m_beUseCount;
	};

	typedef std::map<string, EachFile> FileMap;

	class WholeProject
	{
		WholeProject()
			: m_isFirst(true)
			, m_isOptimized(0)
			, m_onlyHas1File(false)
		{
		}

	public:
		// ����ѱ�ʹ�õ�include����������δ��ʹ�ù���include
		void clear_used_include(const FileMap &newFiles)
		{
			for (auto fileItr : newFiles)
			{
				const string &fileName	= fileItr.first;
				const EachFile &newFile	= fileItr.second;

				auto findItr = m_files.find(fileName);

				bool found = (findItr != m_files.end());
				if (!found)
				{
					m_files[fileName] = newFile;
				}
				else
				{
					EachFile &oldFile = findItr->second;

					EachFile::UnusedLineMap &unusedLines = oldFile.m_unusedLines;

					for (EachFile::UnusedLineMap::iterator lineItr = unusedLines.begin(), end = unusedLines.end(); lineItr != end; )
					{
						int line = lineItr->first;

						if (newFile.is_line_unused(line))
						{
							++lineItr;
						}
						else
						{
							unusedLines.erase(lineItr++);
						}
					}
				}
			}
		}

		void on_cleaned(const string &file)
		{
			// llvm::outs() << "on_cleaned file: " << file << " ...\n";

			m_cleanedFiles.insert(file);
		}

		bool has_cleaned(const string &file)
		{
			return m_cleanedFiles.find(file) != m_cleanedFiles.end();
		}

		bool has_file(const string &file)
		{
			return m_files.find(file) != m_files.end();
		}

		bool need_clean(const char* file)
		{
			auto itr = m_files.find(file);

			bool found = (itr != m_files.end());
			if (!found)
			{
				return false;
			}

			EachFile &eachFile = itr->second;
			return !eachFile.m_unusedLines.empty();
		}

		void AddFile(ParsingFile *file);
		void print_unused_line();
		void print_can_forwarddecl_class();
		void print_replace();
		void print_count();
		void print();

	public:
		static WholeProject instance;

		bool m_isOptimized;
		bool m_isFirst;
		bool m_onlyHas1File;
		FileMap m_files;
		std::set<string> m_cleanedFiles;
	};
}

#endif // _whole_project_h_