#pragma once

// Breed slots for Creatures 2 body parts. A body-part sprite or body-data
// file is named part letter, species/sex digit, life-stage digit, breed slot
// ("a04k.s16"). C2 had 26 slots per species, a-z. Here they go on after z
// as aa, ab ... zz, then aaa, aab ... (owner, 2026-11-09), so a stem is 4 to
// 6 characters.
//
// A genome's appearance gene names a body part's breed in two bytes: the
// slot (C2: 0-255, so the last slot a C2 genome could name was "iv") and the
// species (0-3). The species byte's upper six bits were always 0; they now
// carry the slot's upper bits (owner, 2026-11-12: "I want that fixed"), so a
// species has 256 x 64 = 16384 slots, and every C2 genome reads as before. A
// creature using a slot past "iv" cannot be taken back to the original game.

#include <string>

inline std::string breedSlotName(int slot) {
	if (slot < 0)
		return "?";
	if (slot < 26)
		return std::string(1, (char)('a' + slot));
	if (slot < 26 + 26 * 26) {
		int n = slot - 26;
		return std::string(1, (char)('a' + (n / 26) % 26)) + (char)('a' + n % 26);
	}
	int n = slot - 26 - 26 * 26;
	return std::string(1, (char)('a' + (n / 676) % 26)) + (char)('a' + (n / 26) % 26) + (char)('a' + n % 26);
}

// -1 if the text is not a slot name
inline int breedSlotFromName(const std::string& s) {
	auto letter = [](char c) { return (c >= 'a' && c <= 'z') ? c - 'a' : (c >= 'A' && c <= 'Z') ? c - 'A' : -1; };
	if (s.size() == 1)
		return letter(s[0]);
	if (s.size() == 2 && letter(s[0]) >= 0 && letter(s[1]) >= 0)
		return 26 + letter(s[0]) * 26 + letter(s[1]);
	if (s.size() == 3 && letter(s[0]) >= 0 && letter(s[1]) >= 0 && letter(s[2]) >= 0)
		return 26 + 26 * 26 + letter(s[0]) * 676 + letter(s[1]) * 26 + letter(s[2]);
	return -1;
}

// an appearance gene's species and slot, from its species and slot bytes
inline int geneBreedSpecies(int speciesByte) {
	return speciesByte & 3;
}
inline int geneBreedSlot(int speciesByte, int slotByte) {
	return (slotByte & 255) | ((speciesByte >> 2) << 8);
}
// ... and back
template <typename B>
inline void setGeneBreed(B& speciesByte, B& slotByte, int species, int slot) {
	slotByte = (B)(slot & 255);
	speciesByte = (B)((species & 3) | (((slot >> 8) & 63) << 2));
}

static constexpr int BREED_SLOTS = 16384; // what a genome can name (see above)
