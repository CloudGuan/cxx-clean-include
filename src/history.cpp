//------------------------------------------------------------------------------
// �ļ�: history.cpp
// ����: ������
// ˵��: ���ļ���������ʷ
// Copyright (c) 2016 game. All rights reserved.
//------------------------------------------------------------------------------

#include "history.h"

#include "parser.h"
#include "project.h"
#include "html_log.h"
#include "tool.h"

namespace cxxclean
{
	ProjectHistory ProjectHistory::instance;

	// ��ӡ
	void CompileErrorHistory::Print() const
	{
		if (errNum <= 0)
		{
			return;
		}

		HtmlDiv &div = HtmlLog::instance.m_newDiv;
		div.AddRow(" ", 1, 100, false, Row_Error);

		div.AddRow(cn_error, 1, 100, false, Row_Error, Grid_Error);

		for (const std::string& errTip : errTips)
		{
			div.AddRow(errTip, 2, 100, true, Row_Error);
			div.AddRow(" ", 1, 100, false, Row_Error);
		}

		// div.AddRow(" ", 1, 100, false, true);

		if (!fatalErrors.empty())
		{
			std::string errTexts;

			int i		= 0;
			int size	= fatalErrors.size();

			for (int err : fatalErrors)
			{
				errTexts += htmltool::get_number_html(err);

				if (++i < size)
				{
					errTexts += ",";
				}
			}

			std::string tip = get_text(cn_error_fatal, errTexts.c_str());
			div.AddRow(tip, 1, 100, true, Row_Error);
		}

		if (hasTooManyError)
		{
			std::string tip = get_text(cn_error_too_many, htmltool::get_number_html(errNum).c_str());
			div.AddRow(tip, 1, 100, true, Row_Error);
		}
		else if (fatalErrors.empty())
		{
			if (errNum > 0)
			{
				std::string tip = get_text(cn_error_ignore, htmltool::get_number_html(errNum).c_str());
				div.AddRow(tip, 1, 100, true, Row_Error);
			}
		}

		div.AddRow("");
	}

	// ��ӡ�����ļ���������ʷ
	void FileHistory::Print(int id /* �ļ���� */, bool print_err /* = true */) const
	{
		if (!Project::instance.CanClean(m_filename))
		{
			return;
		}

		HtmlDiv &div = HtmlLog::instance.m_newDiv;

		bool isError	= m_compileErrorHistory.HaveFatalError();
		const char *tip = (isError ? cn_file_history_compile_error : cn_file_history);

		div.AddRow(strtool::get_text(tip, htmltool::get_number_html(id).c_str(), htmltool::get_file_html(m_filename).c_str()), 
			1, 100, false, Row_None, isError ? Grid_Error : Grid_Ok);

		if (print_err)
		{
			m_compileErrorHistory.Print();
		}

		if (m_isSkip)
		{
			// ��ӡ�����ļ�ΪԤ����ͷ�ļ�����ֹ���Ķ�
			div.AddRow(htmltool::get_warn_html(cn_file_skip), 1);
		}

		PrintUnusedInclude();
		PrintForwardClass();
		PrintReplace();
		PrintMove();
		PrintAdd();
	}

	// ��ӡ�����ļ��ڵĿɱ�ɾ#include��¼
	void FileHistory::PrintUnusedInclude() const
	{
		if (m_delLines.empty())
		{
			return;
		}

		HtmlDiv &div = HtmlLog::instance.m_newDiv;

		div.AddRow(strtool::get_text(cn_file_unused_count, htmltool::get_number_html(m_delLines.size()).c_str()), 2);

		for (auto &delLineItr : m_delLines)
		{
			int line = delLineItr.first;

			const DelLine &delLine = delLineItr.second;

			div.AddRow(strtool::get_text(cn_file_unused_line, htmltool::get_number_html(line).c_str()), 3, 25);
			div.AddGrid(strtool::get_text(cn_file_unused_include, htmltool::get_include_html(delLine.text).c_str()), 0, true);
		}

		div.AddRow("");
	}

