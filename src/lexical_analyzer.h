#pragma once

#include <string>

namespace converdi {

////////////////////////////////////////////////////////////////
class CLexicalAnalyzer
{
	struct POS {
		int row, col;
		unsigned const char *pos;
		POS& operator= (POS& other) {
			pos = other.pos;
			row = other.row;
			col = other.col;
			return *this;
		}
		unsigned char c() {
			return *pos;
		}
	};

public:
	////////////////////////////////
	void show(const char* input)
	{
		pos.pos = (unsigned const char*)input;
		pos.row = 0;
		pos.col = 0;
	}

	////////////////////////////////
	char nextChar(bool consume)
	{
		POS tmp = pos;
		if (tmp.c() == '\0') {
			return '\0';
		}

		bool white = false;
		while (tmp.c() <= 32 && tmp.c() > 0)
		{
			white = true;
			if (tmp.c() == 10) {
				tmp.col = 0;
				tmp.row++;
			}
			else
			{
				tmp.col++;
			}
			tmp.pos++;
		}

		if (white)
		{
			if (consume) pos = tmp;
			return ' ';
		}

		unsigned char result = tmp.c();
		tmp.col++;
		tmp.pos++;
		if (consume) pos = tmp;
		return result;
	}

	bool fetchNextToken()
	{
		if (nextChar(false) == ';')
		{
			token = ';';
			nextChar(true);
			return true;
		}

		if (nextChar(false) == '=')
		{
			token = '=';
			nextChar(true);
			return true;
		}

		if (nextChar(false) == ' ')
		{
			nextChar(true);
		}

		if (nextChar(false) == '\0')
		{
			return false;
		}

		token = "";
		bool quotes = false;
		while (true)
		{
			auto c = nextChar(false);
			if (c == '\'')
			{
				nextChar(true);
				quotes = !quotes;
			}
			else if (c == ';' && !quotes)
			{
				return true;
			}
			else if (c == '=' && !quotes)
			{
				return true;
			}
			else if (c <= ' ' && !quotes)
			{
				nextChar(true);
				return true;
			}
			else if (c == 0 && quotes)
			{
				nextChar(true);
				return true;
			}
			else
			{
				nextChar(true);
				token = token + c;
			}
		}
	}

	////////////////////////////////
	std::string getCurrentToken() { return token; };
	int row() { return pos.row; }
	int col() { return pos.col; }

	////////////////////////////////
	POS pos;
	std::string token;
};

}