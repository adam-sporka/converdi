#pragma once

#include <list>
#include <string>

namespace converdi {

////////////////////////////////////////////////////////////////
class CProcessingException
{
public:
	std::string message;
	CProcessingException(std::string message) : message(message) {};
};

////////////////////////////////////////////////////////////////
class CTokenizer
{
public:
	struct TOKEN
	{
		enum EType
		{
			NORMAL,
			END
		};
		std::string text;
		int row, col;
		EType type = NORMAL;
	};

	std::list<TOKEN> m_Tokens;
	std::list<TOKEN>::iterator m_Pos;
	std::vector<std::list<TOKEN>::iterator> m_PosStack;

	void analyze(const char* input)
	{
		CLexicalAnalyzer analyzer;
		analyzer.show(input);
		while (analyzer.fetchNextToken())
		{
			TOKEN token;
			token.col = analyzer.col();
			token.row = analyzer.row();
			token.text = analyzer.getCurrentToken();
			m_Tokens.push_back(token);
		}

		// "BEGIN STAFF", "BEGIN BAR", "END STAFF", "END BAR" should be single tokens
		for (auto it = m_Tokens.begin(); it != m_Tokens.end(); it++)
		{
			auto it2 = it; it2++;
			if (it2 != m_Tokens.end())
			{
				if (it->text.compare("BEGIN") == 0 || it->text.compare("END") == 0)
				{
					if (it2->text.compare("STAFF") == 0 || it2->text.compare("BAR") == 0 || it2->text.compare("DYNTABLE") == 0 || it2->text.compare("PROFILE") == 0 || it2->text.compare("JOBDESC") == 0)
					{
						it->text = it->text + " " + it2->text;
						m_Tokens.erase(it2);
					}
				}
			}
		}
	}

	void reset()
	{
		m_Pos = m_Tokens.begin();
	}

	void pushPos()
	{
		m_PosStack.push_back(m_Pos);
	}

	void popPos()
	{
		m_Pos = m_PosStack.back();
		m_PosStack.pop_back();
	}

	void forgetLastPos()
	{
		m_PosStack.pop_back();
	}

	TOKEN peek()
	{
		if (m_Pos == m_Tokens.end())
		{
			TOKEN t;
			t.type = TOKEN::EType::END;
			return t;
		}

		return *m_Pos;
	}

	std::string text()
	{
		if (m_Pos == m_Tokens.end()) throw new CProcessingException("Token expected (E1)");
		return m_Pos->text;
	}

	int row()
	{
		if (m_Pos == m_Tokens.end()) throw new CProcessingException("Token expected (E2)");
		return m_Pos->row;
	}

	int col()
	{
		if (m_Pos == m_Tokens.end()) throw new CProcessingException("Token expected (E3)");
		return m_Pos->col;
	}

	bool compare(const char* text)
	{
		if (m_Pos == m_Tokens.end()) throw new CProcessingException("Token expected (E4)");
		return m_Pos->text.compare(text) == 0;
	}

	bool compare2(const char* text1, const char* text2)
	{
		if (m_Pos == m_Tokens.end()) throw new CProcessingException("Token expected (E5)");
		auto next = m_Pos;
		next++;
		if (next == m_Tokens.end()) throw new CProcessingException("Token expected (E6)");
		return (m_Pos->text.compare(text1) == 0) && (next->text.compare(text2) == 0);
	}

	bool compare3(const char* text1, const char* text2, const char* text3)
	{
		if (m_Pos == m_Tokens.end()) throw new CProcessingException("Token expected (E7)");
		auto next = m_Pos;
		next++;
		if (next == m_Tokens.end()) throw new CProcessingException("Token expected (E8)");
		auto nextnext = next;
		nextnext++;
		if (nextnext == m_Tokens.end()) throw new CProcessingException("Token expected (E9)");
		return (m_Pos->text.compare(text1) == 0) && (next->text.compare(text2) == 0 && (nextnext->text.compare(text3) == 0));
	}

	bool advance(int number = 1)
	{
		if (m_Pos == m_Tokens.end()) throw new CProcessingException("Token expected (E10)");
		for (int a = 0; a < number; a++)
		{
			m_Pos++;
			if (m_Pos == m_Tokens.end()) return false;
		}
		return true;
	}
};

}