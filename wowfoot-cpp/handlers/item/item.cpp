#define __STDC_FORMAT_MACROS
#include "item.chtml.h"
#include "item_shared.h"
#include "comments.h"
#include "db_npc_vendor.h"
#include "db_gameobject_template.h"
#include "db_loot_template.h"
#include "db_creature_template.h"
#include "dbcItemExtendedCost.h"
#include "dbcItemSubClass.h"
#include "dbcItemClass.h"
#include "chrClasses.h"
#include "chrRaces.h"
#include "ItemExtendedCost.index.h"
#include "money.h"
#include "util/exception.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <sstream>
#include <unordered_map>
#include <inttypes.h>

using namespace std;

static const int ITEM_FLAG_HEROIC = 8;

static void createTabs(vector<Tab*>& tabs, const Item& a);
static Tab* soldBy(const Item& a);
static Tab* currencyFor(const Item& a);
static Tab* sharesModel(const Item& a);
static Tab* droppedBy(const Item& a);
static Tab* pickpocketedFrom(const Item& a);
static Tab* skinnedFrom(const Item& a);
static Tab* containedInObject(const Item& a);
static Tab* referenceLoot(const Item& a);

void init() __attribute((constructor));
void init() {
	printf("init item\n");
}

void fini() __attribute((destructor));
void fini() {
	printf("fini item\n");
}

void itemChtml::getResponse2(const char* urlPart, DllResponseData* drd, ostream& os) {
	gItemDisplayInfos.load();
	gReferenceLoots.load();
	gCreatureLoots.load();
	gPickpocketingLoots.load();
	gSkinningLoots.load();
	gNpcs.load();
	gNpcVendors.load();
	gItems.load();
	gTotemCategories.load();
	gItemExtendedCosts.load();
	ItemExtendedCostIndex::load();
	gSpells.load();
	gObjects.load();
	gGameobjectLoots.load();
	gItemSets.load();
	gQuests.load();

	string buffer;

	int id = toInt(urlPart);
	a = gItems.find(id);
	if(a) {
		if(a->flags & ITEM_FLAG_HEROIC) {
			buffer = "Heroic " + a->name;
			mTitle = buffer.c_str();
		} else {
			mTitle = a->name.c_str();
		}
		mDps = 0.0;
		for(int i=0; i<1; i++) {
			float averageDmg = (a->dmg_min[i] + a->dmg_max[i]) / 2.0;
			if(fnz(averageDmg))
				mDps += averageDmg / (a->delay / 1000.0);
		}
		createTabs(mTabs, *a);
		//printf("sizeof(string): %" PRIuPTR "\n", sizeof(string));
	} else {
		mTitle = urlPart;
		drd->code = 404;
	}
	mTabs.push_back(getComments("item", id));
}

void itemChtml::title(ostream& stream) {
	ESCAPE(mTitle);
}

static void createTabs(vector<Tab*>& tabs, const Item& a) {
	// Sold by (npc)
	tabs.push_back(soldBy(a));
	// Currency for (item)
	tabs.push_back(currencyFor(a));
	// Same model as (item)
	tabs.push_back(sharesModel(a));
	// Disenchants to (item)
	// Disenchanted from (item)
	// Dropped by (npc)
	tabs.push_back(droppedBy(a));
	// Contained in (gameobject)
	tabs.push_back(containedInObject(a));
	// Contained in (item)
	// Milled from (item)
	// Mills to (item)
	// Pickpocketed from (npc)
	tabs.push_back(pickpocketedFrom(a));
	// Prospected from (item)
	// Prospects (item)
	// Skinned from (npc)
	tabs.push_back(skinnedFrom(a));
	// Created by (spell)
	// Reagent for (spell)
	// Provided for quest
	// Required for quest
	// Quest reward choice
	// Quest reward
	// Reference loot (debug)
	tabs.push_back(referenceLoot(a));
}

template<> void streamName(ostream& os, const Item& t) {
	if(t.flags & ITEM_FLAG_HEROIC)
		os << "Heroic ";
	os << t.name;
}

