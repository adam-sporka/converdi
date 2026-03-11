#pragma once

#include <array>
#include <sstream>

namespace converdi {

class CMeasure;

////////////////////////////////////////////////////////////////
struct NOTATION_OBJECT
{
	using TVoiceBitmap = std::array<bool, CConfiguration::m_nMaxVoices>;
	static_assert(CConfiguration::m_nMaxVoices == 4, "Check the number of voices in CConfiguration");

	// Note: KeySignature is not represented here by default.
	// It will be a property of each measure.
	enum EType {
		Type_undef,
		Note,
		Rest,
		TimeSignature,
		Tempo, // (from text)
		RitardLine,
		DynamicsValue, // fff, mf, p... (from text)
		DynamicsLine, // Crescendo, Decrescendo
		PedalLine,
		Slur,
		Trill,
		Symbol, // (mainly for trills)
		Technique, // pizz., nat., ... (from text)
		Text,
		CC,
	};

	const char* getETypeAsString(EType type, bool &has_subtype)
	{
		switch (type)
		{
		case Note:
			has_subtype = false;
			return "Note";
		case Rest:
			has_subtype = false;
			return "Rest";
		case TimeSignature:
			has_subtype = false;
			return "TimeSignature";
		case Tempo:
			has_subtype = false;
			return "Tempo";
		case RitardLine:
			has_subtype = true;
			return "RitardLine";
		case DynamicsValue:
			has_subtype = false;
			return "DynamicsValue";
		case DynamicsLine:
			has_subtype = true;
			return "DynamicsLine";
		case PedalLine:
			has_subtype = false;
			return "PedalLine";
		case Slur:
			has_subtype = false;
			return "Slur";
		case Trill:
			has_subtype = false;
			return "Trill";
		case Symbol:
			has_subtype = true;
			return "Symbol";
		case Technique:
			has_subtype = true;
			return "Technique";
		case Text:
			has_subtype = true;
			return "Text";
		case CC:
			has_subtype = false;
			return "CC";
		default:
			has_subtype = false;
			return "";
		}
	}

	enum ESubtype {
		Subtype_undef,
		Crescendo_crescendo,
		Crescendo_decrescendo,
		Symbol_flat,
		Symbol_sharp,
		Symbol_nat,
		Technique_natural,
		Technique_pizzicato,
		Ritard_molto_accelerando,
		Ritard_accelerando,
		Ritard_poco_accelerando,
		Ritard_molto_ritardando,
		Ritard_ritardando,
		Ritard_poco_ritardando,
		Text_plain_system_text
	};

	const char* getESubtypeAsString(ESubtype subtype)
	{
		switch (subtype)
		{
		case Crescendo_crescendo:
			return "Crescendo_crescendo";
		case Crescendo_decrescendo:
			return "Crescendo_decrescendo";
		case Symbol_flat:
			return "Symbol_flat";
		case Symbol_sharp:
			return "Symbol_sharp";
		case Symbol_nat:
			return "Symbol_nat";
		case Technique_natural:
			return "Technique_natural";
		case Technique_pizzicato:
			return "Technique_pizzicato";
		case Ritard_molto_accelerando:
			return "Ritard_molto_accelerando";
		case Ritard_accelerando:
			return "Ritard_accelerando";
		case Ritard_poco_accelerando:
			return "Ritard_poco_accelerando";
		case Ritard_molto_ritardando:
			return "Ritard_molto_ritardando";
		case Ritard_ritardando:
			return "Ritard_ritardando";
		case Ritard_poco_ritardando:
			return "Ritard_poco_ritardando";
		case Text_plain_system_text:
			return "Text_plain_system_text";
		default:
			return "";
		}
	}

	NOTATION_OBJECT(CMeasure* pMeasure);
	NOTATION_OBJECT(CMeasure* pMeasure, EType type, ESubtype subtype, TIMELINE_POS pos, int voices, int tremolo = 0, int arpeggio = 0);
	NOTATION_OBJECT& operator=(NOTATION_OBJECT &other);
	std::string formatType();
	int localTicksToGlobalTicks();
	void debugDump(std::string indent);

	static void sortNotesAscendingPitch(std::vector<NOTATION_OBJECT*>& to_sort);

public:
	CMeasure*         m_pMeasure;               // Measure this object belongs to
	int               m_ID = -1;

	// PARSED VALUES

	EType             m_Type = EType::Type_undef;
	ESubtype          m_Subtype = ESubtype::Subtype_undef;

	TVoiceBitmap      m_Voices = { false, false, false, false };
	TIMELINE_POS      m_Position;
	int               m_nDuration = 0;          // In ticks
	int               m_nSoundingMidiNr = 0;
	int               m_nTremolo = 0;
	int               m_nArpeggio = 0;          // 0 = none; 1 = up; -1 = down
	std::string       m_sSoundingName = "";
	int               m_nWrittenMidiNr = 0;
	std::string       m_sWrittenName = "";
	int               m_nSigNum = 0;            // For time signature: this number ...
	int               m_nSigDenom = 1;          // ... over this number
	double            m_fTempoBpm = 120.0;
	std::string       m_DynamicsName;           // One of the values of g_Cfg.m_DynamicsNames
	int               m_nSemitonesTrill = 0;    // Number of semitones above the sounding note
	bool              m_bTied = false;             // There is a tie originating in this note
	bool              m_bTiedTremolo = false;   // There is a tie originating in this note and the note is a tremolo or trill
	std::string       m_sText = "";             // The text in RitardLine is fetched from a Text_plain_system_text within the range of the RitardLine

	bool              m_bArticulationStaccato = false;

	// RUN-TIME VALUES

	bool              m_bDeleted = false;
	double            m_fTimeAbs = 0.0f;
	std::string       m_sDynValue = "undef";
	std::string       m_sDynValueEnd = "undef";
	bool              m_bFirstInSegment = false;  // For handling multiple substaves
	bool              m_bLastInSegment = false;   

	int               m_nForcedVelocity = -1;
	int               m_cc = 0;                 // Sibelius does not export CC values
	int               m_ccVal = 0;

	NOTATION_OBJECT*  m_pTiedFrom = nullptr;    // Notation object this note is tied from (from other to this)
	int               m_nSlurObjID = 0;         // Slur that owns this note (zero = no slur)
	int               m_nTrillObjID = 0;        // Trill that owns this note (zero = no trill)
};

}