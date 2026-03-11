#pragma once

#include <vector>

namespace converdi
{

class CStaff;

////////////////////////////////////////////////////////////////
class CMeasure
{
public:
	std::vector<NOTATION_OBJECT *> m_apNotationObjects;
	CMeasure *m_pNextMeasure = nullptr;
	CStaff* m_pStaff = nullptr;

	CMeasure(CStaff *staff, int seq_number) : m_pStaff(staff), m_nNumber(seq_number) {};
	void insertObject(NOTATION_OBJECT *object);
	NOTATION_OBJECT *findSubseqNoteSameAs(NOTATION_OBJECT* as, bool within_this_measure);
	void debugDump(std::string indent);

public:
	int m_nNumber; // Must not be smaller than g_Cfg.m_FirstBarNumber at runtime
	int m_nSignature = 0;
	int m_nDuration = 0; // In ticks
	int m_nPositionAbs = 0; // In ticks, 0 = start of the song
};

}