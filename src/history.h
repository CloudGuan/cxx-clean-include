//------------------------------------------------------------------------------
// �ļ�: history.h
// ����: ������
// ˵��: ���ļ���������ʷ
// Copyright (c) 2016 game. All rights reserved.
//------------------------------------------------------------------------------

#ifndef _history_h_
#define _history_h_

#include <iterator>
#include <vector>
#include <set>
#include <map>

using namespace std;

namespace cxxclean
{
	class ParsingFile;

	// ���ļ���Ӧ�������У�[�ļ�] -> [���ļ�����Щ�н�ֹ��ɾ]
	typedef std::map<string, std::set<int>> FileSkipLineMap;

	// ���õ�#include��
	struct UnUsedLine
	{
		UnUsedLine()
			: beg(0)
			, end(0)
		{
		}

		int							beg;				// ��ʼƫ��
		int							end;				// ����ƫ��
		string						text;				// �������ı���
	};

	// ������ǰ����������
	struct ForwardLine
	{
		ForwardLine()
			: offset(0)
		{
		}

		int							offset;				// ���������ļ��ڵ�ƫ����
		string						oldText;			// ����ԭ�����ı�
		std::set<string>			classes;			// ����ǰ�������б�
	};

	// ���滻��#include��Ϣ
	struct ReplaceTo
	{
		ReplaceTo()
			: line(0)
		{
		}

		string						fileName;			// ��#include��Ӧ���ļ�
		string						inFile;				// ��#include���ĸ��ļ�����
		int							line;				// ��#include���ڵ���
		string						oldText;			// ��#includeԭ������: #include "../b/../b/../a.h"
		string						newText;			// ԭ#include������·����������������#include������: #include "../b/../b/../a.h" -> #include "../a.h"
		FileSkipLineMap				m_rely;				// �����滻�����������ļ��е��ļ���
	};

	// ���滻#include����
	struct ReplaceLine
	{
		ReplaceLine()
			: isSkip(false)
			, beg(0)
			, end(0)
		{
		}

		bool						isSkip;				// ��¼�����滻�Ƿ�Ӧ����������Ϊ��Щ#include�Ǳ�-include����������ģ����޷����滻������Ȼ�д�ӡ�ı�Ҫ
		int							beg;				// ��ʼƫ��
		int							end;				// ����ƫ��
		string						oldText;			// �滻ǰ��#include�ı�����: #include "../b/../b/../a.h��
		string						oldFile;			// �滻ǰ��#include��Ӧ���ļ�
		ReplaceTo					replaceTo;			// �滻���#include���б�
	};

	// ������#include����Դ
	struct MoveFrom
	{
		MoveFrom()
			: fromLine(0)
			, isSkip(false)
			, newTextLine(0)
		{
		}

		string						fromFile;			// ��Դ�ļ�
		int							fromLine;			// ���ļ��ĵڼ���
		string						oldText;			// �ɵ�#inlude�ı�
		string						newText;			// �µ�#inlude�ı�
		string						newTextFile;		// �µ�#include�������ĸ��ļ�
		int							newTextLine;		// �µ�#include���ڵ���
		bool						isSkip;				// ��¼�����ƶ��Ƿ�Ӧ����������Ϊ��Щ#include�������滻������Ȼ�д�ӡ�ı�Ҫ
	};

	// ���ƹ�ȥ��#include����Ϣ
	struct MoveTo
	{
		MoveTo()
			: toLine(0)
			, newTextLine(0)
		{
		}

		string						toFile;				// �Ƶ����ļ���
		int							toLine;				// ���Ƶ��ڼ���
		string						oldText;			// �ɵ�#inlude�ı�
		string						newText;			// �µ�#inlude�ı�
		string						newTextFile;		// �µ�#include�������ĸ��ļ�
		int							newTextLine;		// �µ�#include���ڵ���
	};

	// �ɱ��ƶ�#include����
	struct MoveLine
	{
		MoveLine()
			: line_beg(0)
			, line_end(0)
		{
		}

		int							line_beg;			// ���� - ��ʼλ��
		int							line_end;			// ���� - ����λ��
		string						oldText;			// ����֮ǰ��#include�ı�����: #include "../b/../b/../a.h��

		std::map<string, std::map<int, MoveFrom>>	moveFrom;	// ������#include��Դ����Щ�ļ�
		std::map<string, MoveTo>					moveTo;		// �Ƴ���#include�Ƶ���Щ�ļ���
	};

