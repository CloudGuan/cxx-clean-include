//------------------------------------------------------------------------------
// �ļ�: tool.cpp
// ����: ������
// ˵��: �������õ��ĸ��ֻ����ӿ�
// Copyright (c) 2016 game. All rights reserved.
//------------------------------------------------------------------------------

#include "tool.h"

#include <sys/stat.h>
#include <io.h>
#include <fstream>
#include <stdarg.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Path.h>
#include <llvm/Support/raw_ostream.h>

#include "html_log.h"

#ifdef WIN32
	#include <direct.h>
#else
	#include <unistd.h>
#endif

using namespace cxxclean;

namespace strtool
{
	std::string itoa(int n)
	{
		char buf[14];
		buf[0] = 0;

		::_itoa_s(n, buf, 10);
		return buf;
	}

	int atoi(const char* str)
	{
		int n = ::atoi(str);
		return n;
	}

	// ��Сд
	std::string tolower(const char* str)
	{
		std::string s(str);
		std::transform(s.begin(), s.end(), s.begin(), ::tolower);

		return s;
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
	void split(const std::string &src, std::vector<std::string> &strvec, char cut /* = ';' */)
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

		int i = path.size() - 1;
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
	// ���磺r_trip_at("123_456", '_') = 123
	string trip_at(const string &str, char delimiter)
	{
		string::size_type pos = str.find(delimiter);
		if(pos == string::npos)
		{
			return "";
		}

		return string(str.begin(), str.begin() + pos);
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

	// ��ӡ
	char g_sprintfBuf[10 * 1024] = {0};

	const char* get_text(const char* fmt, ...)
	{
		va_list args;
		va_start(args, fmt);

		vsprintf_s(g_sprintfBuf, sizeof(g_sprintfBuf), fmt, args);

		va_end(args);
		return g_sprintfBuf;
	}
}

namespace pathtool
{
	/*
		��path_1Ϊ��ǰ·��������path_2�����·��
		���磺
			get_relative_path("d:/a/b/c/hello1.cpp", "d:/a/b/c/d/e/f/g/hello2.cpp") = d/e/f/g/hello2.cpp
			get_relative_path("d:/a/b/c/d/e/f/g/hello2.cpp", "d:/a/b/c/hello1.cpp") = ../../../../hello1.cpp

	*/
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
			if (strtool::is_slash(c1) && strtool::is_slash(c2))
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

		while (strtool::is_slash(path_1[diff1_pos])) { ++diff1_pos; }
		while (strtool::is_slash(path_2[diff2_pos])) { ++diff2_pos; }

		int path1_len	= diff1_pos;
		int depth_1		= 0;

		for (; path_1[path1_len]; ++path1_len)
		{
			if (strtool::is_slash(path_1[path1_len]))
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

		for (int i = 0, len = relative_path.size(); i < len; ++i)
		{
			if (relative_path[i] == '\\')
			{
				relative_path[i] = '/';
			}
		}

		strtool::replace(relative_path, "//", "/");
		return relative_path;
	}

	// ��·��ת��linux·����ʽ����·���е�ÿ��'\'�ַ����滻Ϊ'/'
	string to_linux_path(const char *path)
	{
		string ret = path;

		// ��'\'�滻Ϊ'/'
		for (size_t i = 0; i < ret.size(); ++i)
		{
			if (ret[i] == '\\')
				ret[i] = '/';
		}

		strtool::replace(ret, "//", "/");
		return ret;
	}

	// ǿ�ƽ�·����/��β����·���е�ÿ��'\'�ַ����滻Ϊ'/'
	string fix_path(const string& path)
	{
		string ret = to_linux_path(path.c_str());

		if (!end_with(ret, "/"))
			ret += "/";

		return ret;
	}

	// ����·����ȡ�ļ���
	// ���磺/a/b/foo.txt    => foo.txt
	string get_file_name(const string& path)
	{
		return llvm::sys::path::filename(path);
	}

	// ��·��
	// d:/a/b/c/../../d/ -> d:/d/
	std::string simplify_path(const char* path)
	{
		string native_path = to_linux_path(path);
		if (native_path.empty())
		{
			return "";
		}

		if (start_with(native_path, "../") || start_with(native_path, "./"))
		{
			return native_path;
		}

		strtool::replace(native_path, "/./", "/");

		string out(native_path.size(), '\0');

		int o = 0;

		const char up_dir[] = "/../";
		int up_dir_len = strlen(up_dir);

		for (int i = 0, len = native_path.size(); i < len;)
		{
			char c = native_path[i];

			if (c == '/')
			{
				if (i + up_dir_len - 1 >= len || i == 0)
				{
					out[o++] = c;
					++i;
					continue;
				}

				if(0 == strncmp(&native_path[i], "/../", up_dir_len))
				{
					if (out[o] == '/')
					{
						--o;
					}

					while (o >= 0)
					{
						if (out[o] == '/')
						{
							break;
						}
						else if (out[o] == ':')
						{
							++o;
							break;
						}

						--o;
					}

					if (o < 0)
					{
						o = 0;
					}

					i += up_dir_len - 1;
					continue;
				}
				else
				{
					out[o++] = c;
					++i;
				}
			}
			else
			{
				out[o++] = c;
				++i;
			}
		}

		out[o] = '\0';
		out.erase(out.begin() + o, out.end());
		return out;
	}

	std::string append_path(const char* a, const char* b)
	{
		llvm::SmallString<512> path(a);
		llvm::sys::path::append(path, b);

		return path.c_str();
	}

	/*
		���ؼ򻯺�ľ���·�������������·�������� = �򻯣���ǰ·�� + ���·���������������·������� = �򻯺�ľ���·��
		���磺
			���赱ǰ·��Ϊ��d:/a/b/c/
			get_absolute_path("../../d/e/hello2.cpp") = "d:/a/b/d/e/hello2.cpp"
			get_absolute_path("d:/a/b/c/../../d/") = "d:/a/d/"

	*/
	string get_absolute_path(const char *path)
	{
		if (nullptr == path)
		{
			return "";
		}

		if (path[0] == 0x0)
		{
			return "";
		}

		llvm::SmallString<512> filepath(path);
		std::error_code error = llvm::sys::fs::make_absolute(filepath);
		if (error)
		{
			return "";
		}

		filepath = simplify_path(filepath.c_str());
		return tolower(filepath.str());
	}

	/*
		���ؼ򻯺�ľ���·������� = �򻯣�����·�� + ���·����
		���磺
			get_absolute_path("d:/a/b/c/", "../../d/") = "d:/a/d/"
	*/
	string get_absolute_path(const char *base_path, const char* relative_path)
	{
		if (nullptr == base_path || nullptr == relative_path)
		{
			return "";
		}

		if (llvm::sys::path::is_absolute(relative_path))
		{
			return get_absolute_path(relative_path);
		}
		else
		{
			std::string path = append_path(base_path, relative_path);
			return get_absolute_path(path.c_str());
		}

		return "";
	}

	// �ı䵱ǰ�ļ���
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
		return start_with(tolower(folder), tolower(file).c_str());
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
				//���������Ҫ�����Ե�ǰĿ¼���ϲ�Ŀ¼
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

namespace htmltool
{
	std::string escape_html(const char* html)
	{
		return escape_html(std::string(html));
	}

	std::string escape_html(const std::string &html)
	{
		std::string ret(html);

		strtool::replace(ret, "<", "&lt;");
		strtool::replace(ret, ">", "&gt;");

		return ret;
	}

	std::string get_file_html(const std::string &filename)
	{
		std::string html = R"--(<a href="#{file}">#{file}</a>)--";
		strtool::replace(html, "#{file}", filename.c_str());

		return html;
	}

	std::string get_min_file_name_html(const std::string &filename)
	{
		std::string html = R"--(<a href="#{filepath}">#{filename}</a>)--";
		strtool::replace(html, "#{filepath}", filename.c_str());
		strtool::replace(html, "#{filename}", pathtool::get_file_name(filename).c_str());

		return html;
	}

	std::string get_include_html(const std::string &text)
	{
		return strtool::get_text(R"--(<span class="src">%s</span>)--", htmltool::escape_html(text).c_str());
	}

	std::string get_number_html(int num)
	{
		return strtool::get_text(R"--(<span class="num">%s</span>)--", strtool::itoa(num).c_str());
	}

	std::string get_warn_html(const char *text)
	{
		return strtool::get_text(R"--(<span class="num">%s</span>)--", text);
	}

	std::string get_pre_html(const char *text)
	{
		return strtool::get_text(R"--(<pre>%s</span>)--", text);
	}
}

namespace timetool
{
	std::string get_now(const char* format /* = "%04d/%02d/%02d-%02d:%02d:%02d" */)
	{
		if (NULL == format)
		{
			return "";
		}

		time_t now;
		time(&now);// time������ȡ���ڵ�ʱ��(���ʱ�׼ʱ��Ǳ���ʱ��)��Ȼ��ֵ��now

		tm *localnow = localtime(&now); // תΪ��ʱ��ʱ��

		char buf[128] = { 0 };
		sprintf_s(buf, sizeof buf, format,
		          1900 + localnow->tm_year, 1 + localnow->tm_mon, localnow->tm_mday,
		          localnow->tm_hour, localnow->tm_min, localnow->tm_sec
		         );

		return buf;
	}
}

namespace cxx
{
	static std::string s_log_path;

	void init_log(std::string log_path)
	{
		if (s_log_path.empty())
		{
			log_path = strtool::replace(log_path, "[", "");
			log_path = strtool::replace(log_path, "]", "");
			log_path = strtool::replace(log_path, " ", "");
			log_path = strtool::replace(log_path, ".", "-");
			log_path = strtool::replace(log_path, "/", "-");
			log_path = strtool::replace(log_path, "\\", "-");
			log_path = strtool::replace(log_path, ":", "-");

			s_log_path = strtool::get_text(cn_log, log_path.c_str(), timetool::get_now(cn_time).c_str());
		}
	}

	llvm::raw_ostream& log()
	{
		static llvm::raw_fd_ostream *o = nullptr;
		if (nullptr == o)
		{
			FILE *file = fopen(s_log_path.c_str(), "w"); // �ļ��򿪷�ʽ�����ԭ��������Ҳ������
			if (file)
			{
				static llvm::raw_fd_ostream fd_os(file->_file, true);
				o = &fd_os;
			}
			else
			{
				static llvm::raw_fd_ostream err_os(0, true);
				o = &err_os;
			}
		}

		return *o;
	}
}