class B_ClassPointer;

B_ClassPointer *b_ClassPointer;

#include "b.h"


class B_Initializer
{
public:
	B_Initializer();

	B_Class *m_class;
};

// �������Ժ���������ʱ����
class B_TempClass
{
	int num;
};

B_TempClass GetB_TempClass()
{
	return B_TempClass();
}