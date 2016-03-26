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

enum VerboseLvl
{
	VerboseLvl_1 = 1,	// ����ӡ���ļ����������
	VerboseLvl_2,		// ���ڵ��ԣ���ӡ���ļ����õ��������ļ���������������������
	VerboseLvl_3,		// ���ڵ��ԣ���ӡ���ļ�ֱ�ӻ��߼���������ļ���
	VerboseLvl_4,		// ���ڵ��ԣ���ӡc++�ļ��б�
	VerboseLvl_5,		// ���ڵ��ԣ���ӡ�쳣
	VerboseLvl_Max
};

// ��Ŀ����
class Project
{
public:
	Project()
		: m_isDeepClean(false)
		, m_onlyHas1File(false)
		, m_isOverWrite(false)
		, m_verboseLvl(0)
		, m_printIdx(0)
	{
	}

public:
	// ���ļ��Ƿ���������
	bool CanClean(const std::string &filename) const
	{
		return CanClean(filename.c_str());
	}

	// ���ļ��Ƿ���������
	bool CanClean(const char* filename) const;

	// �������������ļ��б�
	void GenerateAllowCleanList();

	// �Ƴ���c++��׺��Դ�ļ�
	void Fix();

	// ��ӡ���� + 1
	std::string AddPrintIdx() const;

	// ��ӡ����������ļ��б�
	void Print() const;

public:
	static Project instance;

public:
	// ����������ļ��У�ֻ�д��ڸ��ļ����µ�c++�ļ��������Ķ���
	std::string					m_allowCleanDir;

	// ����������ļ��б�ֻ�����ڱ��б��ڵ�c++�ļ��������Ķ�����ע�⣺�����������ļ��в�Ϊ��ʱ������������
	std::set<std::string>		m_allowCleanList;	

	// �������c++Դ�ļ��б�ֻ����c++��׺���ļ�����cpp��cxx��
	std::vector<std::string>	m_cpps;

	// ����Ŀ¼
	std::string					m_workingDir;

	// ������ѡ��Ƿ������������Ϊtrue
	bool						m_isDeepClean;

	// ������ѡ��Ƿ�ֻ��һ���ļ�����ֻ��һ���ļ�ʱ��ֻ��Ҫ����һ�Σ�
	bool						m_onlyHas1File;	

	// ������ѡ��Ƿ񸲸�ԭ����c++�ļ�������ѡ��ر�ʱ����Ŀ�ڵ�c++�ļ��������κθĶ���
	bool						m_isOverWrite;

	// ������ѡ���ӡ����ϸ�̶ȣ�0 ~ 9��0��ʾ����ӡ��Ĭ��Ϊ1������ϸ����9
	int							m_verboseLvl;

	// ��ǰ��ӡ��������������־��ӡ
	mutable int					m_printIdx;
};

#endif // _project_h_