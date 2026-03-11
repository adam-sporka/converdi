#include "converdi.h"

namespace converdi
{

////////////////////////////////////////////////////////////////
void CStaffOwner::debugStaves(std::string indent)
{
	reportSelfForDebug(indent);
	for (auto s : m_apStaves)
	{
		s->debugStaves(indent + "  ");
	}
}

}