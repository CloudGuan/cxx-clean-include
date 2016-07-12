///<------------------------------------------------------------------------------
//< @file:   e.h
//< @author: ������
//< @date:   2016��2��8��
//< @brief:	 
//< Copyright (c) 2015 game. All rights reserved.
///<------------------------------------------------------------------------------

#ifndef _e_h_
#define _e_h_

#include "type.h"

typedef int32 E_TypedefInt;

class E_ClassToTypedef
{
public:
	typedef int type;
};

typedef E_ClassToTypedef E_Typedef1;
typedef E_Typedef1 E_Typedef2;
typedef E_Typedef2 E_Typedef3;

template <typename T>
class E
{
public:
	typedef typename T::type type;
};

#endif // _e_h_