#include "money.h"
#include "../build/wowVersion.h"

static void moneyIcon(ostream& stream, const char* name, const char* alt, int left) {
#if CONFIG_WOW_VERSION < 20000
	stream << "\n<div style=\"display:inline-block; overflow:hidden; width:16px; height:16px;\">\n"<<
		"\t<img src=\"output/Interface/MoneyFrame/UI-MoneyIcons.png\" style=\"position:absolute;"<<
		" left:"<<left<<"px;\" alt=\" "<<alt<<"\">\n"
		"</div>\n";
#else
	stream << "\n<img src=\"output/Interface/MoneyFrame/UI-"<<name<<"Icon.png\" alt=\" "<<alt<<"\">\n";
#endif
}

ostream& moneyHtml(ostream& stream, int total) {
	int copper = total % 100;
	total /= 100;
	int silver = total % 100;
	total /= 100;
	int gold = total;
	if(gold > 0) {
		stream << gold;
		moneyIcon(stream, "Gold", "gold", 0);
	}
	if(gold > 0 && silver > 0)
		stream << ' ';
	if(silver > 0) {
		stream << silver;
		moneyIcon(stream, "Silver", "silver", -16);
	}
	if(copper > 0 && silver > 0)
		stream << ' ';
	if(copper > 0) {
		stream << copper;
		moneyIcon(stream, "Copper", "copper", -32);
	}
	else
		stream << copper;
	return stream;
}
