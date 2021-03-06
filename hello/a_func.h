
// 这条#include是用来测试#pragma once的

#include "a.h"

static A A_Func(int a, const char* b)
{
	return A();
}

#define  Macro_A_Func(a, b) A_Func(a, b)

//------------ 测试clang无法识别下面格式的重载函数问题 ------------//

class IBase
{
public:
	void test(){}
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

template<>
inline void convert(const A1& a1, A2& a2)
{
	a2.bbbbbbb = a1.aaaaaaa;
}