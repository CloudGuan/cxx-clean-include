///<------------------------------------------------------------------------------
//< @file:   project.h
//< @author: ������
//< @brief:  ��������c++����������
//< Copyright (c) 2016. All rights reserved.
///<------------------------------------------------------------------------------

#ifndef _project_h_
#define _project_h_

#include <iterator>
#include <set>
#include <vector>

enum LogLvl
{
	LogLvl_0 = 0,		// ����ӡ���յ�ͳ�ƽ��
	LogLvl_1 = 1,		// Ĭ�ϣ���ӡ���ļ���������������յ�ͳ�ƽ��
	LogLvl_2,			// ���ڵ��ԣ���ӡ���ļ���ɾ�����
	LogLvl_3,			// ���ڵ��ԣ������ӡ���ļ����õ��������ļ�������������������������Ŀ��Ա�ļ���
	LogLvl_4,			// ���ڵ��ԣ������ӡ���ļ�ֱ�ӻ��߼���������ļ���
	LogLvl_5,			// ���ڵ��ԣ������ӡ�쳣
	LogLvl_6,			// ���ڵ��ԣ������ӡ�﷨��
	LogLvl_Max
};

// ��Ŀ����
class Project
{
public:
	Project()
		: m_isOverWrite(false)
		, m_logLvl(LogLvl_0)
		, m_printIdx(0)
	{
	}

public:
	// ���ļ��Ƿ���������
	static inline bool CanClean(const std::string &filename)
	{
		return CanClean(filename.c_str());
	}

	// ���ļ��Ƿ���������
	static bool CanClean(const char* filename);

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
	std::string					m_canCleanDirectory;

	// ����������ļ��б�ֻ�����ڱ��б��ڵ�c++�ļ��������Ķ�����ע�⣺�����������ļ��в�Ϊ��ʱ������������
	std::set<std::string>		m_canCleanFiles;

	// �������c++Դ�ļ��б�ֻ����c++��׺���ļ�����cpp��cxx��
	std::vector<std::string>	m_cpps;

	// ����Ŀ¼
	std::string					m_workingDir;

	// ������ѡ��Ƿ񸲸�ԭ����c++�ļ�������ѡ��ر�ʱ����Ŀ�ڵ�c++�ļ��������κθĶ���
	bool						m_isOverWrite;

	// ������ѡ���ӡ����ϸ�̶ȣ�0 ~ 9��0��ʾ����ӡ��Ĭ��Ϊ1������ϸ����9
	LogLvl						m_logLvl;

	// ��ǰ��ӡ��������������־��ӡ
	mutable int					m_printIdx;
};

#endif // _project_h_