static class streamIfNonFirstClass {
private:
	const char* const mSep;
	bool mHit;
public:
	streamIfNonFirstClass(const char* sep) : mSep(sep) {}
	friend ostream& operator<<(ostream& o, streamIfNonFirstClass& s) {
		if(s.mHit)
			o << s.mSep;
		else
			s.mHit = true;
		return o;
	}
	void reset() { mHit = false; }
} costSep(", ");

static void streamCostHtml(ostream& html, const Item& a, int extendedCostId) {
	gItemExtendedCosts.load();
	costSep.reset();
	if(a.buyPrice == 0 && extendedCostId <= 0) {
		html << "No cost";
		return;
	}
	if(a.buyPrice != 0 && (extendedCostId <= 0 || a.flagsExtra == 3))
		moneyHtml(html, a.buyPrice);
	if(extendedCostId <= 0)
		return;

	const ItemExtendedCost* ecp = gItemExtendedCosts.find(extendedCostId);
	if(!ecp) {
		html << "Extended cost not found (id "<<extendedCostId<<")";
		return;
	}
	const ItemExtendedCost& ec(*ecp);
	if(ec.honorPoints != 0)
		html << costSep << ec.honorPoints<<" honor points";
	if(ec.arenaPoints != 0)
		html << costSep << ec.arenaPoints<<" arena points";
	if(ec.arenaRating != 0) {
		html << costSep << ec.arenaRating<<" ";
		switch(ec.arenaSlot) {
		case 0: html << "2v2"; break;
		case 1: html << "3v3/5v5"; break;
		case 2: html << "5v5"; break;
		default: html << "invalid arenaSlot ("<<ec.arenaSlot<<")";
		}
		html << " arena rating";
	}
	for(int i=0; i<5; i++) {
		ItemExtendedCost::ReqItem item(ec.item[i]);
		if(item.id != 0 || item.count != 0) {
			html << costSep << item.count<<"x ";
			streamNameLinkById(html, gItems, item.id);
		}
	}
}

void npcColumns(tabTableChtml& t) {
	t.columns.push_back(Column(NAME, "Name", ENTRY, "npc"));
	//t.columns.push_back(Column(LOCATION, "Location", ZONE, "zone"));
}

void npcRow(Row& r, const Npc& npc) {
	r[ENTRY] = toString(npc.entry);
	r[NAME] = npc.name;
	r[ZONE] = toString(-1);//mainZoneForNpc(nv.entry);
	//r[LOCATION] = "not implemented";//gAreaTable[r[ZONE]].name;
}

static Tab* soldBy(const Item& a) {
	tabTableChtml& t = *new tabTableChtml();
	t.id = "soldBy";
	t.title = "Sold by";
	npcColumns(t);
	t.columns.push_back(Column(STOCK, "Stock"));
	t.columns.push_back(Column(COST, "Cost", Column::NoEscape));
	NpcVendors::ItemPair res = gNpcVendors.findItem(a.entry);
	for(; res.first != res.second; ++res.first) {
		const NpcVendor& nv(*res.first->second);
		Row r;
		npcRow(r, gNpcs[nv.entry]);
		//todo: add nv.incrtime, a.buyCount;
		if(nv.maxcount == 0)
			r[STOCK] = "∞";
		else
			r[STOCK] = toString(nv.maxcount);
		ostringstream oss;
		streamCostHtml(oss, a, nv.extendedCost);
		r[COST] = oss.str();
		t.array.push_back(r);
	}
	t.count = t.array.size();
	return &t;
}

void lootColumns(tabTableChtml& t) {
	t.columns.push_back(Column(CHANCE, "Chance %"));
	t.columns.push_back(Column(MIN_COUNT, "MinCount"));
	t.columns.push_back(Column(MAX_COUNT, "MaxCount"));
}

void lootRow(Row& r, const Loot& loot) {
	r[CHANCE] = toString(loot.chance);
	r[MIN_COUNT] = toString(loot.minCountOrRef);
	r[MAX_COUNT] = toString(loot.maxCount);
}

