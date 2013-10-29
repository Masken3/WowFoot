#define __STDC_FORMAT_MACROS
#include "spells.h"
#include "Spell.index.h"
#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include "dbcSkillLine.h"
#include "db_npc_trainer.h"
#include "db_creature_template.h"
#include "skillShared.h"
#include "win32.h"

void spellsChtml::httpArgument(const char* key, const char* value) {
	if(*value == 0)
		return;
	printf("p %s: %s\n", key, value);
}

struct SpellActiveIconTest {
	static bool have(const Spell& s) {
		return s.activeIconID != 0;
	}
};

#if 0
class SpellActiveIconItrPair : public ItrPair<Quest> {
private:
	mutable Spells::citr mItr;
	const Spells::citr mEnd;
public:
	SpellActiveIconItrPair(Spells::citr b, Spells::citr e) : mItr(b), mEnd(e) {}
	bool hasNext() const {
		while(mItr != mEnd) {
			if(mItr->second.activeIconID != 0)
				break;
			++mItr;
		}
		return mItr != mEnd;
	}
	const Spell& next() {
		const Spell& s(mItr->second);
		++mItr;
		return s;
	}
	virtual ~SpellActiveIconItrPair() {}
};
#endif

class SkillLinePair : public ItrPair<Spell> {
private:
	SkillLineAbilityIndex::SpellPair mPair;
public:
	SkillLinePair(int skillId) {
		mPair = SkillLineAbilityIndex::findSkill(skillId);
	}
	virtual bool hasNext() {
		return mPair.first != mPair.second;
	}
	virtual const Spell& next() {
		const Spell& s(gSpells[mPair.first->second->spell]);
		++mPair.first;
		return s;
	}
	virtual ~SkillLinePair() {}
};

void spellsChtml::getResponse2(const char* urlPart, DllResponseData* drd, ostream& os) {
	gNpcTrainers.load();
	gSpells.load();
	gSkillLines.load();
	gSpellIcons.load();
	SkillLineAbilityIndex::load();
	gItems.load();
	gItemDisplayInfos.load();
	SpellIndex::load();
	gNpcs.load();

	printf("urlPart: %s\n", urlPart);
	getArguments(drd);

	if(urlPart[0] == 0) {
		printf("default: list spells with 2 icons.\n");
		mPair = new SimpleItrPair<Spells, SpellActiveIconTest>(gSpells.begin(), gSpells.end());
	} else {
		// otherwise: list category spells.
		const SkillLine* sl = parseSkillId(os, urlPart);
		if(!sl) {
			mTitle = urlPart;
			drd->code = 404;
			return;
		}
		mTitle = sl->name;
		mPair = new SkillLinePair(sl->id);
	}
}

void spellsChtml::title(ostream& stream) {
	stream << mTitle;
}

spellsChtml::spellsChtml() : PageContext("Spells"), mPair(NULL) {}

spellsChtml::~spellsChtml() {
	if(mPair)
		delete mPair;
}

void spellsChtml::streamMultiItem(ostream& stream, int id, int count) {
	if(id != 0) {
		stream << "<a href=\"item="<<id<<"\" style=\"white-space:nowrap\">";
		const Item* item = gItems.find(id);
		if(item) {
			mItemQuality = &ITEM_QUALITY(item->quality);
			stream << "<span style=\"color:#"<<mItemQuality->color<<";\">";
			const ItemDisplayInfo* di = gItemDisplayInfos.find(item->displayId);
			if(di) {
				stream << "<img src=\"";
				ESCAPE_URL(getIcon(di->icon));
				stream << "\" alt=\""<<item->name<<"\" title=\""<<item->name<<"\">";
			} else {
				stream << "displayId ("<<item->displayId<<")";
			}
		} else {
			stream << "item ("<<id<<")";
		}
		stream << count;
		if(item)
			stream << "</span>";
		stream << "</a>\n";
	}
}

static void streamItemSource(ostream& stream, const Item& item) {
	stream << "<a href=\"item="<<item.entry<<"\">";
	stream << "<span style=\"color:#"<<ITEM_QUALITY(item.quality).color<<";\">";
	streamName(stream, item);
	stream << "</span>";
	stream << "</a>\n";
	// todo: determine if the thing is sold or dropped, and if so, by how many.
}

