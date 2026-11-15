#include "Additions.h"

#include "Engine.h"
#include "common/io/FileReader.h"
#include "PathResolver.h"

#include <algorithm>
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
				byName[n] = Script{n, builtin(n), false};
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
	for (auto& [n, s] : byName)
		out.push_back(s);
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
