#include <stdio.h>
#include <stdlib.h>
#ifdef _WIN32
    #include <conio.h>
#endif
#include "converdi.h"

////////////////////////////////////////////////////////////////
char *read_file(const char *path)
{
	FILE *fp = fopen(path, "rb");
	fseek(fp, 0, SEEK_END);
	auto size = ftell(fp);
	char *temp = new char[size + 1];
	memset(temp, 0, size);
	fseek(fp, 0, SEEK_SET);
	fread(temp, 1, size, fp);
	fclose(fp);
	temp[size] = 0;
	return temp;
}

////////////////////////////////////////////////////////////////
converdi::CJobDatabase db;
converdi::CJobDatabase &converdi::CJobDatabase::getInstance()
{
	return db;
}

////////////////////////////////////////////////////////////////
int handle_any_existing_output_file(const char *output_file)
{
	// Verify output file does not exist
	FILE *fp = fopen(output_file, "rb");
	if (fp)
	{
		printf("This is Converdi.\n");
		printf("Output file '%s' already exists. Overwrite? [Y/N]\n", output_file);
		fclose(fp);

		#ifdef _WIN32
			auto c = _getch();
		#else
			auto c = getchar();
		#endif
		if (!(c == 'Y' || c == 'y'))
		{
			printf("No output written.\n");
			return 0;
		}
	}

	return 1;
}

////////////////////////////////////////////////////////////////
int main(int argc, const char *argv[])
{
	if (argc < 3)
	{
		printf("Converdi v0.9 by Adam Sporka\n");
		printf("with presets by Jan Valta\n");
		printf("\n");
		printf("Usage: converdi <sibelius-dump-file.txt> <profile-name>\n");
		printf("Build date: %s %s\n", __DATE__, __TIME__);
		return 0;
	}

	std::string file_to_read = argv[1];
	std::string profile_name = argv[2];

	std::string output_file = file_to_read + ".mid";
	if (!handle_any_existing_output_file(output_file.c_str()))
	{
		return 1;
	}

	printf("Fetching preset database\n");
	if (!db.readDataFromDirectory("./data/presets", ""))
	{
		printf("Can't proceed. Please fix issues and try again.\n");
		return 1;
	}
	printf("Fetching profile database\n");
	if (!db.readDataFromDirectory("./data/profiles", ""))
	{
		printf("Can't proceed. Please fix issues and try again.\n");
		return 1;
	}
	printf("%zu dynamic tables available\n%zu job descriptions available\n%zu profiles defined\n", db.m_apDynTables.size(), db.m_apJobDescriptions.size(), db.m_apProfiles.size());
	printf("\n");

	if (db.m_apProfiles.find(profile_name) == db.m_apProfiles.end())
	{
		printf("ERROR: Profile '%s' does not exist. Please check the parameters or .ini files and try again.\n", profile_name.c_str());
		return 1;
	}

	printf("Parsing input from Sibelius in '%s'\n", file_to_read.c_str());
	converdi::CSibeliusDataParser parser;
	parser.analyzeSibeliusExport(db.m_apProfiles[profile_name], read_file(file_to_read.c_str()));
	printf("\n");

	parser.chooseJobs(db.m_apProfiles[profile_name]);

	parser.applyJobs(false);
	converdi::CMusicModel::promoteSubstavesOfEmptyStaves(&parser);
	parser.applyJobs(true);

	printf("MIDI staves begin written:\n");
	parser.debugStaves("");
	printf("\n");
	parser.writeAsMidi(output_file.c_str());

	return 0;
}
