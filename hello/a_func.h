// #include <stdio.h>

#include "a.h"

void A::A_ClassMemberDelayImplementFunc3()
{

}

A A_Func(int a, const char* b)
{
	return A();
}

#define  Macro_A_Func(a, b) A_Func(a, b)

//------------ ����clang�޷�ʶ�������ʽ�����غ������� ------------//

class IBase
{
};

class A_Derived : public IBase
{
};

class A_OverloadBug
{
public:
	void func(const char* c, IBase&){}

	void func(unsigned int c, IBase&){}

public:
	static A_OverloadBug* m_a;
};

A_OverloadBug* A_OverloadBug::m_a = new A_OverloadBug;

template<>
void convert(const A1& a1, A2& a2)
{
	a2.bbbbbbb = a1.aaaaaaa;
}