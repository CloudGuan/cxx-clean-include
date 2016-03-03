///<------------------------------------------------------------------------------
//< @file:   vs_config.h
//< @author: ������
//< @date:   2016��2��22��
//< @brief:	 
//< Copyright (c) 2015 game. All rights reserved.
///<------------------------------------------------------------------------------

#ifndef _vs_config_h_
#define _vs_config_h_

#include <string>
#include <vector>
#include <set>

using namespace std;

// vs����ѡ��
struct VsConfiguration
{
	std::string mode; // ����ģʽ��ƽ̨��һ����˵��4�֣�Debug|Win32��Debug|Win64��Release|Win32��Release|Win64

	std::vector<std::string>	force_includes;	// ǿ��include���ļ��б�
	std::vector<std::string>	pre_defines;	// Ԥ��#define�ĺ��б�
	std::vector<std::string>	search_dirs;	// ����·���б�
	std::vector<std::string>	extra_options;	// ����ѡ��

	void fix_wrong_option();

	void dump();

	static bool get_mode(const std::string text, std::string &mode);	
};

// vs�����ļ�����Ӧ��.vcproj��.vcxproj������
struct Vsproject
{
	static Vsproject				instance;

	float							version;
	std::string						project_dir;		// �����ļ�����·��
	std::string						project_full_path;	// �����ļ�ȫ��·������:../../hello.vcproj

	std::vector<VsConfiguration>	configs;
	std::vector<std::string>		hs;					// �����ڵ�h��hpp��hh��hxx��ͷ�ļ��б�
	std::vector<std::string>		cpps;				// �����ڵ�cpp��cc��cxxԴ�ļ��б�

	std::set<std::string>			all;				// ����������c++�ļ�

	bool has_file(const char* file)
	{
		return all.find(file) != all.end();
	}

	bool has_file(const string &file)
	{
		return all.find(file) != all.end();
	}

	VsConfiguration* get_vsconfig(const std::string &mode_and_platform);

	void update();

	void dump();
};


bool parse_vs2005(const char* vcproj, Vsproject &vs2005);

// ����vs2008��vs2008���ϰ汾
bool parse_vs2008_and_uppper(const char* vcxproj, Vsproject &vs2008);

bool parse_vs(std::string &vsproj_path, Vsproject &vs);

void add_source_file(const Vsproject &vs, std::vector<std::string> &source_list);

#endif // _vs_config_h_