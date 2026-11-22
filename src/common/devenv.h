#pragma once

#include <cstdlib>

// The developer switches (OPENC2E_TEST_*, _DUMP_*, _WATCH_*, _NO_* and the
// rest): environment variables read while developing and testing. Players'
// builds are made without them (cmake -DOPENC2E_DEV_HOOKS=OFF), so what they
// guard is left out of the program. The few settings anyone may use
// (OPENC2E_MIND_LOG, OPENC2E_WINDOW_*, ...) are read with getenv as usual.
#ifdef OPENC2E_DEV_HOOKS
inline const char* devenv(const char* name) {
	return std::getenv(name);
}
#else
constexpr const char* devenv(const char*) {
	return nullptr;
}
#endif
