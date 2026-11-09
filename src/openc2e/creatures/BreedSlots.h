#pragma once

// Breed slots for Creatures 2 body parts. A body-part sprite or body-data
// file is named part letter, species/sex digit, life-stage digit, breed slot
// ("a04k.s16"). C2 had 26 slots per species, a-z. Here they go on after z
// as aa, ab ... az, ba, bb ... (owner, 2026-11-09), so a stem is 4 or 5
// characters. A genome's appearance gene holds the slot as a number, 0-255,
// so the last slot reachable from a C2 genome is "iv" (255).

#include <string>

inline std::string breedSlotName(int slot) {
	if (slot < 0)
		return "?";
	if (slot < 26)
		return std::string(1, (char)('a' + slot));
	int n = slot - 26;
	return std::string(1, (char)('a' + (n / 26) % 26)) + (char)('a' + n % 26);
}

// -1 if the text is not a slot name
inline int breedSlotFromName(const std::string& s) {
	auto letter = [](char c) { return (c >= 'a' && c <= 'z') ? c - 'a' : (c >= 'A' && c <= 'Z') ? c - 'A' : -1; };
	if (s.size() == 1)
		return letter(s[0]);
	if (s.size() == 2 && letter(s[0]) >= 0 && letter(s[1]) >= 0)
		return 26 + letter(s[0]) * 26 + letter(s[1]);
	return -1;
}

static constexpr int BREED_SLOTS = 256; // what a C2 genome can name
