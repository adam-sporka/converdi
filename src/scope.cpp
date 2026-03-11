#include "converdi.h"

namespace converdi
{

////////////////////////////////////////////////////////////////
bool CScope::passes(NOTATION_OBJECT *obj)
{
	if (m_Feature == EFeature::All)
	{
		return true;
	}

	if (m_Feature == EFeature::Staccato)
	{
		if (obj->m_bArticulationStaccato) return true;
		return false;
	}

	if (m_Feature == EFeature::Tremolo)
	{
		if (obj->m_nTremolo != 0) 
		{
			return true;
		}
		return false;
	}

	if (m_Feature == EFeature::MidiRange)
	{
		int midi_number_1, midi_number_2;

		if (!CSibeliusDataParser::midiNumberFromNoteName(m_Params[0].c_str(), midi_number_1))
		{
			midi_number_1 = atoi(m_Params[0].c_str());
		}

		if (!CSibeliusDataParser::midiNumberFromNoteName(m_Params[1].c_str(), midi_number_2))
		{
			midi_number_2 = atoi(m_Params[1].c_str());
		}

		if (obj->m_nSoundingMidiNr >= midi_number_1 && obj->m_nSoundingMidiNr <= midi_number_2) return true;
		return false;
	}

	if (m_Feature == EFeature::Slur)
	{
		if (obj->m_Type == NOTATION_OBJECT::EType::Slur)
		{
			return true;
		}
		if (obj->m_nSlurObjID > 0)
		{
			return true;
		}
		return false;
	}

	if (m_Feature == EFeature::Trill)
	{
		if (obj->m_Type == NOTATION_OBJECT::EType::Trill)
		{
			return true;
		}
		if (obj->m_nTrillObjID > 0)
		{
			return true;
		}
		return false;
	}

	throw new CProcessingException("A simple scope that hasn't been implemented yet.");
	return false;
}

////////////////////////////////////////////////////////////////
bool CScopeAnd::passes(NOTATION_OBJECT *obj)
{
	bool total_result = true;
	for (auto s : m_apSimpleScopes)
	{
		bool result = s->passes(obj);
		if (!result) total_result = false;
	}
	return total_result;
}

////////////////////////////////////////////////////////////////
CScope* CScope::parseScope(CTokenizer &tokenizer)
{
	CScopeAnd *andScope = nullptr;
	CScope *newScope = nullptr;
	std::string destination_track = "";

	while (!tokenizer.compare(";"))
	{
		if (tokenizer.compare("Slur") || tokenizer.compare("Legato"))
		{
			newScope = new CScope();
			newScope->m_sName = "Slur";
			newScope->m_Feature = EFeature::Slur;
		}
		else if (tokenizer.compare("Staccato"))
		{
			newScope = new CScope();
			newScope->m_sName = "Staccato";
			newScope->m_Feature = EFeature::Staccato;
		}
		else if (tokenizer.compare("Tremolo"))
		{
			newScope = new CScope();
			newScope->m_sName = "Tremolo";
			newScope->m_Feature = EFeature::Tremolo;
		}
		else if (tokenizer.compare("Pizzicato"))
		{
			newScope = new CScope();
			newScope->m_sName = "Pizzicato";
			newScope->m_Feature = EFeature::Pizzicato;
		}
		else if (tokenizer.compare("MidiRange"))
		{
			newScope = new CScope();
			newScope->m_sName = "MidiRange";
			newScope->m_Feature = EFeature::MidiRange;
		}
		else if (tokenizer.compare("Trill"))
		{
			newScope = new CScope();
			newScope->m_sName = "Trill";
			newScope->m_Feature = EFeature::Trill;
		}
		else if (tokenizer.compare("All"))
		{
			newScope = new CScope();
			newScope->m_sName = "All";
			newScope->m_Feature = EFeature::All;
		}
		else
		{
			std::string s;
			s = "Unknown scope '" + tokenizer.peek().text + "'\n";
			throw new CProcessingException(s);
		}

		tokenizer.advance();

		while ((!tokenizer.compare(";")) && (!tokenizer.compare("AND")) && (!tokenizer.compare2("INTO", "TRACK")))
		{
			std::string param = tokenizer.peek().text;
			newScope->m_Params.push_back(param);
			tokenizer.advance();
		}

		if (tokenizer.compare("AND"))
		{
			tokenizer.advance();
			andScope = new CScopeAnd();
			andScope->m_apSimpleScopes.push_back(newScope);
		}
		else if (tokenizer.compare2("INTO", "TRACK"))
		{
			tokenizer.advance(2);
			destination_track = tokenizer.peek().text;
			tokenizer.advance();
		}
	}
	tokenizer.advance();

	if (andScope)
	{
		andScope->m_apSimpleScopes.push_back(newScope);
		std::string s = "";
		for (auto *p : andScope->m_apSimpleScopes)
		{
			if (s.length() > 0) s = s + "&";
			s = s + p->m_sName;
		}
		andScope->m_sName = s;
	}

	CScope *returnedScope = andScope ? andScope : newScope;
	returnedScope->m_sDestinationTrack = destination_track;

	return returnedScope;
}

}