static Tab* npcLoot(const Item& a, const Loots& loots, const char* id, const char* title) {
	tabTableChtml& t = *new tabTableChtml();
	t.id = id;
	t.title = title;
	npcColumns(t);
	lootColumns(t);
	t.columns.push_back(Column(SPAWN_COUNT, "Spawn count"));
	t.columns.push_back(Column(UTILITY, "Farming value (spawn * chance * (max+min)/2 / eliteFactor)"));
	Loots::ItemPair res = loots.findItem(a.entry);
	for(; res.first != res.second; ++res.first) {
		const Loot& loot(*res.first->second);
		Npcs::LootIdPair nres = gNpcs.findLootId(loot.entry);
		for(; nres.first != nres.second; ++nres.first) {
			const Npc& npc(*nres.first->second);
			Row r;
			npcRow(r, npc);
			lootRow(r, loot);
			r[SPAWN_COUNT] = toString(npc.spawnCount);
			r[UTILITY] = toString(loot.chance * npc.spawnCount * (loot.minCountOrRef + loot.maxCount) / 200.0);
			t.array.push_back(r);
		}
	}
	t.count = t.array.size();
	return &t;
}

static Tab* droppedBy(const Item& a) {
	return npcLoot(a, gCreatureLoots, "droppedBy", "Dropped by");
}
static Tab* pickpocketedFrom(const Item& a) {
	return npcLoot(a, gPickpocketingLoots, "pickpocketedFrom", "Pickpocketed from");
}
static Tab* skinnedFrom(const Item& a) {
	return npcLoot(a, gSkinningLoots, "skinnedFrom", "Skinned from");
}

static Tab* containedInObject(const Item& a) {
	tabTableChtml& t = *new tabTableChtml();
	t.id = "containedInObject";
	t.title = "Contained in object";
	t.columns.push_back(Column(NAME, "Name", ENTRY, "object"));
	lootColumns(t);
	t.columns.push_back(Column(SPAWN_COUNT, "Spawn count"));
	t.columns.push_back(Column(UTILITY, "Farming value (spawn * chance * (max+min)/2 / eliteFactor)"));
	Loots::ItemPair res = gGameobjectLoots.findItem(a.entry);
	for(; res.first != res.second; ++res.first) {
		const Loot& loot(*res.first->second);
		Objects::LootPair nres = gObjects.findLoot(loot.entry);
		for(; nres.first != nres.second; ++nres.first) {
			const Object& o(*nres.first->second);
			Row r;
			r[ENTRY] = toString(o.entry);
			r[NAME] = o.name;
			lootRow(r, loot);
			r[SPAWN_COUNT] = toString(o.spawnCount);
			r[UTILITY] = toString(loot.chance * o.spawnCount * (loot.minCountOrRef + loot.maxCount) / 200.0);
			t.array.push_back(r);
		}
	}
	t.count = t.array.size();
	return &t;
}

static Tab* referenceLoot(const Item& a) {
	tabTableChtml& t = *new tabTableChtml();
	t.id = "referenceLoot";
	t.title = "Reference loot";
	lootColumns(t);
	t.columns.push_back(Column(SPAWN_COUNT, "Other items count"));
	Loots::ItemPair res = gReferenceLoots.findItem(a.entry);
	for(; res.first != res.second; ++res.first) {
		const Loot& loot(*res.first->second);
		Row r;
		r[ENTRY] = toString(loot.entry);
		lootRow(r, loot);
		size_t count = 0;
		Loots::EntryPair ep = gReferenceLoots.findEntry(loot.entry);
		for(; ep.first != ep.second; ++ep.first) {
			count++;
		}
		r[SPAWN_COUNT] = toString(count);
		t.array.push_back(r);
	}
	t.count = t.array.size();
	return &t;
}

