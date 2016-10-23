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

namespace cxxclean
{
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

	// ����ģʽ����ͬ����ģʽ��ɻ�����ʹ��
	enum CleanMode
	{
		CleanMode_Unused = 1,	// ��������#include
		CleanMode_Replace,		// ��Щ#include���������õ������ļ��������ð��������ļ���#include��ȡ������#include
		CleanMode_Need,			// ��Ĭ�ϣ�ÿ���ļ�����ֻ�����Լ��õ����ļ������Զ�����ǰ����������ע�⣺��ģʽ���ܱ�����ʹ�ã�
		CleanMode_Max
	};

	// ��Ŀ����
	class Project
	{
	public:
		Project()
			: m_canCleanAll(false)
			, m_onlyHas1File(false)
			, m_isOverWrite(false)
			, m_isOnlyNeed1Step(false)
			, m_logLvl(LogLvl_0)
			, m_printIdx(0)
		{
		}

	public:
		// ���ļ��Ƿ���������
		static bool CanClean(const std::string &filename)
		{
			return CanClean(filename.c_str());
		}

		// ���ļ��Ƿ���������
		static bool CanClean(const char* filename);

		// ָ��������ѡ���Ƿ���
		static bool IsCleanModeOpen(CleanMode);

		// ��ǰ�Ƿ���������
		bool CanCleanNow();

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

		// �Ƿ�ֻ��һ���ļ�����ֻ��һ���ļ�ʱ��ֻ��Ҫ����һ�Σ�
		bool						m_onlyHas1File;

		// ��ǰ��Ŀ�Ƿ����Ҫ����1�飬�������������������壺true������1�顢false��Ҫ����2��
		bool						m_isOnlyNeed1Step;

		// ������ѡ��Ƿ�����������Ŀ�ڵ�c++�ļ�������Ϊtrue��false��ʾ������cpp�ļ���
		bool						m_canCleanAll;

		// ������ѡ��Ƿ񸲸�ԭ����c++�ļ�������ѡ��ر�ʱ����Ŀ�ڵ�c++�ļ��������κθĶ���
		bool						m_isOverWrite;

		// ������ѡ���ӡ����ϸ�̶ȣ�0 ~ 9��0��ʾ����ӡ��Ĭ��Ϊ1������ϸ����9
		LogLvl						m_logLvl;

		// ������ѡ�����ģʽ�б����ڲ�ͬ����ģʽ�Ŀ��أ��� CleanLvl ö�٣�Ĭ�Ͻ�����1��2����ͬ����ģʽ�ɽ������ʹ�ã�
		std::vector<bool>			m_cleanModes;

		// ��ǰ��ӡ��������������־��ӡ
		mutable int					m_printIdx;
	};
}

#endif // _project_h_