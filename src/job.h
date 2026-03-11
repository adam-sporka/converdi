#pragma once

#include <string>
#include <vector>
#include <map>
#include <algorithm>

namespace converdi
{

struct NOTATION_OBJECT;

////////////////////////////////////////////////////////////////
class CDynamicsTable
{
public:
	std::string m_sName;
	std::map<std::string, double> m_Entries;
	CDynamicsTable();
	bool isValidDynamicsName(std::string name);
	double getNumericValue(std::string name);
	void linearPppFff(double min, double max);
};

////////////////////////////////////////////////////////////////
class CTask
{
public:
	int m_TaskId; 
	std::string m_CommandName; // see CTask::isValidCommand(std::string command) for valid commands
	std::map<std::string, int> m_Parameters;
	CDynamicsTable* m_pDynTable = nullptr;

	CTask(std::string command,
		std::string par1 = "", int val1 = 0,
		std::string par2 = "", int val2 = 0,
		std::string par3 = "", int val3 = 0,
		std::string par4 = "", int val4 = 0)
	{
		m_CommandName = command;
		if (par1.length()) m_Parameters[par1] = val1;
		if (par2.length()) m_Parameters[par2] = val2;
		if (par3.length()) m_Parameters[par3] = val3;
		if (par4.length()) m_Parameters[par4] = val4;
	}

	CTask* getCopy()
	{
		auto copy = new CTask(m_CommandName);
		copy->m_TaskId = m_TaskId;
		copy->m_pDynTable = m_pDynTable;
		for (auto p : m_Parameters)
		{
			copy->m_Parameters[p.first] = p.second;
		}
		return copy;
	}

	int fetchParam(std::string parameter, int default_value);
	static bool isValidCommand(std::string command);

	void perform(CStaff *staff);
};

////////////////////////////////////////////////////////////////
class CJobDescription
{
public:
	using TScopeToTasks = std::map<CScope*, std::vector<CTask*>>;

public:
	// Hierarchy
	std::string            m_sName;
	CJobDescription*       m_pPredecessor = nullptr;

	// Static data
	bool                   m_bMidiOutput = true;
	bool                   m_bSplitVoices = false; // If false ==> voices are flattened into voice 1
	bool                   m_bMergeScopes = false;
	bool                   m_bMergeVoices = false;
	bool                   m_bMergeGrandStaff = false;
	std::vector<CScope*>   m_apScopes;
	TScopeToTasks          m_apTasksAfterScopeSplit;
	std::vector<CTask*>    m_apTasksAfterMerges;

	// Runtime data
	std::map<int, CTask*>  m_IdToTask;

public:
	void                   perform(CStaff &staff, bool do_main, bool do_final);
	void                   copyDataExceptName(CJobDescription &source);
	void                   registerTask(CScope *after_scope, CTask* task);
};

////////////////////////////////////////////////////////////////
struct AVAILABLE_JOB
{
	std::string m_StaffNamePrefix = "";
	CJobDescription *m_pJob = nullptr;
};

////////////////////////////////////////////////////////////////
class CJobDatabase
{
public:
	CTask* parseTask(CJobDescription &in_job_desc, CTokenizer &tokenizer);
	CDynamicsTable* parseDynTable(CTokenizer &tokenizer);
	CJobDescription* parseJobDesc(CTokenizer &tokenizer);
	CProfile* parseProfile(CTokenizer &tokenizer);

	bool attemptParsingFile(std::string path);
	bool readDataFromDirectory(std::string path, std::string indent);

	std::map<std::string, CDynamicsTable*> m_apDynTables;
	std::map<std::string, CJobDescription*> m_apJobDescriptions;
	std::map<std::string, CProfile*> m_apProfiles;

	static CJobDatabase& getInstance();
};

////////////////////////////////////////////////////////////////
class CProfile
{
public:
	std::vector<AVAILABLE_JOB> m_AvailableJobs;
	/*
	AVAILABLE_JOB *getJobByStaffName(std::string staff_name_prefix)
	{
		auto job = std::find_if(m_AvailableJobs.begin(), m_AvailableJobs.end(), [staff_name_prefix](AVAILABLE_JOB& job) {
			if (job.m_StaffNamePrefix.compare(staff_name_prefix.substr(0, job.m_StaffNamePrefix.length())) == 0) return true;
			return false;
		});
		if (job == m_AvailableJobs.end()) return nullptr;
		return &(*job);
	}
	*/
	void getJobsByStaffName(std::string staff_name_prefix, std::vector<AVAILABLE_JOB*> &jobs)
	{
		jobs.clear();
		for (auto &job : m_AvailableJobs)
		{
			if (job.m_pJob->m_sName.length() == 0) continue;
			if (job.m_StaffNamePrefix.compare(staff_name_prefix.substr(0, job.m_StaffNamePrefix.length())) == 0)
			{
				jobs.push_back(&job);
			}
		}
	}

	void addJob(std::string name_prefix, CJobDescription *job)
	{
		m_AvailableJobs.push_back({ name_prefix, job });
	}
};

}