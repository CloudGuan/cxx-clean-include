///<------------------------------------------------------------------------------
//< @file:   project_history.h
//< @author: ������
//< @date:   2016��2��22��
//< @brief:  ���������ߵ���ʷͳ��
//< Copyright (c) 2016. All rights reserved.
///<------------------------------------------------------------------------------

#ifndef _whole_project_h_
#define _whole_project_h_

#include <iterator>
#include <vector>
#include <set>
#include <map>

using namespace std;

namespace cxxcleantool
{
	class ParsingFile;

	// ���õ�#include��
	struct UselessLine
	{
		UselessLine()
			: m_beg(0)
			, m_end(0)
		{
		}

		int							m_beg;				// ��ʼƫ��
		int							m_end;				// ����ƫ��
		string						m_text;				// �������ı���
		std::map<string, string>	m_usingNamespace;	// ����Ӧ��ӵ�using namespace
	};

	// ������ǰ����������
	struct ForwardLine
	{
		ForwardLine()
			: m_offsetAtFile(0)
		{
		}

		int							m_offsetAtFile;		// ���������ļ��ڵ�ƫ����
		string						m_oldText;			// ����ԭ�����ı�
		std::set<string>			m_classes;			// ����ǰ�������б�
	};

	// ���滻����#include����Ϣ
	struct ReplaceTo
	{
		ReplaceTo()
			: m_line(0)
		{
		}

		string						m_fileName;			// ��#include��Ӧ���ļ�
		string						m_inFile;			// ��#include���ĸ��ļ�����
		int							m_line;				// ��#include���ڵ���
		string						m_oldText;			// ��#includeԭ������: #include "../b/../b/../a.h"
		string						m_newText;			// ԭ#include������·����������������#include������: #include "../b/../b/../a.h" -> #include "../a.h"
	};

	// ���滻#include����
	struct ReplaceLine
	{
		ReplaceLine()
			: m_isSkip(false)
			, m_beg(0)
			, m_end(0)
		{
		}

		bool						m_isSkip;			// ��¼�����滻�Ƿ�Ӧ����������Ϊ��Щ#include�Ǳ�-include����������ģ����޷����滻������Ȼ�д�ӡ�ı�Ҫ
		int							m_beg;				// ��ʼƫ��
		int							m_end;				// ����ƫ��
		string						m_oldText;			// �滻ǰ��#include�ı�����: #include "../b/../b/../a.h��
		string						m_oldFile;			// �滻ǰ��#include��Ӧ���ļ�
		ReplaceTo					m_newInclude;		// �滻���#include���б�
		std::map<string, string>	m_frontNamespace;	// ������Ӧ��ӵ�using namespace
		std::map<string, string>	m_backNamespace;	// ����ĩӦ��ӵ�using namespace
	};

	// ÿ���ļ��ı��������ʷ
	struct CompileErrorHistory
	{
		CompileErrorHistory()
			: m_errNum(0)
			, m_hasTooManyError(false)
		{
		}

		// �Ƿ������ر�������������������
		bool HaveFatalError() const
		{
			return !m_fatalErrors.empty();
		}

		int							m_errNum;			// ���������
		int							m_hasTooManyError;	// �Ƿ����������[��clang�����ò�������]
		std::set<int>				m_fatalErrors;		// ���ش����б�
		std::vector<std::string>	m_errTips;			// ���������ʾ�б�
	};

	// �ļ���ʷ����¼����c++�ļ��Ĵ������¼
	class FileHistory
	{
	public:
		FileHistory()
			: m_oldBeIncludeCount(0)
			, m_newBeIncludeCount(0)
			, m_beUseCount(0)
			, m_isWindowFormat(false)
			, m_isSkip(false)
		{
		}

		const char* GetNewLineWord() const
		{
			return (m_isWindowFormat ? "\r\n" : "\n");
		}

		bool IsNeedClean() const
		{
			return !m_unusedLines.empty() || !m_replaces.empty() || !m_forwards.empty();
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

		bool HaveFatalError() const
		{
			return m_compileErrorHistory.HaveFatalError();
		}

		typedef std::map<int, UselessLine> UnusedLineMap;
		typedef std::map<int, ForwardLine> ForwardLineMap;
		typedef std::map<int, ReplaceLine> ReplaceLineMap;

		CompileErrorHistory m_compileErrorHistory;	// ���ļ������ı������
		UnusedLineMap		m_unusedLines;
		ForwardLineMap		m_forwards;
		ReplaceLineMap		m_replaces;
		std::string			m_filename;

		bool				m_isSkip;				// ��¼���ļ��Ƿ��ֹ�Ķ���������Щ�ļ�������stdafx.h��stdafx.cpp�����־Ͳ�Ҫ����
		bool				m_isWindowFormat;		// ���ļ��Ƿ���Windows��ʽ�Ļ��з�[\r\n]������Ϊ��Unix��ʽ[\n]��ͨ���ļ���һ�л��з����жϣ�
		int					m_newBeIncludeCount;
		int					m_oldBeIncludeCount;
		int					m_beUseCount;
	};

	typedef std::map<string, FileHistory> FileHistoryMap;

	// ���ڴ洢ͳ�ƽ���������Ը���c++�ļ�����ʷ��־
	class ProjectHistory
	{
		ProjectHistory()
			: m_isFirst(true)
			, m_printIdx(0)
			, g_printFileNo(0)
		{
		}

	public:
		void OnCleaned(const string &file)
		{
			// cxx::log() << "on_cleaned file: " << file << " ...\n";
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

		// ��ӡ�ļ���ʹ�ô�����δ���ƣ�
		void PrintCount() const;

		// ��ӡ���� + 1
		std::string AddPrintIdx() const;

		// ��ӡ��־
		void Print() const;

	public:
		static ProjectHistory	instance;

	public:
		// ��ǰ�ǵڼ��η�������Դ�ļ�
		bool				m_isFirst;

		// ������c++�ļ��ķ�����ʷ��ע������Ҳ����ͷ�ļ���
		FileHistoryMap		m_files;

		// ����������ļ���ע���б��е��ļ��������ظ�����
		std::set<string>	m_cleanedFiles;

		// ��ǰ���ڴ�ӡ�ڼ����ļ�
		int					g_printFileNo;

	private:
		// ��ǰ��ӡ��������������־��ӡ
		mutable int			m_printIdx;
	};
}

#endif // _whole_project_h_