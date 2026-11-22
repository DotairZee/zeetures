#include "MadeArt.h"
#include "common/devenv.h"

#include "Engine.h"
#include "PathResolver.h"
#include "common/Image.h"
#include "common/creaturesImage.h"
#include "common/case_insensitive_filesystem.h"

#include <set>
#include <SDL3/SDL_timer.h>
#include <SDL3/SDL_filesystem.h>
#include <cctype>

#include <cstring>
#include <fmt/format.h>
#include <fstream>
#include <iterator>
#include <system_error>

namespace fs = std::filesystem;

namespace MadeArt {

namespace {

std::map<std::string, Maker>& recipes() {
	static std::map<std::string, Maker> r;
	return r;
}

std::map<std::string, TextMaker>& bodyRecipes() {
	static std::map<std::string, TextMaker> r;
	return r;
}

std::string lower(std::string s) {
	for (auto& c : s)
		c = char(std::tolower((unsigned char)c));
	return s;
}

std::map<std::string, bool>& replacing() {
	static std::map<std::string, bool> r;
	return r;
}

struct S16 {
	uint32_t flags = 0;
	std::vector<uint8_t> data;
	struct Frame {
		uint32_t off;
		uint16_t w, h;
	};
	std::vector<Frame> frames;
	bool ok() const { return !frames.empty(); }
};

uint16_t le16(const uint8_t* p) {
	return uint16_t(p[0] | (p[1] << 8));
}
uint32_t le32(const uint8_t* p) {
	return uint32_t(p[0] | (p[1] << 8) | (p[2] << 16) | (uint32_t(p[3]) << 24));
}

// an uncompressed .s16 (the game's sprites and background), as art/s16.py
S16 readS16(const fs::path& path) {
	S16 s;
	if (path.empty())
		return s;
	std::ifstream in(path, std::ios::binary);
	if (!in)
		return s;
	s.data.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
	if (s.data.size() < 6)
		return s;
	s.flags = le32(&s.data[0]);
	unsigned n = le16(&s.data[4]);
	if (s.data.size() < 6 + 8 * size_t(n))
		return s;
	for (unsigned i = 0; i < n; i++) {
		const uint8_t* e = &s.data[6 + 8 * i];
		S16::Frame f{le32(e), le16(e + 4), le16(e + 6)};
		if (size_t(f.off) + size_t(f.w) * f.h * 2 > s.data.size())
			return S16();
		s.frames.push_back(f);
	}
	return s;
}

// one 16-bit pixel to R G B, as art/s16.py and art/background.py (no
// rounding up: the low bits are zero)
void rgbOf(uint32_t flags, uint16_t v, uint8_t* out) {
	if (flags & 1) { // 565
		out[0] = uint8_t(((v >> 11) & 31) << 3);
		out[1] = uint8_t(((v >> 5) & 63) << 2);
		out[2] = uint8_t((v & 31) << 3);
	} else { // 555
		out[0] = uint8_t(((v >> 10) & 31) << 3);
		out[1] = uint8_t(((v >> 5) & 31) << 3);
		out[2] = uint8_t((v & 31) << 3);
	}
}

const S16& background() {
	static S16 bg = [] {
		S16 s = readS16(findGameImageFile("Back.s16"));
		if (s.frames.size() != 58 * 16) {
			if (s.ok())
				fmt::print(stderr, "* Made art: the game's Back.s16 has {} tiles, not {}\n", s.frames.size(), 58 * 16);
			return S16();
		}
		return s;
	}();
	return bg;
}

std::map<std::string, std::shared_ptr<creaturesImage>>& made() {
	static std::map<std::string, std::shared_ptr<creaturesImage>> m;
	return m;
}

} // namespace

Rgba backgroundRegion(int x0, int y0, int x1, int y1) {
	const int TW = 144, TH = 150, DOWN = 16;
	const S16& bg = background();
	if (!bg.ok() || x1 <= x0 || y1 <= y0)
		return Rgba();
	Rgba out(x1 - x0, y1 - y0);
	for (int col = x0 / TW; col <= (x1 - 1) / TW; col++)
		for (int row = y0 / TH; row <= (y1 - 1) / TH; row++) {
			size_t index = size_t(col) * DOWN + row;
			if (index >= bg.frames.size())
				continue;
			const auto& f = bg.frames[index];
			int tx = col * TW, ty = row * TH;
			int ax0 = std::max(x0, tx), ay0 = std::max(y0, ty);
			int ax1 = std::min(x1, tx + TW), ay1 = std::min(y1, ty + TH);
			for (int y = ay0; y < ay1; y++)
				for (int x = ax0; x < ax1; x++) {
					uint16_t v = le16(&bg.data[f.off + 2 * (size_t(y - ty) * f.w + (x - tx))]);
					uint8_t* p = out.at(x - x0, y - y0);
					rgbOf(bg.flags, v, p);
					p[3] = 255;
				}
		}
	return out;
}

std::vector<Rgba> gameSprite(const std::string& name) {
	S16 s = readS16(findGameImageFile(name + ".s16"));
	std::vector<Rgba> out;
	for (auto& f : s.frames) {
		Rgba im(f.w, f.h);
		for (int y = 0; y < f.h; y++)
			for (int x = 0; x < f.w; x++) {
				uint16_t v = le16(&s.data[f.off + 2 * (size_t(y) * f.w + x)]);
				uint8_t* p = im.at(x, y);
				rgbOf(s.flags, v, p);
				p[3] = v == 0 ? 0 : 255;
			}
		out.push_back(std::move(im));
	}
	return out;
}

Register::Register(const std::string& name, Maker maker, bool replaces) {
	recipes()[name] = std::move(maker);
	replacing()[name] = replaces;
}

bool replacesGame(const std::string& name) {
	auto it = replacing().find(name);
	if (it == replacing().end() || !it->second || !canMake(name))
		return false;
	std::error_code ec;
	for (const char* ext : {".s16", ".c16", ".png"})
		if (fs::exists(getUserDataDir() / "Overrides" / "Images" / (name + ext), ec))
			return false;
	return true;
}

RegisterBodyData::RegisterBodyData(const std::string& file, TextMaker maker) {
	bodyRecipes()[lower(file)] = std::move(maker);
}

std::string gameFile(const std::string& folder, const std::string& file) {
	if (data_directories.empty())
		return {};
	std::error_code ec;
	fs::path p = case_insensitive_filesystem::canonical(data_directories.front().main / folder / file, ec);
	if (ec)
		return {};
	std::ifstream in(p, std::ios::binary);
	return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

fs::path bodyDataFile(const std::string& file) {
	if (engine.version != 2)
		return {};
	std::string key = lower(file);
	auto it = bodyRecipes().find(key);
	if (it == bodyRecipes().end())
		return {};
	std::error_code ec;
	if (fs::exists(case_insensitive_filesystem::canonical(getUserDataDir() / "Overrides" / "Body Data" / file, ec)) && !ec)
		return {};
	static std::map<std::string, fs::path> written;
	auto w = written.find(key);
	if (w != written.end())
		return w->second;
	fs::path out;
	std::string text;
	try {
		text = it->second();
	} catch (std::exception& e) {
		fmt::print(stderr, "* Made body data: {} failed: {}\n", file, e.what());
	}
	if (!text.empty()) {
		fs::path dir = getUserDataDir() / "Zeetures made" / "Body Data";
		fs::create_directories(dir, ec);
		out = dir / key;
		std::ofstream o(out, std::ios::binary);
		o << text;
		if (!o)
			out.clear();
		else
			fmt::print("* Made body data: {} from the game's own\n", key);
	}
	written[key] = out;
	return out;
}

bool canMake(const std::string& name) {
	return engine.version == 2 && recipes().count(name) && background().ok();
}

// Pictures that take long to make (the glass tube's, 2-3 s at every start) are
// kept in the user folder's "Zeetures made/Images", on the player's own
// machine like the body data, and made again when the program or the game's
// own pictures change.
static std::string keptKey() {
	static const std::string key = [] {
		std::string k = "ZMA1";
		std::error_code ec;
		auto add = [&](const fs::path& f) {
			auto size = fs::is_directory(f, ec) ? 0 : fs::file_size(f, ec);
			auto when = fs::last_write_time(f, ec);
			k += fmt::format(" {}:{}", ec ? 0 : (unsigned long long)size, ec ? 0 : (long long)when.time_since_epoch().count());
			ec.clear();
		};
		if (const char* base = SDL_GetBasePath()) {
			// (the program: whichever file it is here)
			for (const char* exe : {"openc2e", "Zeetures.exe", "../MacOS/Zeetures"})
				add(fs::path(reinterpret_cast<const char8_t*>(base)) / exe);
		}
		add(findMainDirectoryFile("Eden.sfc"));
		for (const char* dir : {"Images", "Backgrounds"})
			add(findMainDirectoryFile("Eden.sfc").parent_path() / dir);
		return k;
	}();
	return key;
}

static fs::path keptFile(const std::string& name) {
	return getUserDataDir() / "Zeetures made" / "Images" / (name + ".rgba");
}

static std::vector<Rgba> readKept(const std::string& name) {
	std::ifstream in(keptFile(name), std::ios::binary);
	std::string key;
	if (!in || !std::getline(in, key) || key != keptKey())
		return {};
	size_t n = 0;
	if (!(in >> n) || n == 0 || n > 4096)
		return {};
	in.get();
	std::vector<Rgba> out(n);
	for (auto& f : out) {
		if (!(in >> f.w >> f.h) || f.w <= 0 || f.h <= 0 || f.w > 8192 || f.h > 8192)
			return {};
		in.get();
		f.px.resize((size_t)f.w * f.h * 4);
		if (!in.read(reinterpret_cast<char*>(f.px.data()), (std::streamsize)f.px.size()))
			return {};
	}
	return out;
}

static void keep(const std::string& name, const std::vector<Rgba>& fr) {
	std::error_code ec;
	fs::create_directories(keptFile(name).parent_path(), ec);
	fs::path tmp = keptFile(name);
	tmp += ".new";
	{
		std::ofstream out(tmp, std::ios::binary);
		out << keptKey() << "\n" << fr.size() << "\n";
		for (auto& f : fr) {
			out << f.w << " " << f.h << "\n";
			out.write(reinterpret_cast<const char*>(f.px.data()), (std::streamsize)f.px.size());
		}
		if (!out.good())
			return;
	}
	fs::rename(tmp, keptFile(name), ec);
}

std::vector<Rgba> frames(const std::string& name) {
	auto it = recipes().find(name);
	if (it == recipes().end() || !background().ok())
		return {};
	if (auto kept = readKept(name); !kept.empty())
		return kept;
	try {
		uint64_t from = SDL_GetTicks();
		auto fr = it->second();
		// (several pictures may share one slow making: each is kept)
		static std::set<std::string> slowFamily;
		std::string family = name.substr(0, 3);
		if (SDL_GetTicks() - from > 100)
			slowFamily.insert(family);
		if (!fr.empty() && slowFamily.count(family))
			keep(name, fr);
		return fr;
	} catch (std::exception& e) {
		fmt::print(stderr, "* Made art: {} failed: {}\n", name, e.what());
		return {};
	}
}

std::shared_ptr<creaturesImage> make(const std::string& name) {
	if (!canMake(name))
		return nullptr;
	auto& m = made();
	auto found = m.find(name);
	if (found != m.end())
		return found->second;
	auto fr = frames(name);
	std::shared_ptr<creaturesImage> img;
	if (!fr.empty()) {
		img = std::make_shared<creaturesImage>(name);
		for (auto& f : fr) {
			Image im;
			im.width = f.w;
			im.height = f.h;
			im.format = if_rgba32;
			im.data = shared_array<uint8_t>(f.px.begin(), f.px.end());
			img->images.push_back(im);
		}
		fmt::print("* Made art: {} ({} frame{}, {}x{}) from the game's own pictures\n", name, fr.size(), fr.size() == 1 ? "" : "s", fr[0].w, fr[0].h);
	}
	m[name] = img;
	return img;
}

std::vector<std::string> names() {
	std::vector<std::string> n;
	for (auto& [k, v] : recipes())
		n.push_back(k);
	return n;
}

void dumpIfAsked() {
	const char* dir = devenv("OPENC2E_DUMP_MADE_ART");
	if (!dir)
		return;
	std::error_code ec;
	fs::create_directories(dir, ec);
	for (auto& n : names()) {
		auto fr = frames(n);
		if (fr.empty()) {
			fmt::print("* Made art dump: {} not made\n", n);
			continue;
		}
		std::ofstream out(fs::path(dir) / (n + ".rgba"), std::ios::binary);
		out << fr[0].w << " " << fr[0].h << " " << fr.size() << "\n";
		// every frame's size, for pictures whose frames differ (a60a)
		for (size_t i = 0; i < fr.size(); i++)
			out << (i ? " " : "") << fr[i].w << "x" << fr[i].h;
		out << "\n";
		for (auto& f : fr)
			out.write(reinterpret_cast<const char*>(f.px.data()), f.px.size());
		fmt::print("* Made art dump: {}\n", n);
	}
	// body data: the made text as the file itself
	for (auto& [file, maker] : bodyRecipes()) {
		std::string text;
		try {
			text = maker();
		} catch (std::exception& e) {
			fmt::print("* Made art dump: {} failed: {}\n", file, e.what());
		}
		if (text.empty()) {
			fmt::print("* Made art dump: {} not made\n", file);
			continue;
		}
		std::ofstream out(fs::path(dir) / file, std::ios::binary);
		out << text;
		fmt::print("* Made art dump: {}\n", file);
	}
}

} // namespace MadeArt
