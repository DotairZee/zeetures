// a{sex}{stage}k.att, b{sex}{stage}k.att: art/golden_desert_heads.py. Seat
// the Golden Desert norn's head (C2 norn slot k) the way Hebe's sits (slot
// h), for sexes 0 and 4 and life stages 0, 1, 3 and 5 (16 files).
//
// 1. Body: each body's neck joint (the first point of b??k.att) is set to
//    Hebe's for that frame.
// 2. Head: each head picture (right-facing 0-3, left-facing 4-7, front 8)
//    is placed by its face: the skin-colored area of the Golden Desert
//    face is matched against Hebe's in the same picture (the offset with
//    the most overlap), and the neck point is set where Hebe's is relative
//    to that face. The back view (9) moves up or down with the front view.

#include "MadeArt.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <map>
#include <stdexcept>

namespace MadeArt {

namespace {

using Rows = std::vector<std::vector<long>>;

// read_att: the numbers of each non-blank line (Python's text mode: \r\n,
// \r and \n all end a line)
Rows parseAtt(const std::string& text) {
	Rows rows;
	std::vector<std::string> lines;
	std::string cur;
	for (size_t i = 0; i < text.size(); i++) {
		char c = text[i];
		if (c == '\r' || c == '\n') {
			lines.push_back(cur);
			cur.clear();
			if (c == '\r' && i + 1 < text.size() && text[i + 1] == '\n')
				i++;
		} else
			cur += c;
	}
	if (!cur.empty())
		lines.push_back(cur);
	for (auto& l : lines) {
		std::vector<long> r;
		size_t i = 0;
		while (i < l.size()) {
			while (i < l.size() && std::isspace((unsigned char)l[i]))
				i++;
			if (i >= l.size())
				break;
			size_t j = i;
			while (j < l.size() && !std::isspace((unsigned char)l[j]))
				j++;
			std::string tok = l.substr(i, j - i);
			char* end = nullptr;
			long v = std::strtol(tok.c_str(), &end, 10);
			if (!end || *end)
				throw std::runtime_error("not a number in body data: " + tok);
			r.push_back(v);
			i = j;
		}
		if (!r.empty())
			rows.push_back(r);
	}
	return rows;
}

Rows readAtt(const std::string& name) {
	std::string text = gameFile("Body Data", name + ".att");
	if (text.empty())
		throw std::runtime_error("the game has no " + name + ".att");
	return parseAtt(text);
}

// write_att: keep the game's layout: a trailing space after each number if
// its first line had one, CRLF
std::string writeAtt(const Rows& rows, const std::string& like) {
	std::string game = gameFile("Body Data", like + ".att");
	size_t e = game.find("\r\n");
	std::string first = e == std::string::npos ? game : game.substr(0, e);
	bool trailing = !first.empty() && first.back() == ' ';
	std::string out;
	for (auto& r : rows) {
		for (size_t i = 0; i < r.size(); i++) {
			if (i)
				out += ' ';
			out += std::to_string(r[i]);
		}
		if (trailing)
			out += ' ';
		out += "\r\n";
	}
	return out;
}

// a w x h mask, rows top to bottom
struct Mask {
	int w = 0, h = 0;
	std::vector<uint8_t> m;
	uint8_t operator()(int x, int y) const { return m[size_t(y) * w + x]; }
};

// skin(): the face's colors (RGB as art/s16.py read; transparent is black)
Mask skin(const Rgba& im) {
	Mask k;
	k.w = im.w;
	k.h = im.h;
	k.m.resize(size_t(im.w) * im.h);
	for (int y = 0; y < im.h; y++)
		for (int x = 0; x < im.w; x++) {
			const uint8_t* p = im.at(x, y);
			int r = p[0], g = p[1], b = p[2];
			k.m[size_t(y) * im.w + x] = (r > 170) && (g > 110) && (b > 90) && (r >= g) && (g >= b - 10);
		}
	return k;
}

long long count(const Mask& k) {
	long long n = 0;
	for (auto v : k.m)
		n += v;
	return n;
}

// face_offset(): (dx, dy) putting Golden Desert's face mask mk best onto
// Hebe's mh; the first best (dy, then dx, from -15) wins
void faceOffset(const Mask& mk, const Mask& mh, int& bdx, int& bdy) {
	const int H = mh.h, W = mh.w, h = mk.h, w = mk.w;
	const long long sh = count(mh), sk = count(mk);
	bdx = 0;
	bdy = 0;
	double best = -1.0;
	for (int dy = -15; dy < 16; dy++)
		for (int dx = -15; dx < 16; dx++) {
			int y0 = std::max(0, dy), x0 = std::max(0, dx);
			int y1 = std::min(H, h + dy), x1 = std::min(W, w + dx);
			if (y1 <= y0 || x1 <= x0)
				continue;
			long long inter = 0;
			for (int y = y0; y < y1; y++)
				for (int x = x0; x < x1; x++)
					inter += mh(x, y) & mk(x - dx, y - dy);
			// 0 / 0 is NaN, as numpy's, and never the best
			double s = double(inter) / double(sh + sk - inter);
			if (s > best) {
				bdx = dx;
				bdy = dy;
				best = s;
			}
		}
}

struct Fit {
	std::string a, b; // the files' text
};

// fit(): both files of one sex and stage
Fit fit(const std::string& sex, const std::string& stage) {
	std::string k = sex + stage + "k", h = sex + stage + "h";
	Rows bk = readAtt("b" + k), bh = readAtt("b" + h), ak = readAtt("a" + k), ah = readAtt("a" + h);
	auto need = [](const Rows& r, size_t n) {
		if (r.size() < n)
			throw std::runtime_error("body data too short");
		for (size_t i = 0; i < n; i++)
			if (r[i].size() < 2)
				throw std::runtime_error("body data line too short");
	};
	need(bk, 10);
	need(bh, 10);
	need(ak, 10);
	need(ah, 9);
	for (int i = 0; i < 10; i++) {
		bk[i][0] = bh[i][0];
		bk[i][1] = bh[i][1];
	}
	auto heads_k = gameSprite("a" + k), heads_h = gameSprite("a" + h);
	if (heads_k.size() < 9 || heads_h.size() < 9)
		throw std::runtime_error("head sprite missing or short");
	long move8y = 0;
	for (int j = 0; j < 9; j++) {
		int dx, dy;
		faceOffset(skin(heads_k[j]), skin(heads_h[j]), dx, dy);
		long nx = ah[j][0] - dx, ny = ah[j][1] - dy;
		if (j == 8)
			move8y = ny - ak[j][1];
		ak[j][0] = nx;
		ak[j][1] = ny;
	}
	ak[9][1] += move8y; // the back view with the front
	return Fit{writeAtt(ak, "a" + k), writeAtt(bk, "b" + k)};
}

// the a and b files are made together; once per sex and stage
const Fit& fitted(const std::string& sex, const std::string& stage) {
	static std::map<std::string, Fit> cache;
	std::string key = sex + stage;
	auto it = cache.find(key);
	if (it != cache.end())
		return it->second;
	return cache[key] = fit(sex, stage); // throws if it can't be made
}

struct RegisterAll {
	RegisterAll() {
		for (const char* sex : {"0", "4"})
			for (const char* stage : {"0", "1", "3", "5"}) {
				std::string s = sex, t = stage;
				RegisterBodyData("a" + s + t + "k.att", [s, t] { return fitted(s, t).a; });
				RegisterBodyData("b" + s + t + "k.att", [s, t] { return fitted(s, t).b; });
			}
	}
};

RegisterAll registerAll;

} // namespace

} // namespace MadeArt
