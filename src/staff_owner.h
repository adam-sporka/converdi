#pragma once

#include <vector>

namespace converdi
{

class CStaff;

////////////////////////////////////////////////////////////////
class CStaffOwner
{
public:
	void debugStaves(std::string indent);
	virtual void reportSelfForDebug(std::string indent) = 0;

public:
	std::vector<CStaff*> m_apStaves;
};

}