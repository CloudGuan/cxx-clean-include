///<------------------------------------------------------------------------------
//< @file:   whole_project.h
//< @author: ������
//< @date:   2016��2��22��
//< @brief:
//< Copyright (c) 2016. All rights reserved.
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
		int							m_beg;			// ��ʼƫ��
		int							m_end;			// ����ƫ��
		string						m_text;			// �������ı���
	};

	struct ForwardLine
	{
		int							m_offsetAtFile; // ���������ļ��ڵ�ƫ����
		string						m_oldText;		// ����ԭ�����ı�
		std::set<string>			m_classes;		// ����ǰ�������б�
	};

	struct ReplaceInfo
	{
		string						m_fileName;		// ��#include��Ӧ���ļ�
		string						m_inFile;		// ��#include���ĸ��ļ�����
		int							m_line;			// ��#include���ڵ���
		string						m_oldText;		// �滻ǰ��#include������: #include "../b/../b/../a.h��
		string						m_newText;		// �滻���#include������: #include "../a.h"
	};

	struct ReplaceLine
	{
		bool						m_isSkip;		// ��¼�����滻�Ƿ�Ӧ����������Ϊ��Щ#include�Ǳ�-include����������ģ����޷����滻������Ȼ�д�ӡ�ı�Ҫ
		int							m_beg;			// ��ʼƫ��
		int							m_end;			// ����ƫ��
		string						m_oldText;		// �滻ǰ��#include�ı�����: #include "../b/../b/../a.h��
		string						m_oldFile;		// �滻ǰ��#include��Ӧ���ļ�
		std::vector<ReplaceInfo>	m_newInclude;	// �滻���#include���б�
	};

	// ��Ŀ��ʷ����¼��c++�ļ��Ĵ������¼
	class FileHistory
	{
	public:
		FileHistory()
			: m_oldBeIncludeCount(0)
			, m_newBeIncludeCount(0)
			, m_beUseCount(0)
			, m_isWindowFormat(false)
		{
		}

		bool IsLineUnused(int line) const
		{
			return m_unusedLines.find(line) != m_unusedLines.end();
		}

		bool IsLineBeReplaced(int line) const
		{
			return m_replaces.find(line) != m_replaces.end();
		}

		bool IsLineAddForward(int line) const
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

	typedef std::map<string, FileHistory> FileHistoryMap;

	class ProjectHistory
	{
		ProjectHistory()
			: m_isFirst(true)
		{
		}

	public:
		void OnCleaned(const string &file)
		{
			// llvm::outs() << "on_cleaned file: " << file << " ...\n";
			m_cleanedFiles.insert(file);
		}

		bool HasCleaned(const string &file) const
		{
			return m_cleanedFiles.find(file) != m_cleanedFiles.end();
		}

		bool HasFile(const string &file) const
		{
			return m_files.find(file) != m_files.end();
		}

		void AddFile(ParsingFile *file);

		void PrintUnusedLine() const;

		void PrintCanForwarddeclClass() const;

		void PrintReplace() const;

		// ��ӡ�ļ���ʹ�ô�����δ���ƣ�
		void PrintCount() const;

		void Print() const;

	public:
		static ProjectHistory	instance;

	public:
		bool				m_isFirst;
		FileHistoryMap		m_files;
		std::set<string>	m_cleanedFiles;
	};
}

#endif // _whole_project_h_