#include <new>

//------------------- ����operator new -------------------//

class B
{
};

void B_test_operator_new()
{
	B* c = new ((void*)0) B();
}

//------------------- ����ָ�롢ǰ������ -------------------//

class B_ClassPointer;

B_ClassPointer *b_ClassPointer;

#include "b.h"

//------------------- ���Ի��ࡢ���� -------------------//

class B_DerivedFunction : public B_BaseClass
{
};

class B_Ctor : private B_BaseClass
{
public:
	B_Ctor();

	B_Class *m_class;
};

//------------------- ������ʱ���� -------------------//

// �������Ժ���������ʱ����
class B_TempClass
{
	int num;
};

B_TempClass GetB_TempClass()
{
	return B_TempClass();
}

//------------------- ����ģ���� -------------------//

template<typename T>
class B_Color
{
	enum Color
	{
		Green,
		Yellow,
		Blue,
		Red
	};

private:
	static const Color color = (Color)2;
};

//------------------- ���Զ������ʽ���� -------------------//

class B_2
{ 
public:
	B_1 *m_b1;
};
class B_3
	{ 
public:
	B_2 *m_b2;
};
class B_4
	{ 
public:
	B_3 *m_b3;
};
class B_5
{ 
public:
	B_5(){}

public:
	B_4 *m_b4;
};