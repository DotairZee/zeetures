// The toolbar's Help window (openc2e addition, owner 2026-11-11): how the
// hand and the toolbars work, and what this version adds to the game. Our
// own text.

#include "HelpWindow.h"

#include <imgui.h>

namespace Openc2eImgui {

static bool s_help_open = false;

void SetHelpOpen(bool open) {
	s_help_open = open;
}

static void Section(const char* title, const char* text) {
	if (ImGui::CollapsingHeader(title)) {
		ImGui::PushTextWrapPos(0);
		ImGui::TextUnformatted(text);
		ImGui::PopTextWrapPos();
		ImGui::Spacing();
	}
}

void DrawHelpWindow() {
	if (!s_help_open)
		return;
	ImGui::SetNextWindowSize(ImVec2(560, 460), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("Help", &s_help_open)) {
		ImGui::End();
		return;
	}
	Section("The hand",
		"Left-click: use things (press a button, start a lesson on a teaching machine, tickle or slap a creature with the teach hand).\n"
		"Right-click: pick something up, and right-click again to put it down.\n"
		"Type while a creature is near, then press Return: the hand speaks, and creatures that hear it learn the words. Ctrl+S says it again.\n"
		"Ctrl+wheel zooms; Ctrl+0 is actual size.");
	Section("Creatures' words",
		"Creatures are born knowing no words. They speak their own tongue (Nornish, and the grendel and ettin dialects) until they learn yours: "
		"from the teaching machines and laptops, from the hand's words, from each other, or all at once from the Stone of Knowledge (kissed by a norn). "
		"Being taught, a creature repeats the word, in its tongue at first and then the word itself (babies only in their tongue).\n"
		"Speak to creatures, or tickle one, and they look at the hand; say one's name, and only it is meant. Say the name of a thing they know the "
		"word for, and they look at the thing. Verbs they know are for them to consider: stay, push, pull, stop, come, run, get, drop, rest, left, "
		"right, eat and hit, with a thing's name or without (\"push toy\"). The older they are the more they follow, and the good-tempered follow more. "
		"Tickle one that does as it was told and it will again; unpraised, it heeds you less and less.\n"
		"View > Speech Bubbles hides their bubbles; "
		"Speech History lists what was said lately.");
	Section("Temperament and relationships",
		"Every creature is born with its own temperament: some of each of boldness, curiosity, energy, sociability, temper, reactivity and steadiness "
		"(Owner's Kit, Temperament). Creatures remember one another: help and harm, time together and fear shape how each feels about each other "
		"(Owner's Kit, Relationships), and that changes whom they approach, avoid and mate with.");
	Section("Breeding",
		"Grown creatures of the other sex may mate; a pregnant female lays an egg that hatches into a child mixing both parents' genes. "
		"Close kin do not breed while another partner is alive. Scents (pheromones.txt in your openc2e folder) help mates find each other. "
		"The Breeder's Kit lists breeds and lays an egg of a breed and sex you choose.");
	Section("The kits",
		"Tools menu or the applet toolbar; a kit minimises to a chip on the status bar.\n"
		"Hatchery: hatch eggs.\n"
		"Owner's Kit: a creature's registration, photos, family, temperament, relationships and life story.\n"
		"Health Kit: its fitness, drives and medicine. Graveyard Kit: the stones of those who have died.\n"
		"Breeder's and Science Kits: breeding, breeds, body, genes and chemistry.\n"
		"Mind Kit: what it feels, is weighing, has learned and remembers.\n"
		"Observation Kit: every creature at a glance, with alarms.\n"
		"Agent Injector: add objects. Chronicles Kit: the world's history and statistics.\n"
		"Ecology Kit: the world's rooms (light, warmth, air, soil), rooms to watch, and a census.");
	Section("Saving",
		"The world is saved when you quit or switch worlds (or File > Save World): creatures with their minds, words, bonds and stories, and eggs. "
		"Other objects start as in the world's own file each time.");
	ImGui::End();
}

} // namespace Openc2eImgui