void itemColumns(tabTableChtml& t) {
	t.columns.push_back(Column(NAME, "Name", ENTRY, "item"));
	t.columns.push_back(Column(RESTRICTIONS, "Class/Race"));
	// name of and link to single vendor, or number of vendors.
	//t.columns.push_back(Column(VENDOR, "Vendor", Column::NoEscape));
	t.columns.push_back(Column(ILEVEL, "iLevel"));
	t.columns.push_back(Column(CLEVEL, "Req."));	//Required character level
	//t.columns.push_back(Column(SIDE, "Side"));	// Horde, Alliance, or none
	t.columns.push_back(Column(SLOT, "Slot"));
	t.columns.push_back(Column(TYPE, "Type"));
	t.columns.push_back(Column(SOURCE, "Source"));
	t.columns.push_back(Column(COST, "Cost", Column::NoEscape));
}

void streamAllCostHtml(std::ostream& o, const Item& i) {
	gNpcVendors.load();
	// check every vendor selling this item, to make sure costs are identical.
	NpcVendors::ItemPair nip = gNpcVendors.findItem(i.entry);
	int ec = -1;
	bool identicalCost = true;
	bool hasVendor = false;
	for(; nip.first != nip.second; ++nip.first) {
		hasVendor = true;
		const NpcVendor& nv(*nip.first->second);
		if(ec == -1)
			ec = nv.extendedCost;
		else if(ec != nv.extendedCost)
			identicalCost = false;
	}
	if(hasVendor) {
		if(identicalCost) {
			streamCostHtml(o, i, ec);
		} else {
			o << "Differs between vendors";
		}
	}
}

void streamItemClassHtml(std::ostream& o, const Item& i) {
	gItemClasses.load();
	gItemSubClasses.load();
	if((size_t)i.class_ >= gItemClasses.size()) {
		o << "Bad item.class (" << i.class_ << ")";
		return;
	}
	const ItemClass& c(gItemClasses[i.class_]);
	streamEscape(streamHtmlEscape, o, c.name);
	if((size_t)i.class_ >= gItemSubClasses.size()) {
		o << " / Bad item.class (" << i.class_ << ")";
		return;
	}
	auto isc = gItemSubClasses[i.class_];
	if(isc.size() > 1) {
		o << " / ";
		if((size_t)i.subclass >= isc.size()) {
			o << "Bad item.subclass (" << i.subclass << ")";
			return;
		}
		const ItemSubClass& sc(isc[i.subclass]);
		if(*sc.plural)
			streamEscape(streamHtmlEscape, o, sc.plural);
		else
			streamEscape(streamHtmlEscape, o, sc.name);
	}
}

void addItem(tabTableChtml& t, const Item& i) {
	Row r;
	itemRow(r, i);
	t.array.push_back(r);
}

void itemRow(Row& r, const Item& i) {
	ostringstream oss;
	r[ENTRY] = toString(i.entry);

	string classes = chrClasses(i.allowableClass);
	string races = chrRaces(i.allowableRace);
	r[RESTRICTIONS] = classes;
	if(!classes.empty() && !races.empty())
		r[RESTRICTIONS] += " / ";
	r[RESTRICTIONS] += races;

	if(i.flags & ITEM_FLAG_HEROIC)
		r[NAME] = string("Heroic ") + i.name;
	else
		r[NAME] = i.name;
	r[ILEVEL] = toString(i.itemLevel);
	r[CLEVEL] = toString(i.requiredLevel);
	r[SLOT] = itemChtml::ITEM_EQUIP(i.inventoryType);
	oss.str("");
	streamItemClassHtml(oss, i);
	r[TYPE] = oss.str();

	// check every vendor selling this item, to make sure costs are identical.
	oss.str("");
	streamAllCostHtml(oss, i);
	r[COST] = oss.str();
}

static Tab* currencyFor(const Item& a) {
	tabTableChtml& t = *new tabTableChtml();
	t.id = "currencyFor";
	t.title = "Currency for";
	itemColumns(t);
	ItemExtendedCostIndex::ItemItemPair res = ItemExtendedCostIndex::findItemItem(a.entry);
	for(; res.first != res.second; ++res.first) {
		const Item& i(*res.first->second);
		addItem(t, i);
	}
	t.count = t.array.size();
	return &t;
}

