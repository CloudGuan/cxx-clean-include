#include "a_func.h"

void A::A_ClassMemberDelayImplementFunc3()
{
}

A_OverloadBug* A_OverloadBug::m_a = new A_OverloadBug;

///////////////////// 1. ����a_func.h��ĳ���ļ��ڵĺ��������ʹ�õ������Ӧ��#includeӦ�ñ����� /////////////////////

int a = 99999;

void A_Func_Test(A_Derived *derived)
{
	derived->test();

	int n = 0;

	n = A_TopFunc(100);
	n = A_TemplateFunc(100, 200);
	n = A::A_StaticClassMemberFunc();
	n = unsigned int(0); // ������б����������clang��bug��clang��֧��int(0)��ȴ��֧��unsigned int(0)������Ҫ��clang\lib\Parse\ParseExprCXX.cpp�е�Parser::ParseCXXSimpleTypeSpecifier�������޸�ʹ��whileѭ�������Ų����б������

	auto func = A::A_FuncPointer;

	A a;
	a.A_ClassMemberFunc();

	Macro_A_Func(100, "abcdefg");

	A_OverloadBug::m_a->func("", A_Derived());

	size_t s = sizeof(A_Func(10, ""));

	A1 a1;
	A2 a2;
	convert2(a1, a2);
}