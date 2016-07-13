#define _CRT_SECURE_NO_WARNINGS
#include <time.h>						// ����localtimeδ�ɹ����õ����
#include <sstream>
#include <fstream>
#include <sys/stat.h>
#include <io.h>
#include <windows.h>
#include <math.h>
#include <atlcomcli.h>

#define TEXT_C_MACRO_H	"c_macro.h"
#define INCLUDE_C_MACRO_H TEXT_C_MACRO_H

#include "a_func.h"						// ���Ժ���			:a_func.h
#include "b_class.h"					// ������			:b_class.h
#include INCLUDE_C_MACRO_H				// ���Ժ�			:c_macro.h
#include "d_template.h"					// ����ģ��			:d_template.h
#include "e_typedef.h"					// �����Զ���		:e_typedef.h
#include "f_forwarddecl.h"				// ����ǰ������		:f_forwarddecl.h
#include "g_default_arg.h"				// ����Ĭ�ϲ���		:g_default_arg.h
#include "h_use_forwarddecl.h"			// ����ʹ��ǰ������	:h_use_forwarddecl.h
#include "i_deeply_include.h"			// ���Զ��#include	:i_deeply_include.h
#include "j_enumeration.h"				// ����ö��			:j_enumeration.h
#include "k_namespace.h"				// ���������ռ�		:k_namespace.h

#include <stdio.h>
#include <stdio.h>
#include <stdio.h>
#include <stdio.h>
#include <stdio.h>
#include <stdio.h>
#include <stdio.h>
#include <stdio.h>
#include <stdio.h>
#include <stdio.h>
#include <stdio.h>


#include "../hello/../hello/nil.h"		// ��������#include	:j_enumeration.h
#include "nil.h"
#include "nil.h"
#include "nil.h"
#include "nil.h"
#include "nil.h"
#include "nil.h"
#include "nil.h"
#include "nil.h"
#include "nil.h"
#include "nil.h"
#include "nil.h"
#include "nil.h"
#include "nil.h"
#include "nil.h"

#include <iostream>

using namespace std;

///////////////////// 1. ����a_func.h��ĳ���ļ��ڵĺ��������ʹ�õ������Ӧ��#includeӦ�ñ����� /////////////////////

void A_Func_Test()
{
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
}

///////////////////// 2. ����b_class.h��ĳ���ļ��ڵ��������ʹ�õ������Ӧ��#includeӦ�ñ����� /////////////////////

B_Ctor::B_Ctor()
	: m_class(NULL)
{
}

B_Class		b_Class;
B_Struct	b_Struct;
B_Union		b_Union;

class B_DerivedClass : public B_BaseClass
{
	void print() {}

private:
	int m_num;
};

B_ReturnClass GetClass()
{
	return B_ReturnClass();
}

B_ReturnReferenceClass& GetClassReference()
{
	return *(new B_ReturnReferenceClass);
}

void B_Class_Test()
{
	new B_NewClass;
	B_ImplicitConstructorClass b_ImplicitConstructorClass;
	B_ExplicitConstructorClass b_ExplicitConstructorClass(100, 200);
	B_NoNameClass();
	B_ClassPointer *b_ClassPointer;

	B_TempClass b = GetB_TempClass();
	B_DerivedFunction::Print();

	B_5 b5;
	b5.m_b4->m_b3->m_b2->m_b1->test();
}

template<typename T>
const typename B_Color<T>::Color B_Color<T>::color;

///////////////////// 3. ����c_macro.h��������չ����ʹ�� /////////////////////

#if defined C_IfDefined
	#define ok
#endif

#undef C_MacroUndefine

void C_Macro_Test()
{
	int c1 = C_Macro_1('a');
	int c3 = C_Macro_3(1, 2, 3);
	int c4 = C_Macro_4(1, 2, 3, 4);
	int c5 = C_Macro_6(1, 2, 3, 4, 5, 6);

	bool c = C_MacroRedefine(100, 200);
}

///////////////////// 4. ����d_template.h������ģ�� /////////////////////

template <typename T1 = D_1, typename T2 = D_2, typename T3 = D_3, typename T4 = D_Class1<D_Class2<D_Class3<D_4>>>, typename T5 = int>
          char* tostring()
{
	return "";
}

void d_test()
{
	std::set<int> nums = split_str_to_int_set<int>();
	tostring<D_1>();
	D_Class<D_4> d;
}

///////////////////// 5. ����e_typedef.h������typedef /////////////////////


E_TypedefInt GetTypedefInt()
{
	return 0;
}

void E_Typedef_Test()
{
	E_TypedefTemplateClass::type j = 0;
}

///////////////////// 6. ����f_forwarddecl.h������ǰ������ /////////////////////

F_Fowward *f_Fowward = nullptr;

///////////////////// 7. ����g_default_arg.h������Ĭ�ϲ��� /////////////////////

G_DefaultArgument g_DefaultArgument;


///////////////////// 8. ����h_use_forwarddecl.h�������ظ���������� /////////////////////

H* h_Redeclare;

///////////////////// 9. ����i_deeply_include.h�� �����������õ��ļ���#include����������� /////////////////////

I i_DeeplyInclude;

///////////////////// 10. ����j_enumeration.h�� �����������õ��ļ���#include����������� /////////////////////

int j_Enum = (int)J_Enum_4;

void j_func()
{
	std::cout << J_Enum_1 << J_Enum_2 << J_Enum_3 << J_Enum_4;
}

                                   ///////////////////// 11. ����k_namespace.h�����ĳ���ļ���using namespace���ã�����ɶ��û�ã����Կ��ǰ�using namespaceŲ���� /////////////////////

                                   K k_Namespace;
                       K1 k1_Namespace;
                       k_ns::k_2::K2 k2_Namespace;

                       void k_ns::k_1::k_function_in_namespace()
{

}


///////////////////// 12. ����nil.h��nil.h�е����ݲ�Ӧ��ʶ��Ϊ������ /////////////////////

int nil1_func_has_implementation()
{
	return 0;
}

/////////////////////  main /////////////////////

int main(int argc, char **argv)
{
	return 0;
}