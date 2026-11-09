#include "partzorder.h"

#include "Agent.h"
#include "CompoundPart.h"
#include "Engine.h"

bool partzorder::operator()(const CompoundPart* s1, const CompoundPart* s2) const {
	// TODO: unsure about all of this, needs a check (but seems to work)
	if (s1->getParent() == s2->getParent()) {
		return s1->part_sequence_number > s2->part_sequence_number;
	}
	if (s1->getParent()->getZOrder() == s2->getParent()->getZOrder()) {
		return s1->getZOrder() > s2->getZOrder();
	}
	// TODO: we shouldn't be checking engine.bmprenderer for this, but it's a cheap/easy way to check for seamonkeys
	if (engine.bmprenderer)
		return (int)s1->getParent()->getZOrder() > (int)s2->getParent()->getZOrder();
	// C1/C2: an object placed below plane 0 (behind the plane-0 machines)
	// must sort behind them, not wrap round to the front (see c2PlaneKey)
	if (engine.version < 3)
		return c2PlaneKey(s1->getParent()->getZOrder()) > c2PlaneKey(s2->getParent()->getZOrder());
	else
		return s1->getParent()->getZOrder() > s2->getParent()->getZOrder();
}