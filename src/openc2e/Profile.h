// Developer aid (performance work, 2026-11-13): with OPENC2E_TICKRATE set,
// PROFILE_SCOPE("name") adds the time spent in the enclosing block to that
// name's total, and the totals are printed with the tick rate (Engine.cpp).
#pragma once

#include <chrono>
#include <cstdlib>
#include <map>
#include <string>

namespace Profile {
inline bool enabled() {
	static const bool on = getenv("OPENC2E_TICKRATE") != nullptr;
	return on;
}
inline std::map<std::string, double>& totals() {
	static std::map<std::string, double> t;
	return t;
}
struct Scope {
	const char* name;
	std::chrono::steady_clock::time_point start;
	bool on;
	explicit Scope(const char* n)
		: name(n), on(enabled()) {
		if (on)
			start = std::chrono::steady_clock::now();
	}
	~Scope() {
		if (on)
			totals()[name] += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
	}
};
} // namespace Profile

#define PROFILE_CAT2(a, b) a##b
#define PROFILE_CAT(a, b) PROFILE_CAT2(a, b)
#define PROFILE_SCOPE(n) Profile::Scope PROFILE_CAT(profile_scope_, __LINE__)(n)
