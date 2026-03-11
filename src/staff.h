#pragma once

#include <vector>
#include <string>



namespace converdi
{

class CMusicModel;
class CDynamicsTable;
class CScope;
class CScopeJob;
class CJobDescription;

////////////////////////////////////////////////////////////////
class CStaff : public CStaffOwner
{
public:
	using TVecMeasurePtr = std::vector<CMeasure*>;
	using TVecSegments = std::vector<std::pair<int, int> >;

	int                          m_nRuntimeNumber;

	CMusicModel*                 m_pMusicModel = nullptr;
	std::string                  m_sName;
	std::string                  m_sTrackName; // For merging
	int                          m_nStaffCountPerInstrument = 1;
	bool                         m_bSystemStaff = false;
	bool                         m_bGrandStaff = false;
	CStaff*                      m_pGrandStaffDynamics = nullptr; // The staff that contains the "master" dynamics for the grand staff
	CStaff*                      m_pUseDynamics; // Use dynamics from this staff (not the same as above)
	CStaffOwner*                 m_pParentStaff = nullptr;

	CJobDescription*             m_pJobToApply = nullptr;
	bool                         m_bMidiOutputEnabled = false;

	TVecMeasurePtr               m_apMeasures;
														   
	int                          m_nTrackIndex = 0;
	CScope*                      m_pRelatedScope = nullptr;
	bool                         m_bDynamicsLookedUp = false;

	TVecSegments                 m_aSegments;

	std::string                  m_sLocalLabel;
	std::string                  m_sLocalIndex;

	CStaff(CMusicModel* model, CStaff *pParentStaff, std::string local_label, std::string local_index);
	void copyMeasureStructure(CStaff *source);

	int purgeDeletedObjects(); // Return the number of purged objects
	void sortNotationObjects();
	bool getNotesPresenceInVoice(int voice); // voice == 0, 1, 2, 3
	int getObjectCount(NOTATION_OBJECT::EType object_type);
	int getTotalTrackCount(); // Return the number of tracks created for this staff
	double getDynValueAtGlobalTicksAtStaff(int abs, CDynamicsTable &table, int &effective_since);
	double getDynValueAtGlobalTicks(int abs, CDynamicsTable &table);
	NOTATION_OBJECT* findLastNoteEndingOnOrBefore(int global_tick, CMeasure* start_with);
	NOTATION_OBJECT* getNextNoteAfter(NOTATION_OBJECT* object);
	void gatherSegments();
	bool areDelimitedSegments();
	bool insideSegment(int gl);
	CMeasure* getMeasureByNumber(int measure);

	bool isNotePlaying(int abs, int note_no, NOTATION_OBJECT *except);

	bool globalTicksToLocalTicks(int abs, int &measure, int &tick);
	bool globalTicksToLocalTicks(int abs, TIMELINE_POS &pos);
	bool globalSecondsToLocalTicks(double abs, int &measure, int &tick);
	bool globalTicksToGlobalSeconds(int abs, double &global_seconds);
	bool localTicksToGlobalTicks(int measure, int tick, int &global_ticks);
	bool localTicksToGlobalTicks(TIMELINE_POS &pos, int &global_ticks);

	// SPLIT OPERATIONS
	// Note: there is no "splitGrandStaff". The grand staff is already build by the music model
	void flattenVoices();
	void splitByVoices();
	void splitByScopes(std::vector<CScope*> scopes);

	void moveObjectToStaff(NOTATION_OBJECT *obj, CStaff *pDestStaff); // Make sure to sort the destination staff afterwards
	static CStaff *mergeStaves(CMusicModel *pModel, CStaff *parent, std::vector<CStaff*> staves, std::string name, std::string local_label, std::string local_index);

	void mergeChildSubtrees2();

	// Mandatory processing -- all
	void buildMeasureLinkedList();
	void calcMeasureAbsPositions();

	// Mandatory processing -- system staff
	void fetchRitardParams();
	void copyFirstTempoMark();
	void createTempoMarksByRetards();
	void calcTempoMarkAbsTimes();

	// Mandatory processing -- music staves
	void applyNoteTies();
	void assignNotesToSlursAndTrills();
	void lookupDynamicsValues();
	void lookupDynamicsValuesTree();
	void delimitSegmentsInSubstaves();

	// Optional processing
	void handleGrandStaffDynamics();
	void injectCcForDynamics(int cc, CDynamicsTable &table);
	void adjustVelocitiesForDynamics(CDynamicsTable &table);
	void arpeggio(int each_ms, int pitch_ms);
	void injectCcForRegions(int cc, int val, bool start); // start == false for region ends
	void injectKeyLatchForRegions(int key, int anticipate_start_ms, int prolong_end_ms); // start == false for region ends
	void shortenRegionNotes(int by_ms, int by_percent, int min_duration, int except_tied_tremolos, bool last_note_only);
	void prolongRegionNotes(int ms);
	void treatStaccatosAsShortNotes(float seconds);
	void setAllNotesToVelocity(int velocity);
	void setAllNotesToPitch(int midi_no);
	void trailStaffByCC(int cc, int val);
	void transposeAllNotes(int delta);
	void pedalAsCC(int cc, int val_begin, int postpone_ms, int val_end, int shorten_ms);
	void shiftAllEvents(float delta_t);

	void process();
	void debugDump(std::string indent);
	virtual void reportSelfForDebug(std::string indent);

	void generateMidiNotesAndCcs(smf::MidiFile &output, int tr);
	void generateMidiSignatures(smf::MidiFile &output);
};

}

