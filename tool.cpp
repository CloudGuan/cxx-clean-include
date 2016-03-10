///<------------------------------------------------------------------------------
//< @file:   cxx_clean_tool.cpp
//< @author: ������
//< @date:   2016��2��22��
//< @brief:
//< Copyright (c) 2016. All rights reserved.
///<------------------------------------------------------------------------------

#include "tool.h"

#include <sys/stat.h>
#include <io.h>
#include <llvm/Support/FileSystem.h>

#ifdef WIN32
	#include <direct.h>
#else
	#include <unistd.h>
#endif

namespace filetool
{
	// ��path_1Ϊ��ǰ·��������path_2�����·��
	std::string get_relative_path(const char *path_1, const char *path_2)
	{
		if (nullptr == path_1 || nullptr == path_2)
		{
			return "";
		}

		int diff1_pos = 0;
		int diff2_pos = 0;

		int last_same_slash = 0;

		for (char c1 = 0, c2 = 0; (c1 = path_1[diff1_pos]) && (c2 = path_2[diff1_pos]);)
		{
			if (is_slash(c1) && is_slash(c2))
			{
				last_same_slash = diff1_pos;
			}
			else if (c1 == c2)
			{
			}
			else
			{
				break;
			}

			++diff1_pos;
		}

		diff1_pos = diff2_pos = last_same_slash;

		while (is_slash(path_1[diff1_pos])) { ++diff1_pos; }
		while (is_slash(path_2[diff2_pos])) { ++diff2_pos; }

		int path1_len	= diff1_pos;
		int depth_1		= 0;

		for (; path_1[path1_len]; ++path1_len)
		{
			if (is_slash(path_1[path1_len]))
			{
				++depth_1;
			}
		}

		std::string relative_path;
		relative_path.reserve(2 * depth_1);

		for (int i = 0; i < depth_1; ++i)
		{
			relative_path.append("../");
		}

		relative_path.append(&path_2[diff2_pos]);

		for (int i = depth_1 * 3, len = relative_path.size(); i < len; ++i)
		{
			if (relative_path[i] == '\\')
			{
				relative_path[i] = '/';
			}
		}

		return relative_path;
	}

	bool cd(const char *path)
	{
		return 0 == _chdir(path);
	}

	// ָ��·���Ƿ����
	// ���磺dir = "../../example"
	bool is_dir_exist(const std::string &dir)
	{
		struct _stat fileStat;
		if ((_stat(dir.c_str(), &fileStat) == 0) && (fileStat.st_mode & _S_IFDIR))
		{
			return true;
		}

		return false;
	}

	// ָ��·���Ƿ���ڣ���Ϊ�ļ�·�������ļ���·��
	// ���磺path = "../../example"
	// ���磺path = "../../abc.xml"
	// ���磺path = "../../"
	bool exist(const std::string &path)
	{
		return _access(path.c_str(), 0) != -1;
	}

	// �г�ָ���ļ����µ��ļ����б����ļ��н������ԣ���������windows�������µ�dir
	// ���磺path = ../../*.*,   �� files = { "a.txt", "b.txt", "c.exe" }
	// ���磺path = ../../*.txt, �� files = { "a.txt", "b.txt" }
	typedef std::vector<string> filevec_t;
	bool dir(const std::string &path, /* out */filevec_t &files)
	{
		struct _finddata_t filefind;

		int handle = 0;
		if(-1 == (handle = _findfirst(path.c_str(), &filefind)))
		{
			return false;
		}

		do
		{
			if(_A_SUBDIR != filefind.attrib)
			{
				// ����Ŀ¼�����ļ�
				files.push_back(filefind.name);
			}
		}
		while(!_findnext(handle, &filefind));

		_findclose(handle);
		return true;
	}

	// �ļ��Ƿ���ָ���ļ����£������ļ��У�
	bool is_at_folder(const char* folder, const char *file)
	{
		return start_with(string(file), folder);
	}

