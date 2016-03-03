///<------------------------------------------------------------------------------
//< @file:   cxx_clean_tool.h
//< @author: ������
//< @date:   2016��2��22��
//< @brief:
//< Copyright (c) 2015 game. All rights reserved.
///<------------------------------------------------------------------------------

#ifndef _cxx_clean_tool_h_
#define _cxx_clean_tool_h_

#include <string>
#include <vector>

using namespace std;

namespace filetool
{
	inline bool is_slash(char c)
	{
		return (c == '\\' || c == '/');
	}

	// ��path_1Ϊ��ǰ·��������path_2�����·��
	std::string get_relative_path(const char *path_1, const char *path_2);

	bool cd(const char *path);

	// ָ��·���Ƿ����
	// ���磺dir = "../../example"
	bool is_dir_exist(const std::string &dir);

	// ָ��·���Ƿ���ڣ���Ϊ�ļ�·�������ļ���·��
	// ���磺path = "../../example"
	// ���磺path = "../../abc.xml"
	// ���磺path = "../../"
	bool exist(const std::string &path);

	// �г�ָ���ļ����µ��ļ����б����ļ��н������ԣ���������windows�������µ�dir
	// ���磺path = ../../*.*,   �� files = { "a.txt", "b.txt", "c.exe" }
	// ���磺path = ../../*.txt, �� files = { "a.txt", "b.txt" }
	typedef std::vector<string> filevec_t;
	bool dir(const std::string &path, /* out */filevec_t &files);
}

namespace strtool
{
	std::string itoa(int n);

	// �滻�ַ�����������ַ��������޸�
	// ���磺replace("this is an expmple", "is", "") = "th  an expmple"
	// ����: replace("acac", "ac", "ca") = "caca"
	string& replace(string &str, const char *old, const char* to);

	// ���ַ������ݷָ����ָ�Ϊ�ַ�������
	void split(const std::string src, std::vector<std::string> &strvec, char cut = ';');

	// �����ļ���·�������ؽ��ĩβ��/��\
	// ���磺get_dir(../../xxxx.txt) = ../../
	string get_dir(const string &path);

	// �Ƶ�·����ֻ�����ļ�����
	// ���磺../../xxxx.txt -> xxxx.txt
	string strip_dir(const string &path);

	// ��������ֱ��ָ���ָ������ַ���
	// ���磺r_trip_at("123_456", '_') = 456
	string r_trip_at(const string &str, char delimiter);

	// ��ȡ�ļ���׺
	// ���磺get_ext("../../abc.txt", '_') = txt
	string get_ext(const string &path);

	// �Ƿ���ָ���ַ�����ͷ
	inline bool start_with(const string &text, const char *prefix)
	{
		int prefix_len	= strlen(prefix);
		int text_len	= text.length();

		if (prefix_len > text_len)
		{
			return false;
		}

		for (int i = 0; i < prefix_len; ++i)
		{
			if (text[i] != prefix[i])
			{
				return false;
			}
		}

		return true;
	}

	// �Ƿ���ָ���ַ�����β
	inline bool end_with(const string& text, const string& suffix)
	{
		int suffix_len	= suffix.length();
		int text_len	= text.length();

		if (suffix_len > text_len)
		{
			return false;
		}

		for (int i = text_len - suffix_len; i < text_len; ++i)
		{
			if (text[i] != suffix[i])
			{
				return false;
			}
		}

		return true;
	}

	// �Ƿ����ָ���ַ�
	inline bool contain(const char *text, char x)
	{
		while (*text && *text != x) { ++text; }
		return *text == x;
	}

	// ����ָ��ǰ׺��ͷ�����Ƴ�ǰ׺������ʣ�µ��ַ���
	inline bool try_strip_left(string& str, const string& prefix)
	{
		if (strtool::start_with(str, prefix.c_str()))
		{
			str = str.substr(prefix.length());
			return true;
		}

		return false;
	}
}

using namespace strtool;

#endif // _cxx_clean_tool_h_