	// ��ӡ�����ļ��ڵĿ�����ǰ��������¼
	void FileHistory::PrintForwardClass() const
	{
		if (m_forwards.empty())
		{
			return;
		}

		HtmlDiv &div = HtmlLog::instance.m_newDiv;

		div.AddRow(strtool::get_text(cn_file_add_forward_num, htmltool::get_number_html(m_forwards.size()).c_str()), 2);

		for (auto &forwardItr : m_forwards)
		{
			int line = forwardItr.first;

			const ForwardLine &forwardLine = forwardItr.second;

			div.AddRow(strtool::get_text(cn_file_add_forward_line, htmltool::get_number_html(line).c_str(), htmltool::get_include_html(forwardLine.oldText).c_str()), 3);

			for (const string &name : forwardLine.classes)
			{
				div.AddRow(strtool::get_text(cn_file_add_forward_new_text, htmltool::get_include_html(name).c_str()), 4);
			}

			div.AddRow("");
		}
	}

	// ��ӡ�����ļ��ڵĿɱ��滻#include��¼
	void FileHistory::PrintReplace() const
	{
		if (m_replaces.empty())
		{
			return;
		}

		HtmlDiv &div = HtmlLog::instance.m_newDiv;

		div.AddRow(strtool::get_text(cn_file_can_replace_num, htmltool::get_number_html(m_replaces.size()).c_str()), 2);

		for (auto & replaceItr : m_replaces)
		{
			int line = replaceItr.first;

			const ReplaceLine &replaceLine = replaceItr.second;

			std::string oldText = strtool::get_text(cn_file_can_replace_line, htmltool::get_number_html(line).c_str(), htmltool::get_include_html(replaceLine.oldText).c_str());
			if (replaceLine.isSkip)
			{
				oldText += cn_file_force_include_text;
			}

			div.AddRow(oldText.c_str(), 3);

			// ����滻�б����磬���: replace to = #include <../1.h> in [./2.h : line = 3]
			{
				const ReplaceTo &replaceInfo = replaceLine.replaceTo;

				// ���滻�Ĵ����ݲ��䣬��ֻ��ӡһ��
				if (replaceInfo.newText == replaceInfo.oldText)
				{
					div.AddRow(strtool::get_text(cn_file_replace_same_text, htmltool::get_include_html(replaceInfo.oldText).c_str()), 4, 40, true);
				}
				// ���򣬴�ӡ�滻ǰ���滻���#include����
				else
				{
					div.AddRow(strtool::get_text(cn_file_replace_old_text, htmltool::get_include_html(replaceInfo.oldText).c_str()), 4, 40, true);
					div.AddGrid(strtool::get_text(cn_file_replace_new_text, htmltool::get_include_html(replaceInfo.newText).c_str()));
				}

				// ����β���[in �������ļ� : line = xx]
				div.AddRow(strtool::get_text(cn_file_replace_in_file, htmltool::get_file_html(replaceInfo.inFile).c_str(), htmltool::get_number_html(replaceInfo.line).c_str()), 5);
			}

			div.AddRow("");
		}
	}