	// �г�ָ���ļ����µ��ļ����б������ļ����µ��ļ���
	// ���磬����../../�����ļ�"a", "b", "c", "a.txt", "b.txt", "c.exe"
	//     ��path = ../../*.*,   �� files = { "a.txt", "b.txt", "c.exe" }
	//     ��path = ../../*.txt, �� files = { "a.txt", "b.txt" }
	typedef std::vector<string> FileVec;
	bool ls(const string &path, FileVec &files)
	{
		std::string folder	= get_dir(path);
		std::string pattern	= strip_dir(path);

		if (pattern.empty())
		{
			pattern = "*";
		}

		string fixedPath = folder + "/" + pattern;

		struct _finddata_t fileinfo;

		int handle = _findfirst(fixedPath.c_str(), &fileinfo);
		if(-1 == handle)
		{
			return false;
		}

		do
		{
			//�ж��Ƿ�����Ŀ¼
			if (fileinfo.attrib & _A_SUBDIR)
			{
				//���������Ҫ
				if( (strcmp(fileinfo.name,".") != 0) &&(strcmp(fileinfo.name,"..") != 0))
				{
					string subdir = folder + "/" + fileinfo.name + "/" + pattern;
					ls(subdir, files);
				}
			}
			else
			{
				// ����Ŀ¼�����ļ�
				files.push_back(folder + "/" + fileinfo.name);
			}
		}
		while (_findnext(handle, &fileinfo) == 0);

		_findclose(handle);
		return true;
	}

	std::string get_current_path()
	{
		llvm::SmallString<512> path;
		std::error_code err = llvm::sys::fs::current_path(path);
		return err ? "" : path.str();
	}
}

namespace strtool
{
	std::string itoa(int n)
	{
		char buf[14];
		buf[0] = 0;

		::_itoa_s(n, buf, 10);
		return buf;
	}

	// �滻�ַ�����������ַ��������޸�
	// ���磺replace("this is an expmple", "is", "") = "th  an expmple"
	// ����: replace("acac", "ac", "ca") = "caca"
	string& replace(string &str, const char *old, const char* to)
	{
		string::size_type pos = 0;
		int len_old = strlen(old);
		int len_new = strlen(to);

		while((pos = str.find(old, pos)) != string::npos)
		{
			str.replace(pos, len_old, to);
			pos += len_new;
		}

		return str;
	}

	// ���ַ������ݷָ����ָ�Ϊ�ַ�������
	void split(const std::string src, std::vector<std::string> &strvec, char cut /* = ';' */)
	{
		std::string::size_type pos1 = 0, pos2 = 0;
		while (pos2 != std::string::npos)
		{
			pos1 = src.find_first_not_of(cut, pos2);
			if (pos1 == std::string::npos)
			{
				break;
			}

			pos2 = src.find_first_of(cut, pos1 + 1);
			if (pos2 == std::string::npos)
			{
				if (pos1 != src.size())
				{
					strvec.push_back(src.substr(pos1));
				}

				break;

			}

			strvec.push_back(src.substr(pos1, pos2 - pos1));
		}
	}

	// �����ļ���·�������ؽ��ĩβ��/��\
	// ���磺get_dir(../../xxxx.txt) = ../../
	string get_dir(const string &path)
	{
		if(path.empty())
		{
			return path;
		}

		int i = path.size() - 1;
		for(; i >= 0; i--)
		{
			if('\\' == path[i] || '/' == path[i])
			{
				break;
			}
		}

		if(i < 0)
		{
			return "";
		}

		if (i <= 0)
		{
			return "";
		}

		return string(path.begin(), path.begin() + i);
	}

	// �Ƶ�·����ֻ�����ļ�����
	// ���磺../../xxxx.txt -> xxxx.txt
	string strip_dir(const string &path)
	{
		if(path.empty())
		{
			return path;
		}

		int i = path.size();
		for(; i >= 0; i--)
		{
			if('\\' == path[i] || '/' == path[i])
			{
				break;
			}
		}

		return path.c_str() + (i + 1);
	}

	// ��������ֱ��ָ���ָ������ַ���
	// ���磺r_trip_at("123_456", '_') = 456
	string r_trip_at(const string &str, char delimiter)
	{
		string::size_type pos = str.rfind(delimiter);
		if(pos == string::npos)
		{
			return "";
		}

		return string(str.begin() + pos + 1, str.end());
	}

	// ��ȡ�ļ���׺
	// ���磺get_ext("../../abc.txt", '_') = txt
	string get_ext(const string &path)
	{
		string file = strip_dir(path);
		if (file.empty())
		{
			return "";
		}

		return r_trip_at(file, '.');
	}
}
