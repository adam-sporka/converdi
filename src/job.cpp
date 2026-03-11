#include <stdio.h>
#include "converdi.h"

namespace converdi
{

////////////////////////////////////////////////////////////////
void CJobDescription::registerTask(CScope *after_scope, CTask* task)
{
	if (after_scope)
	{ 
		m_apTasksAfterScopeSplit[after_scope].push_back(task);
	}
	else
	{
		m_apTasksAfterMerges.push_back(task);
	}
	m_IdToTask[task->m_TaskId] = task;
}

////////////////////////////////////////////////////////////////
void CJobDescription::copyDataExceptName(CJobDescription &source)
{
	m_bSplitVoices            = source.m_bSplitVoices;
	m_bMergeScopes 					  = source.m_bMergeScopes;
	m_bMergeVoices 					  = source.m_bMergeVoices;
	m_bMergeGrandStaff 			  = source.m_bMergeGrandStaff;
	m_apScopes							  = source.m_apScopes;
	m_bMidiOutput             = source.m_bMidiOutput;

	for (auto s : source.m_apTasksAfterScopeSplit)
	{
		auto *scope = s.first;
		for (auto t : s.second)
		{
			auto *task = t->getCopy();
			m_apTasksAfterScopeSplit[scope].push_back(task);
			m_IdToTask[task->m_TaskId] = task;
		}
	}

	for (auto t : source.m_apTasksAfterMerges)
	{
		auto *task = t->getCopy();
		m_apTasksAfterMerges.push_back(task);
		m_IdToTask[task->m_TaskId] = task;
	}
}

////////////////////////////////////////////////////////////////
void CJobDescription::perform(CStaff &staff, bool do_main, bool do_final)
{
	if (do_main)
	{
		for (auto g : staff.m_apStaves)
		{
			if (!m_bSplitVoices) g->flattenVoices();
			g->splitByVoices();
		}

		for (auto g : staff.m_apStaves) {
			for (auto v : g->m_apStaves) {
				v->splitByScopes(m_apScopes);
				v->delimitSegmentsInSubstaves();
			}
		}

		staff.lookupDynamicsValuesTree();

		for (auto g : staff.m_apStaves) {
			for (auto v : g->m_apStaves) {
				for (auto s : v->m_apStaves) {
					if (s->m_pRelatedScope) {
						for (auto t : m_apTasksAfterScopeSplit[s->m_pRelatedScope]) {
							t->perform(s);
						}
					}
				}
			}
		}

		if (m_bMergeScopes)
		{
			for (auto g : staff.m_apStaves) {
				for (auto v : g->m_apStaves) {
					v->mergeChildSubtrees2();
				}
			}
		}

		if (m_bMergeVoices)
		{
			for (auto g : staff.m_apStaves) {
				g->mergeChildSubtrees2();
			}
		}

		if (m_bMergeGrandStaff)
		{
			staff.mergeChildSubtrees2();
		}
	}

	if (do_final)
	{
		for (auto t : m_apTasksAfterMerges)
		{
			t->perform(&staff);
		}
	}
}

}