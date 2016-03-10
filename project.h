///<------------------------------------------------------------------------------
//< @file:   project.h
//< @author: ������
//< @date:   2016��3��7��
//< @brief:
//< Copyright (c) 2016. All rights reserved.
///<------------------------------------------------------------------------------

#ifndef _project_h_
#define _project_h_

#include <string>
#include <set>
#include <vector>

// ��Ŀ����
class Project
{
public:
	Project()
		: m_isDeepClean(false)
		, m_onlyHas1File(false)
	{
	}

public:
	// ���ļ��Ƿ���������
	bool CanClean(const std::string &filename)
	{
		return CanClean(filename.c_str());
	}

	bool CanClean(const char* filename);

	void ToAbsolute();

	void GenerateAllowCleanList();

	void Fix();

	void Print();

public:
	static Project instance;

public:
	// ����������ļ��б�ֻ�����ڱ��б��ڵ�c++�ļ��������Ķ���
	std::set<std::string>		m_allowCleanList;

	// ����������ļ��У�ֻ�д��ڸ��ļ����µ�c++�ļ��������Ķ�����ע�⣺�����������ļ��б�Ϊ��ʱ������������
	std::string					m_allowCleanDir;

	// c++Դ�ļ��б�
	std::vector<std::string>	m_cpps;

	// �Ƿ������������Ϊtrue
	bool						m_isDeepClean;

	// �Ƿ�ֻ��һ���ļ�
	bool						m_onlyHas1File;

	// ����Ŀ¼
	std::string					m_workingDir;
};

#endif // _project_h_