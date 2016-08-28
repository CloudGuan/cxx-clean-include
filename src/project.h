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
	enum VerboseLvl
	{
		VerboseLvl_0 = 0,		// ����ӡ���յ�ͳ�ƽ��
		VerboseLvl_1 = 1,		// Ĭ�ϣ���ӡ���ļ���������������յ�ͳ�ƽ��
		VerboseLvl_2,			// ���ڵ��ԣ���ӡ���ļ���ɾ�����
		VerboseLvl_3,			// ���ڵ��ԣ������ӡ���ļ����õ��������ļ�������������������������Ŀ��Ա�ļ���
		VerboseLvl_4,			// ���ڵ��ԣ������ӡ���ļ�ֱ�ӻ��߼���������ļ���
		VerboseLvl_5,			// ���ڵ��ԣ������ӡ�쳣
		VerboseLvl_6,			// ���ڵ��ԣ������ӡ�﷨��
		VerboseLvl_Max
	};

	// ����ģʽ����ͬ����ģʽ��ɻ�����ʹ��
	enum CleanMode
	{
		CleanMode_Unused = 1,	// ��������#include
		CleanMode_Replace,		// �滻һЩ#include
		CleanMode_Move,			// ��һЩ#includeת�Ƶ�ֱ��ʹ�ø�#include���ļ���
		CleanMode_Max
	};

	// ��Ŀ����
	class Project
	{
	public:
		Project()
			: m_isCleanAll(false)
			, m_onlyHas1File(false)
			, m_isOverWrite(false)
			, m_need2Step(false)
			, m_verboseLvl(VerboseLvl_0)
			, m_printIdx(0)
		{
		}

	public:
		// ���ļ��Ƿ���������
		bool CanClean(const std::string &filename) const
		{
			return CanClean(filename.c_str());
		}

		// ָ��������ѡ���Ƿ���
		bool IsCleanModeOpen(CleanMode);

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

		// �Ƿ�ֻ��һ���ļ�����ֻ��һ���ļ�ʱ��ֻ��Ҫ����һ�Σ�
		bool						m_onlyHas1File;

		// �Ƿ����Ҫ����������Ŀ2�飬����������������true����Ҫ����2�顢false��ֻҪ����1��
		bool						m_need2Step;

		// ������ѡ��Ƿ�����������Ŀ�ڵ�c++�ļ�������Ϊtrue��false��ʾ������cpp�ļ���
		bool						m_isCleanAll;

		// ������ѡ��Ƿ񸲸�ԭ����c++�ļ�������ѡ��ر�ʱ����Ŀ�ڵ�c++�ļ��������κθĶ���
		bool						m_isOverWrite;

		// ������ѡ���ӡ����ϸ�̶ȣ�0 ~ 9��0��ʾ����ӡ��Ĭ��Ϊ1������ϸ����9
		VerboseLvl					m_verboseLvl;

		// ������ѡ�����ģʽ�б����ڲ�ͬ����ģʽ�Ŀ��أ��� CleanLvl ö�٣�Ĭ�Ͻ�����1��2����ͬ����ģʽ�ɽ������ʹ�ã�
		std::vector<bool>			m_cleanModes;

		// ������ѡ��Ƿ��������
		int							m_isDeepClean;

		// ��ǰ��ӡ��������������־��ӡ
		mutable int					m_printIdx;
	};
}

#endif // _project_h_