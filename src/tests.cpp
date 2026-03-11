#include <stdio.h>
#include "converdi.h"

void test_timeline_format_conversions(converdi::CSibeliusDataParser &parser)
{
	for (int a = 0; a < 100; a++)
	{
		int test_gt = (rand() % 256) << 8 | (rand() % 256);

		double s;
		int m, t;
		int gt;

		if (!parser.m_pSystemStaff->globalTicksToGlobalSeconds(test_gt, s))
		{
			printf("%d out of bounds\n", test_gt);
		}

		if (!parser.m_pSystemStaff->globalSecondsToLocalTicks(s, m, t))
		{
			printf("%f out of bounds\n", s);
		}


		if (!parser.m_pSystemStaff->localTicksToGlobalTicks(m, t, gt))
		{
			printf("%d:%d out of bounds\n", m, t);
		}

		printf("%d --> %f --> %d:%d --> %d\n", test_gt, s, m, t, gt);
	}

	for (int a = 0; a < 100; a++)
	{
		double test_s = (rand() % 30000) / 100.0f;

		int m, t;
		int gt;
		double s2;

		if (!parser.m_pSystemStaff->globalSecondsToLocalTicks(test_s, m, t))
		{
			printf("%f out of bounds\n", test_s);
		}

		if (!parser.m_pSystemStaff->localTicksToGlobalTicks(m, t, gt))
		{
			printf("%d:%d out of bounds\n", m, t);
		}

		if (!parser.m_pSystemStaff->globalTicksToGlobalSeconds(gt, s2))
		{
			printf("%d out of bounds\n", gt);
		}

		printf("global seconds %f --> measure:tick %d:%d --> global ticks %d --> global seconds %f   (diff %f)\n", test_s, m, t, gt, s2, s2 - test_s);
	}
}