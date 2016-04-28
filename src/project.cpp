///<------------------------------------------------------------------------------
//< @file:   project.cpp
//< @author: ������
//< @date:   2016��3��7��
//< @brief:
//< Copyright (c) 2016. All rights reserved.
///<------------------------------------------------------------------------------

#include "project.h"

#include <llvm/Support/raw_ostream.h>

#include "tool.h"
#include "parsing_cpp.h"
#include "html_log.h"

Project Project::instance;

// ��ӡ����������ļ��б�
void Project::Print() const
{
	HtmlDiv div;
	div.AddTitle(cn_project_text);

	// ���������c++�ļ��б�
	if (!m_allowCleanList.empty())
	{
		div.AddRow(AddPrintIdx() + ". " + strtool::get_text(cn_project_allow_files, htmltool::get_number_html(m_allowCleanList.size()).c_str()));

		for (const string &file : m_allowCleanList)
		{
			div.AddRow(strtool::get_text(cn_project_allow_file, htmltool::get_file_html(file).c_str()), 2);
		}

		div.AddRow("");
	}

	// ����������ļ���·��
	if (!m_allowCleanDir.empty())
	{
		div.AddRow(AddPrintIdx() + ". " + cn_project_allow_dir + m_allowCleanDir);
		div.AddRow("");
	}

	// �������c++Դ�ļ��б�
	if (!m_cpps.empty())
	{
		div.AddRow(AddPrintIdx() + ". " + strtool::get_text(cn_project_source_list, htmltool::get_number_html(m_cpps.size()).c_str()));

		for (const string &file : m_cpps)
		{
			const string absoluteFile = pathtool::get_absolute_path(file.c_str());
			div.AddRow(strtool::get_text(cn_project_source, htmltool::get_file_html(absoluteFile).c_str()), 2);
		}

		div.AddRow("");
	}

	HtmlLog::instance.AddDiv(div);
}

// ��ӡ���� + 1
std::string Project::AddPrintIdx() const
{
	return strtool::itoa(++m_printIdx);
}

// �������������ļ��б�
void Project::GenerateAllowCleanList()
{
	if (!m_allowCleanDir.empty())
	{
		return;
	}

	// ���������.cpp�ļ�����������б�
	{
		for (const string &cpp : m_cpps)
		{
			string absolutePath = pathtool::get_absolute_path(cpp.c_str());
			m_allowCleanList.insert(absolutePath);
		}
	}
}

// ���ļ��Ƿ���������
bool Project::CanClean(const char* filename) const
{
	if (!m_allowCleanDir.empty())
	{
		return pathtool::is_at_folder(m_allowCleanDir.c_str(), filename);
	}
	else
	{
		return m_allowCleanList.find(filename) != m_allowCleanList.end();
	}

	return false;
}

// �Ƴ���c++��׺��Դ�ļ�
void Project::Fix()
{
	// ��������Ŀ�꣬����c++��׺���ļ����б����Ƴ�
	for (int i = 0, size = m_cpps.size(); i < size;)
	{
		std::string &cpp = m_cpps[i];
		string ext = strtool::get_ext(cpp);

		if (cpptool::is_cpp(ext))
		{
			++i;
		}
		else
		{
			m_cpps[i] = m_cpps[--size];
			m_cpps.pop_back();
		}
	}

	// ����
	//     ��������������ļ��б��򲻼���׺����Ϊ�ܿ�����ĳ��cpp�ļ�������������䣺
	//         #include "common.def"
	//     ������:
	//		   #include "common.cpp"
}