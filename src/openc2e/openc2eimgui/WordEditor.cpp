// Preferences > Words: the words creatures learn and the teaching computers
// show (WordLists; owner, 2026-11-23, after a player asked to edit the words
// on the computers). Per world, or one list for every world; filled from a
// preset (the game's languages, Nornish, Zeetures's draft translations) or
// another world's.

#include "WordEditor.h"

#include "Engine.h"
#include "UiFont.h"
#include "WordLists.h"

#include <fmt/format.h>
#include <imgui.h>
#include <optional>
#include <vector>

namespace Openc2eImgui {

namespace {

std::optional<std::vector<std::string>> s_undo; // before the last preset
std::string s_undoName;

void choose(const WordLists::Preset& p) {
	s_undo = engine.wordlist;
	s_undoName = p.name;
	WordLists::setWords(p.words);
}

// a slot's row: what it means, and its word
void row(size_t slot) {
	auto& en = WordLists::english();
	std::string meaning = slot < en.size() ? en[slot] : std::string();
	if (meaning.empty())
		return;
	ImGui::TableNextRow();
	ImGui::TableSetColumnIndex(0);
	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted(meaning.c_str());
	ImGui::TableSetColumnIndex(1);
	std::string word = slot < engine.wordlist.size() ? engine.wordlist[slot] : std::string();
	while (!word.empty() && word.back() == ' ')
		word.pop_back();
	UiFontNeed(word);
	char buf[96];
	snprintf(buf, sizeof(buf), "%s", word.c_str());
	ImGui::SetNextItemWidth(-FLT_MIN);
	ImGui::PushID((int)slot);
	if (ImGui::InputText("##word", buf, sizeof(buf)))
		UiFontNeed(buf);
	if (ImGui::IsItemDeactivatedAfterEdit())
		WordLists::setWord(slot, buf);
	ImGui::PopID();
	ImGui::TableSetColumnIndex(2);
	// a right-to-left word (Hebrew, Arabic) is edited in typing order; as
	// it is drawn, beside it
	std::string shown = UiShown(word);
	if (shown != word) {
		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted(shown.c_str());
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("As creatures and the computers show it");
		ImGui::SameLine();
	}
	int other = WordLists::clash(slot, word);
	if (other >= 0) {
		ImGui::AlignTextToFramePadding();
		ImGui::TextColored(ImVec4(0.95f, 0.55f, 0.3f, 1), "also \"%s\"", other < (int)en.size() ? en[other].c_str() : "?");
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Two meanings share this word: when you type it, creatures take it as both.");
	}
}

} // namespace

void DrawWordEditor() {
	if (engine.version != 2 || WordLists::english().empty()) {
		ImGui::TextWrapped("The word list comes from Creatures 2's own program (Creatures2.exe), which was not found.");
		return;
	}
	ImGui::TextWrapped("The words creatures learn and say, and the teaching computers show. "
					   "Creatures keep what they have learned when a word changes, and still understand the game's English words when you type them.");
	ImGui::Spacing();

	bool all = WordLists::allWorlds();
	if (ImGui::Checkbox("Use these words in all worlds", &all))
		WordLists::setAllWorlds(all);
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Ticked, one list for every world. Unticked, each world keeps its own.");
	ImGui::TextDisabled("%s", all ? "Changing the words of every world" : fmt::format("Changing the words of {}", engine.currentworld).c_str());

	// fill it from a preset or another world
	ImGui::SetNextItemWidth(200);
	if (ImGui::BeginCombo("##load", "Load words from...")) {
		auto& presets = WordLists::presets();
		ImGui::SeparatorText("Languages");
		for (auto& p : presets)
			if (!p.draft) {
				UiFontNeed(p.name);
				if (ImGui::Selectable(p.name.c_str()))
					choose(p);
			}
		ImGui::SeparatorText("Draft translations");
		for (auto& p : presets)
			if (p.draft) {
				std::string shown = UiShown(p.name);
				if (ImGui::Selectable(shown.c_str()))
					choose(p);
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("Translated for Zeetures, not yet checked by a native speaker: corrections welcome");
			}
		auto worlds = WordLists::worldPresets();
		if (!worlds.empty()) {
			ImGui::SeparatorText("Other worlds");
			for (auto& p : worlds)
				if (ImGui::Selectable(p.name.c_str()))
					choose(p);
		}
		ImGui::EndCombo();
	}
	ImGui::SameLine();
	if (ImGui::Button("The game's own")) {
		s_undo = engine.wordlist;
		s_undoName = "the game's own";
		WordLists::setWords(WordLists::defaultWords());
	}
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Creatures 2's own words, in your system's language");
	if (s_undo) {
		ImGui::SameLine();
		if (ImGui::Button("Undo")) {
			WordLists::setWords(*s_undo);
			s_undo.reset();
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Back to the words before %s", s_undoName.c_str());
	}
	ImGui::Spacing();

	if (ImGui::BeginTable("words", 3, ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH, ImVec2(0, ImGui::GetContentRegionAvail().y))) {
		ImGui::TableSetupScrollFreeze(0, 1);
		ImGui::TableSetupColumn("Meaning", ImGuiTableColumnFlags_WidthFixed, 100);
		ImGui::TableSetupColumn("Word", ImGuiTableColumnFlags_WidthFixed, 150);
		ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableHeadersRow();
		struct Group {
			const char* name;
			size_t from, to;
		};
		static const Group groups[] = {{"What to do", 0, 13}, {"Things", 16, 55}, {"Feelings", 56, 72}, {"Others", 73, 81}};
		for (auto& g : groups) {
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::TextDisabled("%s", g.name);
			for (size_t s = g.from; s <= g.to; s++)
				row(s);
		}
		ImGui::EndTable();
	}
}

} // namespace Openc2eImgui
