
# todo: prepare statements in advance.
stm = TDB::C.prepare('select entry, zoneOrSort, minlevel, maxlevel, questlevel'+
	', requiredRaces, prevQuestId, nextQuestId, nextQuestInChain, title, details'+
	', objectives, offerRewardText, requestItemsText, completedText'+
	' from quest_template where entry = ?')
stm.execute(@id)
@template = stm.fetch
bPattern = /\$[bB]/
@template[:details].gsub!(bPattern, '<br>')
@template[:offerRewardText].gsub!(bPattern, '<br>') if(@template[:offerRewardText])
@template[:requestItemsText].gsub!(bPattern, '<br>') if(@template[:requestItemsText])
@template[:completedText].gsub!(bPattern, '<br>')

stm = TDB::C.prepare('select id, name'+
	' from creature_questrelation'+
	' INNER JOIN creature_template ON creature_template.entry = creature_questrelation.id'+
	' where creature_questrelation.quest = ?')
stm.execute(@id)
@givers = stm.fetch_all

stm = TDB::C.prepare('select id, name'+
	' from creature_involvedrelation'+
	' INNER JOIN creature_template ON creature_template.entry = creature_involvedrelation.id'+
	' where creature_involvedrelation.quest = ?')
stm.execute(@id)
@finishers = stm.fetch_all
