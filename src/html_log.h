///<------------------------------------------------------------------------------
//< @file:   html_log.h
//< @author: ������
//< @brief:  html��־�࣬����������ӡ��־��
//< Copyright (c) 2016 game. All rights reserved.
///<------------------------------------------------------------------------------

#ifndef _html_log_h_
#define _html_log_h_

#include <iterator>
#include <vector>

namespace cxxclean
{
	// 1. ����
	static const char* cn_log							= "����%s����־-%s.html";
	static const char* cn_time							= "%04d��%02d��%02d��%02dʱ%02d��%02d��";
	static const char* cn_cpp_file						= "[ %s ] c++ �ļ�";
	static const char* cn_folder						= "[ %s ] �ļ���";
	static const char* cn_project						= "[ %s ] visual studio����";
	static const char* cn_project_1						= "%s����";
	static const char* cn_clean							= "��ҳ���Ƕ� %s �ķ�����־�����ս���Ա�ҳ����ײ���ͳ�ƽ��Ϊ׼";
	static const char* cn_project_text					= "���������c++�ļ��б��Լ���������c++Դ�ļ��б�";
	static const char* cn_project_allow_files			= "���������c++�ļ��б��ļ����� = %s�������ڸ��б��c++�ļ��������Ķ���";
	static const char* cn_project_allow_file			= "����������ļ� = %s";
	static const char* cn_project_source_list			= "��������c++Դ�ļ��б��ļ����� = %s�������ڸ��б��c++�ļ����ᱻ������";
	static const char* cn_project_source				= "��������c++Դ�ļ� = %s";
	static const char* cn_project_allow_dir				= "���������ļ���";
	static const char* cn_line_old_text					= "����ԭ�������� = ";

	static const char* cn_file_history					= "��%s���ļ�%s�ɱ���������������£�";
	static const char* cn_file_history_compile_error	= "��%s���ļ�%s���������ر�������޷�������һ������־���£�";
	static const char* cn_file_history_title			= "%s/%s. ��������%s�ļ�����־";
	static const char* cn_file_skip						= "ע�⣺��⵽���ļ�ΪԤ�����ļ������ļ������ᱻ�Ķ�";

	static const char* cn_error							= "���󣺱��뱾�ļ�ʱ���������±������";
	static const char* cn_error_num_tip					= "�����˵�%s��������󣬱������� = %s";
	static const char* cn_fatal_error_num_tip			= "�����˵�%s��������󣬱������� = %s���������ر������";
	static const char* cn_error_fatal					= "==> ע�⣺���ڷ������ش���[�����=%s]�����ļ��ķ��������������";
	static const char* cn_error_too_many				= "==> ע�⣺���ٲ�����%s������������ڱ�����������࣬���ļ��ķ��������������";
	static const char* cn_error_ignore					= "==> ����������������%s������������ڴ�����ٻ����أ����ļ��ķ�������Խ���ͳ��";

	static const char* cn_file_count_unused				= "����%s���ļ��ж����#include";
	static const char* cn_file_unused_count				= "���ļ�����%s�ж����#include";
	static const char* cn_file_unused_line				= "���Ƴ���%s��";
	static const char* cn_file_unused_include			= "����ԭ����#include�ı� = %s";
	static const char* cn_file_add_using_namespace		= "Ӧ����%s";
	static const char* cn_file_add_front_using_ns		= "Ӧ����������%s";
	static const char* cn_file_add_back_using_ns		= "Ӧ����ĩ����%s";

	static const char* cn_file_can_replace_num			= "���ļ�����%s��#include�ɱ��滻";
	static const char* cn_file_can_replace_line			= "��%s�п��Ա��滻������ԭ�������� = %s";
	static const char* cn_file_replace_same_text		= "���Ա��滻Ϊ�µ� = %s";
	static const char* cn_file_replace_old_text			= "ԭ����#include = %s";
	static const char* cn_file_replace_new_text			= "����·�������ó����µ�#include = %s";
	static const char* cn_file_force_include_text		= " ==>  [ע��: �����滻������������Ϊ���п����ѱ�ǿ�ư���]";
	static const char* cn_file_replace_in_file			= "��ע���µ�#include������%s�ļ��ĵ�%s�У�";

	static const char* cn_file_count_add_forward		= "����%s���ļ���������ǰ������";
	static const char* cn_file_add_forward_num			= "���ļ��п�������%s��ǰ������";
	static const char* cn_file_add_forward_line			= "���ڵ�%s������ǰ������������ԭ�������� = %s";
	static const char* cn_file_add_forward_old_text		= "����ԭ�������� = %s";
	static const char* cn_file_add_forward_new_text		= "����ǰ������ = %s";
	static const char* cn_file_add_line_num				= "���ļ��п�������%s��";
	static const char* cn_file_add_line					= "���ڵ�%s�������У�����ԭ�������� = %s";
	static const char* cn_file_add_line_new				= "������ = %s(��Ӧ�ļ� = %s)";

