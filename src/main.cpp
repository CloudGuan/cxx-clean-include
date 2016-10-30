//------------------------------------------------------------------------------
// �ļ�: main.cpp
// ����: ������
// ˵��: ����ļ�
// Copyright (c) 2016 game. All rights reserved.
//------------------------------------------------------------------------------

#include "cxx_clean.h"

#include <llvm/Support/Signals.h>
#include <llvm/Support/TargetSelect.h>

#include "project.h"
#include "vs.h"
#include "history.h"
#include "tool.h"

using namespace cxxclean;

// ��ʼ����������
bool Init(CxxCleanOptionsParser &optionParser, int argc, const char **argv)
{
	llvm::sys::PrintStackTraceOnErrorSignal("");

	llvm::InitializeNativeTarget();								// ��ʼ����ǰƽ̨����
	llvm::InitializeNativeTargetAsmParser();					// ֧�ֽ���asm

	// ���������в���
	bool ok = optionParser.ParseOptions(argc, argv);
	return ok;
}

// ��ʼ����
void Run(const CxxCleanOptionsParser &optionParser)
{
	ClangTool tool(optionParser.getCompilations(), Project::instance.m_cpps);
	tool.clearArgumentsAdjusters();
	tool.appendArgumentsAdjuster(getClangSyntaxOnlyAdjuster());

	optionParser.AddCleanVsArgument(VsProject::instance, tool);
	optionParser.AddArgument(tool, "-fcxx-exceptions");
	optionParser.AddArgument(tool, "-nobuiltininc");		// ��ֹʹ��clang���õ�ͷ�ļ�
	optionParser.AddArgument(tool, "-w");					// ���þ���
	optionParser.AddArgument(tool, "-Wno-everything");		// �����κξ��棬��-w�����
	optionParser.AddArgument(tool, "-ferror-limit=5");		// ���Ƶ���cpp�����ı�����������������ٱ���
	optionParser.AddArgument(tool, "-fpermissive");			// �Բ�ĳЩ�����ϱ�׼����Ϊ���������ͨ�������Ա�׼����������

	DiagnosticOptions diagnosticOptions;
	diagnosticOptions.ShowOptionNames = 1;
	tool.setDiagnosticConsumer(new CxxcleanDiagnosticConsumer(&diagnosticOptions)); // ע�⣺������newû��ϵ���ᱻ�ͷ�

	// ��ÿ���ļ������﷨����
	std::unique_ptr<FrontendActionFactory> factory = newFrontendActionFactory<CxxCleanAction>();
	tool.run(factory.get());
}

// ��1��������ÿ���ļ�
void Run1(const CxxCleanOptionsParser &optionParser)
{
	Run(optionParser);

	ProjectHistory::instance.Fix();
	ProjectHistory::instance.Print();
}

// ��2������ʼ����
void Run2(const CxxCleanOptionsParser &optionParser)
{
	if (!Project::instance.m_isOnlyNeed1Step)
	{
		ProjectHistory::instance.m_isFirst	= false;
		ProjectHistory::instance.g_fileNum	= 0;

		Run(optionParser);
	}
}

int main(int argc, const char **argv)
{
	Log("-- now = " << timetool::get_now() << " --!");

	// �����н�����
	CxxCleanOptionsParser optionParser;

	// ��ʼ��
	if (!Init(optionParser, argc, argv))
	{
		return 0;
	}

	// ��ʼ����������
	Run1(optionParser);
	Run2(optionParser);

	Log("-- now = " << timetool::get_now() << " --!");
	Log("-- finished --!");
	return 0;
}