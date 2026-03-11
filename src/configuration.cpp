#include "converdi.h"

namespace converdi
{

CConfiguration g_Cfg;

CConfiguration::CConfiguration()
{
	m_DynamicsNames = {
		"ppppp",
		"pppp",
		"ppp",
		"pp",
		"p",
		"pocop",
		"mp",
		"mf",
		"pocof",
		"f",
		"ff",
		"fff",
		"ffff",
		"fffff",
	};
}

}