static Tab* sharesModel(const Item& a) {
	tabTableChtml& t = *new tabTableChtml();
	t.id = "sharesModel";
	//t.title = "Same model as";
	itemColumns(t);
	// insufficient; displayId controls more than model.
	t.title = "Same look as";
	Items::DisplayIdPair res = gItems.findDisplayId(a.displayId);
	for(; res.first != res.second; ++res.first) {
		const Item& i(*res.first->second);
		if(i.entry == a.entry)
			continue;
#if 1
		addItem(t, i);	// causes ConstMap[] exception
#else
		Row r;
		r[ENTRY] = toString(i.entry);
		r[NAME] = i.name;
		t.array.push_back(r);
#endif
	}
	t.count = t.array.size();
	return &t;
}


#define ITEM_RESISTANCES(m)\
	m(holy)\
	m(fire)\
	m(nature)\
	m(frost)\
	m(shadow)\
	m(arcane)\

#define ENUM_RESISTANCE(id) {&Item::id##_res, #id},

const itemChtml::Resistance itemChtml::mResistances[] = {
	ITEM_RESISTANCES(ENUM_RESISTANCE)
};
const int itemChtml::mnResistances = sizeof(itemChtml::mResistances) / sizeof(itemChtml::Resistance);

const char* itemChtml::ITEM_EQUIP(int id) {
	switch(id) {
	case 0: return "";
	case 1: return "Head";
	case 2: return "Neck";
	case 3: return "Shoulder";
	case 4: return "Shirt";
	case 5: return "Chest";
	case 6: return "Waist";
	case 7: return "Legs";
	case 8: return "Feet";
	case 9: return "Wrists";
	case 10: return "Hands";
	case 11: return "Finger";
	case 12: return "Trinket";
	case 13: return "Weapon";
	case 14: return "Shield";
	case 15: return "Ranged";
	case 16: return "Back";
	case 17: return "Two-Hand";
	case 18: return "Bag";
	case 19: return "Tabard";
	case 20: return "Robe";
	case 21: return "Main hand";
	case 22: return "Off hand";
	case 23: return "Holdable (Tome)";
	case 24: return "Ammo";
	case 25: return "Thrown";
	case 26: return "Ranged (right-hand)";
	case 27: return "Quiver";
	case 28: return "Relic";
	default: return "bad ITEM_EQUIP";
	}
}
const char* itemChtml::ITEM_BONDING(int id) {
	switch(id) {
	case 1: return "Bind on Pickup";
	case 2: return "Bind on Equip";
	case 3: return "Bind on Use";
	case 4: return "Quest item";
	case 5: return "Quest Item1";
	default: return "bad ITEM_BONDING";
	}
}
const char* itemChtml::ITEM_MATERIAL(int id) {
	switch(id) {
	case -1: return "Consumable";
	case 0: return "Not Defined";
	case 1: return "Metal";
	case 2: return "Wood";
	case 3: return "Liquid";
	case 4: return "Jewelry";
	case 5: return "Chain";
	case 6: return "Plate";
	case 7: return "Cloth";
	case 8: return "Leather";
	default: return "bad ITEM_MATERIAL";
	}
}
const char* itemChtml::ITEM_DAMAGE_TYPE(int id) {
	switch(id) {
	case 0: return "Physical";
	case 1: return "Holy";
	case 2: return "Fire";
	case 3: return "Nature";
	case 4: return "Frost";
	case 5: return "Shadow";
	case 6: return "Arcane";
	default: return "bad ITEM_DAMAGE_TYPE";
	}
}
const char* itemChtml::ITEM_STAT(int id) {
	switch(id) {
	case 0: return "Mana";
	case 1: return "Health";
	case 3: return "Agility";
	case 4: return "Strength";
	case 5: return "Intellect";
	case 6: return "Spirit";
	case 7: return "Stamina";
	case 12: return "Defense rating";
	case 13: return "Dodge rating";
	case 14: return "Parry rating";
	case 15: return "Block rating";
	case 16: return "Hit melee rating";
	case 17: return "Hit ranged rating";
	case 18: return "Hit spell rating";
	case 19: return "Crit melee rating";
	case 20: return "Crit ranged rating";
	case 21: return "Crit spell rating";
	case 22: return "Hit taken melee rating";
	case 23: return "Hit taken ranged rating";
	case 24: return "Hit taken spell rating";
	case 25: return "Crit taken melee rating";
	case 26: return "Crit taken ranged rating";
	case 27: return "Crit taken spell rating";
	case 28: return "Haste melee rating";
	case 29: return "Haste ranged rating";
	case 30: return "Haste spell rating";
	case 31: return "Hit rating";
	case 32: return "Crit rating";
	case 33: return "Hit taken rating";
	case 34: return "Crit taken rating";
	case 35: return "Resilience rating";
	case 36: return "Haste rating";
	case 37: return "Expertise rating";
	case 38: return "Attack power";
	case 39: return "Ranged attack power";
	case 40: return "Feral attack power (not used as of 3.3)";
	case 41: return "Spell healing done";
	case 42: return "Spell damage done";
	case 43: return "Mana regeneration";
	case 44: return "Armor penetration rating";
	case 45: return "Spell power";
	case 46: return "Health regen";
	case 47: return "Spell penetration";
	case 48: return "Block value";
	default: return "bad ITEM_STAT";
	}
}
const char* itemChtml::SPELLTRIGGER(int id) {
	switch(id) {
	case 0: return "Use";
	case 1: return "Equip";
	case 2: return "Chance on Hit";
	case 4: return "Soulstone";
	case 5: return "Use with no delay";
	case 6: return "Learn";
	default: return "bad SPELLTRIGGER";
	}
}

