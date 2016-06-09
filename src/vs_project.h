///<------------------------------------------------------------------------------
//< @file:   vs_config.h
//< @author: ������
//< @date:   2016��2��22��
//< @brief:  vs��Ŀ������
//< Copyright (c) 2016. All rights reserved.
///<------------------------------------------------------------------------------

#ifndef _vs_config_h_
#define _vs_config_h_

#include <iterator>
#include <vector>
#include <set>

namespace cxxcleantool
{
	class Project;

	using namespace std;

	// vs����ѡ��
	struct VsConfiguration
	{
		// ����ģʽ��ƽ̨��һ����˵��4�֣�Debug|Win32��Debug|Win64��Release|Win32��Release|Win64
		std::string					mode;

		std::vector<std::string>	forceIncludes;	// ǿ��include���ļ��б�
		std::vector<std::string>	preDefines;		// Ԥ��#define�ĺ��б�
		std::vector<std::string>	searchDirs;		// ����·���б�
		std::vector<std::string>	extraOptions;	// ����ѡ��

		// ������һЩ��Ҫ������������·��
		void Fix();

		void Print() const;

		static bool FindMode(const std::string text, std::string &mode);
	};

	// vs�����ļ�����Ӧ��.vcproj��.vcxproj������
	class Vsproject
	{
	public:
		// ����vs2005�汾�Ĺ����ļ���vcproj��׺��
		static bool ParseVs2005(const std::string &vcproj, Vsproject &vs2005);

		// ����vs2008��vs2008���ϰ汾�Ĺ����ļ���vcxproj��׺��
		static bool ParseVs2008AndUppper(const std::string &vcxproj, Vsproject &vs2008);

		// ����visual studio�����ļ�
		bool ParseVs(const std::string &vsproj_path);

	public:
		VsConfiguration* GetVsconfigByMode(const std::string &modeAndPlatform);

		void GenerateMembers();

		void TakeSourceListTo(Project &project) const;

		// ��ӡvs��������
		void Print() const;

	public:
		static Vsproject				instance;

		float							m_version;				// �汾��
		std::string						m_project_dir;			// �����ļ�����·��
		std::string						m_project_full_path;	// �����ļ�ȫ��·������:../../hello.vcproj

		std::vector<VsConfiguration>	m_configs;
		std::vector<std::string>		m_headers;				// �����ڵ�h��hpp��hh��hxx��ͷ�ļ��б�
		std::vector<std::string>		m_cpps;					// �����ڵ�cpp��cc��cxxԴ�ļ��б�

		std::set<std::string>			m_all;					// ����������c++�ļ�
	};
}

#endif // _vs_config_h_