	static const char* cn_file_move_num					= "���ļ�����%s��#include�ɱ�ת��";
	static const char* cn_file_move_to_line				= "��%s�п���ת�Ƶ������ļ�������ԭ�������� = %s";
	static const char* cn_file_move_to					= "����ת�Ƶ�%s�ļ��еĵ�%s��֮��";
	static const char* cn_file_move_to_old				= "����ԭ�����ı� = %s";
	static const char* cn_file_move_to_new				= "ת��֮����ı� = %s";
	static const char* cn_file_move_to_replace			= "��ע��ת��֮���#include������%s�ļ��ĵ�%s�У�";
	static const char* cn_file_move_from_line			= "��%s��Ӧ���������ļ�ת�Ƶ�#include������ԭ�������� = %s";
	static const char* cn_file_move_from				= "������%s�ļ��ĵ�%s��ת��";
	static const char* cn_file_move_from_old			= "����ԭ�����ı� = %s";
	static const char* cn_file_move_from_new			= "ת��֮����ı� = %s";
	static const char* cn_file_move_skip				= "ע�⣺��⵽���ļ�ΪԤ�����ļ��������ԸøĶ�";

	static const char* cn_file_min_use					= "%s. ���ļ�����С�����ļ������ļ��� = %s";
	static const char* cn_file_min_kid					= "%s. ���ļ�����С���ú���ļ������ļ��� = %s";
	static const char* cn_file_sys_ancestor				= "%s. ��ϵͳ�ļ��������ļ����ļ��� = %s";
	static const char* cn_file_user_use					= "%s. ���û��ļ������ü�¼���ļ��� = %s";

	static const char* cn_project_history_title			= "ͳ�ƽ��";
	static const char* cn_project_history_clean_count	= "������������%s��c++�ļ��ɱ�����";
	static const char* cn_project_history_src_count		= "���ι�������%s��cpp����cxx��cc��Դ�ļ�";

	static const char* cn_parent_file_debug_text		= "�ļ� = %s, ���ļ��� = %s";
	static const char* cn_file_debug_text				= "%s[%s](�ļ�ID = %d)(���� = %s){���ļ�������%s}";
	static const char* cn_file_include_line				= "[%s��%s��%s]";
	static const char* cn_main_file_debug_text			= "[%s](�ļ�ID = %d)(���� = %s)";
	static const char* cn_outer_file_flag				= "[�ⲿ�ļ�]";

	// ������
	enum RowType
	{
		Row_None	= 0,	// ����
		Row_Error	= 1,	// ����
	};

	// ��������
	enum GridType
	{
		Grid_None	= 0,	// ����
		Grid_Ok		= 1,	// ��ȷ
		Grid_Error	= 2,	// ����
	};

	struct DivGrid
	{
		DivGrid()
			: width(0)
			, gridType(Grid_None)
		{
		}

		std::string text;
		int			width;
		GridType	gridType;	// �����ӵ�����
	};

	struct DivRow
	{
		DivRow()
			: tabCount(0)
			, rowType(Row_None)
		{
		}

		RowType					rowType;	// ���е�����
		int						tabCount;
		std::vector<DivGrid>	grids;
	};

	struct HtmlDiv
	{
		HtmlDiv()
			: hasErrorTip(false)
			, errorTipCount(0)
		{
		}

		void Clear()
		{
			titles.clear();
			rows.clear();
			hasErrorTip = false;
		}

		void AddTitle(const char* title, int width = 100);

		void AddTitle(const std::string &title, int width = 100)
		{
			AddTitle(title.c_str(), width);
		}

		void AddRow(const char* text, int tabCount = 1, int width = 100, bool needEscape = false, RowType rowType = Row_None, GridType gridType = Grid_None);

		void AddRow(const std::string &text, int tabCount = 1 /* ����tab�� */, int width = 100, bool needEscape = false, RowType rowType = Row_None, GridType gridType = Grid_None)
		{
			AddRow(text.c_str(), tabCount, width, needEscape, rowType, gridType);
		}

		void AddGrid(const char* text, int width = 0, bool needEscape = false, GridType gridType = Grid_None);

		void AddGrid(const std::string &text, int width = 0, bool needEscape = false, GridType gridType = Grid_None)
		{
			AddGrid(text.c_str(), width, needEscape, gridType);
		}

		std::vector<DivGrid>	titles;
		std::vector<DivRow>		rows;
		bool					hasErrorTip;
		int						errorTipCount;
	};

	// ���ڽ���־ת��html��ʽ������鿴
	class HtmlLog
	{
	public:
		// ������ҳ�ļ��ı���
		void SetHtmlTitle(const std::string &title);

		// ������ҳ�ڴ����
		void SetBigTitle(const std::string &title);

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
		std::string		m_bigTitle;
		HtmlDiv			m_newDiv;
	};
}

#endif // _html_log_h_