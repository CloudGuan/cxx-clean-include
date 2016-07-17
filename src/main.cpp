///<------------------------------------------------------------------------------
//< @file:   main.cpp
//< @author: ������
//< @date:   2016��1��16��
//< @brief:
//< Copyright (c) 2016 game. All rights reserved.
///<------------------------------------------------------------------------------

#include "cxx_clean.h"

#include "llvm/Support/Signals.h"
#include "llvm/Support/TargetSelect.h"

#include "project.h"
#include "vs_project.h"
#include "project_history.h"

bool init(cxxcleantool::CxxCleanOptionsParser &optionParser, int argc, const char **argv)
{
	llvm::sys::PrintStackTraceOnErrorSignal("");

	llvm::InitializeNativeTarget();								// ��ʼ����ǰƽ̨����
	llvm::InitializeNativeTargetAsmParser();					// ֧�ֽ���asm

	// ���������в���
	bool ok = optionParser.ParseOptions(argc, argv, cxxcleantool::g_optionCategory);
	return ok;
}

void run(const cxxcleantool::CxxCleanOptionsParser &optionParser)
{
	ClangTool tool(optionParser.getCompilations(), cxxcleantool::Project::instance.m_cpps);
	tool.clearArgumentsAdjusters();
	tool.appendArgumentsAdjuster(getClangSyntaxOnlyAdjuster());

	optionParser.AddCleanVsArgument(cxxcleantool::Vsproject::instance, tool);

	tool.appendArgumentsAdjuster(getInsertArgumentAdjuster("-fcxx-exceptions",			ArgumentInsertPosition::BEGIN));
	tool.appendArgumentsAdjuster(getInsertArgumentAdjuster("-Winvalid-source-encoding", ArgumentInsertPosition::BEGIN));
	tool.appendArgumentsAdjuster(getInsertArgumentAdjuster("-Wdeprecated-declarations", ArgumentInsertPosition::BEGIN));
	tool.appendArgumentsAdjuster(getInsertArgumentAdjuster("-nobuiltininc",				ArgumentInsertPosition::BEGIN));	// ��ֹʹ��clang���õ�ͷ�ļ�
	tool.appendArgumentsAdjuster(getInsertArgumentAdjuster("-w",						ArgumentInsertPosition::BEGIN));	// ���þ���
	tool.appendArgumentsAdjuster(getInsertArgumentAdjuster("-ferror-limit=5",			ArgumentInsertPosition::BEGIN));	// ���Ƶ���cpp�����ı�����������������ٱ���
	tool.appendArgumentsAdjuster(getInsertArgumentAdjuster("-fpermissive",				ArgumentInsertPosition::BEGIN));	// �Բ�ĳЩ�����ϱ�׼����Ϊ���������ͨ�������Ա�׼����������
	
	DiagnosticOptions diagnosticOptions;
	diagnosticOptions.ShowOptionNames = 1;
	tool.setDiagnosticConsumer(new cxxcleantool::CxxcleanDiagnosticConsumer(&diagnosticOptions)); // ע�⣺������newû��ϵ���ᱻ�ͷ�

	// ��ÿ���ļ������﷨����
	std::unique_ptr<FrontendActionFactory> factory = newFrontendActionFactory<cxxcleantool::CxxCleanAction>();
	tool.run(factory.get());
}

int main(int argc, const char **argv)
{
	// �����н�����
	cxxcleantool::CxxCleanOptionsParser optionParser;

	// ��ʼ��
	if (!init(optionParser, argc, argv))
	{
		return 0;
	}

	// ��1���ÿ���ļ����з�����Ȼ����ܲ���ӡͳ����־
	run(optionParser);

	// ��ӡͳ����־
	cxxcleantool::ProjectHistory::instance.Print();

	// ��2��ſ�ʼ������2��Ͳ���ӡhtml��־��
	if (cxxcleantool::Project::instance.m_need2Step)
	{
		cxxcleantool::ProjectHistory::instance.m_isFirst		= false;
		cxxcleantool::ProjectHistory::instance.g_printFileNo	= 0;

		run(optionParser);
	}

	// ��������ӡ��err���
	llvm::errs() << "-- finished --!\n";
	return 0;
}