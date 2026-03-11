#pragma once
#include <set>

namespace converdi
{

////////////////////////////////////////////////////////////////
class CScope
{
public:
	using TMovedTypesList = std::set<NOTATION_OBJECT::EType>;

	enum EFeature
	{
		All,
		Staccato,   // No regions
		Tremolo,    // No regions
		Slur,       // One slur <=> one region
		Pizzicato,  // Consecutive pizzicato notes <=> one region
		MidiRange,  // Notes in the range <from, to>
		Trill,      // One trill <=> one region
		/* ADD MORE */
	};

	static void getMovedTypes(EFeature feature, TMovedTypesList &list)
	{
		list.clear();
		if (feature == EFeature::Slur)
		{
			list.insert(NOTATION_OBJECT::EType::Note);
			list.insert(NOTATION_OBJECT::EType::Slur);
			list.insert(NOTATION_OBJECT::EType::PedalLine);
		}
		if (feature == EFeature::Pizzicato)
		{
			list.insert(NOTATION_OBJECT::EType::Note);
			list.insert(NOTATION_OBJECT::EType::PedalLine);
		}
		if (feature == EFeature::Staccato)
		{
			list.insert(NOTATION_OBJECT::EType::Note);
			list.insert(NOTATION_OBJECT::EType::PedalLine);
		}
		if (feature == EFeature::Tremolo)
		{
			list.insert(NOTATION_OBJECT::EType::Note);
			list.insert(NOTATION_OBJECT::EType::PedalLine);
		}
		if (feature == EFeature::MidiRange)
		{
			list.insert(NOTATION_OBJECT::EType::Note);
			list.insert(NOTATION_OBJECT::EType::PedalLine);
		}
		if (feature == EFeature::Trill)
		{
			list.insert(NOTATION_OBJECT::EType::Note);
			list.insert(NOTATION_OBJECT::EType::Trill);
			list.insert(NOTATION_OBJECT::EType::PedalLine);
		}
	}

	std::string               m_sName = "";
	EFeature                  m_Feature = EFeature::All;
	std::vector<std::string>  m_Params;
	std::string               m_sDestinationTrack;

	static CScope* parseScope(CTokenizer &tokenizer);

	virtual bool passes(NOTATION_OBJECT *object);
};

////////////////////////////////////////////////////////////////
class CScopeAnd : public CScope
{
public:
	// Scope is a logical and over multiple CSimpleScope objects
	std::vector<CScope*> m_apSimpleScopes;

	virtual bool passes(NOTATION_OBJECT *object);
};

}