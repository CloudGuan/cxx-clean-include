#define _CRT_SECURE_NO_WARNINGS
#include <time.h>						// ����localtimeδ�ɹ����õ����
#include <sstream>
#include <fstream>
#include <sys/stat.h>
#include <io.h>
#include <windows.h>
#include <math.h>

#define TEXT_C_MACRO_H	"c_macro.h"
#define INCLUDE_C_MACRO_H TEXT_C_MACRO_H

#include "a_func.h"						// ���Ժ���
#include "b_class.h"					// ������
#include INCLUDE_C_MACRO_H				// ���Ժ�
#include "d_template.h"					// ����ģ��
#include "e_typedef.h"					// �����Զ���
#include "f_forwarddecl.h"				// ����ǰ������
#include "g_default_arg.h"				// ����Ĭ�ϲ���
#include "h_use_forwarddecl.h"			// ����ʹ��ǰ������
#include "i_deeply_include.h"			// ���Զ��#include
#include "j_enumeration.h"				// ����ö��
#include "k_namespace.h"				// ���������ռ�
#include "l_use_template.h"				// ģ���������
#include "m_using_namespace.h"			// ����using�����ռ�
#include "n_same_forward.h"				// ����ǰ����������ΰ���
#include "o_nested_class.h"				// ����ǰ�������ֲ��ڸ����ļ������
#include "p_same_file.h"				// �����ļ����ظ�����
#include "q_using_class.h"				// ����using��

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
	A_Func(0, "");
}

///////////////////// 2. ����b_class.h��ĳ���ļ��ڵ��������ʹ�õ������Ӧ��#includeӦ�ñ����� /////////////////////

B_Ctor b;

///////////////////// 3. ����c_macro.h��������չ����ʹ�� /////////////////////

int c = C_Macro_1('a');

///////////////////// 4. ����d_template.h������ģ�� /////////////////////

void d_test()
{
	D_Class<D_4> d;
}

///////////////////// 5. ����e_typedef.h������typedef /////////////////////

void e_test()
{
	E_TypedefTemplateClass::type e = 0;
}

///////////////////// 6. ����f_forwarddecl.h������ǰ������ /////////////////////

F_Fowward *f = nullptr;

///////////////////// 7. ����g_default_arg.h������Ĭ�ϲ��� /////////////////////

G_DefaultArgument g;


///////////////////// 8. ����h_use_forwarddecl.h�������ظ���������� /////////////////////

H* h;

///////////////////// 9. ����i_deeply_include.h�� �����������õ��ļ���#include����������� /////////////////////

I i;

///////////////////// 10. ����j_enumeration.h�� ����ö�� /////////////////////

J_Enum j;

///////////////////// 11. ����k_namespace.h�����ĳ���ļ���using namespace���ã�����ɶ��û�ã����Կ��ǰ�using namespaceŲ���� /////////////////////

K k;

///////////////////// 13. ����l_use_template.h��Ӧ��ʶ��ģ��������� /////////////////////

L5_HashMap l;


///////////////////// ����nil.h��nil.h��Ӧ���ظ����� /////////////////////

int nil1_func_has_implementation()
{
	return 0;
}

/////////////////////  main /////////////////////

int main(int argc, char **argv)
{
	return 0;
}