	// ��ӡ�����ļ��ڵ�#include���ƶ��������ļ��ļ�¼
	void FileHistory::PrintMove() const
	{
		if (m_moves.empty())
		{
			return;
		}

		HtmlDiv &div = HtmlLog::instance.m_newDiv;

		div.AddRow(strtool::get_text(cn_file_move_num, htmltool::get_number_html(m_moves.size()).c_str()), 2);

		for (auto & itr : m_moves)
		{
			int line					= itr.first;
			const MoveLine &moveLine	= itr.second;

			if (!moveLine.moveFrom.empty())
			{
				div.AddRow(strtool::get_text(cn_file_move_from_line, htmltool::get_number_html(line).c_str(), htmltool::get_include_html(moveLine.oldText).c_str()), 3);

				for (auto & itr : moveLine.moveFrom)
				{
					const std::map<int, MoveFrom> &froms = itr.second;

					for (auto & innerItr : froms)
					{
						const MoveFrom &from = innerItr.second;

						if (from.isSkip)
						{
							div.AddRow(htmltool::get_warn_html(cn_file_move_skip), 4);
						}

						div.AddRow(strtool::get_text(cn_file_move_from, htmltool::get_include_html(from.fromFile).c_str(), htmltool::get_number_html(from.fromLine).c_str()), 4);
						div.AddRow(strtool::get_text(cn_file_move_from_old, htmltool::get_include_html(from.oldText).c_str()), 5);
						div.AddRow(strtool::get_text(cn_file_move_from_new, htmltool::get_include_html(from.newText).c_str()), 5);

						if (from.newText != moveLine.oldText)
						{
							div.AddRow(strtool::get_text(cn_file_move_to_replace, htmltool::get_include_html(from.newTextFile).c_str(), htmltool::get_number_html(from.newTextLine).c_str()), 5);
						}
					}
				}
			}

			if (!moveLine.moveTo.empty())
			{
				div.AddRow(strtool::get_text(cn_file_move_to_line, htmltool::get_number_html(line).c_str(), htmltool::get_include_html(moveLine.oldText).c_str()), 3);

				for (auto & itr : moveLine.moveTo)
				{
					const MoveTo &to = itr.second;

					div.AddRow(strtool::get_text(cn_file_move_to, htmltool::get_include_html(to.toFile).c_str(), htmltool::get_number_html(to.toLine).c_str()), 4);
					div.AddRow(strtool::get_text(cn_file_move_to_old, htmltool::get_include_html(to.oldText).c_str()), 5);
					div.AddRow(strtool::get_text(cn_file_move_to_new, htmltool::get_include_html(to.newText).c_str()), 5);

					if (to.newText != moveLine.oldText)
					{
						div.AddRow(strtool::get_text(cn_file_move_to_replace, htmltool::get_include_html(to.newTextFile).c_str(), htmltool::get_number_html(to.newTextLine).c_str()), 5);
					}
				}
			}

			div.AddRow("");
		}
	}

	void FileHistory::PrintAdd() const
	{
		if (m_adds.empty())
		{
			return;
		}

		HtmlDiv &div = HtmlLog::instance.m_newDiv;

		div.AddRow(strtool::get_text(cn_file_add_line_num, htmltool::get_number_html(m_adds.size()).c_str()), 2);

		for (auto &addItr : m_adds)
		{
			int line = addItr.first;

			const AddLine &addLine = addItr.second;

			div.AddRow(strtool::get_text(cn_file_add_line, htmltool::get_number_html(line).c_str(), htmltool::get_include_html(addLine.oldText).c_str()), 3);

			for (const BeAdd &beAdd : addLine.adds)
			{
				div.AddRow(strtool::get_text(cn_file_add_line_new, htmltool::get_include_html(beAdd.text).c_str(), htmltool::get_file_html(beAdd.fileName).c_str()), 4);
			}

			div.AddRow("");
		}
	}

	// �޸����ļ���һЩ�����ͻ���޸�
	void FileHistory::Fix()
	{
		enum LineType
		{
			LineType_Del		= 1,	// ɾ��
			LineType_Replace	= 2,	// ���滻
			LineType_Move		= 3,	// ��ת�Ƶ������ļ���
		};

		typedef std::map<int, std::vector<LineType>> LineMap;

		// 1. ���ռ�ÿ�е���Ϣ
		LineMap lines;

		for (auto & itr : m_delLines)
		{
			int line = itr.first;
			lines[line].push_back(LineType_Del);
		}

		for (auto & itr : m_replaces)
		{
			int line = itr.first;
			lines[line].push_back(LineType_Replace);
		}

		for (auto & itr : m_moves)
		{
			int line = itr.first;
			const MoveLine &moveLine = itr.second;

			if (!moveLine.moveTo.empty())
			{
				lines[line].push_back(LineType_Move);
			}
		}

		// 2. �޸��г�ͻ����
		for (auto & itr : lines)
		{
			int line	= itr.first;
			auto &types = itr.second;

			int n		= (int)types.size();
			for (int i = 0; i + 1 < n; ++i)
			{
				LineType lineType = types[i];
				switch (lineType)
				{
				case LineType_Del:
					llvm::errs() << "LineType_Del\n";

					m_delLines.erase(line);
					break;

				case LineType_Replace:
					llvm::errs() << "LineType_Replace\n";

					m_replaces.erase(line);
					break;

				default:
					break;
				}
			}
		}

		// 3. ȡ����ĳЩ�е��޸ģ���Ϊ��Щ���Ѿ������Ϊ���ɸ��ģ�
		if (ProjectHistory::instance.IsAnyLineSkip(m_filename))
		{
			const std::set<int> &skipLines = ProjectHistory::instance.m_skips[m_filename];
			for (int line : skipLines)
			{
				m_delLines.erase(line);
				m_replaces.erase(line);

				auto & moveItr = m_moves.find(line);
				if (moveItr != m_moves.end())
				{
					MoveLine &moveLine = moveItr->second;
					moveLine.moveTo.clear();
				}
			}
		}
	}

