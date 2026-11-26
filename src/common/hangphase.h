#pragma once

#include <atomic>
#include <cstdio>

// What the main loop is doing, for hang reports (common/backtrace.h; 2026-11-26:
// a Windows player's game froze "white" now and then, with nothing in its
// log). A short fixed label, and while agents are being ticked or their
// scripts fired, the classifier and UNID of the one in hand, so a report
// names the object or creature whose script or step never finished. Plain
// relaxed atomics: cheap enough to set for every agent every tick.
namespace hangphase {

inline std::atomic<const char*> what{"starting"};
inline std::atomic<int> family{-1}, genus{0}, species{0}, unid{0}, event{-1};

inline void set(const char* label) {
	what.store(label, std::memory_order_relaxed);
}

inline void setAgent(int f, int g, int s, int u, int ev = -1) {
	family.store(f, std::memory_order_relaxed);
	genus.store(g, std::memory_order_relaxed);
	species.store(s, std::memory_order_relaxed);
	unid.store(u, std::memory_order_relaxed);
	event.store(ev, std::memory_order_relaxed);
}

inline void clearAgent() {
	family.store(-1, std::memory_order_relaxed);
}

// one line describing the phase, into buf (no allocation: safe in the
// Linux signal handler)
inline void describe(char* buf, size_t size) {
	const char* w = what.load(std::memory_order_relaxed);
	int f = family.load(std::memory_order_relaxed);
	if (f < 0)
		snprintf(buf, size, "* HANG: the main loop was in: %s\n", w ? w : "?");
	else {
		int ev = event.load(std::memory_order_relaxed);
		if (ev >= 0)
			snprintf(buf, size, "* HANG: the main loop was in: %s, agent %d %d %d (unid %d), script %d\n", w ? w : "?", f,
				genus.load(std::memory_order_relaxed), species.load(std::memory_order_relaxed), unid.load(std::memory_order_relaxed), ev);
		else
			snprintf(buf, size, "* HANG: the main loop was in: %s, agent %d %d %d (unid %d)\n", w ? w : "?", f,
				genus.load(std::memory_order_relaxed), species.load(std::memory_order_relaxed), unid.load(std::memory_order_relaxed));
	}
}

} // namespace hangphase