	// ÿ���ļ��ı��������ʷ
	struct CompileErrorHistory
	{
		CompileErrorHistory()
			: errNum(0)
			, hasTooManyError(false)
		{
		}

		// �Ƿ������ر�������������������
		bool HaveFatalError() const
		{
			return !fatalErrors.empty();
		}

		// ��ӡ
		void Print() const;

		int							errNum;				// ���������
		int							hasTooManyError;	// �Ƿ����������[��clang�����ò�������]
		std::set<int>				fatalErrors;		// ���ش����б�
		std::vector<std::string>	errTips;			// ���������ʾ�б�
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

		// ��ӡ�����ļ���������ʷ
		void Print(int id /* �ļ���� */, bool print_err = true) const;

		// ��ӡ�����ļ��ڵĿɱ�ɾ#include��¼
		void PrintUnusedInclude() const;

		// ��ӡ�����ļ��ڵĿ�����ǰ��������¼
		void PrintForwardClass() const;

		// ��ӡ�����ļ��ڵĿɱ��滻#include��¼
		void PrintReplace() const;

		// ��ӡ�����ļ��ڵ�#include���ƶ��������ļ��ļ�¼
		void PrintMove() const;

		const char* GetNewLineWord() const
		{
			return (m_isWindowFormat ? "\r\n" : "\n");
		}

		bool IsNeedClean() const
		{
			return !m_unusedLines.empty() || !m_replaces.empty() || !m_forwards.empty() || !m_moves.empty();
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

		bool IsMoved(int line) const
		{
			auto & itr = m_moves.find(line);
			if (itr == m_moves.end())
			{
				return false;
			}

			const MoveLine &moveLine = itr->second;
			return !moveLine.moveTo.empty();
		}

		bool HaveFatalError() const
		{
			return m_compileErrorHistory.HaveFatalError();
		}

		// �޸����ļ���һЩ�����ͻ���޸�
		void Fix();

		typedef std::map<int, UnUsedLine> UnusedLineMap;
		typedef std::map<int, ForwardLine> ForwardLineMap;
		typedef std::map<int, ReplaceLine> ReplaceLineMap;
		typedef std::map<int, MoveLine> MoveLineMap;

		std::string			m_filename;
		bool				m_isSkip;				// ��¼���ļ��Ƿ��ֹ�Ķ���������Щ�ļ�������stdafx.h��stdafx.cpp�����־Ͳ�Ҫ����
		bool				m_isWindowFormat;		// ���ļ��Ƿ���Windows��ʽ�Ļ��з�[\r\n]������Ϊ��Unix��ʽ[\n]��ͨ���ļ���һ�л��з����жϣ�
		int					m_newBeIncludeCount;
		int					m_oldBeIncludeCount;
		int					m_beUseCount;

		CompileErrorHistory m_compileErrorHistory;	// ���ļ������ı������
		UnusedLineMap		m_unusedLines;
		ForwardLineMap		m_forwards;
		ReplaceLineMap		m_replaces;
		MoveLineMap			m_moves;
	};

	// ���ļ�����������[�ļ�] -> [���ļ���������]
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

		// ��ָ���ļ���ָ���б�ǳɽ�ֹ���Ķ�
		void AddSkipLine(const string &file, int line);

		// ָ���ļ����Ƿ���һЩ�н�ֹ���Ķ�
		bool IsAnyLineSkip(const string &file);

		void AddFile(ParsingFile *file);

		// ��ӡ���� + 1
		std::string AddPrintIdx() const;

		// ����
		void Fix();

		// ��ӡ��־
		void Print() const;

		// ��ӡ���ļ������Ϊ����ɾ������
		void PrintSkip() const;

	public:
		static ProjectHistory	instance;

	public:
		// ��ǰ�ǵڼ��η�������Դ�ļ�
		bool				m_isFirst;

		// ������c++�ļ��ķ�����ʷ��ע������Ҳ����ͷ�ļ���
		FileHistoryMap		m_files;

		// ���ļ���Ӧ�������У�������ɾ����Щ�У�
		FileSkipLineMap		m_skips;

		// ����������ļ���ע���б��е��ļ��������ظ�����
		std::set<string>	m_cleanedFiles;

		// ��ǰ���ڴ�ӡ�ڼ����ļ�
		int					g_printFileNo;

	private:
		// ��ǰ��ӡ��������������־��ӡ
		mutable int			m_printIdx;
	};
}

#endif // _history_h_