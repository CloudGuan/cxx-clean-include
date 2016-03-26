///<------------------------------------------------------------------------------
//< @file:   html_log.h
//< @author: ������
//< @date:   2016��3��19��
//< @brief:
//< Copyright (c) 2016 game. All rights reserved.
///<------------------------------------------------------------------------------

#ifndef _html_log_h_
#define _html_log_h_

#include <string>
#include <vector>

namespace cxxcleantool
{
	static const char* cn_file							= "�ļ�";
	static const char* cn_folder						= "�ļ���";
	static const char* cn_project						= "����";
	static const char* cn_clean							= "����#{beclean}����־";
	static const char* cn_project_text					= "���������c++�ļ��б��Լ���������c++Դ�ļ��б�";
	static const char* cn_project_allow_files			= "���������c++�ļ��б������ڸ��б��c++�ļ��������Ķ���";
	static const char* cn_project_allow_file			= "����������ļ� = %s";
	static const char* cn_project_source_list			= "��������c++Դ�ļ��б������ڸ��б��c++�ļ����ᱻ������";
	static const char* cn_project_source				= "��������c++Դ�ļ� = %s";
	static const char* cn_project_allow_dir				= "���������ļ���";
	static const char* cn_line_old_text					= "����ԭ�������� = ";

	static const char* cn_parsing_file					= "����%s�ļ�����־";
	static const char* cn_file_count_unused				= "����%s���ļ��ж����#include";
	static const char* cn_file_unused_count				= "�ļ�%s����%s�ж����#include";
	static const char* cn_file_unused_line				= "���Ƴ���%s��";
	static const char* cn_file_unused_include			= "ԭ����#include�ı� = %s";

	static const char* cn_file_count_can_replace		= "����%s���ļ������滻#include";
	static const char* cn_file_can_replace_num			= "�ļ�%s�п�����%s��#include�ɱ��滻";
	static const char* cn_file_can_replace_line			= "��%s�п��Ա��滻";
	static const char* cn_file_replace_same_text		= "���Ա��滻Ϊ = %s";
	static const char* cn_file_replace_old_text			= "ԭ����#include = %s";
	static const char* cn_file_replace_new_text			= "����·�������ó����µ�#include = %s";
	static const char* cn_file_force_include_text		= " ==>  [ע��: �����滻������������Ϊ���п����ѱ�ǿ�ư���]";
	static const char* cn_file_replace_in_file			= "λ��%s�ļ���%s��";

	static const char* cn_file_count_add_forward		= "����%s���ļ���������ǰ������";
	static const char* cn_file_add_forward_num			= "�ļ�%s�п�������%s��ǰ������";
	static const char* cn_file_add_forward_line			= "���ڵ�%s������ǰ������";
	static const char* cn_file_add_forward_old_text		= "����ԭ�������� = %s";
	static const char* cn_file_add_forward_new_text		= "����ǰ������ = %s";

	static const char* cn_project_history_title			= "����������";
	static const char* cn_project_history_clean_count	= "������������%s��c++�ļ��ɱ�����";
	static const char* cn_project_history_src_count		= "��Ŀ�ڹ���%s��cpp����cxx��cc��Դ�ļ�";

	struct DivGrid
	{
		std::string text;
		int width;
	};

	struct DivRow
	{
		DivRow()
			: tabCount(0)
		{
		}

		int						tabCount;
		std::vector<DivGrid>	grids;
	};

	struct HtmlDiv
	{
		void Clear()
		{
			titles.clear();
			rows.clear();
		}

		void AddTitle(const char* title, int width = 100);

		void AddRow(const char* text, int tabCount = 1, int width = 100, bool needEscape = false);

		void AddGrid(const char* text, int width, bool needEscape = false);

		void AddTitle(const std::string &title, int width = 100)
		{
			AddTitle(title.c_str(), width);
		}

		void AddGrid(const std::string &text, int width, bool needEscape = false)
		{
			AddGrid(text.c_str(), width, needEscape);
		}

		void AddRow(const std::string &text, int tabCount = 1 /* ����tab�� */, int width = 100, bool needEscape = false)
		{
			AddRow(text.c_str(), tabCount, width, needEscape);
		}

		std::vector<DivGrid>	titles;
		std::vector<DivRow>		rows;
	};

	// ���ڽ���־ת��html��ʽ������鿴
	class HtmlLog
	{
	public:
		void SetTitle(const std::string &title);

		void BeginLog();

		void EndLog();

		void AddDiv(const HtmlDiv &div);

		// ��Ӵ����
		void AddBigTitle(const std::string &title);

	private:
		static std::string GetHtmlStart(const char* title);

	public:
		static HtmlLog instance;

	public:
		std::string		m_htmlTitle;
		HtmlDiv			m_newDiv;
	};
}

#endif // _html_log_h_