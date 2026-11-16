#include "Additions.h"

#include "Engine.h"
#include "PathResolver.h"
#include "madeart/MadeArt.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <map>
#include <sstream>
#include <system_error>

namespace fs = std::filesystem;

namespace Additions {

fs::path userPath(const std::string& name) {
	if (name == "pheromones.txt" || name == "breed-names.txt")
		return getUserDataDir() / name;
	return getUserDataDir() / "Additions" / name;
}

bool builtinsApply() {
	return engine.version == 2;
}

static bool readFile(const fs::path& p, std::string& out) {
	std::error_code ec;
	if (!fs::is_regular_file(p, ec))
		return false;
	std::ifstream in(p, std::ios::binary);
	if (!in)
		return false;
	std::ostringstream ss;
	ss << in.rdbuf();
	out = ss.str();
	return true;
}

// the first of our sprites (four letters starting with z, outside comment
// lines) that a script uses and no image file is installed for, or empty
static std::string missingArtIn(const std::string& script) {
	std::istringstream in(script);
	std::string line;
	while (std::getline(in, line)) {
		size_t start = line.find_first_not_of(" \t");
		if (start == std::string::npos || line[start] == '*')
			continue;
		std::istringstream words(line);
		std::string w;
		while (words >> w) {
			if (w.size() != 4 || w[0] != 'z')
				continue;
			if (!std::all_of(w.begin(), w.end(), [](char c) { return std::islower((unsigned char)c) || std::isdigit((unsigned char)c); }))
				continue;
			if (findImageFile(w + ".s16").empty() && findImageFile(w + ".c16").empty() && findImageFile(w + ".png").empty() && !MadeArt::canMake(w))
				return w;
		}
	}
	return {};
}

static std::string builtin(const std::string& name) {
	auto d = EmbeddedAddition(name);
	return std::string(reinterpret_cast<const char*>(d.data()), d.size());
}

std::string text(const std::string& name, bool* fromPlayer) {
	std::string s;
	if (readFile(userPath(name), s)) {
		if (fromPlayer)
			*fromPlayer = true;
		return s;
	}
	if (fromPlayer)
		*fromPlayer = false;
	return builtinsApply() ? builtin(name) : std::string();
}

std::vector<Script> scripts() {
	std::map<std::string, Script> byName;
	if (builtinsApply())
		for (auto& n : EmbeddedAdditionNames())
			if (fs::path(n).extension() == ".cos")
				byName[n] = Script{n, builtin(n), false, {}};
	std::error_code ec;
	fs::path dir = getUserDataDir() / "Additions";
	if (fs::is_directory(dir, ec))
		for (auto& f : fs::directory_iterator(dir, ec))
			if (f.path().extension() == ".cos") {
				Script s{f.path().filename().string(), {}, true};
				if (readFile(f.path(), s.text))
					byName[s.name] = s;
			}
	std::vector<Script> out;
	for (auto& [n, s] : byName) {
		if (!s.fromPlayer)
			s.missingArt = missingArtIn(s.text);
		out.push_back(s);
	}
	return out;
}

void ensureUserCopy(const std::string& name) {
	fs::path p = userPath(name);
	std::error_code ec;
	if (fs::exists(p, ec) || !builtinsApply())
		return;
	std::string s = builtin(name);
	if (s.empty())
		return;
	fs::create_directories(p.parent_path(), ec);
	std::ofstream out(p, std::ios::binary);
	out << s;
}

} // namespace Additions