const itemChtml::Flag itemChtml::ITEM_FLAGS[] = {
	{1, "Soulbound"},
	{2, "Conjured"},
	{4, "Lootable"},
	{8, "Heroic"},
	{16, "Deprecated"},
	{32, "Totem"},
	{64, "Activatable"},
	{256, "Wrapper"},
	{1024, "Gift"},
	{2048, "Party loot"},
	{8192, "Charter (Arena or Guild)"},
	{32768, "PvP reward"},
	{524288, "Unique-equipped"},
	{4194304, "Throwable"},
	{8388608, "Special Use"},
	//{134221824, "Bind to Account"},
	{268435456, "Enchanting scroll"},
	{536870912, "Millable"},
	{(int)2147483648u, "Bind on Pickup tradeable"},
};
const int itemChtml::nITEM_FLAGS = sizeof(ITEM_FLAGS) / sizeof(itemChtml::Flag);

const itemChtml::Flag itemChtml::ITEM_BAG_FAMILY[] = {
	{1, "Arrows"},
	{2, "Bullets"},
	{4, "Soul Shards"},
	{8, "Leatherworking Supplies"},
	{16, "Inscription Supplies"},
	{32, "Herbs"},
	{64, "Enchanting Supplies"},
	{128, "Engineering Supplies"},
	{256, "Keys"},
	{512, "Gems"},
	{1024, "Mining Supplies"},
	{2048, "Soulbound Equipment"},
	{4096, "Vanity Pets"},
	{8192, "Currency Tokens"},
	{16384, "Quest Items"},
};
const int itemChtml::nITEM_BAG_FAMILY = sizeof(ITEM_BAG_FAMILY) / sizeof(itemChtml::Flag);

const itemChtml::Quality& itemChtml::ITEM_QUALITY(int id) {
	switch(id) {
	case 0: { static const Quality q = { "9d9d9d", "Poor" }; return q; }
	case 1: { static const Quality q = { "ffffff", "Common" }; return q; }
	case 2: { static const Quality q = { "1eff00", "Uncommon" }; return q; }
	case 3: { static const Quality q = { "0080ff", "Rare" }; return q; }
	case 4: { static const Quality q = { "a335ee", "Epic" }; return q; }
	case 5: { static const Quality q = { "ff8000", "Legendary" }; return q; }
	case 6: { static const Quality q = { "ff0000", "Artifact" }; return q; }
	case 7: { static const Quality q = { "e6cc80", "Bind to Account" }; return q; }
	default: throw Exception("bad ITEM_QUALITY");
	}
}
