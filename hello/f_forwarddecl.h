#ifndef _f_forwarddecl_h_
#define _f_forwarddecl_h_

class F_2;
#include "f_1.h"

class F_Fowward
{
public:
	F_Fowward(F_1& f)
		: m_f1(f)
	{
	}

public:
	F_1 m_f1;
};

#endif // _f_forwarddecl_h_