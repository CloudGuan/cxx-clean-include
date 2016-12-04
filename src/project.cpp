///<------------------------------------------------------------------------------
//< @file:   project.cpp
//< @author: ������
//< @brief:  ��������c++����������
//< Copyright (c) 2016. All rights reserved.
///<------------------------------------------------------------------------------

#include "project.h"

#include <llvm/Support/raw_ostream.h>

#include "tool.h"
#include "parser.h"
#include "html_log.h"

Project Project::instance;

// ��ӡ����������ļ��б�
void Project::Print() const
{
	if (Project::instance.m_logLvl < LogLvl_5)
	{
		return;
	}

	HtmlDiv div;
	div.AddTitle(cn_project_text);

	// ���������c++�ļ��б�
	if (!m_canCleanFiles.empty())
	{
		div.AddRow(AddPrintIdx() + ". " + strtool::get_text(cn_project_allow_files, get_number_html(m_canCleanFiles.size()).c_str()));

		for (const string &file : m_canCleanFiles)
		{
			div.AddRow(strtool::get_text(cn_project_allow_file, get_file_html(file.c_str()).c_str()), 2);
		}

		div.AddRow("");
	}

	// ����������ļ���·��
	if (!m_canCleanDirectory.empty())
	{
		div.AddRow(AddPrintIdx() + ". " + cn_project_allow_dir + m_canCleanDirectory);
		div.AddRow("");
	}

	// �������c++Դ�ļ��б�
	if (!m_cpps.empty())
	{
		div.AddRow(AddPrintIdx() + ". " + strtool::get_text(cn_project_source_list, get_number_html(m_cpps.size()).c_str()));

		for (const string &file : m_cpps)
		{
			const string absoluteFile = pathtool::get_absolute_path(file.c_str());
			div.AddRow(strtool::get_text(cn_project_source, get_file_html(absoluteFile.c_str()).c_str()), 2);
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
	if (!m_canCleanDirectory.empty())
	{
		return;
	}

	// ���������.cpp�ļ�����������б�
	for (const string &cpp : m_cpps)
	{
		string absolutePath = pathtool::get_absolute_path(cpp.c_str());
		m_canCleanFiles.insert(tolower(absolutePath.c_str()));
	}
}

// ���ļ��Ƿ���������
bool Project::CanClean(const char* filename)
{
	if (!instance.m_canCleanDirectory.empty())
	{
		return pathtool::is_at_directory(instance.m_canCleanDirectory.c_str(), filename);
	}
	else
	{
		return instance.m_canCleanFiles.find(tolower(filename)) != instance.m_canCleanFiles.end();
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

		if (cpptool::is_cpp(ext) && CanClean(cpp.c_str()))
		{
			++i;
		}
		else
		{
			m_cpps[i] = m_cpps[--size];
			m_cpps.pop_back();
		}
	}
}