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

// ��ʼ����������
bool Init(cxxclean::CxxCleanOptionsParser &optionParser, int argc, const char **argv)
{
	llvm::sys::PrintStackTraceOnErrorSignal("");

	llvm::InitializeNativeTarget();								// ��ʼ����ǰƽ̨����
	llvm::InitializeNativeTargetAsmParser();					// ֧�ֽ���asm

	// ���������в���
	bool ok = optionParser.ParseOptions(argc, argv);
	return ok;
}

// ��ʼ����
void Run(const cxxclean::CxxCleanOptionsParser &optionParser)
{
	ClangTool tool(optionParser.getCompilations(), cxxclean::Project::instance.m_cpps);
	tool.clearArgumentsAdjusters();
	tool.appendArgumentsAdjuster(getClangSyntaxOnlyAdjuster());

	optionParser.AddCleanVsArgument(cxxclean::VsProject::instance, tool);

	tool.appendArgumentsAdjuster(getInsertArgumentAdjuster("-fcxx-exceptions",			ArgumentInsertPosition::BEGIN));
	tool.appendArgumentsAdjuster(getInsertArgumentAdjuster("-nobuiltininc",				ArgumentInsertPosition::BEGIN));	// ��ֹʹ��clang���õ�ͷ�ļ�
	tool.appendArgumentsAdjuster(getInsertArgumentAdjuster("-w",						ArgumentInsertPosition::BEGIN));	// ���þ���
	tool.appendArgumentsAdjuster(getInsertArgumentAdjuster("-Wno-everything",			ArgumentInsertPosition::BEGIN));	// �����κξ��棬��-w�����
	tool.appendArgumentsAdjuster(getInsertArgumentAdjuster("-ferror-limit=5",			ArgumentInsertPosition::BEGIN));	// ���Ƶ���cpp�����ı�����������������ٱ���
	tool.appendArgumentsAdjuster(getInsertArgumentAdjuster("-fpermissive",				ArgumentInsertPosition::BEGIN));	// �Բ�ĳЩ�����ϱ�׼����Ϊ���������ͨ�������Ա�׼����������
	
	DiagnosticOptions diagnosticOptions;
	diagnosticOptions.ShowOptionNames = 1;
	tool.setDiagnosticConsumer(new cxxclean::CxxcleanDiagnosticConsumer(&diagnosticOptions)); // ע�⣺������newû��ϵ���ᱻ�ͷ�

	// ��ÿ���ļ������﷨����
	std::unique_ptr<FrontendActionFactory> factory = newFrontendActionFactory<cxxclean::CxxCleanAction>();
	tool.run(factory.get());
}

int main(int argc, const char **argv)
{
	// �����н�����
	cxxclean::CxxCleanOptionsParser optionParser;

	// ��ʼ��
	if (!Init(optionParser, argc, argv))
	{
		return 0;
	}

	// 1. ����ÿ���ļ�������ӡͳ����־
	Run(optionParser);

	cxxclean::ProjectHistory::instance.Fix();
	cxxclean::ProjectHistory::instance.Print();

	// 2. ��ʼ����
	if (cxxclean::Project::instance.m_need2Step)
	{
		cxxclean::ProjectHistory::instance.m_isFirst		= false;
		cxxclean::ProjectHistory::instance.g_fileNum	= 0;

		Run(optionParser);
	}

	Log("-- finished --!");
	return 0;
}