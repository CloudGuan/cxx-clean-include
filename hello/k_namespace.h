///<------------------------------------------------------------------------------
//< @file:   k_namespace.h
//< @author: ������
//< @date:   2016��3��26��
//< @brief:	 ��Ҫ���������ռ�
//< Copyright (c) 2016 game. All rights reserved.
///<------------------------------------------------------------------------------

#ifndef _k_namespace_h_
#define _k_namespace_h_

namespace k_ns
{
	namespace k_1
	{
		void k_function_in_namespace();
	}
}

using namespace k_ns::k_1;

#include "k.h"

// ����Ҫ���Ե�����ǣ����豾�ļ�ɶҲû�У���һ��using namespace����ȡ���������ļ�ȴ�ᵼ�±������
using namespace k_ns;

#endif // _k_namespace_h_