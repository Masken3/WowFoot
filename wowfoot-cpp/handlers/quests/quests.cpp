#define __STDC_FORMAT_MACROS
#include "quests.h"
#include <stdio.h>
#include <inttypes.h>
#include <string.h>

void questsChtml::httpArgument(const char* key, const char* value) {
	if(*value == 0)
		return;
	printf("p %s: %s\n", key, value);
}

struct QuestMaxRepTest {
	bool have(const Quest& q) {
		return q.requiredMaxRepFaction != 0;
	}
};

void questsChtml::getResponse2(const char* urlPart, DllResponseData* drd, ostream& os) {
	gQuests.load();
	gFactions.load();

	getArguments(drd);

	mPair = new SimpleItrPair<Quests, QuestMaxRepTest>(gQuests.begin(), gQuests.end());

	drd->code = run(os);
}

questsChtml::questsChtml() : mPair(NULL) {}

questsChtml::~questsChtml() {
	if(mPair)
		delete mPair;
}
