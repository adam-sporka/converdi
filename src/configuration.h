#pragma once

#include <map>
#include <vector>
#include <string>

namespace converdi {

class CConfiguration
{
public:
	static const int m_nMaxVoices = 4;
	int m_TicksPerQuarterNote = 256;
	int m_FirstBarNumber = 1;
	std::vector<std::string> m_DynamicsNames; // Name-to-internal value table

	CConfiguration();
};

extern CConfiguration g_Cfg;

}