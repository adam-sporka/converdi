#include "converdi.h"

#include <iterator>
#include <set>

namespace converdi
{

int g_NextStaffRuntimeNumber = 1;

CStaff::CStaff(CMusicModel *model, CStaff *pParentStaff, std::string local_label, std::string local_index)
	: m_pMusicModel(model)
{
	m_pUseDynamics = this;
	m_sLocalLabel = local_label;
	m_sLocalIndex = local_index;
	m_pParentStaff = pParentStaff;

	m_nRuntimeNumber = g_NextStaffRuntimeNumber++;
}

////////////////////////////////////////////////////////////////
void CStaff::copyMeasureStructure(CStaff *source)
{
	for (auto *m : source->m_apMeasures)
	{
		CMeasure *measure = new CMeasure(this, m->m_nNumber);
		measure->m_nDuration = m->m_nDuration;
		measure->m_nSignature = m->m_nSignature;
		m_apMeasures.push_back(measure);
	}
	process();
}

////////////////////////////////////////////////////////////////
int CStaff::getTotalTrackCount()
{
	int count = 1;
	for (auto s : m_apStaves)
	{
		count += s->getTotalTrackCount();
	}
	return count;
}

////////////////////////////////////////////////////////////////
int CStaff::getObjectCount(NOTATION_OBJECT::EType object_type)
{
	int count = 0;
	for (auto m : m_apMeasures)
	{
		for (auto n : m->m_apNotationObjects)
		{
			if (n->m_Type != object_type) continue;
			count++;
		}
	}
	return count;
}

////////////////////////////////////////////////////////////////
bool CStaff::getNotesPresenceInVoice(int voice)
{
	for (auto m : m_apMeasures)
	{
		for (auto n : m->m_apNotationObjects)
		{
			if (n->m_Type != NOTATION_OBJECT::EType::Note) continue;
			if (n->m_Voices[voice]) return true;
		}
	}
	return false;
}

////////////////////////////////////////////////////////////////
int CStaff::purgeDeletedObjects()
{
	int removed = 0;
	for (auto m : m_apMeasures)
	{
		for (auto it = m->m_apNotationObjects.begin(); it != m->m_apNotationObjects.end(); )
		{
			if ((*it)->m_bDeleted)
			{
				it = m->m_apNotationObjects.erase(it);
				removed++;
			}
			else
			{
				it++;
			}
		}
	}
	return removed;
}

////////////////////////////////////////////////////////////////
void CStaff::process()
{
	buildMeasureLinkedList();
	calcMeasureAbsPositions();

	if (m_bSystemStaff)
	{
		fetchRitardParams();
		copyFirstTempoMark();
		createTempoMarksByRetards();
		calcTempoMarkAbsTimes();
	}
	else
	{
		assignNotesToSlursAndTrills();
		applyNoteTies();
	}
}

////////////////////////////////////////////////////////////////
void CStaff::shiftAllEvents(float delta_t)
{
	std::map<int, std::vector<NOTATION_OBJECT *>> temp;
	for (auto m : m_apMeasures)
	{
		for (auto n : m->m_apNotationObjects)
		{
			int global_ticks = n->localTicksToGlobalTicks();
			double seconds;
			if (m_pMusicModel->m_pSystemStaff->globalTicksToGlobalSeconds(global_ticks, seconds))
			{
				seconds += delta_t;
				int measure = n->m_Position.m_Measure;
				int local_ticks = n->m_Position.m_TicksInMeasure;
				if (m_pMusicModel->m_pSystemStaff->globalSecondsToLocalTicks(seconds, measure, local_ticks))
				{
					n->m_Position.m_Measure = measure;
					n->m_Position.m_TicksInMeasure = local_ticks;
				}
				temp[measure].push_back(n);
			}
		}
	}
	
	for (auto m : m_apMeasures)
	{
		m->m_apNotationObjects.clear();
	}

	for (auto &d : temp)
	{
		auto *pMeasure = getMeasureByNumber(d.first);
		if (!pMeasure) continue;
		for (auto o : d.second)
		{
			pMeasure->insertObject(o);
			o->m_pMeasure = pMeasure;
		}
	}

	sortNotationObjects();
}

////////////////////////////////////////////////////////////////
void CStaff::pedalAsCC(int cc, int val_begin, int postpone_ms, int val_end, int shorten_ms)
{
	int measure_begin, ticks_begin;
	int measure_end, ticks_end;

	std::map<CMeasure*, std::vector<NOTATION_OBJECT*>> objects_to_add;
	float shorten_seconds = (float)shorten_ms / 1000.0f;
	float postpone_seconds = (float)postpone_ms / 1000.0f;

	for (auto m : m_apMeasures)
	{
		for (auto n : m->m_apNotationObjects)
		{
			int abs_begin, abs_end;
			if (n->m_Type == NOTATION_OBJECT::EType::PedalLine)
			{
				// global ticks begin
				if (!localTicksToGlobalTicks(n->m_Position, abs_begin))
				{
					continue;
				}

				// global ticks end
				abs_end = abs_begin + n->m_nDuration;

				// time begin
				double seconds_begin;
				if (!m_pMusicModel->m_pSystemStaff->globalTicksToGlobalSeconds(abs_begin, seconds_begin))
				{
					continue;
				}

				// postpone begin
				seconds_begin += postpone_seconds;
				if (!m_pMusicModel->m_pSystemStaff->globalSecondsToLocalTicks(seconds_begin, measure_begin, ticks_begin))
				{
					continue;
				}

				// check where landed
				if (!localTicksToGlobalTicks(measure_begin, ticks_begin, abs_begin))
				{
					continue;
				}

				// time end
				double seconds_end;
				if (!m_pMusicModel->m_pSystemStaff->globalTicksToGlobalSeconds(abs_end, seconds_end))
				{
					continue;
				}

				// shorten the end
				seconds_end -= shorten_seconds;
				if (!m_pMusicModel->m_pSystemStaff->globalSecondsToLocalTicks(seconds_end, measure_end, ticks_end))
				{
					continue;
				}

				// Check where landed
				if (!localTicksToGlobalTicks(measure_end, ticks_end, abs_end))
				{
					continue;
				}

				// Must be at least SOME length. Skip otherwise.
				if (abs_end <= abs_begin)
				{
					continue;
				}

				int begin_measure_index = measure_begin - g_Cfg.m_FirstBarNumber;
				int end_measure_index = measure_end - g_Cfg.m_FirstBarNumber;

				auto *begin_measure = m_apMeasures[begin_measure_index];
				auto *end_measure = m_apMeasures[end_measure_index];

				NOTATION_OBJECT *object = new NOTATION_OBJECT(begin_measure, NOTATION_OBJECT::EType::CC, NOTATION_OBJECT::ESubtype::Subtype_undef, { measure_begin, ticks_begin }, 15);
				object->m_nDuration = 0;
				object->m_cc = cc;
				object->m_ccVal = val_begin;
				objects_to_add[begin_measure].push_back(object);

				NOTATION_OBJECT *object2 = new NOTATION_OBJECT(end_measure, NOTATION_OBJECT::EType::CC, NOTATION_OBJECT::ESubtype::Subtype_undef, { measure_end, ticks_end }, 15);
				object2->m_nDuration = 0;
				object2->m_cc = cc;
				object2->m_ccVal = 0;
				objects_to_add[end_measure].push_back(object2);
			}
		}
	}

	for (auto &m2oo : objects_to_add)
	{
		for (auto *o : m2oo.second)
		{
			m2oo.first->insertObject(o);
		}
	}

	sortNotationObjects();
}

////////////////////////////////////////////////////////////////
void CStaff::transposeAllNotes(int delta)
{
	for (auto m : m_apMeasures)
	{
		for (auto n : m->m_apNotationObjects)
		{
			if (n->m_Type == NOTATION_OBJECT::EType::Note)
			{
				n->m_nSoundingMidiNr += delta;
				if (n->m_nSoundingMidiNr < 0) n->m_nSoundingMidiNr = 0;
				if (n->m_nSoundingMidiNr > 127) n->m_nSoundingMidiNr = 127;
			}
		}
	}
}

////////////////////////////////////////////////////////////////
void CStaff::setAllNotesToPitch(int midi_no)
{
	for (auto m : m_apMeasures)
	{
		for (auto n : m->m_apNotationObjects)
		{
			if (n->m_Type == NOTATION_OBJECT::EType::Note)
			{
				n->m_nSoundingMidiNr = midi_no;
			}
		}
	}
}

////////////////////////////////////////////////////////////////
void CStaff::setAllNotesToVelocity(int velocity)
{
	for (auto m : m_apMeasures)
	{
		for (auto n : m->m_apNotationObjects)
		{
			if (n->m_Type == NOTATION_OBJECT::EType::Note)
			{
				n->m_nForcedVelocity = velocity;
			}
		}
	}
}

////////////////////////////////////////////////////////////////
void CStaff::treatStaccatosAsShortNotes(float seconds)
{
	for (auto m : m_apMeasures)
	{
		for (auto n : m->m_apNotationObjects)
		{
			if (n->m_Type == NOTATION_OBJECT::EType::Note)
			{
				if (n->m_bArticulationStaccato)
				{
					double start_at, end_at;
					globalTicksToGlobalSeconds(n->localTicksToGlobalTicks(), start_at);
					end_at = start_at + seconds;
					int m, t;
					globalSecondsToLocalTicks(end_at, m, t);
					int gl;
					localTicksToGlobalTicks(m, t, gl);
					int delta_ticks = gl - n->localTicksToGlobalTicks();
					n->m_nDuration = delta_ticks;
				}
				/*
				else if (n->m_pSlur != nullptr)
				{
					n->m_nDuration = (n->m_nDuration * 3) / 4;
				}
				*/
			}
		}
	}
}

////////////////////////////////////////////////////////////////
void CStaff::handleGrandStaffDynamics()
{
	if (m_pGrandStaffDynamics == nullptr) return;

	std::vector<NOTATION_OBJECT*> available_from_source;
	for (auto m : m_pGrandStaffDynamics->m_apMeasures)
	{
		for (auto n : m->m_apNotationObjects)
		{
			if (n->m_Type == NOTATION_OBJECT::EType::DynamicsLine) available_from_source.push_back(n);
			if (n->m_Type == NOTATION_OBJECT::EType::DynamicsValue) available_from_source.push_back(n);
		}
	}

	printf("\n");
	for (auto i : available_from_source)
	{
		i->debugDump("   AVAILABLE:  ");
		int measure_index = i->m_Position.m_Measure - g_Cfg.m_FirstBarNumber;
	}

	printf("\n");
	for (auto m : m_apMeasures)
	{
		for (auto n : m->m_apNotationObjects)
		{
			if (n->m_Type == NOTATION_OBJECT::EType::DynamicsLine)
			{
				n->debugDump("   EXISTS   ");
			}
			if (n->m_Type == NOTATION_OBJECT::EType::DynamicsValue)
			{
				n->debugDump("   EXISTS   ");
			}
		}
	}

	printf("\n");
	for (auto m : m_apMeasures)
	{
		for (auto n : m->m_apNotationObjects)
		{
			if (n->m_Type != NOTATION_OBJECT::EType::DynamicsLine && n->m_Type != NOTATION_OBJECT::EType::DynamicsValue)
			{
				continue;
			}

			int start_masking = n->localTicksToGlobalTicks();
			int end_masking = start_masking + n->m_nDuration;

			for (auto i = available_from_source.begin(); i != available_from_source.end(); )
			{
				int start_available = (*i)->localTicksToGlobalTicks();
				int end_available = start_available + (*i)->m_nDuration;

				// the start of the available is hidden by the masking
				if (start_available >= start_masking && start_available <= end_masking)
				{
					(*i)->debugDump("   DEFAULT OBJECT ");
					n->debugDump("   -- HIDDEN BY ");
					i = available_from_source.erase(i);
					continue;
				}
				// the start of the default happens before the onset of the non-default
				// ==> terminate at the start of the non-default
				else if (start_available < start_masking && end_available >= start_masking)
				{
					(*i)->debugDump("   DEFAULT OBJECT ");
					n->debugDump("   -- SHORTENED BY ");
					(*i)->m_nDuration = (start_masking - start_available) - 1;
				}
				i++;
			}
		}
	}

	printf("\n");

	// Copy all default
	for (auto i : available_from_source)
	{
		i->debugDump("   COPYING ");
		int measure_index = i->m_Position.m_Measure - g_Cfg.m_FirstBarNumber;
		NOTATION_OBJECT *copy = new NOTATION_OBJECT(nullptr);
		copy->operator=(*i);
		copy->m_pMeasure = m_apMeasures[measure_index];
		copy->m_pMeasure->m_apNotationObjects.push_back(copy);
	}

	sortNotationObjects();

	printf("\n");
	for (auto m : m_apMeasures)
	{
		for (auto n : m->m_apNotationObjects)
		{
			if (n->m_Type == NOTATION_OBJECT::EType::DynamicsLine)
			{
				n->debugDump("   EXISTS ");
			}
			if (n->m_Type == NOTATION_OBJECT::EType::DynamicsValue)
			{
				n->debugDump("   EXISTS");
			}
		}
	}
}

////////////////////////////////////////////////////////////////
CMeasure* CStaff::getMeasureByNumber(int measure)
{
	for (auto *m : m_apMeasures)
	{
		if (m->m_nNumber == measure)
		{
			return m;
		}
	}
	return nullptr;
}

////////////////////////////////////////////////////////////////
bool CStaff::insideSegment(int gl)
{
	// No segments? Consider the entire staff one long segment.
	if (m_aSegments.size() == 0) return true;

	for (auto s : m_aSegments)
	{
		if (gl >= s.first && gl < s.second) return true;
	}
	return false;
}

////////////////////////////////////////////////////////////////
void CStaff::gatherSegments()
{
	int start = -1;
	int end = -1;

	for (auto m : m_apMeasures)
	{
		for (auto n : m->m_apNotationObjects)
		{
			if (n->m_bFirstInSegment)
			{
				start = n->localTicksToGlobalTicks();
			}
			if (n->m_bLastInSegment)
			{	
				end = n->localTicksToGlobalTicks() + n->m_nDuration;
				m_aSegments.push_back({ start, end });
			}
		}
	}
}

////////////////////////////////////////////////////////////////
bool CStaff::areDelimitedSegments()
{
	return m_aSegments.size() > 0;
}

////////////////////////////////////////////////////////////////
NOTATION_OBJECT* CStaff::getNextNoteAfter(NOTATION_OBJECT* object)
{
	// Walk forward through the measure linked list until we find a Note.
	bool found_self = false;

	for (auto *measure = object->m_pMeasure; measure; measure = measure->m_pNextMeasure)
	{
		for (auto n : measure->m_apNotationObjects)
		{
			if (!found_self)
			{
				if (n == object) found_self = true;
				continue;
			}

			if (n->m_Type == NOTATION_OBJECT::EType::Note)
			{
				return n;
			}
		}
	}

	return nullptr;
}

////////////////////////////////////////////////////////////////
NOTATION_OBJECT* CStaff::findLastNoteEndingOnOrBefore(int border_tick, CMeasure* start_with)
{
	if (m_apMeasures.size() == 0)
	{
		return nullptr;
	}

	if (start_with == nullptr)
	{
		start_with = m_apMeasures[0];
	}

	NOTATION_OBJECT *latest = nullptr;
	int latest_end_tick = -1;
	int latest_pitch = -1;

	for (auto *measure = start_with; measure && (measure->m_nPositionAbs < border_tick); measure = measure->m_pNextMeasure)
	{
		for (auto *n : measure->m_apNotationObjects)
		{
			int this_end_tick = n->localTicksToGlobalTicks() + n->m_nDuration;
			int this_pitch = n->m_nWrittenMidiNr;
			
			if (this_end_tick > border_tick) continue;

			if (this_end_tick > latest_end_tick)
			{
				latest = n;
				latest_end_tick = this_end_tick;
				latest_pitch = -1;
			}
			else if (this_end_tick == latest_end_tick)
			{
				if (this_pitch > latest_pitch) {
					latest = n;
					latest_pitch = this_pitch;
				}
			}
		}
	}

	return latest;
}

////////////////////////////////////////////////////////////////
void CStaff::delimitSegmentsInSubstaves()
{
	// Gather all notes

	std::vector<NOTATION_OBJECT*> all_notes;
	for (auto s : m_apStaves)
	{
		for (auto m : s->m_apMeasures)
		{
			for (auto n : m->m_apNotationObjects)
			{
				if (n->m_Type == NOTATION_OBJECT::EType::Note)
				{
					all_notes.push_back(n);
				}
			}
		}
	}

	// Sort notes

	std::sort(all_notes.begin(), all_notes.end(), [](NOTATION_OBJECT *o1, NOTATION_OBJECT *o2)
	{
		auto lt1 = o1->localTicksToGlobalTicks();
		auto lt2 = o2->localTicksToGlobalTicks();
		bool earlier = lt1 < lt2;
		if (earlier) return true;
		bool later = lt1 > lt2;
		if (later) return false;

		auto staff1 = o1->m_pMeasure->m_pStaff;
		auto staff2 = o2->m_pMeasure->m_pStaff;
		bool staff_above = staff1->m_nTrackIndex < staff2->m_nTrackIndex;
		if (staff_above) return true;
		bool staff_below = staff1->m_nTrackIndex > staff2->m_nTrackIndex;
		if (staff_below) return false;

		bool lower_pitch = o1->m_nSoundingMidiNr < o2->m_nSoundingMidiNr;
		return lower_pitch;
	});

	// Determine the region boundaries

	CStaff *pActiveStaff = nullptr;
	CStaff *pLastStaff = nullptr;
	NOTATION_OBJECT *pLastObject = nullptr;
	int nLastGlobalTick = -1;

	for (auto n : all_notes)
	{
		auto this_global_tick = n->localTicksToGlobalTicks();
		auto *this_staff = n->m_pMeasure->m_pStaff;

		// printf("%s %s\n", n->m_Position.format(temp, 32), this_staff->m_sName.c_str());

		if (pLastStaff != this_staff)
		{
			pLastStaff = this_staff;
			// printf("BOUNDARY FOUND %s, %s takes over\n", n->m_Position.format(temp, 32), this_staff->m_sName.c_str());

			n->m_bFirstInSegment = true;
			if (pLastObject)
			{
				pLastObject->m_bLastInSegment = true;
				// printf("LAST OBJECT IN SEGMENT: %s %d %d %d\n", m_sName.c_str(), pLastObject->m_nSoundingMidiNr, pLastObject->m_Position.m_Measure, pLastObject->m_Position.m_TicksInMeasure);
			}
		}
		pLastObject = n;
	}

	if (all_notes.size() > 0)
	{
		auto last = all_notes.end();
		(*(--last))->m_bLastInSegment = true;
	}

	gatherSegments();
	for (auto s : m_apStaves)
	{
		s->gatherSegments();
	}

	/*
	for (auto b : boundaries)
	{
		printf("  %s (%d instances)\n", b.first.c_str(), b.second);
	}
	*/
}

////////////////////////////////////////////////////////////////
void CStaff::lookupDynamicsValuesTree()
{
	lookupDynamicsValues();
	for (auto s : m_apStaves)
	{
		s->lookupDynamicsValuesTree();
	}
}

////////////////////////////////////////////////////////////////
void CStaff::lookupDynamicsValues()
{
	if (m_bDynamicsLookedUp)
	{
		return;
	}

	// CONVERT NAMES TO NUMERIC VALUES

	for (auto m : m_apMeasures)
	{
		for (auto n : m->m_apNotationObjects)
		{
			if (n->m_Type != NOTATION_OBJECT::EType::DynamicsValue) continue;
			n->m_sDynValue = n->m_DynamicsName;
		}
	}

	// ASSIGN VALUES TO LINES

	NOTATION_OBJECT *pDynamicsLine = nullptr;

	double last_dyn_value = 0.0;

	std::vector<NOTATION_OBJECT*> values;
	std::vector<NOTATION_OBJECT*> lines;

	for (auto m : m_apMeasures)
	{
		for (auto n : m->m_apNotationObjects)
		{
			if (n->m_Type == NOTATION_OBJECT::EType::DynamicsValue) values.push_back(n);
			if (n->m_Type == NOTATION_OBJECT::EType::DynamicsLine) lines.push_back(n);
		}
	}

	for (auto l : lines)
	{
		int start = l->localTicksToGlobalTicks();
		int end = l->localTicksToGlobalTicks() + l->m_nDuration;

		for (auto v : values)
		{
			// Need to truncate the end
			if (v->localTicksToGlobalTicks() > start && v->localTicksToGlobalTicks() < end - 1)
			{
				l->m_nDuration = v->localTicksToGlobalTicks() - start;
			}
		}

		std::string at_start = "";
		std::string at_end = "";
		for (auto v : values)
		{
			if (v->localTicksToGlobalTicks() <= l->localTicksToGlobalTicks()) at_start = v->m_sDynValue;
			if (v->localTicksToGlobalTicks() > (l->localTicksToGlobalTicks() + l->m_nDuration - 1))
			{
				at_end = v->m_sDynValue;
				l->m_sDynValue = at_start;
				l->m_sDynValueEnd = at_end;
				break;
			}
		}
	}

	purgeDeletedObjects();

	for (auto m : m_apMeasures)
	{
		for (auto n : m->m_apNotationObjects)
		{
			if (n->m_Type == NOTATION_OBJECT::EType::DynamicsValue) n->m_Voices = { true, true, true, true };
			if (n->m_Type == NOTATION_OBJECT::EType::DynamicsLine) n->m_Voices = { true, true, true, true };
		}
	}

	m_bDynamicsLookedUp = true;
}

////////////////////////////////////////////////////////////////
void CStaff::shortenRegionNotes(int by_ms, int by_percent, int min_duration, int except_tied_tremolos, bool last_note_only)
{
	float fMs = 0.0f;
	float fPerc = 0.0f;

	if (by_ms > 0)
	{
		fMs = static_cast<float>(by_ms) / 1000.0f;
	}
	else if (by_percent > 0)
	{
		fPerc = static_cast<float>(by_percent) / 100.0f;
	}
	
	float fMinDur = static_cast<float>(min_duration) / 1000.0f;

	for (auto m : m_apMeasures)
	{
		for (auto n : m->m_apNotationObjects)
		{
			if (n->m_Type != NOTATION_OBJECT::EType::Note)
			{
				continue;
			}

			bool last_because_last_in_segment = n->m_bLastInSegment;

			bool last_because_different_slur = false;
			auto *next_note = getNextNoteAfter(n);
			if (next_note)
			{
				if (next_note->m_nSlurObjID != n->m_nSlurObjID)
				{
					last_because_different_slur = true;
				}
			}

			if (last_note_only && (!(last_because_last_in_segment || last_because_different_slur)))
			{
				continue;
			}

			if (except_tied_tremolos)
			{
				if (n->m_bTiedTremolo)
				{
					continue;
				}
			}

			// Global start
			double start_original;
			if (!m_pMusicModel->m_pSystemStaff->globalTicksToGlobalSeconds(n->localTicksToGlobalTicks(), start_original))
			{
				continue;
			}

			// Global end tick
			auto global_ticks_end = n->localTicksToGlobalTicks() + n->m_nDuration;
			double end_original;
			if (!m_pMusicModel->m_pSystemStaff->globalTicksToGlobalSeconds(global_ticks_end, end_original))
			{
				continue;
			}

			// Don't do anything, the note is already shorter than the minimum
			if (end_original - start_original < fMinDur)
			{
				continue;
			}

			double end_new;

			// Calculate new duration, if static delta provided
			if (fMs > 0.0f)
			{
				end_new = end_original - fMs;
			}
			// Calculate new duration, if percentage provided
			else
			{
				auto dur_original = end_original - start_original;
				auto dur_new = dur_original * (1.0f - fPerc);
				end_new = start_original + dur_new;
			}

			// Don't do anything, otherwise the note would be shorter than the minimum
			if (end_new - start_original < fMinDur)
			{
				continue;
			}

			int measure, tick, end_new_ticks;
			if (!m_pMusicModel->m_pSystemStaff->globalSecondsToLocalTicks(end_new, measure, tick))
			{
				continue;
			}

			m_pMusicModel->m_pSystemStaff->localTicksToGlobalTicks(measure, tick, end_new_ticks);

			int delta_ticks = global_ticks_end - end_new_ticks;
			n->m_nDuration -= delta_ticks;
		}
	}
}

////////////////////////////////////////////////////////////////
void CStaff::prolongRegionNotes(int ms)
{
	float fMs = static_cast<float>(ms) / 1000.0f;

	for (auto m : m_apMeasures)
	{
		for (auto n : m->m_apNotationObjects)
		{
			if (n->m_Type != NOTATION_OBJECT::EType::Note)
			{
				continue;
			}

			bool last_because_last_in_segment = n->m_bLastInSegment;

			bool last_because_different_slur = false;
			auto *next_note = getNextNoteAfter(n);
			if (next_note)
			{
				if (next_note->m_nSlurObjID != n->m_nSlurObjID)
				{
					last_because_different_slur = true;
				}
			}
			else
			{
				continue;
			}

			// Don't do this one, because it's last in the segment
			// or last in the slur
			if (last_because_last_in_segment || last_because_different_slur)
			{
				continue;
			}

			// Don't do this one because the next tone is identical
			if (next_note->m_nSoundingMidiNr == n->m_nSoundingMidiNr)
			{
				continue;
			}

			// Global start
			double start_original;
			if (!m_pMusicModel->m_pSystemStaff->globalTicksToGlobalSeconds(n->localTicksToGlobalTicks(), start_original))
			{
				continue;
			}

			// Global end tick
			auto global_ticks_end = n->localTicksToGlobalTicks() + n->m_nDuration;
			double end_original;
			if (!m_pMusicModel->m_pSystemStaff->globalTicksToGlobalSeconds(global_ticks_end, end_original))
			{
				continue;
			}

			// New duration
			double end_new = end_original + fMs;

			int measure, tick, end_new_ticks;
			if (!m_pMusicModel->m_pSystemStaff->globalSecondsToLocalTicks(end_new, measure, tick))
			{
				continue;
			}

			m_pMusicModel->m_pSystemStaff->localTicksToGlobalTicks(measure, tick, end_new_ticks);

			int delta_ticks = global_ticks_end - end_new_ticks;
			n->m_nDuration -= delta_ticks;
		}
	}
}

////////////////////////////////////////////////////////////////
void CStaff::arpeggio(int each_ms, int pitch_ms)
{
	for (auto m : m_apMeasures)
	{
		std::vector<NOTATION_OBJECT*> notes;
		for (auto note_in_measure : m->m_apNotationObjects)
		{
			if (note_in_measure->m_Type != NOTATION_OBJECT::EType::Note)
			{
				continue;
			}

			notes.push_back(note_in_measure);
			auto *next = getNextNoteAfter(note_in_measure);
			// Process the notes at the same position
			if ((next == nullptr) || (next->m_Position != note_in_measure->m_Position))
			{
				// Inverse the list
				std::vector<NOTATION_OBJECT*> actual_notes;
				if (notes[0]->m_nArpeggio == 0)
				{
					notes.clear();
					continue;
				}
				else if (notes[0]->m_nArpeggio == -1)
				{
					NOTATION_OBJECT::sortNotesAscendingPitch(notes);
					for (auto it = notes.rbegin(); it != notes.rend(); ++it)
					{
						actual_notes.push_back(*it);
					}
				}
				else 
				{
					NOTATION_OBJECT::sortNotesAscendingPitch(notes);
					actual_notes = notes;
				}

				// Get the shortest duration of the cohort
				int min_dur = 999999;
				for (auto *n : actual_notes)
				{
					if (n->m_nDuration < min_dur) min_dur = n->m_nDuration;
				}

				int order = 0;
				for (auto *n : actual_notes)
				{
					// Calculate desired start offset
					double delay_sec, each_sec, delay;
					int interval_from_first = n->m_nSoundingMidiNr - actual_notes[0]->m_nSoundingMidiNr;
					if (actual_notes[0]->m_nArpeggio == -1) interval_from_first = -interval_from_first;
					delay_sec = static_cast<double>(interval_from_first * pitch_ms) / 1000.0;
					each_sec = static_cast<double>(order * each_ms) / 1000.0;
					delay = delay_sec + each_sec;

					bool b = true;

					// Get global ticks
					int global_start, global_end;
					b = b && localTicksToGlobalTicks(n->m_Position.m_Measure, n->m_Position.m_TicksInMeasure, global_start);
					global_end = global_start + n->m_nDuration;

					// Get global seconds
					double start_sec, end_sec;
					b = b && m_pMusicModel->m_pSystemStaff->globalTicksToGlobalSeconds(global_start, start_sec);
					b = b && m_pMusicModel->m_pSystemStaff->globalTicksToGlobalSeconds(global_end, end_sec);

					start_sec += delay;
					if (end_sec < start_sec) 
					{
						start_sec = end_sec;
					}

					// Get local ticks
					TIMELINE_POS new_start;
					int new_global_ticks;
					b = b && m_pMusicModel->m_pSystemStaff->globalSecondsToLocalTicks(start_sec, new_start.m_Measure, new_start.m_TicksInMeasure);
					b = b && localTicksToGlobalTicks(new_start.m_Measure, new_start.m_TicksInMeasure, new_global_ticks);

					if (!b)
					{
						printf("ERROR: Unsuccessful lookup of timing info while processing arpeggios in bar %d\n", m->m_nNumber);
						continue;
					}

					int new_duration = global_end - new_global_ticks;

					if (n->m_Position.m_Measure != new_start.m_Measure)
					{
						printf("      INFO: Shifting note %d from measure %d tick %d to measure %d tick %d\n", n->m_nSoundingMidiNr, n->m_Position.m_Measure, n->m_Position.m_TicksInMeasure, new_start.m_Measure, new_start.m_TicksInMeasure);
					}

					n->m_Position.m_Measure = new_start.m_Measure;
					n->m_Position.m_TicksInMeasure = new_start.m_TicksInMeasure;
					n->m_nDuration = new_duration;

					order++;
				}

				notes.clear();
			}
		}
	}

	sortNotationObjects();
}

////////////////////////////////////////////////////////////////
void CStaff::injectCcForRegions(int cc, int val, bool start)
{
	std::map<NOTATION_OBJECT*, CMeasure*> to_insert;

	for (auto m : m_apMeasures)
	{
		double prev_dyn = -1;

		for (auto n : m->m_apNotationObjects)
		{
			if ((n->m_bFirstInSegment && start) || (n->m_bLastInSegment && (!start)))
			{
				int point = n->localTicksToGlobalTicks();
				if (!start)
				{
					point += n->m_nDuration;
				}

				int mi, ti;
				globalTicksToLocalTicks(point, mi, ti);
				auto measure = m_apMeasures[mi - g_Cfg.m_FirstBarNumber];

				NOTATION_OBJECT *nNew = new NOTATION_OBJECT(measure, converdi::NOTATION_OBJECT::CC, converdi::NOTATION_OBJECT::ESubtype::Subtype_undef, { mi, ti }, 15);
				nNew->m_cc = cc;
				nNew->m_ccVal = val;
				to_insert[nNew] = measure;
			}
		}
	}

	if (to_insert.size() > 0)
	{
		for (auto n : to_insert)
		{
			auto obj = n.first;
			auto measure = n.second;
			measure->m_apNotationObjects.push_back(obj);
		}
		sortNotationObjects();
	}
}

////////////////////////////////////////////////////////////////
void CStaff::injectKeyLatchForRegions(int key, int anticipate_ms, int prolong_ms)
{
	// STEP 1: GATHER THE LATCH POSITIONS

	using TLatchPosition = std::pair<int, int>;
	std::vector<TLatchPosition> latch_positions;

	int open_at = -1;
	for (auto m : m_apMeasures)
	{
		double prev_dyn = -1;
		for (auto n : m->m_apNotationObjects)
		{
			if (n->m_bFirstInSegment)
			{
				localTicksToGlobalTicks(n->m_Position.m_Measure, n->m_Position.m_TicksInMeasure, open_at);
			}
			if (n->m_bLastInSegment)
			{
				int close_at;
				if (localTicksToGlobalTicks(n->m_Position.m_Measure, n->m_Position.m_TicksInMeasure, close_at))
				{
					close_at += n->m_nDuration;
					int span = close_at - open_at;

					latch_positions.push_back({ open_at, span });
				}
			}
		}
	}

	// STEP 1.5: JOIN SUBSEQUENT OBJECTS

	auto prev_it = latch_positions.end();
	for (auto it = latch_positions.begin(); it != latch_positions.end(); )
	{
		int this_start = it->first;
		int this_end = it->first + it->second;

		int last_start = -1;
		int last_end = -1;
		if (prev_it != latch_positions.end())
		{
			last_start = prev_it->first;
			last_end = prev_it->first + prev_it->second;
		}

		if (last_end == this_start)
		{
			prev_it->second = this_end - last_start;
			it = latch_positions.erase(it);
		}
		else
		{
			prev_it = it;
			it++;
		}
	}

	// STEP 2: CREATE NOTATION OBJECTS

	std::map<NOTATION_OBJECT*, CMeasure*> to_insert;

	for (auto &p : latch_positions)
	{
		auto open_at = p.first;
		auto span = p.second;
		TIMELINE_POS open_pos;
		TIMELINE_POS to_pos;

		// OPEN AT
		if (!globalTicksToLocalTicks(open_at, open_pos))
		{
			continue;
		}

		double at_sec;
		if (!m_pMusicModel->m_pSystemStaff->globalTicksToGlobalSeconds(open_at, at_sec))
		{
			continue;
		}

		// ENDS AT
		double to_sec;
		if (!m_pMusicModel->m_pSystemStaff->globalTicksToGlobalSeconds(open_at + span, to_sec))
		{
			continue;
		}

		at_sec -= static_cast<double>(anticipate_ms) / 1000.0;
		if (at_sec < 0.0) at_sec = 0.0;
		if (!m_pMusicModel->m_pSystemStaff->globalSecondsToLocalTicks(at_sec, open_pos.m_Measure, open_pos.m_TicksInMeasure))
		{
			continue;
		}

		to_sec += static_cast<double>(prolong_ms) / 1000.0f;
		if (!m_pMusicModel->m_pSystemStaff->globalSecondsToLocalTicks(to_sec, to_pos.m_Measure, to_pos.m_TicksInMeasure))
		{
			continue;
		}
		int to_abs;
		if (!localTicksToGlobalTicks(to_pos.m_Measure, to_pos.m_TicksInMeasure, to_abs))
		{
			continue;
		}
		int open_abs;
		if (!localTicksToGlobalTicks(open_pos.m_Measure, open_pos.m_TicksInMeasure, open_abs))
		{
			continue;
		}

		auto measure = m_apMeasures[open_pos.m_Measure - g_Cfg.m_FirstBarNumber];

		NOTATION_OBJECT *nNew = new NOTATION_OBJECT(measure, converdi::NOTATION_OBJECT::Note, converdi::NOTATION_OBJECT::ESubtype::Subtype_undef, open_pos, 15);
		nNew->m_nSoundingMidiNr = key;
		nNew->m_nDuration = to_abs - open_abs;
		nNew->m_Position = open_pos;
		to_insert[nNew] = measure;
	}

	// STEP 3: INSERT NEW OBJECTS

	if (to_insert.size() > 0)
	{
		for (auto n : to_insert)
		{
			auto obj = n.first;
			auto measure = n.second;
			measure->insertObject(obj);
		}
		sortNotationObjects();
	}
}

////////////////////////////////////////////////////////////////
void CStaff::trailStaffByCC(int cc, int val)
{
	auto measure = m_apMeasures[0];
	int mi, ti;
	globalTicksToLocalTicks(0, mi, ti);

	NOTATION_OBJECT *pNew = new NOTATION_OBJECT(measure, converdi::NOTATION_OBJECT::CC, converdi::NOTATION_OBJECT::ESubtype::Subtype_undef, { mi, ti }, 15);
	pNew->m_cc = cc;
	pNew->m_ccVal = val;
	measure->m_apNotationObjects.push_back(pNew);

	sortNotationObjects();
}

////////////////////////////////////////////////////////////////
void CStaff::adjustVelocitiesForDynamics(CDynamicsTable &table)
{
	for (auto m : m_apMeasures)
	{
		for (auto n : m->m_apNotationObjects)
		{
			if (n->m_Type == converdi::NOTATION_OBJECT::EType::Note)
			{
				auto gt = n->localTicksToGlobalTicks();
				double this_dyn = getDynValueAtGlobalTicks(gt, table);
				if (this_dyn < 1) this_dyn = 1;
				n->m_nForcedVelocity = (int)this_dyn;
			}
		}
	}
}

////////////////////////////////////////////////////////////////
void CStaff::injectCcForDynamics(int cc, CDynamicsTable &table)
{
	double prev_dyn = -1;
	for (auto m : m_apMeasures)
	{
		for (int p = 0; p < m->m_nDuration; p += g_Cfg.m_TicksPerQuarterNote / 16)
		{
			int gl;
			localTicksToGlobalTicks(m->m_nNumber, p, gl);

			if (!insideSegment(gl)) 
			{
				continue;
			}

			double this_dyn = getDynValueAtGlobalTicks(gl, table);
			if (this_dyn != prev_dyn)
			{
				NOTATION_OBJECT *n = new NOTATION_OBJECT(m, converdi::NOTATION_OBJECT::CC, converdi::NOTATION_OBJECT::ESubtype::Subtype_undef, { m->m_nNumber, p }, 15);
				m->m_apNotationObjects.push_back(n);
				n->m_cc = cc;
				n->m_ccVal = (int)this_dyn;
				prev_dyn = this_dyn;
			}
		}
	}

	sortNotationObjects();
}

////////////////////////////////////////////////////////////////
double CStaff::getDynValueAtGlobalTicksAtStaff(int abs, CDynamicsTable &table, int &effective_since)
{
	CStaff *source = m_pUseDynamics != nullptr ? m_pUseDynamics : this;

	for (auto m : source->m_apMeasures)
	{
		for (auto n : m->m_apNotationObjects)
		{
			if (n->m_Type == NOTATION_OBJECT::EType::DynamicsLine)
			{
				if (abs >= n->localTicksToGlobalTicks() && abs < n->localTicksToGlobalTicks() + n->m_nDuration)
				{
					double start = table.getNumericValue(n->m_sDynValue);
					double end = table.getNumericValue(n->m_sDynValueEnd);

					int elapsed = abs - n->localTicksToGlobalTicks();

					double ratio = (double)elapsed / (double)n->m_nDuration;
					double value = ratio * (end - start) + start;

					effective_since = abs;

					return value;
				}
			}
		}
	}

	for (auto im = source->m_apMeasures.rbegin(); im != source->m_apMeasures.rend(); im++)
	{
		for (auto in = (*im)->m_apNotationObjects.rbegin(); in != (*im)->m_apNotationObjects.rend(); in++)
		{
			auto n = (*in);
			if (n->m_Type == NOTATION_OBJECT::EType::DynamicsValue)
			{
				if (abs >= n->localTicksToGlobalTicks()) 
				{
					effective_since = n->localTicksToGlobalTicks();
					return table.getNumericValue(n->m_sDynValue);
				}
			}
		}
	}

	effective_since = 0;
	return 0;
}

////////////////////////////////////////////////////////////////
double CStaff::getDynValueAtGlobalTicks(int abs, CDynamicsTable &table)
{
	int effective_since;
	double this_dyn = getDynValueAtGlobalTicksAtStaff(abs, table, effective_since);

	if (m_pGrandStaffDynamics != nullptr)
	{
		int other_effective_since;
		double other_dyn = m_pGrandStaffDynamics->getDynValueAtGlobalTicksAtStaff(abs, table, other_effective_since);
		if (other_effective_since > effective_since)
		{
			this_dyn = other_dyn;
		}
	}

	return this_dyn;
}

////////////////////////////////////////////////////////////////
void CStaff::calcTempoMarkAbsTimes()
{
	auto prev_tick = -1;
	double prev_bpm = -1;
	double time = 0.0;

	for (auto m : m_apMeasures)
	{
		for (auto n : m->m_apNotationObjects)
		{
			if (n->m_Type == NOTATION_OBJECT::Tempo)
			{
				auto this_tick = n->localTicksToGlobalTicks();
				double this_bpm = n->m_fTempoBpm;

				if (prev_tick >= 0)
				{
					auto count_ticks = this_tick - prev_tick;
					double count_quarter_notes = (double)count_ticks / (double)g_Cfg.m_TicksPerQuarterNote;
					double dur_quarter_note = 60.0 / prev_bpm;
					double dur_between_marks = count_quarter_notes * dur_quarter_note;
					time += dur_between_marks;
					n->m_fTimeAbs = time;
				}

				prev_bpm = this_bpm;
				prev_tick = this_tick;
			}
		}
	}
}

////////////////////////////////////////////////////////////////
void CStaff::copyFirstTempoMark()
{
	if (m_apMeasures.size() == 0) return;

	auto *first_measure = m_apMeasures[0];

	for (auto m : m_apMeasures)
	{
		for (auto n : m->m_apNotationObjects)
		{
			if (n->m_Type == NOTATION_OBJECT::Tempo)
			{
				if (n->localTicksToGlobalTicks() != 0)
				{
					NOTATION_OBJECT *nn = new NOTATION_OBJECT(first_measure, NOTATION_OBJECT::EType::Tempo, NOTATION_OBJECT::ESubtype::Subtype_undef, { 1, 0 }, 15);
					first_measure->insertObject(nn);
					sortNotationObjects();
					return;
				}
			}
		}
	}
}

////////////////////////////////////////////////////////////////
bool CStaff::isNotePlaying(int abs, int note_no, NOTATION_OBJECT *except)
{
	for (auto m : m_apMeasures)
	{
		for (auto n : m->m_apNotationObjects)
		{
			if (n->m_Type == NOTATION_OBJECT::Note)
			{
				if (n->m_nSoundingMidiNr != note_no)
				{
					continue;
				}

				if (n == except)
				{
					continue;
				}

				auto gtb = n->localTicksToGlobalTicks();
				auto gte = gtb + n->m_nDuration;

				if ((abs >= gtb) && (abs < gte))
				{
					return true;
				}
			}
		}
	}
	return false;
}

////////////////////////////////////////////////////////////////
bool CStaff::globalTicksToLocalTicks(int abs, int &measure, int &tick)
{
	for (auto it = m_apMeasures.rbegin(); it != m_apMeasures.rend(); it++)
	{
		auto m = *it;
		if (abs >= m->m_nPositionAbs)
		{
			measure = m->m_nNumber;
			tick = abs - m->m_nPositionAbs;
			return true;
		}
	}
	return false;
}

////////////////////////////////////////////////////////////////
bool CStaff::globalTicksToLocalTicks(int abs, TIMELINE_POS &pos)
{
	return globalTicksToLocalTicks(abs, pos.m_Measure, pos.m_TicksInMeasure);
}

////////////////////////////////////////////////////////////////
bool CStaff::localTicksToGlobalTicks(TIMELINE_POS &pos, int &global_ticks)
{
	int measure_index = pos.m_Measure - g_Cfg.m_FirstBarNumber;
	if (measure_index >= m_apMeasures.size()) return false;
	if (measure_index < 0) return false;
	global_ticks = m_apMeasures[measure_index]->m_nPositionAbs + pos.m_TicksInMeasure;
	return true;
}

////////////////////////////////////////////////////////////////
bool CStaff::localTicksToGlobalTicks(int measure, int tick, int &global_ticks)
{
	int measure_index = measure - g_Cfg.m_FirstBarNumber;
	if (measure_index >= m_apMeasures.size()) return false;
	if (measure_index < 0) return false;
	global_ticks = m_apMeasures[measure_index]->m_nPositionAbs + tick;
	return true;
}

////////////////////////////////////////////////////////////////
bool CStaff::globalTicksToGlobalSeconds(int gt, double &global_seconds)
{
	for (auto im = m_apMeasures.rbegin(); im != m_apMeasures.rend(); im++)
	{
		for (auto in = (*im)->m_apNotationObjects.rbegin(); in != (*im)->m_apNotationObjects.rend(); in++)
		{
			auto n = (*in);
			if (n->m_Type == NOTATION_OBJECT::Tempo)
			{
				if (gt >= n->localTicksToGlobalTicks())
				{
					double gs_mark = n->m_fTimeAbs;
					int gt_mark = n->localTicksToGlobalTicks();

					int delta_ticks = gt - gt_mark;
					double beat_dur = 60.0 / n->m_fTempoBpm;
					double tick_dur = beat_dur / (double)g_Cfg.m_TicksPerQuarterNote;
					double delta_seconds = delta_ticks * tick_dur;

					global_seconds = gs_mark + delta_seconds;
					return true;
				}
			}
		}
	}
	return false;
}

////////////////////////////////////////////////////////////////
bool CStaff::globalSecondsToLocalTicks(double abs, int &measure, int &tick)
{
	for (auto im = m_apMeasures.rbegin(); im != m_apMeasures.rend(); im++)
	{
		for (auto in = (*im)->m_apNotationObjects.rbegin(); in != (*im)->m_apNotationObjects.rend(); in++)
		{
			auto n = (*in);
			if (n->m_Type == NOTATION_OBJECT::Tempo)
			{
				if (abs >= n->m_fTimeAbs)
				{
					// This is the latest tempo update before the time in question
					double abs_diff = abs - n->m_fTimeAbs;
					double beat_dur = 60.0 / n->m_fTempoBpm;
					double tick_dur = beat_dur / (double)g_Cfg.m_TicksPerQuarterNote;
					double count_ticks = abs_diff / tick_dur;

					int mark_pos_abs = n->localTicksToGlobalTicks();
					int this_pos_abs = mark_pos_abs + (int)floor(count_ticks);
					return globalTicksToLocalTicks(this_pos_abs, measure, tick);
				}
			}
		}
	}
	return false;
}

////////////////////////////////////////////////////////////////
void CStaff::createTempoMarksByRetards()
{
	std::vector<NOTATION_OBJECT *> to_insert;

	double current_bpm;
	for (auto m : m_apMeasures)
	{
		for (auto n : m->m_apNotationObjects)
		{
			if (n->m_Type == NOTATION_OBJECT::EType::Tempo)
			{
				current_bpm = n->m_fTempoBpm;
			}
			else if (n->m_Type == NOTATION_OBJECT::EType::RitardLine)
			{
				// Deduce the target BPM if not specified
				if (n->m_fTempoBpm == 0.0)
				{
					// TODO: Lookup the session configuration for the acutal ratios
					double ratio;
					switch (n->m_Subtype)
					{
					case NOTATION_OBJECT::ESubtype::Ritard_molto_ritardando:  ratio = 1.5; break;
					case NOTATION_OBJECT::ESubtype::Ritard_ritardando:			  ratio = 1.25; break;
					case NOTATION_OBJECT::ESubtype::Ritard_poco_ritardando:	  ratio = 1.1; break;
					case NOTATION_OBJECT::ESubtype::Ritard_poco_accelerando:  ratio = 0.9; break;
					case NOTATION_OBJECT::ESubtype::Ritard_accelerando:			  ratio = 0.75; break;
					case NOTATION_OBJECT::ESubtype::Ritard_molto_accelerando: ratio = 0.5; break;
					default: ratio = 1.0;
					}
					n->m_fTempoBpm = current_bpm / ratio;
				}

				int start = n->localTicksToGlobalTicks();
				int end = n->localTicksToGlobalTicks() + n->m_nDuration - 1;

				std::vector<std::pair<int, double> > tempoMarksToAdd;
				
				int ticks_per_16th = g_Cfg.m_TicksPerQuarterNote / 4;
				for (int p = start; p < end; p += ticks_per_16th)
				{
					double start_value = current_bpm;
					double end_value = n->m_fTempoBpm;
					double time_pos_ratio = (double)(p - start) / (double)(end - start);
					double value_now = start_value + (end_value - start_value) * time_pos_ratio;
					tempoMarksToAdd.push_back({ p, value_now });
				}

				tempoMarksToAdd.push_back({ end - 1, n->m_fTempoBpm });

				for (auto add : tempoMarksToAdd)
				{
					int measure, tick;
					if (globalTicksToLocalTicks(add.first, measure, tick))
					{
						auto *pMeasure = m_apMeasures[measure - g_Cfg.m_FirstBarNumber];

						NOTATION_OBJECT *nn = new NOTATION_OBJECT(pMeasure, NOTATION_OBJECT::EType::Tempo, NOTATION_OBJECT::ESubtype::Subtype_undef, { measure, tick }, 15);
						nn->m_fTempoBpm = add.second;
						to_insert.push_back(nn);
					}
				}

				current_bpm = n->m_fTempoBpm;
				n->m_bDeleted = true;
			}
		}
	}

	for (auto n : to_insert)
	{
		n->m_pMeasure->insertObject(n);
	}
	purgeDeletedObjects();
	sortNotationObjects();
}

////////////////////////////////////////////////////////////////
void CStaff::fetchRitardParams()
{
	int ritard_from = -1;
	int ritard_to = -1;
	NOTATION_OBJECT *ritard = nullptr;

	for (auto m : m_apMeasures)
	{
		for (auto n : m->m_apNotationObjects)
		{
			if (n->m_Type == NOTATION_OBJECT::RitardLine)
			{
				ritard_from = n->localTicksToGlobalTicks();
				ritard_to = ritard_from + n->m_nDuration;
				ritard = n;
				ritard->m_fTempoBpm = 0.0;
			}
			else if (n->m_Type == NOTATION_OBJECT::Text)
			{
				// TODO: Make sure slurs are ALWAYS listed before the notes they span
				if (n->localTicksToGlobalTicks() >= ritard_from && n->localTicksToGlobalTicks() <= ritard_to)
				{
					ritard->m_sText = n->m_sText;
					auto bpm = CSibeliusDataParser::guessTempoFromString(ritard->m_sText);
					ritard->m_fTempoBpm = bpm;
					n->m_bDeleted = true;
				}
			}
		}
	}

	purgeDeletedObjects();
}

////////////////////////////////////////////////////////////////
void CStaff::assignNotesToSlursAndTrills()
{
	// TODO at Sibelius side:
	// Make sure slurs and trills are ALWAYS listed before the notes they span

	int slur_from = -1;
	int slur_to = -1;
	NOTATION_OBJECT *slur = nullptr;

	int trill_from = -1;
	int trill_to = -1;
	NOTATION_OBJECT *trill = nullptr;

	for (auto m : m_apMeasures)
	{
		for (auto n : m->m_apNotationObjects)
		{
			if (n->m_Type == NOTATION_OBJECT::Slur)
			{
				slur_from = n->localTicksToGlobalTicks();
				slur_to = slur_from + n->m_nDuration;
				slur = n;
			}

			else if (n->m_Type == NOTATION_OBJECT::Trill)
			{
				trill_from = n->localTicksToGlobalTicks();
				trill_to = trill_from + n->m_nDuration;
				trill = n;
			}

			else if (n->m_Type == NOTATION_OBJECT::Note)
			{	
				if (n->localTicksToGlobalTicks() >= slur_from && n->localTicksToGlobalTicks() <= slur_to)
				{
					n->m_nSlurObjID = slur->m_ID;
					// printf("SLUR %d for %d %d %d\n", slur->m_ID, n->m_nSoundingMidiNr, n->m_Position.m_Measure, n->m_Position.m_TicksInMeasure);
				}
				else if (n->localTicksToGlobalTicks() >= trill_from && n->localTicksToGlobalTicks() <= trill_to)
				{
					n->m_nTrillObjID = trill->m_ID;
				}
				else
				{
					// printf("OUTSIDE SLUR for %d %d %d\n", n->m_nSoundingMidiNr, n->m_Position.m_Measure, n->m_Position.m_TicksInMeasure);
				}
			}
		}
	}
}

////////////////////////////////////////////////////////////////
void CStaff::applyNoteTies()
{
	for (auto m : m_apMeasures)
	{
		for (auto n : m->m_apNotationObjects)
		{
			if (n->m_Type != NOTATION_OBJECT::EType::Note) continue;
			if (!n->m_bTied) continue;

			// TODO: Check note -- pause -- note
			auto tied_to = m->findSubseqNoteSameAs(n, true);
			if (tied_to)
			{
				if (tied_to->m_nTremolo != n->m_nTremolo)
				{
					if (n->m_nTremolo)
					{
						n->m_bTiedTremolo = true;
					}
					continue;
				}

				if (tied_to->m_nTrillObjID != n->m_nTrillObjID)
				{
					if (n->m_nTrillObjID)
					{
						n->m_bTiedTremolo = true;
					}
					continue;
				}

				tied_to->m_pTiedFrom = n;
			}
		}
	}

	for (auto mit = m_apMeasures.rbegin(); mit != m_apMeasures.rend(); mit++)
	{
		for (auto nit = (*mit)->m_apNotationObjects.rbegin(); nit != (*mit)->m_apNotationObjects.rend(); nit++)
		{
			auto *object = (*nit);
			if (object->m_pTiedFrom != nullptr)
			{
				// Extend the chronologically sooner
				object->m_pTiedFrom->m_nDuration += object->m_nDuration;
				// Mark the latter as deleted
				object->m_bDeleted = true;
			}
		}
	}

	auto count = purgeDeletedObjects();
	if (count > 0)
	{
		printf("  %d objects deleted while applying ties\n", count);
	}
}

////////////////////////////////////////////////////////////////
void CStaff::calcMeasureAbsPositions()
{
	int abs = 0;
	for (auto m : m_apMeasures)
	{
		m->m_nPositionAbs = abs;
		abs += m->m_nDuration;
	}
}

////////////////////////////////////////////////////////////////
void CStaff::buildMeasureLinkedList()
{
	int abs = 0;
	CMeasure *pMeasure = nullptr;
	for (auto m : m_apMeasures)
	{
		if (pMeasure) {
			pMeasure->m_pNextMeasure = m;
		}
		pMeasure = m;
	}
}

////////////////////////////////////////////////////////////////
void CStaff::generateMidiNotesAndCcs(smf::MidiFile &output, int tr)
{
	// WRITE OWN NOTES AND CCS

	auto stripped_name = CSibeliusDataParser::stripMidiTrackName(m_sName);
	output.addTrackName(tr, 0, stripped_name);

	for (auto m : m_apMeasures)
	{
		for (auto n : m->m_apNotationObjects)
		{
			if (n->m_Type == NOTATION_OBJECT::EType::CC)
			{
				int start_tick = n->localTicksToGlobalTicks();
				output.addController(tr, start_tick, 0, n->m_cc, n->m_ccVal);
			}

			if (n->m_Type == NOTATION_OBJECT::EType::Note)
			{
				int start_tick = n->localTicksToGlobalTicks();
				int end_tick   = start_tick + n->m_nDuration;
				int velocity   = (n->m_nForcedVelocity >= 0) ? n->m_nForcedVelocity : 42;

				output.addNoteOn (tr, start_tick, 0, n->m_nSoundingMidiNr, velocity);
				output.addNoteOff(tr, end_tick,   0, n->m_nSoundingMidiNr, velocity);
			}
		}
	}

	// WRITE SUBSTAVES

	for (auto ss : m_apStaves)
	{
		ss->generateMidiNotesAndCcs(output, tr + 1);
	}
}

////////////////////////////////////////////////////////////////
void CStaff::generateMidiSignatures(smf::MidiFile &output)
{
	// Track 0 is the tempo/signature map track in a Type-1 MIDI file.

	for (auto m : m_apMeasures)
	{
		for (auto n : m->m_apNotationObjects)
		{
			if (n->m_Type == NOTATION_OBJECT::EType::Tempo)
			{
				int tick = n->localTicksToGlobalTicks();
				output.addTempo(0, tick, n->m_fTempoBpm);
			}

			if (n->m_Type == NOTATION_OBJECT::EType::TimeSignature)
			{
				int tick = n->localTicksToGlobalTicks();
				// midifile addTimeSignature takes (track, tick, numerator, denominator_power_of_2)
				// m_nSigDenom is already stored as a power of 2 (e.g. 4 for quarter-note beat)
				int denom_pow2 = 0;
				int d = n->m_nSigDenom;
				while (d > 1) { d >>= 1; denom_pow2++; }
				output.addTimeSignature(0, tick, n->m_nSigNum, denom_pow2);
			}
		}
	}
}

// Compares two intervals according to starting times.
bool compareEvents(NOTATION_OBJECT* i1, NOTATION_OBJECT* i2)
{
	return (i1->m_Position.m_TicksInMeasure < i2->m_Position.m_TicksInMeasure);
}

////////////////////////////////////////////////////////////////
void CStaff::sortNotationObjects()
{
	for (auto tm : m_apMeasures)
	{
		std::sort(tm->m_apNotationObjects.begin(), tm->m_apNotationObjects.end(), compareEvents);
	}
}

////////////////////////////////////////////////////////////////
void CStaff::reportSelfForDebug(std::string indent)
{
	printf("%s(#%03d) %-10s%-30s%3zu measures %4d notes %4d CCs"
		, indent.c_str()
		, m_nRuntimeNumber
		, (m_sLocalLabel + m_sLocalIndex).c_str()
		, m_sName.c_str()
		, m_apMeasures.size()
		, getObjectCount(NOTATION_OBJECT::EType::Note)
		, getObjectCount(NOTATION_OBJECT::EType::CC));
	printf("\n");
}

////////////////////////////////////////////////////////////////
void CStaff::debugDump(std::string indent)
{
	printf("%sSTAFF '%s' (%d notes)\n", indent.c_str(), m_sName.c_str(), getObjectCount(NOTATION_OBJECT::EType::Note));
	for (auto m : m_apMeasures) m->debugDump(indent + "  ");
	for (auto s : m_apStaves) s->debugDump(indent + "} ");
}

////////////////////////////////////////////////////////////////
void CStaff::moveObjectToStaff(NOTATION_OBJECT *obj, CStaff *pDestStaff)
{
	int mi = obj->m_Position.m_Measure - g_Cfg.m_FirstBarNumber;
	auto measure_here = m_apMeasures[mi];
	auto measure_dest = pDestStaff->m_apMeasures[mi];

	auto f = std::find(measure_here->m_apNotationObjects.begin(), measure_here->m_apNotationObjects.end(), obj);
	if (f == measure_here->m_apNotationObjects.end())
	{
		printf("ERROR: Moving object that was not present in the measure\n");
		return;
	}

	NOTATION_OBJECT *n = new NOTATION_OBJECT(nullptr);
	*n = **f;
	n->m_pMeasure = measure_dest;
	measure_dest->insertObject(n);

	measure_here->m_apNotationObjects.erase(f);
}

////////////////////////////////////////////////////////////////
void CStaff::flattenVoices()
{
	for (auto m : m_apMeasures)
	{
		for (auto n : m->m_apNotationObjects)
		{
			n->m_Voices[0] = true;
			n->m_Voices[1] = false;
			n->m_Voices[2] = false;
			n->m_Voices[3] = false;
		}
	}
}

////////////////////////////////////////////////////////////////
void CStaff::splitByVoices()
{
	bool need_v[g_Cfg.m_nMaxVoices];
	for (int v = 0; v < g_Cfg.m_nMaxVoices; v++)
	{
		need_v[v] = (v == 0) || (getNotesPresenceInVoice(v));
		// need_v[v] = true;
	}

	CScope::TMovedTypesList moved_types;
	moved_types.insert(NOTATION_OBJECT::EType::Note);
	moved_types.insert(NOTATION_OBJECT::EType::Slur);
	moved_types.insert(NOTATION_OBJECT::EType::Trill);
	moved_types.insert(NOTATION_OBJECT::EType::Technique);
	moved_types.insert(NOTATION_OBJECT::EType::PedalLine);

	for (int v = 0; v < g_Cfg.m_nMaxVoices; v++)
	{
		if (!need_v[v]) continue;

		std::string local_name = "V";
		std::string local_index = std::to_string(v);

		CStaff *pNewStaff = new CStaff(m_pMusicModel, this, local_name, local_index);
		pNewStaff->m_pUseDynamics = m_pUseDynamics;
		pNewStaff->m_sTrackName = m_sTrackName;
		pNewStaff->m_sName = m_sName;
		pNewStaff->copyMeasureStructure(this);
		pNewStaff->m_pGrandStaffDynamics = m_pGrandStaffDynamics;
		m_apStaves.push_back(pNewStaff);

		std::vector<NOTATION_OBJECT*> to_move;
		for (auto m : m_apMeasures)
		{
			for (auto n : m->m_apNotationObjects)
			{
				if (moved_types.find(n->m_Type) == moved_types.end()) continue;

				if (n->m_Type == NOTATION_OBJECT::EType::Note)
				{
					// Notes carry an explicit voice bitmap — only move to the matching voice.
					if (!n->m_Voices[v]) continue;
				}
				else
				{
					// Non-note types (Slur, Trill, Technique, PedalLine) have no voice
					// bitmap; move them only into voice 0 so they aren't duplicated.
					if (v != 0) continue;
				}

				to_move.push_back(n);
			}
		}

		for (auto n : to_move)
		{
			moveObjectToStaff(n, pNewStaff);
		}
		pNewStaff->sortNotationObjects();
	}
}

////////////////////////////////////////////////////////////////
void CStaff::splitByScopes(std::vector<CScope*> scopes)
{
	for (auto scope : scopes)
	{
		std::string local_label = "S";
		std::string local_index = scope->m_sName;

		CStaff *pNewStaff = new CStaff(m_pMusicModel, this, local_label, local_index);
		pNewStaff->m_pUseDynamics = m_pUseDynamics;
		pNewStaff->m_sName = m_sName;
		pNewStaff->m_sTrackName = scope->m_sDestinationTrack;
		pNewStaff->m_pRelatedScope = scope;
		pNewStaff->copyMeasureStructure(this);
		pNewStaff->m_pGrandStaffDynamics = m_pGrandStaffDynamics;
		m_apStaves.push_back(pNewStaff);

		std::vector<NOTATION_OBJECT*> to_move;
		for (auto m : m_apMeasures)
		{
			for (auto n : m->m_apNotationObjects)
			{
				if (!scope->passes(n)) continue;
				to_move.push_back(n);
			}
		}

		for (auto n : to_move)
		{
			moveObjectToStaff(n, pNewStaff);
		}
		pNewStaff->sortNotationObjects();
		pNewStaff->assignNotesToSlursAndTrills();
		pNewStaff->sortNotationObjects();
	}
}

////////////////////////////////////////////////////////////////
CStaff *CStaff::mergeStaves(CMusicModel *pModel, CStaff *parent, std::vector<CStaff*> staves, std::string name, std::string local_label, std::string local_index)
{
	if (staves.size() == 0) {
		return nullptr;
	}
	CStaff *pNewStaff = new CStaff(pModel, parent, local_label, local_index);
	pNewStaff->m_sName = name;
	pNewStaff->copyMeasureStructure(staves[0]);

	for (auto s : staves)
	{
		for (auto m : s->m_apMeasures)
		{
			for (auto n : m->m_apNotationObjects)
			{
				if (n->m_Type == NOTATION_OBJECT::EType::Note || n->m_Type == NOTATION_OBJECT::EType::CC)
				{
					CMeasure *measure = pNewStaff->m_apMeasures[n->m_Position.m_Measure - g_Cfg.m_FirstBarNumber];
					NOTATION_OBJECT *new_n = new NOTATION_OBJECT(measure, n->m_Type, n->m_Subtype, n->m_Position, 15);
					new_n->operator=(*n);
					new_n->m_pMeasure = measure;
					measure->m_apNotationObjects.push_back(new_n);
				}
			}
		}
	}

	std::map<std::string, std::vector<CStaff*>> merge_list;
	for (auto s : staves)
	{
		for (auto t : s->m_apStaves)
		{
			std::string local_name = t->m_sLocalLabel + t->m_sLocalIndex;
			merge_list[local_name].push_back(t);
		}
	}

	for (auto m : merge_list)
	{
		std::string new_name = name;
		if (m.second[0]->m_sTrackName.length() > 0)
		{
			new_name = new_name + " " + m.second[0]->m_sTrackName;
		}

		auto *new_substaff = mergeStaves(pModel, pNewStaff, m.second, new_name, m.second[0]->m_sLocalLabel, m.second[0]->m_sLocalIndex);
		new_substaff->m_sTrackName = m.second[0]->m_sTrackName;
		pNewStaff->m_apStaves.push_back(new_substaff);
	}

	pNewStaff->sortNotationObjects();
	return pNewStaff;
}

////////////////////////////////////////////////////////////////
void CStaff::mergeChildSubtrees2()
{
	printf("    Merging in: %s (%s%s)\n", m_sName.c_str(), m_sLocalLabel.c_str(), m_sLocalIndex.c_str()	);

	if (m_apStaves.size() == 0)
	{
		return;
	}

	// Generate merge lists per track names
	std::map<std::string, std::vector<converdi::CStaff*>> staves_by_track_names;
	for (auto staff : m_apStaves)
	{
		staves_by_track_names[staff->m_sTrackName].push_back(staff);
	}

	m_apStaves.clear();

	for (auto staves : staves_by_track_names)
	{
		// Merge the events of the substaves of this staff.
		// The result will be a single staff that will be the only substave of this staff.
		std::string new_name = m_sName;
		if (staves.second[0]->m_sTrackName.length() > 0)
		{
			new_name = new_name + " " + staves.second[0]->m_sTrackName;
		}

		auto *new_staff = mergeStaves(m_pMusicModel, this, staves.second, new_name, staves.second[0]->m_sLocalLabel + staves.first, "_");
		new_staff->m_sTrackName = staves.first;
		m_apStaves.push_back(new_staff);
	}
}

}