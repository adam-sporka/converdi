#pragma once 

#include <list>

namespace converdi {

////////////////////////////////////////////////////////////////
class CSibeliusDataParser : public CMusicModel
{  
private:
	CStaff* parseStaff();
	CMeasure* parseBar(CStaff* pTheStaff);
	void parseObject(CMeasure* pTheMeasure, std::vector<NOTATION_OBJECT*> &new_objects);

	bool decodeNoteData(std::string text, NOTATION_OBJECT *obj);

public:
	static std::string guessDynamicsNameFromString(const char *txt);
	static NOTATION_OBJECT::ESubtype guessTechniqueFromString(const char *txt);
	static NOTATION_OBJECT::ESubtype guessSymbolFromString(const char *txt);
	static double guessTempoFromString(std::string tempo);
	static NOTATION_OBJECT::ESubtype guessRitardFromString(const char *txt);
	static void guessRatioFromString(const char *txt, int &num, int &denom);

	// Return false if failed
	static bool midiNumberFromNoteName(const char *notename, int &out); 

	static std::string stripMidiTrackName(std::string name);

	void analyzeSibeliusExport(CProfile *profile, const char* input);

private:
	CTokenizer tokenizer;
};

}