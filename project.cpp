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

Project Project::instance;

// ��ӡ����������ļ��б�
void Project::Print() const
{
	llvm::outs() << "\n////////////////////////////////\n";
	llvm::outs() << "allow clean c++ files and c++ source list \n";
	llvm::outs() << "////////////////////////////////\n";

	// ���������c++�ļ��б�
	if (!m_allowCleanList.empty())
	{
		llvm::outs() << "\n";
		llvm::outs() << "    allow clean file in project:\n";
		for (const string &file : m_allowCleanList)
		{
			llvm::outs() << "        file = " << file << "\n";
		}
	}

	// ����������ļ���·��
	if (!m_allowCleanDir.empty())
	{
		llvm::outs() << "\n";
		llvm::outs() << "    allow clean directory = " << m_allowCleanDir << "\n";
	}

	// �������c++Դ�ļ��б�
	if (!m_cpps.empty())
	{
		llvm::outs() << "\n";
		llvm::outs() << "    source list in project:\n";
		for (const string &file : m_cpps)
		{
			llvm::outs() << "        file = " << file << "\n";
		}
	}
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