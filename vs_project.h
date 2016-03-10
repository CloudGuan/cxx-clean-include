///<------------------------------------------------------------------------------
//< @file:   vs_config.h
//< @author: ������
//< @date:   2016��2��22��
//< @brief:
//< Copyright (c) 2016. All rights reserved.
///<------------------------------------------------------------------------------

#ifndef _vs_config_h_
#define _vs_config_h_

#include <string>
#include <vector>
#include <set>

class Project;

using namespace std;

// vs����ѡ��
struct VsConfiguration
{
	std::string mode; // ����ģʽ��ƽ̨��һ����˵��4�֣�Debug|Win32��Debug|Win64��Release|Win32��Release|Win64

	std::vector<std::string>	forceIncludes;	// ǿ��include���ļ��б�
	std::vector<std::string>	preDefines;	// Ԥ��#define�ĺ��б�
	std::vector<std::string>	searchDirs;	// ����·���б�
	std::vector<std::string>	extraOptions;	// ����ѡ��

	void FixWrongOption();

	void Print();

	static bool FindMode(const std::string text, std::string &mode);
};

// vs�����ļ�����Ӧ��.vcproj��.vcxproj������
struct Vsproject
{
	static Vsproject				instance;

	float							m_version;
	std::string						m_project_dir;		// �����ļ�����·��
	std::string						m_project_full_path;	// �����ļ�ȫ��·������:../../hello.vcproj

	std::vector<VsConfiguration>	m_configs;
	std::vector<std::string>		m_headers;			// �����ڵ�h��hpp��hh��hxx��ͷ�ļ��б�
	std::vector<std::string>		m_cpps;				// �����ڵ�cpp��cc��cxxԴ�ļ��б�

	std::set<std::string>			m_all;				// ����������c++�ļ�

	bool HasFile(const char* file)
	{
		return m_all.find(file) != m_all.end();
	}

	bool HasFile(const string &file)
	{
		return m_all.find(file) != m_all.end();
	}

	VsConfiguration* GetVsconfigByMode(const std::string &modeAndPlatform);

	void GenerateMembers();

	void TakeSourceListTo(Project &project) const;

	void Print();
};


bool ParseVs2005(const char* vcproj, Vsproject &vs2005);

// ����vs2008��vs2008���ϰ汾
bool ParseVs2008AndUppper(const char* vcxproj, Vsproject &vs2008);

bool ParseVs(std::string &vsproj_path, Vsproject &vs);

#endif // _vs_config_h_