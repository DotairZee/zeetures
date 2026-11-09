#pragma once

class CompoundPart;

#include <cstdint>

// C1/C2 planes are stored unsigned. Small negative planes (down to -65536,
// for objects placed behind the plane-0 machines) wrap round to near the
// top of the range; read those as negative. Everything else stays as it
// is, so very high planes such as the pointer's (2^31) stay on top.
inline int64_t c2PlaneKey(unsigned int z) {
	return z >= 0xFFFF0000u ? (int64_t)(int32_t)z : (int64_t)z;
}

struct partzorder {
	bool operator()(const CompoundPart* s1, const CompoundPart* s2) const;
};