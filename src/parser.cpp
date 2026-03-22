#include "converdi.h"

#include <algorithm>
#include <set>
#include <vector>

namespace converdi
{

// Constants
static const char* note_names[] =
{
	"C-1", "C#-1", "D-1", "D#-1", "E-1", "F-1", "F#-1", "G-1", "G#-1", "A-1", "A#-1", "B-1",
	"C0", "C#0", "D0", "D#0", "E0", "F0", "F#0", "G0", "G#0", "A0", "A#0", "B0",
	"C1", "C#1", "D1", "D#1", "E1", "F1", "F#1", "G1", "G#1", "A1", "A#1", "B1",
	"C2", "C#2", "D2", "D#2", "E2", "F2", "F#2", "G2", "G#2", "A2", "A#2", "B2",
	"C3", "C#3", "D3", "D#3", "E3", "F3", "F#3", "G3", "G#3", "A3", "A#3", "B3",
	"C4", "C#4", "D4", "D#4", "E4", "F4", "F#4", "G4", "G#4", "A4", "A#4", "B4",
	"C5", "C#5", "D5", "D#5", "E5", "F5", "F#5", "G5", "G#5", "A5", "A#5", "B5",
	"C6", "C#6", "D6", "D#6", "E6", "F6", "F#6", "G6", "G#6", "A6", "A#6", "B6",
	"C7", "C#7", "D7", "D#7", "E7", "F7", "F#7", "G7", "G#7", "A7", "A#7", "B7",
	"C8", "C#8", "D8", "D#8", "E8", "F8", "F#8", "G8", "G#8", "A8", "A#8", "B8",
	"C9", "C#9", "D9", "D#9", "E9", "F9", "F#9", "G9", "G#9"
};

////////////////////////////////////////////////////////////////
std::string CSibeliusDataParser::guessDynamicsNameFromString(const char *txt)
{
	std::string out;
	for (const char *c = txt; *c; c++)
	{
		if (*c >= 'a' && *c <= 'z') out = out + *c;
		if (*c >= 'A' && *c <= 'Z') out = out + (char)(*c + ('a' - 'A'));
	}

	// Ignore "sempre"
	auto sempre = out.find("sempre");
	if (sempre != std::string::npos)
	{
		out = out.substr(0, sempre) + out.substr(sempre + 6);
	}

	auto pos = std::find(g_Cfg.m_DynamicsNames.begin(), g_Cfg.m_DynamicsNames.end(), out);
	if (pos != g_Cfg.m_DynamicsNames.end())
	{
		return *pos;
	}

	/*
	std::string s;
	s = "Unknown dynamics name ";
	s += txt;
	throw new CProcessingException(s);
	*/

	return "";
}

////////////////////////////////////////////////////////////////
NOTATION_OBJECT::ESubtype CSibeliusDataParser::guessTechniqueFromString(const char *txt)
{
	if (strcmp(txt, "pizz.") == 0) return NOTATION_OBJECT::ESubtype::Technique_pizzicato;
	if (strcmp(txt, "nat.") == 0) return NOTATION_OBJECT::ESubtype::Technique_natural;
	return NOTATION_OBJECT::ESubtype::Subtype_undef;
}

////////////////////////////////////////////////////////////////
NOTATION_OBJECT::ESubtype CSibeliusDataParser::guessSymbolFromString(const char *txt)
{
	if (strcmp(txt, "Flat") == 0) return NOTATION_OBJECT::ESubtype::Symbol_flat;
	if (strcmp(txt, "Natural") == 0) return NOTATION_OBJECT::ESubtype::Symbol_nat;
	if (strcmp(txt, "Sharp") == 0) return NOTATION_OBJECT::ESubtype::Symbol_sharp;
	return NOTATION_OBJECT::ESubtype::Subtype_undef;
}

////////////////////////////////////////////////////////////////
int lengthNameToTicks(char c, bool dot)
{
	int ticks = g_Cfg.m_TicksPerQuarterNote;
	switch (c)
	{
		case 'y': ticks /= (2 * 2 * 2); break;
		case 'x': ticks /= (2 * 2); break;
		case 'e': ticks /= 2; break;
		case 'h': ticks *= 2; break;
		/* case 'q': break; */
		case 'w': ticks *= 2 * 2; break;
		case 'W': ticks *= 2 * 2 * 2; break;
		case 'V': ticks *= 2 * 2 * 2 * 2; break;
		default:
			// do nothing
			break;
	}

	if (dot)
	{
		ticks *= 3;
		ticks /= 2;
	}

	return ticks;
}

////////////////////////////////////////////////////////////////
double CSibeliusDataParser::guessTempoFromString(std::string tempo)
{
	const char* c = tempo.c_str();
	const char* eq = strstr(c, "=");

	if (eq == nullptr)
	{
		return 20.0;
	}

	const char* filter = "yxehqwWV";
	char length_name = 0;
	bool dot = false;
	for (auto p = eq; (p >= c) && (length_name == 0); p--)
	{
		if (*p == '.') dot = true;
		for (int a = 0; a < strlen(filter); a++)
		{
			if (*p == filter[a])
			{
				length_name = *p;
				break;
			}
		}
	}

	if (length_name == 0)
	{
		return 20.0;
	}

	bool afterfpt = false;
	double number = 0;
	double order = 0.1;
	for (auto p = eq; *p; p++)
	{
		if (*p >= '0' && *p <= '9')
		{
			if (!afterfpt) number = number * 10 + (*p - '0');
			else 
			{
				number = number + (*p - '0') * order;
				order /= 10.0;
			}
		}
		else if (*p == '.') afterfpt = true;
		else if (*p == ' ');
		else if (*p == '=');
		else
		{
			break;
		}
	}

	if (length_name != 0)
	{
		double ticks = (double)lengthNameToTicks(length_name, dot);
		double factor = ticks / g_Cfg.m_TicksPerQuarterNote;

		double dur = factor * (60.0 / number);
		double bpm = 60.0 / dur;
		return bpm;
	}

	return 20.0;
}

////////////////////////////////////////////////////////////////
void hackAndSlash(std::string &text)
{
	std::string output;
	for (auto a = 0; a < text.length(); a++)
	{
		char c = text[a];
		if (c >= '0' && c <= '9') output = output + c;
		else if (c >= 'a' && c <= 'z') output = output + c;
		else if (c >= 'A' && c <= 'Z') output = output + ((char)(c + ('a' - 'A')));
		else if (c == '=') output = output + c;
		else if (c == ' ') output = output + c;
	}
	text = output;
}

////////////////////////////////////////////////////////////////
NOTATION_OBJECT::ESubtype CSibeliusDataParser::guessRitardFromString(const char *txt)
{
	std::string hacked = txt;
	hackAndSlash(hacked);

	bool poco = strstr(hacked.c_str(), "poco") != nullptr;
	bool molto = strstr(hacked.c_str(), "molto") != nullptr;

	bool rit = strstr(hacked.c_str(), "rit") != nullptr;
	bool accell = strstr(hacked.c_str(), "accel") != nullptr;

	if (accell)
	{
		if (poco) return NOTATION_OBJECT::ESubtype::Ritard_poco_accelerando;
		if (molto) return NOTATION_OBJECT::ESubtype::Ritard_molto_accelerando;
		return NOTATION_OBJECT::ESubtype::Ritard_accelerando;
	}

	if (rit)
	{
		if (poco) return NOTATION_OBJECT::ESubtype::Ritard_poco_ritardando;
		if (molto) return NOTATION_OBJECT::ESubtype::Ritard_molto_ritardando;
		return NOTATION_OBJECT::ESubtype::Ritard_ritardando;
	}

	return NOTATION_OBJECT::ESubtype::Subtype_undef;
}

////////////////////////////////////////////////////////////////
std::string CSibeliusDataParser::stripMidiTrackName(std::string name)
{
	std::string out;
	for (char c : name)
	{
		if (c >= 'a' && c <= 'z') out += c;
		else if (c >= 'A' && c <= 'Z') out += c;
		else if (c >= '0' && c <= '9') out += c;
		else if (c == ' ') out += c;
		else if (c == '_') out += c;
		else if (c == '-') out += c;
		else out += '_';
	}
	return out;
}

////////////////////////////////////////////////////////////////
bool CSibeliusDataParser::midiNumberFromNoteName(const char *notename, int &out)
{
	for (int m = 0; m < 128; m++)
	{
		if (strcmp(notename, note_names[m]) == 0)
		{
			out = m;
			return true;
		}
	}
	return false;
}

////////////////////////////////////////////////////////////////
void CSibeliusDataParser::guessRatioFromString(const char *txt, int &num, int &denom)
{
	// TODO: ".23../...." (dots mean spaces) should be 23/1 and not 23/0

	auto slash = strstr(txt, "/");
	if (slash == nullptr)
	{
		num = atoi(txt);
		denom = 1;
		return;
	}

	int pos = (int)(slash - txt);
	if (pos == 0)
	{
		num = 0;
		denom = atoi(txt + 1);
		return;
	}

	if (pos == strlen(txt) - 1)
	{
		num = atoi(txt);
		denom = 1;
		return;
	}

	char* tmp = new char[strlen(txt) + 1];
	std::strncpy(tmp, txt, strlen(txt) + 1);
	tmp[slash - txt] = 0;

	num = atoi(tmp);
	denom = atoi(tmp + pos + 1);
}

////////////////////////////////////////////////////////////////
void CSibeliusDataParser::analyzeSibeliusExport(CProfile *profile, const char* input)
{
	tokenizer.analyze(input);

	try
	{
		tokenizer.reset();
		int repeated_staff = 0;
		while (tokenizer.peek().type != CTokenizer::TOKEN::EType::END)
		{
			if (tokenizer.compare("BEGIN STAFF"))
			{
				tokenizer.pushPos();
				tokenizer.advance();
				auto new_staff = parseStaff();

				if (repeated_staff == 0)
				{
					std::vector<AVAILABLE_JOB*> jobs;
					profile->getJobsByStaffName(new_staff->m_sName, jobs);
					repeated_staff = (int)jobs.size();
				}

				if (new_staff->m_bSystemStaff)
				{
					m_pSystemStaff = new_staff;
				}
				else
				{
					m_apStaves.push_back(new_staff);
				}

				if (repeated_staff > 1)
				{
					tokenizer.popPos();
					repeated_staff--;
				}
				else
				{
					tokenizer.forgetLastPos();
					repeated_staff = 0;
				}
			}
			else
			{
				tokenizer.advance();
			}
		}

		process();
	}
	catch (CProcessingException *e)
	{
		printf("Error processing input: %s\n", e->message.c_str());
		delete e;
	}
}

////////////////////////////////////////////////////////////////
CStaff* CSibeliusDataParser::parseStaff()
{
	bool system = false;
	int compound = 1;
	std::string name;

	if (tokenizer.compare("SYSTEM"))
	{
		tokenizer.advance();
		system = true;
		name = "System";
	}

	if (tokenizer.compare("COMPOUND"))
	{
		tokenizer.advance();
		compound = atoi(tokenizer.text().c_str());
		tokenizer.advance();
	}

	if (tokenizer.compare("NAME"))
	{
		tokenizer.advance();
		name = tokenizer.text();
		tokenizer.advance();
	}

	CStaff *pNewStaff = new CStaff(this, nullptr, "-", "-");
	pNewStaff->m_bSystemStaff = system;
	pNewStaff->m_nStaffCountPerInstrument = compound;
	pNewStaff->m_sName = name;

	printf("Parsing %s staff '%s' (compound %d)\n", pNewStaff->m_bSystemStaff ? "system" : "instrument", pNewStaff->m_sName.c_str(), pNewStaff->m_nStaffCountPerInstrument);
	
	while (true)
	{
		if (tokenizer.compare("BEGIN BAR"))
		{
			tokenizer.advance();
			auto *new_bar = parseBar(pNewStaff);
			pNewStaff->m_apMeasures.push_back(new_bar);
			continue;
		}
		else if (tokenizer.compare("END STAFF"))
		{
			tokenizer.advance();
			break;
		}
		else
		{
			tokenizer.advance();
		}
	}

	pNewStaff->process();
	
	return pNewStaff;
}

////////////////////////////////////////////////////////////////
CMeasure* CSibeliusDataParser::parseBar(CStaff* pTheStaff)
{
	int number = 0;
	int duration = 0;
	int sharps = 0;

	number = atoi(tokenizer.text().c_str());
	tokenizer.advance();

	if (tokenizer.compare("DUR"))
	{
		tokenizer.advance();
		duration = atoi(tokenizer.text().c_str());
		tokenizer.advance();
	}

	if (tokenizer.compare("SHARPS"))
	{
		tokenizer.advance();
		sharps = atoi(tokenizer.text().c_str());
		tokenizer.advance();
	}

	CMeasure *pNewMeasure = new CMeasure(pTheStaff, number);
	pNewMeasure->m_nSignature = sharps;
	pNewMeasure->m_nDuration = duration;

	while (true)
	{
		if (tokenizer.compare("OBJ"))
		{
			tokenizer.advance();
			std::vector<NOTATION_OBJECT*> new_objects;
			parseObject(pNewMeasure, new_objects);

			for (auto o : new_objects)
			{
				pNewMeasure->m_apNotationObjects.push_back(o);
			}
		}
		else if (tokenizer.compare("END BAR"))
		{
			tokenizer.advance();
			return pNewMeasure;
		}
		else
		{
			tokenizer.advance();
		}
	}
}

////////////////////////////////////////////////////////////////
bool CSibeliusDataParser::decodeNoteData(std::string text, NOTATION_OBJECT *obj)
{
	if (text.length() < 2)
	{
		return false;
	}
	if (text[0] != '<') 
	{
		return false;
	}
	if (text[text.length() - 1] != '>') 
	{
		return false;
	}

	std::vector<std::string> parts;
	std::string current = "";
	for (int a = 1; a < text.length() - 1; a++)
	{
		char c = text[a];
		if (c == ',') {
			parts.push_back(current);
			current = "";
		}
		else {
			current = current + c;
		}
	}
	if (current.length() > 0) parts.push_back(current);

	if (parts.size() < 5)
	{
		return false;
	}

	bool tied_into = false; // Ignored for now
	std::string sounding_name = "";
	int sounding_midi = 0;
	std::string written_name = "";
	int written_midi = 0;
	int style = 0;
	bool tied_from = false;

	int a = 0;
	if (parts[0].compare("TIED_INTO") == 0) { tied_into = true; a++; }
	sounding_name = parts[a]; a++;
	sounding_midi = atoi(parts[a].c_str()); a++;
	written_name = parts[a]; a++;
	written_midi = atoi(parts[a].c_str()); a++;
	style = atoi(parts[a].c_str()); a++;
	if (parts.size() > a)
	{
		if (parts[a].compare("TIED") == 0) { tied_from = true; a++; }
	}

	obj->m_nSoundingMidiNr = sounding_midi;
	obj->m_nWrittenMidiNr = written_midi;
	obj->m_sSoundingName = sounding_name;
	obj->m_sWrittenName = written_name;
	obj->m_bTied = tied_from;

	return true;
}

std::vector<std::string> g_asIgnoredTypes;

////////////////////////////////////////////////////////////////
void CSibeliusDataParser::parseObject(CMeasure* pTheMeasure, std::vector<NOTATION_OBJECT*> &new_objects)
{
	std::string type = tokenizer.text();
	tokenizer.advance();

	int vox_flags = 0;
	int pos = 0;
	int dur = 0;
	int symbol_index = 0;
	int tremolo = 0;
	int arpeggio = 0;
	std::string style = "";
	std::set<std::string> articulations;
	std::string txt = "";
	std::string symbol_name = "";
	std::string ratio = "";

	std::vector<std::string> notes;

	while (true)
	{
		if (tokenizer.compare("VOX"))
		{
			tokenizer.advance();
			vox_flags = atoi(tokenizer.text().c_str());
			tokenizer.advance();
		}

		else if (tokenizer.compare("POS"))
		{
			tokenizer.advance();
			pos = atoi(tokenizer.text().c_str());
			tokenizer.advance();
		}

		else if (tokenizer.compare("DUR"))
		{
			tokenizer.advance();
			dur = atoi(tokenizer.text().c_str());
			tokenizer.advance();
		}

		else if (tokenizer.compare("TREMOLO"))
		{
			tokenizer.advance();
			tremolo = atoi(tokenizer.text().c_str());
			tokenizer.advance();
		}

		else if (tokenizer.compare2("ARP", "UP"))
		{
			tokenizer.advance(2);
			arpeggio = 1;
		}

		else if (tokenizer.compare2("ARP", "DOWN"))
		{
			tokenizer.advance(2);
			arpeggio = -1;
		}

		else if (tokenizer.compare("STYLE"))
		{
			tokenizer.advance();
			style = tokenizer.text();
			tokenizer.advance();
		}

		else if (tokenizer.compare("A10N"))
		{
			tokenizer.advance();
			articulations.insert(tokenizer.text());
			tokenizer.advance();
		}

		else if (tokenizer.compare("TXT"))
		{
			tokenizer.advance();
			txt = tokenizer.text();
			tokenizer.advance();
		}

		else if (tokenizer.compare("IDX"))
		{
			tokenizer.advance();
			symbol_index = atoi(tokenizer.text().c_str());
			tokenizer.advance();
		}

		else if (tokenizer.compare("NAME"))
		{
			tokenizer.advance();
			symbol_name = tokenizer.text();
			tokenizer.advance();
		}

		else if (tokenizer.compare("RATIO"))
		{
			tokenizer.advance();
			ratio = tokenizer.text();
			tokenizer.advance();
		}

		else if (tokenizer.compare("NOTES"))
		{
			tokenizer.advance();
			int count_notes = atoi(tokenizer.text().c_str());
			tokenizer.advance();

			for (int a = 0; a < count_notes; a++)
			{
				notes.push_back(tokenizer.text());
				tokenizer.advance();
			}
		}

		else if (tokenizer.compare(";"))
		{
			tokenizer.advance();
			// For notes
			if (notes.size() > 0)
			{
				for (auto n : notes)
				{
					NOTATION_OBJECT *object = new NOTATION_OBJECT(pTheMeasure, NOTATION_OBJECT::EType::Note, NOTATION_OBJECT::ESubtype::Subtype_undef, { pTheMeasure->m_nNumber, pos }, vox_flags);
					object->m_nDuration = dur;
					if (articulations.count("STACCATO")) object->m_bArticulationStaccato = true;
					object->m_nTremolo = tremolo;
					object->m_nArpeggio = arpeggio;
					decodeNoteData(n, object);
					new_objects.push_back(object);
				}
			}
			// For other objects
			else
			{
				if (type.compare("NoteRest") == 0)
				{
					// No need to do anyting
				}
				else if (type.compare("BarRest") == 0)
				{
					// No need to do anything
				}
				else if (type.compare("SystemTextItem") == 0 || type.compare("Text") == 0)
				{
					if ((style.compare("Tempo") == 0) || (style.compare("Metronome mark") == 0))
					{
						auto tempo = guessTempoFromString(txt);
						NOTATION_OBJECT *object = new NOTATION_OBJECT(pTheMeasure, NOTATION_OBJECT::EType::Tempo, NOTATION_OBJECT::ESubtype::Subtype_undef, { pTheMeasure->m_nNumber, pos }, vox_flags);
						object->m_fTempoBpm = tempo;
						new_objects.push_back(object);
					}
					else if (style.compare("Expression") == 0)
					{
						
						auto dyn = guessDynamicsNameFromString(txt.c_str());
						if (dyn.length())
						{
							NOTATION_OBJECT *object = new NOTATION_OBJECT(pTheMeasure, NOTATION_OBJECT::EType::DynamicsValue, NOTATION_OBJECT::ESubtype::Subtype_undef, { pTheMeasure->m_nNumber, pos }, vox_flags);
							object->m_DynamicsName = dyn;
							new_objects.push_back(object);
						}
						else
						{
							printf("NOTE: Expression %s ignored\n", txt.c_str());
						}
					}
					else if (style.compare("Technique") == 0) {
						auto tech = guessTechniqueFromString(txt.c_str());
						NOTATION_OBJECT *object = new NOTATION_OBJECT(pTheMeasure, NOTATION_OBJECT::EType::Technique, tech, { pTheMeasure->m_nNumber, pos }, vox_flags);
						new_objects.push_back(object);
					}
					else if (style.compare("Plain system text") == 0) {
						auto value = txt.c_str();
						NOTATION_OBJECT *object = new NOTATION_OBJECT(pTheMeasure, NOTATION_OBJECT::EType::Text, NOTATION_OBJECT::ESubtype::Text_plain_system_text, { pTheMeasure->m_nNumber, pos }, vox_flags);
						object->m_sText = value;
						new_objects.push_back(object);
					}
				}

				else if (type.compare("TimeSignature") == 0)
				{
					int num, denom;
					guessRatioFromString(ratio.c_str(), num, denom);
					NOTATION_OBJECT *object = new NOTATION_OBJECT(pTheMeasure, NOTATION_OBJECT::EType::TimeSignature, NOTATION_OBJECT::ESubtype::Subtype_undef, { pTheMeasure->m_nNumber, pos }, vox_flags);
					object->m_nSigNum = num;
					object->m_nSigDenom = denom;
					new_objects.push_back(object);
				}

				else if (type.compare("CrescendoLine") == 0 || type.compare("DiminuendoLine") == 0)
				{
					NOTATION_OBJECT::ESubtype subtype = NOTATION_OBJECT::ESubtype::Subtype_undef;
					if (type.compare("CrescendoLine") == 0) subtype = NOTATION_OBJECT::ESubtype::Crescendo_crescendo;
					if (type.compare("DiminuendoLine") == 0) subtype = NOTATION_OBJECT::ESubtype::Crescendo_decrescendo;
					NOTATION_OBJECT *object = new NOTATION_OBJECT(pTheMeasure, NOTATION_OBJECT::EType::DynamicsLine, subtype, { pTheMeasure->m_nNumber, pos }, vox_flags);
					object->m_nDuration = dur;
					new_objects.push_back(object);
				}

				else if (type.compare("Slur") == 0)
				{
					NOTATION_OBJECT *object = new NOTATION_OBJECT(pTheMeasure, NOTATION_OBJECT::EType::Slur, NOTATION_OBJECT::ESubtype::Subtype_undef, { pTheMeasure->m_nNumber, pos }, vox_flags);
					object->m_nDuration = dur;
					new_objects.push_back(object);
				}

				else if (type.compare("Trill") == 0)
				{
					NOTATION_OBJECT *object = new NOTATION_OBJECT(pTheMeasure, NOTATION_OBJECT::EType::Trill, NOTATION_OBJECT::ESubtype::Subtype_undef, { pTheMeasure->m_nNumber, pos }, vox_flags);
					object->m_nDuration = dur;
					new_objects.push_back(object);
				}

				else if (type.compare("PedalLine") == 0)
				{
					NOTATION_OBJECT *object = new NOTATION_OBJECT(pTheMeasure, NOTATION_OBJECT::EType::PedalLine, NOTATION_OBJECT::ESubtype::Subtype_undef, { pTheMeasure->m_nNumber, pos }, vox_flags);
					object->m_nDuration = dur;
					new_objects.push_back(object);
				}

				else if (type.compare("RitardLine") == 0)
				{
					auto subtype = guessRitardFromString(style.c_str());
					NOTATION_OBJECT *object = new NOTATION_OBJECT(pTheMeasure, NOTATION_OBJECT::EType::RitardLine, subtype, { pTheMeasure->m_nNumber, pos }, vox_flags);
					object->m_nDuration = dur;
					new_objects.push_back(object);
				}

				else if (type.compare("SymbolItem") == 0)
				{
					auto symbol = guessSymbolFromString(symbol_name.c_str());
					NOTATION_OBJECT *object = new NOTATION_OBJECT(pTheMeasure, NOTATION_OBJECT::EType::Symbol, symbol, { pTheMeasure->m_nNumber, pos }, vox_flags);
					new_objects.push_back(object);
				}

				else
				{
					if (std::find(g_asIgnoredTypes.begin(), g_asIgnoredTypes.end(), type) == g_asIgnoredTypes.end())
					{
						printf("  WARNING: Ignored type %s (further messages suppressed)\n", type.c_str());
						g_asIgnoredTypes.push_back(type);
					}
				}
			}
			break;
		}

		else
		{
			printf("  Ignored token %s at line %d, col %d\n", tokenizer.text().c_str(), tokenizer.row(), tokenizer.col());
			tokenizer.advance();
		}
	}
}

}