#pragma once

namespace converdi
{

////////////////////////////////////////////////////////////////
struct TIMELINE_POS
{
	int m_Measure = 1;
	int m_TicksInMeasure = 0;

	const char* format(char *s, int length)
	{
		sprintf_s(s, length, "%d:%d", m_Measure, m_TicksInMeasure);
		return s;
	}

	bool operator==(TIMELINE_POS &other)
	{
		if (other.m_Measure != m_Measure) return false;
		if (other.m_TicksInMeasure != m_TicksInMeasure) return false;
		return true;
	}

	bool operator!=(TIMELINE_POS &other)
	{
		return !operator==(other);
	}
};

}