#include "chtmlBase.h"
#include "tabs.h"
#include "db_creature_template.h"
#include "spawnPoints.h"

class npcChtml : public tabsChtml, public PageContext {
public:
	int run(ostream& stream);
	void getResponse2(const char* urlPart, DllResponseData* drd, ostream& os);

	const char* mTitle;
	string mMapName;
	const Npc* a;
	spawnPointsChtml mSpawnPointsChtml;
};
