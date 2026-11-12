#include "BirthPortrait.h"

#include "Camera.h"
#include "Engine.h"
#include "World.h"
#include "common/backend/Backend.h"
#include "creatures/Creature.h"
#include "creatures/CreatureAgent.h"
#include "creatures/SkeletalCreature.h"
#include "historyManager.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fmt/format.h>
#include <functional>
#include <vector>

namespace fs = std::filesystem;

namespace BirthPortrait {

namespace {

constexpr int OUT_W = 240, OUT_H = 180; // the album's photo size

class PortraitCamera : public Camera {
  public:
	unsigned int w = OUT_W, h = OUT_H;
	unsigned int getWidth() const override { return w; }
	unsigned int getHeight() const override { return h; }
};

// The poses: direction, then head, body, left thigh, shin, foot, right
// thigh, shin, foot, left upper arm, forearm, right upper arm, forearm, tail
// root, tail tip (angles 0-3; head 4: turned to you). Facing you, every
// part has one picture only, so the lively ones are side views with the
// face turned to you.
struct Pose {
	const char* name;
	const char* pose;
};
const std::vector<Pose> poses = {
	{"hello", "111111111111111"},
	{"wave", "241222222321111"},
	{"cheer", "241222222331133"},
	{"wave, facing left", "341222222321111"},
	{"looking up", "231222222111112"},
	{"arms out", "241222222222211"},
};

struct Pending {
	AgentRef creature;
	unsigned int due;
};
std::vector<Pending> s_pending;

// draws a's portrait in the given pose into a new 240 x 180 picture and
// saves it to file
bool draw(SkeletalCreature* sc, const Pose& p, const std::string& file) {
	Agent* a = sc;
	// framed so the creature fills about half the height: newborns are
	// drawn at twice their size, pixel for pixel
	auto memo = sc->memoPose();
	sc->poseInPlace("111111111111111"); // measured standing, facing you
	int scale = a->getHeight() * 2 <= OUT_H * 2 / 3 ? 2 : 1; // twice its size if that leaves room
	sc->poseInPlace(p.pose);
	sc->setFacialExpression(1); // happy
	sc->setEyesClosed(false);
	static PortraitCamera cam;
	cam.w = OUT_W / scale;
	cam.h = OUT_H / scale;
	auto small = get_backend()->newRenderTarget(cam.w, cam.h);
	// its feet a little above the bottom (or, if it is too tall for that,
	// centred)
	float cx = a->x + a->getWidth() / 2.0f;
	int top = (int)(a->y + a->getHeight()) + (int)cam.h / 8 - (int)cam.h;
	if (a->getHeight() > cam.h * 7 / 8)
		top = (int)(a->y + a->getHeight() / 2.0f) - (int)cam.h / 2;
	cam.moveToGlobal((int)cx - (int)cam.w / 2, top, jump);
	world.drawWorld(&cam, small.get(), true, a, true);
	sc->restorePose(memo);
	std::shared_ptr<RenderTarget> out = small;
	if (scale != 1) {
		out = get_backend()->newRenderTarget(OUT_W, OUT_H);
		out->blitRenderTarget(small.get(), Rect2f(0, 0, OUT_W, OUT_H));
	}
	return out->saveToFile(file);
}

void take(SkeletalCreature* sc) {
	if (engine.currentworlddir.empty())
		return;
	auto* ca = dynamic_cast<CreatureAgent*>(sc);
	if (!ca || !ca->getCreature())
		return;
	std::string mon = world.history->findMoniker(ca->getCreature()->getGenome());
	if (mon.empty())
		return;
	monikerData& m = world.history->getMoniker(mon);
	const Pose& p = poses[std::hash<std::string>{}(mon) % poses.size()];
	fs::path dir = engine.currentworlddir / "photos";
	std::error_code ec;
	fs::create_directories(dir, ec);
	std::string file;
	for (int n = 0; n < 1000; n++) {
		file = fmt::format("photo_{}_{}.bmp", world.worldtickcount, n);
		if (!fs::exists(dir / file, ec))
			break;
	}
	if (!draw(sc, p, (dir / file).string()))
		return;
	m.photos.insert(m.photos.begin(), "photos/" + file);
	fmt::print("* Birth portrait of {} ({}): {}\n", m.getName(), p.name, file);
}

} // namespace

void schedule(AgentRef creature) {
	if (engine.version != 2)
		return;
	s_pending.push_back({creature, world.worldtickcount + 10});
}

void tick() {
	// developer aid: OPENC2E_TEST_PORTRAITS=<folder> draws every pose of
	// every creature there at tick 30 (<moniker>_<pose number>.bmp)
	static const char* test = std::getenv("OPENC2E_TEST_PORTRAITS");
	if (test && world.worldtickcount == 30) {
		for (auto& a : world.agents)
			if (auto* sc = dynamic_cast<SkeletalCreature*>(a.get())) {
				auto* ca = dynamic_cast<CreatureAgent*>(sc);
				std::string mon = ca && ca->getCreature() ? world.history->findMoniker(ca->getCreature()->getGenome()) : "x";
				// (OPENC2E_TEST_PORTRAIT_POSES="pose;pose;..." tries others)
				std::vector<Pose> list = poses;
				std::vector<std::string> keep;
				if (const char* tp = std::getenv("OPENC2E_TEST_PORTRAIT_POSES")) {
					list.clear();
					std::string all = tp;
					for (size_t at = 0; at <= all.size();) {
						size_t e = all.find(';', at);
						if (e == std::string::npos)
							e = all.size();
						keep.push_back(all.substr(at, e - at));
						at = e + 1;
					}
					for (auto& k : keep)
						list.push_back({k.c_str(), k.c_str()});
				}
				for (size_t i = 0; i < list.size(); i++)
					draw(sc, list[i], fmt::format("{}/{}_{}.bmp", test, mon, i));
			}
		fmt::print("* TEST PORTRAITS written to {}\n", test);
	}
	for (size_t i = 0; i < s_pending.size();) {
		if (world.worldtickcount < s_pending[i].due) {
			i++;
			continue;
		}
		if (auto* sc = dynamic_cast<SkeletalCreature*>(s_pending[i].creature.get()))
			if (!sc->isDying() && !sc->carriedby)
				take(sc);
		s_pending.erase(s_pending.begin() + i);
	}
}

} // namespace BirthPortrait
