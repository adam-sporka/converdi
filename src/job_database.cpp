#include <stdio.h>
#include "converdi.h"

#include <string>
#include <iostream>
#include <filesystem>

namespace converdi
{

////////////////////////////////////////////////////////////////
bool CJobDatabase::readDataFromDirectory(std::string path, std::string indent)
{
	namespace fs = std::filesystem;

	// Read files here
	// Then read the subdirectories

	for (const auto & entry : fs::directory_iterator(path))
	{
		if (fs::is_regular_file(entry.path()))
		{
			if (entry.path().string().find(".ini") != entry.path().string().npos)
			{
				bool result = attemptParsingFile(entry.path().string());
				if (!result)
				{
					return false;
				}
			}
		}
	}

	for (const auto & entry : fs::directory_iterator(path))
	{
		if (fs::is_directory(entry.path()))
		{
			bool result = readDataFromDirectory(entry.path().string(), indent + "  ");
			if (!result)
			{
				return false;
			}
		}
	}

	return true;
}

////////////////////////////////////////////////////////////////
CTask* CJobDatabase::parseTask(CJobDescription &in_job_desc, CTokenizer &tokenizer)
{
	tokenizer.advance();

	int id = atoi(tokenizer.peek().text.c_str());
	tokenizer.advance();

	CTask *pTask;
	if (in_job_desc.m_IdToTask.find(id) == in_job_desc.m_IdToTask.end())
	{
		pTask = new CTask("");
		pTask->m_TaskId = id;
	}
	else
	{
		pTask = in_job_desc.m_IdToTask[id];
	}

	std::string command = tokenizer.peek().text;
	if (CTask::isValidCommand(command))
	{
		pTask->m_CommandName = command;
		tokenizer.advance();
	}

	if (pTask->m_CommandName == "")
	{
		std::string s;
		s = "Task has an incorrect command name";
		throw new CProcessingException(s);
	}

	while (!tokenizer.compare(";"))
	{
		auto param = tokenizer.peek().text;
		tokenizer.advance();

		if (!tokenizer.compare("="))
		{
			std::string s;
			s = "'=' expected ";
			throw new CProcessingException(s);
		}
		tokenizer.advance();

		auto value = tokenizer.peek().text;
		tokenizer.advance();

		// Parameter is a dyn table name rather than a number
		if (param.compare("table") == 0)
		{
			if (m_apDynTables.find(value) == m_apDynTables.end())
			{
				std::string s;
				s = "Dynamic table '" + value + "' does not exist";
				throw new CProcessingException(s);
			}
			pTask->m_pDynTable = m_apDynTables[value];
		}
		else
		{
			int midi_no;
			if (CSibeliusDataParser::midiNumberFromNoteName(value.c_str(), midi_no))
			{
				pTask->m_Parameters[param] = midi_no;
			}
			else
			{
				pTask->m_Parameters[param] = atoi(value.c_str());
			}
		}
	}
	tokenizer.advance();
	return pTask;
}

////////////////////////////////////////////////////////////////
CJobDescription* CJobDatabase::parseJobDesc(CTokenizer &tokenizer)
{
	tokenizer.advance();

	std::string name = tokenizer.peek().text;
	tokenizer.advance();

	auto *pJobDesc = new CJobDescription();
	pJobDesc->m_sName = name;
	m_apJobDescriptions[name] = pJobDesc;

	CScope *pLastScopeMentioned = nullptr;

	while (tokenizer.peek().text.compare("END JOBDESC") != 0)
	{
		if (tokenizer.compare("BEGIN DYNTABLE")) parseDynTable(tokenizer);

		else if (tokenizer.compare("REFINE"))
		{
			tokenizer.advance();
			std::string predecessor = tokenizer.peek().text;
			tokenizer.advance();

			if (m_apJobDescriptions.find(predecessor) == m_apJobDescriptions.end())
			{
				std::string s;
				s = "Job description " + name + " tries inheriting from " + predecessor + " which does not exist.";
				throw new CProcessingException(s);
			}
			else
			{
				pJobDesc->m_pPredecessor = m_apJobDescriptions[predecessor];
				pJobDesc->copyDataExceptName(*m_apJobDescriptions[predecessor]);
			}
		}

		else if (tokenizer.compare("SCOPE"))
		{
			tokenizer.advance();
			auto scope = CScope::parseScope(tokenizer);
			pJobDesc->m_apScopes.push_back(scope);
			pLastScopeMentioned = scope;
		}

		else if (tokenizer.compare3("SUPPRESS", "MIDI", "OUTPUT"))
		{
			tokenizer.advance(3);
			pJobDesc->m_bMidiOutput = false;
		}

		else if (tokenizer.compare2("SPLIT", "VOICES"))
		{
			tokenizer.advance(2);
			pJobDesc->m_bSplitVoices = true;
		}

		else if (tokenizer.compare2("MERGE", "SCOPES"))
		{
			tokenizer.advance(2);
			pJobDesc->m_bMergeScopes = true;
		}

		else if (tokenizer.compare2("MERGE", "VOICES"))
		{
			tokenizer.advance(2);
			pJobDesc->m_bMergeVoices = true;
		}

		else if (tokenizer.compare3("MERGE", "GRAND", "STAFF"))
		{
			tokenizer.advance(3);
			pJobDesc->m_bMergeGrandStaff = true;
		}

		else if (tokenizer.compare("TASK"))
		{
			auto *pTask = parseTask(*pJobDesc, tokenizer);
			pJobDesc->registerTask(pLastScopeMentioned, pTask);
		}
		
		else if (tokenizer.compare("FINALLY"))
		{
			pLastScopeMentioned = nullptr;
			tokenizer.advance();
		}

		else
		{
			tokenizer.advance();
		}
	}
	return pJobDesc;
}

////////////////////////////////////////////////////////////////
CProfile* CJobDatabase::parseProfile(CTokenizer &tokenizer)
{
	CProfile* profile = new CProfile;
	CJobDescription *desc = nullptr;

	tokenizer.advance();

	std::string name = tokenizer.peek().text;
	tokenizer.advance();
	
	m_apProfiles[name] = profile;

	while (tokenizer.peek().text.compare("END PROFILE") != 0)
	{
		if (tokenizer.compare("BEGIN DYNTABLE"))
		{
			parseDynTable(tokenizer);
		}

		else if (tokenizer.compare("HANDLE"))
		{
			tokenizer.advance();

			std::string for_instruments = tokenizer.peek().text;
			tokenizer.advance();

			// "USING"
			if (tokenizer.peek().text.compare("USING") != 0)
			{
				throw new CProcessingException("Expected USING keyword");
			}
			tokenizer.advance();

			std::string job_desc_name = tokenizer.peek().text;
			tokenizer.advance();

			if (m_apJobDescriptions.find(job_desc_name) == m_apJobDescriptions.end())
			{
				std::string s;
				s = "Job description " + job_desc_name + " does not exist or did not parse correctly";
				throw new CProcessingException(s);
			}

			CJobDescription *desc = new CJobDescription();
			desc->copyDataExceptName(*m_apJobDescriptions[job_desc_name]);
			desc->m_sName = m_apJobDescriptions[job_desc_name]->m_sName;
			profile->addJob(for_instruments, desc);
		}

		else if (tokenizer.compare("TASK"))
		{
			if (desc)
			{
				CTask *task = parseTask(*desc, tokenizer);
			}
		}

		tokenizer.advance();
	}

	return profile;
}

////////////////////////////////////////////////////////////////
CDynamicsTable* CJobDatabase::parseDynTable(CTokenizer &tokenizer)
{
	tokenizer.advance();

	CDynamicsTable *new_table = new CDynamicsTable();
	new_table->m_sName = tokenizer.peek().text;
	tokenizer.advance();

	while (tokenizer.peek().text.compare("END DYNTABLE") != 0)
	{
		std::string dyn_name_candidate = tokenizer.peek().text;
		if (new_table->isValidDynamicsName(dyn_name_candidate))
		{
			tokenizer.advance();
			if (tokenizer.peek().text.compare("=") != 0)
			{
				std::string s;
				s = "'=' expected ";
				throw new CProcessingException(s);
			}
			tokenizer.advance();

			auto value = atoi(tokenizer.peek().text.c_str());
			tokenizer.advance();

			new_table->m_Entries[dyn_name_candidate] = value;
		}
		tokenizer.advance();
	}
	tokenizer.advance();

	m_apDynTables[new_table->m_sName] = new_table;

	return new_table;
}

////////////////////////////////////////////////////////////////
bool CJobDatabase::attemptParsingFile(std::string path)
{
	FILE *fp;
	fopen_s(&fp, path.c_str(), "rb");
	fseek(fp, 0, SEEK_END);
	auto size = ftell(fp);
	char *temp = new char[size + 1];
	memset(temp, 0, size);
	fseek(fp, 0, SEEK_SET);
	fread(temp, 1, size, fp);
	fclose(fp);
	temp[size] = 0;

	CTokenizer tokenizer;
	tokenizer.analyze(temp);

	try
	{
		tokenizer.reset();
		while (tokenizer.peek().type != CTokenizer::TOKEN::EType::END)
		{
			if (tokenizer.compare("BEGIN DYNTABLE"))
			{
				parseDynTable(tokenizer);
			}
			else if (tokenizer.compare("BEGIN PROFILE"))
			{
				parseProfile(tokenizer);
			}
			else if (tokenizer.compare("BEGIN JOBDESC"))
			{
				parseJobDesc(tokenizer);
			}
			else
			{
				tokenizer.advance();
			}
		}
	}

	catch (CProcessingException *e)
	{
		printf("PARSE ERROR %d:%d, file %s\n  %s\n", tokenizer.row(), tokenizer.col(), path.c_str(), e->message.c_str());
		delete e;
		return false;
	}

	delete[] temp;

	return true;
}


}