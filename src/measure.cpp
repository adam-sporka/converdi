#include "converdi.h"

namespace converdi
{

////////////////////////////////////////////////////////////////
void CMeasure::insertObject(NOTATION_OBJECT *object)
{
	m_apNotationObjects.push_back(object);
}

////////////////////////////////////////////////////////////////
NOTATION_OBJECT *CMeasure::findSubseqNoteSameAs(NOTATION_OBJECT* as, bool within_this_measure)
{
	bool woke = !within_this_measure;
	for (auto n : m_apNotationObjects)
	{
		if (as->m_Type != NOTATION_OBJECT::EType::Note)
		{
			continue;
		}

		if (n == as)
		{
			woke = true;
		}
		else
		{
			if (woke)
			{
				if (n->m_nSoundingMidiNr == as->m_nSoundingMidiNr)
				{
					// Check continuity
					auto end_of_one = as->localTicksToGlobalTicks() + as->m_nDuration;
					auto start_of_other = n->localTicksToGlobalTicks();

					if (abs(start_of_other - end_of_one) < 3)
					{
						return n;
					}
				}
			}
		}
	}

	if (within_this_measure)
	{
		if (m_pNextMeasure)
		{
			return m_pNextMeasure->findSubseqNoteSameAs(as, false);
		}
	}

	return nullptr;
}

////////////////////////////////////////////////////////////////
void CMeasure::debugDump(std::string indent)
{
	if (m_apNotationObjects.size() > 0)
	{
		printf("%sMEASURE %d\n", indent.c_str(), m_nNumber);
		for (auto n : m_apNotationObjects) n->debugDump(indent + "  @ ");
	}
}

}
