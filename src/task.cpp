#include <stdio.h>
#include "converdi.h"

namespace converdi
{

const char* dynamics[] = { "ppppp", "pppp", "ppp", "pp", "p", "pocop", "mp", "mf", "pocof", "f", "ff", "fff", "ffff", "fffff", "" };

////////////////////////////////////////////////////////////////
CDynamicsTable::CDynamicsTable()
{
	m_Entries["undef"] = 64;
	for (int a = 0; strlen(dynamics[a]); a++)
	{
		m_Entries[dynamics[a]] = 64;
	}
}

////////////////////////////////////////////////////////////////
void CDynamicsTable::linearPppFff(double min, double max)
{
	// dynamics[] has 14 entries: ppppp(0) pppp(1) ppp(2) ... fff(11) ffff(12) fffff(13)
	// Clamp the extremes, then linearly interpolate indices 0..13 across [min, max].
	const int count = 14; // number of named dynamics (excluding sentinel "")
	for (int a = 0; strlen(dynamics[a]); a++)
	{
		double val = min + ((max - min) * ((double)a / (double)(count - 1)));
		m_Entries[dynamics[a]] = val;
	}

	// Hard-clamp the outer extremes
	m_Entries["ppppp"] = min;
	m_Entries["pppp"]  = min;
	m_Entries["ffff"]  = max;
	m_Entries["fffff"] = max;
}

////////////////////////////////////////////////////////////////
bool CDynamicsTable::isValidDynamicsName(std::string name)
{
	if (m_Entries.find(name) != m_Entries.end())
	{
		return true;
	}
	return false;
}

////////////////////////////////////////////////////////////////
double CDynamicsTable::getNumericValue(std::string name)
{
	if (m_Entries.find(name) != m_Entries.end())
	{
		return m_Entries[name];
	}
	return m_Entries["undef"];
}

////////////////////////////////////////////////////////////////
int CTask::fetchParam(std::string parameter, int default_value)
{
	if (m_Parameters.find(parameter) == m_Parameters.end())
	{
		return default_value;
	}
	return m_Parameters[parameter];
}

////////////////////////////////////////////////////////////////
bool CTask::isValidCommand(std::string command)
{
	std::vector<std::string> valid_commands = { 
		"RegionStartCC",
		"RegionEndCC",
		"Arpeggio",
		"RegionLatchKey",
		"SetVelocity",
		"ShiftAllEvents",
		"DynamicsAsVelocity",
		"DynamicsAsCC",
		"TransposeBy",
		"SetNotesTo",
		"TrailByCC",
		"ShortenAllNotes",
		"ShortenLastNote",
		"ProlongExceptLast",
		"PedalAsCC",
		"MergeSubstaves"
	};

	if (std::find(valid_commands.begin(), valid_commands.end(), command) != valid_commands.end())
	{
		return true;
	}
	return false;
}

////////////////////////////////////////////////////////////////
void CTask::perform(CStaff *staff)
{
	printf("    Performing '%s' on '%s' (%s%s, #%d, %d notes)\n", m_CommandName.c_str(), staff->m_sName.c_str(), staff->m_sLocalLabel.c_str(), staff->m_sLocalIndex.c_str(), staff->m_nRuntimeNumber, staff->getObjectCount(NOTATION_OBJECT::EType::Note));

	if (m_CommandName.compare("RegionStartCC") == 0)
	{
		auto cc = fetchParam("cc", 64);
		auto val = fetchParam("val", 127);
		staff->injectCcForRegions(cc, val, true);
	}

	else if (m_CommandName.compare("RegionEndCC") == 0)
	{
		auto cc = fetchParam("cc", 64);
		auto val = fetchParam("val", 0);
		staff->injectCcForRegions(cc, val, false);
	}

	else if (m_CommandName.compare("Arpeggio") == 0)
	{
		auto cc = fetchParam("each_ms", 20);
		auto val = fetchParam("pitch_ms", 1);
		staff->arpeggio(cc, val);
	}

	else if (m_CommandName.compare("ShortenAllNotes") == 0)
	{
		auto ms = fetchParam("ms", -1);
		auto percent = fetchParam("percent", -1);
		auto min_dur = fetchParam("min", 80);
		auto except_tied_tremolos = fetchParam("except_tied_tremolos", 0);
		staff->shortenRegionNotes(ms, percent, min_dur, except_tied_tremolos, false);
	}

	else if (m_CommandName.compare("ShortenLastNote") == 0)
	{
		auto ms = fetchParam("ms", -1);
		auto percent = fetchParam("percent", -1);
		auto min_dur = fetchParam("min", 80);
		auto except_tied_tremolos = fetchParam("except_tied_tremolos", 0);
		staff->shortenRegionNotes(ms, percent, min_dur, except_tied_tremolos, true);
	}

	else if (m_CommandName.compare("ProlongExceptLast") == 0)
	{
		auto ms = fetchParam("ms", 100);
		staff->prolongRegionNotes(ms);
	}

	else if (m_CommandName.compare("RegionLatchKey") == 0)
	{
		auto midi_no = fetchParam("midi_no", 24);
		auto anticipate = fetchParam("anticipate_ms", 0);
		auto prolong = fetchParam("prolong_ms", 0);
		staff->injectKeyLatchForRegions(midi_no, anticipate, prolong);
	}

	else if (m_CommandName.compare("SetVelocity") == 0)
	{
		auto vel = fetchParam("vel", 64);
		staff->setAllNotesToVelocity(vel);
	}

	else if (m_CommandName.compare("DynamicsAsVelocity") == 0)
	{
		staff->adjustVelocitiesForDynamics(*m_pDynTable);
	}

	else if (m_CommandName.compare("DynamicsAsCC") == 0)
	{
		auto cc = fetchParam("cc", 1);
		staff->injectCcForDynamics(cc, *m_pDynTable);
	}

	else if (m_CommandName.compare("TransposeBy") == 0)
	{
		auto semitones = fetchParam("semitones", 2);
		staff->transposeAllNotes(semitones);
	}

	else if (m_CommandName.compare("SetNotesTo") == 0)
	{
		auto midi_no = fetchParam("midi_no", 60);
		staff->setAllNotesToPitch(midi_no);
	}

	else if (m_CommandName.compare("ShiftAllEvents") == 0)
	{
		int delta_t = fetchParam("delta_t", 500);
		float seconds = static_cast<float>(delta_t) / 1000.f;
		staff->shiftAllEvents(seconds);
	}

	else if (m_CommandName.compare("TrailByCC") == 0)
	{
		auto cc = fetchParam("cc", 7);
		auto val = fetchParam("val", 64);
		staff->trailStaffByCC(cc, val);
	}

	else if (m_CommandName.compare("PedalAsCC") == 0)
	{
		auto cc = fetchParam("cc", 64);
		auto val_begin = fetchParam("val_begin", 127);
		auto postpone_ms = fetchParam("postpone_ms", 20);
		auto val_end = fetchParam("val_end", 0);
		auto shorten_ms = fetchParam("shorten_ms", 20);
		staff->pedalAsCC(cc, val_begin, postpone_ms, val_end, shorten_ms);
	}
}

}