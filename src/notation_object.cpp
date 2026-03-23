#include "converdi.h"

namespace converdi
{

int g_nextId = 1;

////////////////////////////////////////////////////////////////
NOTATION_OBJECT::NOTATION_OBJECT(CMeasure* pMeasure) : m_pMeasure(pMeasure)
{
	m_ID = g_nextId++;
}

////////////////////////////////////////////////////////////////
NOTATION_OBJECT::NOTATION_OBJECT(CMeasure* pMeasure, EType type, ESubtype subtype, TIMELINE_POS pos, int voices, int tremolo, int arpeggio)
	: m_pMeasure(pMeasure)
{
	m_Type = type;
	m_Subtype = subtype;
	m_Position = pos;
	m_nTremolo = tremolo;
	m_nArpeggio = arpeggio;
	m_Voices[0] = (voices & 1) > 0;
	m_Voices[1] = (voices & 2) > 0;
	m_Voices[2] = (voices & 4) > 0;
	m_Voices[3] = (voices & 8) > 0;
	m_ID = g_nextId++;
}

////////////////////////////////////////////////////////////////
NOTATION_OBJECT& NOTATION_OBJECT::operator=(NOTATION_OBJECT &other)
{
	m_pMeasure = other.m_pMeasure;
	m_Type = other.m_Type;
	m_Subtype = other.m_Subtype;
	m_Voices = other.m_Voices;
	m_Position = other.m_Position;
	m_nDuration = other.m_nDuration;
	m_nTremolo = other.m_nTremolo;
	m_nArpeggio = other.m_nArpeggio;
	m_nSoundingMidiNr = other.m_nSoundingMidiNr;
	m_sSoundingName = other.m_sSoundingName;
	m_nWrittenMidiNr = other.m_nWrittenMidiNr;
	m_sWrittenName = other.m_sWrittenName;
	m_nSigNum = other.m_nSigNum;
	m_nSigDenom = other.m_nSigDenom;
	m_fTempoBpm = other.m_fTempoBpm;
	m_DynamicsName = other.m_DynamicsName;
	m_nSemitonesTrill = other.m_nSemitonesTrill;
	m_bTied = other.m_bTied;
	m_bTiedTremolo = other.m_bTiedTremolo;
	m_sText = other.m_sText;
	m_cc = other.m_cc;
	m_ccVal = other.m_ccVal;
	m_bArticulationStaccato = other.m_bArticulationStaccato;
	m_nForcedVelocity = other.m_nForcedVelocity;
	m_nSlurObjID = other.m_nSlurObjID;
	m_nTrillObjID = other.m_nTrillObjID;
	// Runtime fields
	m_bDeleted = other.m_bDeleted;
	m_fTimeAbs = other.m_fTimeAbs;
	m_sDynValue = other.m_sDynValue;
	m_sDynValueEnd = other.m_sDynValueEnd;
	m_bFirstInSegment = other.m_bFirstInSegment;
	m_bLastInSegment = other.m_bLastInSegment;
	return *this;
}

////////////////////////////////////////////////////////////////
std::string NOTATION_OBJECT::formatType()
{
	bool has_subtype;
	std::string str = getETypeAsString(m_Type, has_subtype);
	if (has_subtype)
	{
		str = str + "." + getESubtypeAsString(m_Subtype);
	}
	return str;
}

////////////////////////////////////////////////////////////////
int NOTATION_OBJECT::localTicksToGlobalTicks()
{
	return m_pMeasure->m_nPositionAbs + m_Position.m_TicksInMeasure;
}

////////////////////////////////////////////////////////////////
void NOTATION_OBJECT::debugDump(std::string indent)
{
	auto t = formatType();

	std::string custom;
	if (m_Type == NOTATION_OBJECT::EType::Note)
	{
		custom = " " + std::to_string(m_nSoundingMidiNr) + " " + m_sSoundingName;
	}

	if (m_Type == NOTATION_OBJECT::EType::RitardLine)
	{
		std::ostringstream ss;
		ss << m_fTempoBpm;
		std::string s(ss.str());
		custom = " " + s + " BPM from '" + m_sText + "'";
	}

	if (m_Type == NOTATION_OBJECT::EType::DynamicsValue)
	{
		// std::ostringstream ss;
		// ss << m_fDynValue;
		// std::string s(ss.str());
		custom = " Dyn = " + m_DynamicsName;
	}

	if (m_Type == NOTATION_OBJECT::EType::DynamicsLine)
	{
		std::ostringstream ss;
		ss << m_sDynValue;
		std::string s(ss.str());

		std::ostringstream tt;
		tt << m_sDynValueEnd;
		std::string t(tt.str());

		std::ostringstream uu;
		uu << m_nDuration;
		std::string u(uu.str());

		custom = " Dur = " + u + " From = " + s + " To = " + t;
	}

	if (m_Type == NOTATION_OBJECT::EType::Tempo)
	{
		std::ostringstream ss;
		ss << m_fTempoBpm;
		std::string s(ss.str());

		std::ostringstream tt;
		tt << m_fTimeAbs;
		std::string t(tt.str());

		custom = " " + s + " BPM at " + t + " sec.";
	}

	/*
	if (m_pSlur) {
		char temp[16];
		sprintf_s(temp, 16, "%x", (int)m_pSlur);
		custom += " S";
		custom += temp;
	}
	*/

	if (m_nSlurObjID > 0)
	{
		char temp[16];
		snprintf(temp, 16, " Slur ID = %d", m_nSlurObjID);
		custom += temp;
	}

	if (m_nTrillObjID > 0)
	{
		char temp[16];
		snprintf(temp, 16, " Trill ID = %d", m_nTrillObjID);
		custom += temp;
	}

	printf("%s%d:%d (d=%d) %s %s\n", indent.c_str(), m_Position.m_Measure, m_Position.m_TicksInMeasure, m_nDuration, t.c_str(), custom.c_str());
}

////////////////////////////////////////////////////////////////
void NOTATION_OBJECT::sortNotesAscendingPitch(std::vector<NOTATION_OBJECT*>& to_sort)
{
	std::map<int, NOTATION_OBJECT*> temp;
	for (auto o : to_sort)
	{
		temp[o->m_nSoundingMidiNr] = o;
	}
	to_sort.clear();
	for (auto p : temp)
	{
		to_sort.push_back(p.second);
	}
}

}
