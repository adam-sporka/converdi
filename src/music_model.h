#pragma once

#include <vector>
#include <map>
#include <string>
#include <array>

namespace converdi
{

class CProfile;
struct AVAILABLE_JOB;

////////////////////////////////////////////////////////////////
// All staves.
// The two-hand staves have a CStaff for each. The lower one will be the "co-staff".
class CMusicModel : public CStaffOwner
{
public:
	void debugDump(std::string indent)
	{
		printf("%sMUSIC MODEL\n", indent.c_str());
		if (m_pSystemStaff) m_pSystemStaff->debugDump(indent + "  ");
		for (auto s : m_apStaves) s->debugDump(indent + "  ");
		printf("%sEND MUSIC MODEL\n", indent.c_str());
	}

	void buildSingleAndGrandStaves();
	void process();
	int getTotalTrackCount();
	void writeAsMidi(const char *filename);

	void chooseJobs(CProfile *job_list);
	void applyJobs(bool do_final_only);

	// void applyJobList(CProfile *job_list, bool do_final_only);
	int getMeasureCount();

	static void promoteSubstavesOfEmptyStaves(CStaffOwner *owner);
	virtual void reportSelfForDebug(std::string indent);

public:
	CStaff* m_pSystemStaff = nullptr;
};

}