// returns trainerCount.
static int streamNpcTrainers(ostream& stream, int id, int& requiredSkillLevel, bool pass) {
	int trainerCount = 0;
	for(auto p = SpellIndex::findLearnSpell(id); p.first != p.second; ++p.first) {
		for(auto t = gNpcTrainers.findSpell(p.first->second->id); t.first != t.second; ++t.first) {
			const NpcTrainer& nt(*t.first->second);
			if(nt.reqSkill) {
				requiredSkillLevel = nt.reqSkillValue;
			}
			if(!gNpcs.find(nt.entry)) {
				int count = 0;
				for(auto n = gNpcTrainers.findSpell(-nt.entry); n.first != n.second; ++n.first) {
					const NpcTrainer& nn(*n.first->second);
					if(pass) {
						if(trainerCount + count > 0)
							stream << ", ";
						NAMELINK(gNpcs, nn.entry);
					}
					count++;
				}
				EASSERT(count > 0);
				trainerCount += count;
			} else {
				if(pass) {
					if(trainerCount > 0)
						stream << ", ";
					NAMELINK(gNpcs, nt.entry);
				}
				trainerCount++;
			}
		}
	}
	return trainerCount;
}

int spellsChtml::streamSource(ostream& stream, int id) {
	int requiredSkillLevel = -1;
	// Find spells that teach this spell, and items that use the teacher spell.
	for(auto p = SpellIndex::findLearnSpell(id); p.first != p.second; ++p.first) {
		const Spell& s(*p.first->second);
		for(auto r = gItems.findSpellId(s.id); r.first != r.second; ++r.first) {
			const Item& item(*r.first->second);
			streamItemSource(stream, item);
			if(item.requiredSkill) {
				requiredSkillLevel = item.requiredSkillRank;
			}
		}
	}
	// Find items that teach this spell directly (SPELLTRIGGER 6).
	for(auto r = gItems.findSpellId(id); r.first != r.second; ++r.first) {
		const Item& item(*r.first->second);
		for(uint i=0; i<ARRAY_SIZE(item.spellTrigger); i++) {
			if(item.spellTrigger[i] == 6) {
				streamItemSource(stream, item);
				if(item.requiredSkill) {
					requiredSkillLevel = item.requiredSkillRank;
				}
			}
		}
	}

	// todo: some spells are learned as part of starting the skill line.

	int trainerCount = streamNpcTrainers(stream, id, requiredSkillLevel, false);
	if(trainerCount > 0) {
		if(trainerCount < 5) {
			streamNpcTrainers(stream, id, requiredSkillLevel, true);
			stream << " ("<<trainerCount<<" trainers)\n";
		} else {
			stream << trainerCount<<" trainers\n";
		}
	}
	return requiredSkillLevel;
}

int spellsChtml::slaYellow(int id) {
	auto slas = SkillLineAbilityIndex::findSpell(id);
	for(; slas.first != slas.second; ++slas.first) {
		const SkillLineAbility* sla = slas.first->second;
		return sla->minValue;
	}
	return -1;
}

#if 0
<td style="padding: 0px;"><div style="width: 44px; margin: 0px auto;"><div style="float: left;" class="iconmedium"><ins style="background-image: url(&quot;http://wow.zamimg.com/images/wow/icons/medium/inv_fabric_linen_01.jpg&quot;);"></ins><del></del><a href="http://www.wowhead.com/item=2589"></a><span class="glow q1" style="position: absolute; right: 0px; bottom: 0px;"><div style="position: absolute; white-space: nowrap; left: -1px; top: -1px; color: black; z-index: 2;">2</div><div style="position: absolute; white-space: nowrap; left: -1px; top: 0px; color: black; z-index: 2;">2</div><div style="position: absolute; white-space: nowrap; left: -1px; top: 1px; color: black; z-index: 2;">2</div><div style="position: absolute; white-space: nowrap; left: 0px; top: -1px; color: black; z-index: 2;">2</div><div style="position: absolute; white-space: nowrap; left: 0px; top: 0px; z-index: 4;">2</div><div style="position: absolute; white-space: nowrap; left: 0px; top: 1px; color: black; z-index: 2;">2</div><div style="position: absolute; white-space: nowrap; left: 1px; top: -1px; color: black; z-index: 2;">2</div><div style="position: absolute; white-space: nowrap; left: 1px; top: 0px; color: black; z-index: 2;">2</div><div style="position: absolute; white-space: nowrap; left: 1px; top: 1px; color: black; z-index: 2;">2</div><span style="visibility: hidden;">2</span></span></div></div></td>
#endif