	// ����
	void ProjectHistory::Fix()
	{
		for (auto &itr : m_files)
		{
			FileHistory &history = itr.second;
			history.Fix();
		}
	}

	// ��ӡ��־
	void ProjectHistory::Print() const
	{
		if (Project::instance.m_verboseLvl >= VerboseLvl_3)
		{
			PrintSkip();
		}

		if (m_files.empty())
		{
			return;
		}

		int canCleanFileCount = 0;

		for (auto & itr : m_files)
		{
			const FileHistory &fileHistory = itr.second;

			if (fileHistory.IsNeedClean() && !fileHistory.m_compileErrorHistory.HaveFatalError())
			{
				canCleanFileCount += 1;
			}
		}

		HtmlLog::instance.AddBigTitle(cn_project_history_title);

		HtmlDiv &div = HtmlLog::instance.m_newDiv;
		div.Clear();

		div.AddTitle(strtool::get_text(cn_project_history_clean_count,	htmltool::get_number_html(canCleanFileCount).c_str()), 40);
		div.AddTitle(strtool::get_text(cn_project_history_src_count,	htmltool::get_number_html(Project::instance.m_cpps.size()).c_str()), 59);

		int i = 0;

		for (auto & itr : m_files)
		{
			const FileHistory &history = itr.second;
			if (!history.IsNeedClean())
			{
				continue;
			}

			history.Print(++i);
		}

		HtmlLog::instance.AddDiv(div);
		HtmlLog::instance.EndLog();
	}

	// ��ӡ���ļ������Ϊ����ɾ������
	void ProjectHistory::PrintSkip() const
	{
		HtmlDiv &div = HtmlLog::instance.m_newDiv;
		div.Clear();
		div.AddRow(AddPrintIdx() + ". list of skip : count = " + htmltool::get_number_html(m_skips.size()), 1);

		for (auto &itr : m_skips)
		{
			const std::string &file = itr.first;

			div.AddRow("file = " + htmltool::get_file_html(file), 2);

			for (int relyLineNo : itr.second)
			{
				div.AddRow("include = " + htmltool::get_number_html(relyLineNo), 3);
			}

			div.AddRow("");
		}

		div.AddRow("");
		HtmlLog::instance.AddDiv(div);
	}

	// ��ӡ���� + 1
	std::string ProjectHistory::AddPrintIdx() const
	{
		return strtool::itoa(++m_printIdx);
	}

	// ��ָ���ļ���ָ���б�ǳɽ�ֹ���Ķ�
	void ProjectHistory::AddSkipLine(const string &file, int line)
	{
		m_skips[file].insert(line);
	}

	// ָ���ļ����Ƿ���һЩ�н�ֹ���Ķ�
	bool ProjectHistory::IsAnyLineSkip(const string &file)
	{
		return m_skips.find(file) != m_skips.end();
	}

	void ProjectHistory::AddFile(ParsingFile *newFile)
	{
		newFile->MergeTo(m_files);
	}
}