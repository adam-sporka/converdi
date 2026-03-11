#include <stdio.h>
#include "converdi.h"

namespace converdi
{

////////////////////////////////////////////////////////////////
int CMusicModel::getTotalTrackCount()
{
	int count = 0;
	for (auto m : m_apStaves)
	{
		count += m->getTotalTrackCount();
	}
	return count;
}

////////////////////////////////////////////////////////////////
int CMusicModel::getMeasureCount()
{
	int max = 0;
	for (auto st : m_apStaves)
	{
		max = max > (int)st->m_apMeasures.size() ? max : (int)st->m_apMeasures.size();
	}
	return max;
}

////////////////////////////////////////////////////////////////
void CMusicModel::chooseJobs(CProfile *job_list)
{
	std::string last_name;
	int reoccurrence = 0;
	for (auto staff : m_apStaves)
	{
		if (staff->m_sName.compare(last_name) == 0) reoccurrence++;
		else reoccurrence = 0;
		last_name = staff->m_sName;

		std::vector<AVAILABLE_JOB*> jobs;
		job_list->getJobsByStaffName(staff->m_sName, jobs);
		if (jobs.size() == 0)
		{
			printf("NOTE: No job for '%s'\n", staff->m_sName.c_str());
			continue;
		}

		auto index = reoccurrence % jobs.size();
		auto job = jobs[index];
		staff->m_pJobToApply = job->m_pJob;

		printf("NOTE: Will use job '%s' on '%s'\n", staff->m_pJobToApply->m_sName.c_str(), staff->m_sName.c_str());
	}

	printf("\n");
}

////////////////////////////////////////////////////////////////
void CMusicModel::applyJobs(bool do_final_only)
{
	for (auto st : m_apStaves)
	{
		if (!st->m_pJobToApply)
		{
			if (!do_final_only)
			{
				printf("  Skipping staff '%s' (#%d). No job for it.\n\n", st->m_sName.c_str(), st->m_nRuntimeNumber);
			}
			continue;
		}

		printf("  Applying %sjob '%s' to staff '%s' (#%d)\n", do_final_only ? "final " : "", st->m_pJobToApply->m_sName.c_str(), st->m_sName.c_str(), st->m_nRuntimeNumber);
		bool do_before_merges = do_final_only ? false : true;
		bool do_after_merges = do_final_only ? true : false;
		st->m_pJobToApply->perform(*st, do_before_merges, do_after_merges);
		st->debugStaves("    ");
		printf("\n");
	}
}

////////////////////////////////////////////////////////////////
void CMusicModel::writeAsMidi(const char *filename)
{
	printf("Writing MIDI file '%s'\n", filename);

	int tracks_needed = getTotalTrackCount();

	smf::MidiFile output;
	output.setTPQ(g_Cfg.m_TicksPerQuarterNote);
	output.absoluteTicks();

	// Track 0 already exists; add one track per instrument staff.
	// Format 1 is written automatically when there is more than one track.
	for (int i = 0; i < tracks_needed; i++)
	{
		output.addTrack();
	}

	// Track 0: tempo and time-signature map
	m_pSystemStaff->generateMidiSignatures(output);

	// Tracks 1..N: notes and CCs, one per staff/substave
	int tr = 1;
	for (auto st : m_apStaves)
	{
		st->generateMidiNotesAndCcs(output, tr);
		tr += st->getTotalTrackCount();
	}

	output.sortTracks();
	output.write(filename);
}

////////////////////////////////////////////////////////////////
void CMusicModel::process()
{
	buildSingleAndGrandStaves();
	if (m_pSystemStaff)
	{
		m_pSystemStaff->process();
	}
	for (auto staff : m_apStaves)
	{
		staff->process();
	}
}

////////////////////////////////////////////////////////////////
void CMusicModel::buildSingleAndGrandStaves()
{
	int compound = 0;
	int g_number = 0;
	CStaff *pGrandStaff = nullptr;
	CStaff *pHighStaff = nullptr;

	// Build grand staves if needed

	for (auto im = m_apStaves.begin(); im != m_apStaves.end(); /**/)
	{
		if (compound == 0)
		{
			if ((*im)->m_nStaffCountPerInstrument > 1)
			{
				compound = (*im)->m_nStaffCountPerInstrument;
				pGrandStaff = new CStaff(this, nullptr, "0", "0");
				pGrandStaff->m_sName = (*im)->m_sName;
				pGrandStaff->m_bGrandStaff = true;
				pGrandStaff->copyMeasureStructure(*im);
				pHighStaff = *im;
				g_number = 0;
				im = m_apStaves.insert(im, pGrandStaff);
				im++;
			}
		}

		if (compound > 0)
		{
			(*im)->m_sLocalLabel = "G";
			(*im)->m_sLocalIndex = std::to_string(++g_number);

			(*im)->m_pParentStaff = pGrandStaff;
			pGrandStaff->m_apStaves.push_back(*im);
			if (*im != pHighStaff)
			{
				(*im)->m_pGrandStaffDynamics = pHighStaff;
			}
			im = m_apStaves.erase(im);
			compound--;
			continue;
		}

		im++;
	}

	// Build single staves out of those which remained

	for (auto i = m_apStaves.begin(); i != m_apStaves.end(); /**/)
	{
		// Processed already
		if ((*i)->m_bGrandStaff)
		{
			i++;
			continue;
		}

		CStaff *pStaff = new CStaff(this, nullptr, "0", "0");
		(*i)->m_sLocalLabel = "G";
		(*i)->m_sLocalIndex = "0";
		(*i)->m_pParentStaff = pGrandStaff;

		pStaff->m_sName = (*i)->m_sName;
		pStaff->m_apStaves.push_back(*i);
		pStaff->copyMeasureStructure(*i);
		i = m_apStaves.insert(i, pStaff);
		i++;

		i = m_apStaves.erase(i);
		continue;
	}
}

////////////////////////////////////////////////////////////////
void CMusicModel::promoteSubstavesOfEmptyStaves(CStaffOwner *owner)
{
	for (auto s : owner->m_apStaves)
	{
		CMusicModel::promoteSubstavesOfEmptyStaves(s);
	}

	std::vector<CStaff*> to_delete;
	std::vector<CStaff*> to_promote;

	for (auto s : owner->m_apStaves)
	{
		if (s->getObjectCount(NOTATION_OBJECT::EType::Note) == 0)
		{
			to_delete.push_back(s);
			for (auto t : s->m_apStaves)
			{
				to_promote.push_back(t);
			}
		}
	}

	for (auto p : to_promote)
	{
		owner->m_apStaves.push_back(p);
		p->m_pParentStaff = owner;
	}

	for (auto it = owner->m_apStaves.begin(); it != owner->m_apStaves.end(); /**/)
	{
		auto found = std::find(to_delete.begin(), to_delete.end(), (*it));
		if (found != to_delete.end())
		{
			it = owner->m_apStaves.erase(it);
		}
		else
		{
			it++;
		}
	}
}

////////////////////////////////////////////////////////////////
void CMusicModel::reportSelfForDebug(std::string indent)
{
}

}