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

int main(int argc, const char **argv)
{
	llvm::sys::PrintStackTraceOnErrorSignal();

	llvm::InitializeNativeTarget();								// ��ʼ����ǰƽ̨����
	llvm::InitializeNativeTargetAsmParser();					// ֧�ֽ���asm

	// ���������в���
	cxxcleantool::CxxCleanOptionsParser optionParser;

	bool ok = optionParser.ParseOptions(argc, argv, cxxcleantool::g_optionCategory);
	if (!ok)
	{
		return 0;
	}

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
	
	DiagnosticOptions diagnosticOptions;
	diagnosticOptions.ShowOptionNames = 1;
	tool.setDiagnosticConsumer(new cxxcleantool::CxxcleanDiagnosticConsumer(&diagnosticOptions)); // ע�⣺������newû��ϵ���ᱻ�ͷ�

	// ��1���ÿ���ļ����з�����Ȼ����ܲ���ӡͳ����־
	std::unique_ptr<FrontendActionFactory> factory = newFrontendActionFactory<cxxcleantool::CxxCleanAction>();
	tool.run(factory.get());

	// ��ӡͳ����־
	cxxcleantool::ProjectHistory::instance.Print();

	// ��2��ſ�ʼ������2��Ͳ���ӡhtml��־��
	if (cxxcleantool::Project::instance.m_need2Step)
	{
		cxxcleantool::ProjectHistory::instance.m_isFirst		= false;
		cxxcleantool::ProjectHistory::instance.g_printFileNo	= 0;
		tool.run(factory.get());
	}

	// ��������ӡ��err���
	llvm::errs() << "-- finished --!\n";
	return 0;
}