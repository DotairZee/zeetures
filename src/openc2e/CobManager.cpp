#include "CobManager.h"

#include "Agent.h"
#include "Engine.h"
#include "PathResolver.h"
#include "World.h"
#include "common/Ascii.h"
#include "common/Exception.h"
#include "common/PathUtils.h"
#include "common/Ranges.h"
#include "common/case_insensitive_filesystem.h"
#include "common/io/FileWriter.h"
#include "fileformats/c1cobfile.h"
#include "fileformats/c2cobfile.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <set>
#include <fmt/format.h>

namespace fs = std::filesystem;

std::vector<std::string> CobManager::readNameList(const fs::path& file) {
	std::vector<std::string> names;
	std::ifstream in(file);
	std::string line;
	while (std::getline(in, line)) {
		auto hash = line.find('#');
		if (hash != std::string::npos)
			line = line.substr(0, hash);
		while (!line.empty() && std::isspace((unsigned char)line.back()))
			line.pop_back();
		size_t start = 0;
		while (start < line.size() && std::isspace((unsigned char)line[start]))
			start++;
		if (start < line.size())
			names.push_back(line.substr(start));
	}
	return names;
}

static std::string trimmedName(std::string line) {
	auto hash = line.find('#');
	if (hash != std::string::npos)
		line = line.substr(0, hash);
	while (!line.empty() && std::isspace((unsigned char)line.back()))
		line.pop_back();
	size_t start = 0;
	while (start < line.size() && std::isspace((unsigned char)line[start]))
		start++;
	return line.substr(start);
}

bool CobManager::addToNameList(const fs::path& file, const std::string& name, const std::string& comment) {
	auto names = readNameList(file);
	if (std::find(names.begin(), names.end(), name) != names.end())
		return true;
	std::error_code ec;
	fs::create_directories(file.parent_path(), ec);
	// a newline first if the file doesn't end with one
	bool needNewline = false;
	{
		std::ifstream in(file, std::ios::binary);
		if (in) {
			in.seekg(0, std::ios::end);
			if (in.tellg() > 0) {
				in.seekg(-1, std::ios::end);
				needNewline = in.get() != '\n';
			}
		}
	}
	std::ofstream out(file, std::ios::app);
	if (needNewline)
		out << "\n";
	if (!comment.empty())
		out << "# " << comment << "\n";
	out << name << "\n";
	return (bool)out;
}

bool CobManager::removeFromNameList(const fs::path& file, const std::string& name) {
	std::ifstream in(file);
	if (!in)
		return true;
	std::vector<std::string> lines;
	std::string line;
	while (std::getline(in, line))
		lines.push_back(line);
	in.close();
	std::vector<std::string> kept;
	for (auto& l : lines) {
		if (trimmedName(l) == name) {
			// the note written when it was added from the injector goes too
			if (!kept.empty() && kept.back().rfind("# added from the Agent Injector", 0) == 0)
				kept.pop_back();
			continue;
		}
		kept.push_back(l);
	}
	std::ofstream out(file, std::ios::trunc);
	for (auto& l : kept)
		out << l << "\n";
	return (bool)out;
}

fs::path CobManager::builtinListFile() {
	return getUserDataDir() / "Additions" / "builtin-cobs.txt";
}

fs::path CobManager::uninstalledListFile() {
	return getUserDataDir() / "uninstalled-cobs.txt";
}

int CobManager::adoptHandAdded(const std::string& name) {
	int n = 0;
	for (auto& a : world.agents)
		if (a && a->injectedFrom == name) {
			a->injectedFrom.clear();
			n++;
		}
	auto& list = world.injectedCobs;
	list.erase(std::remove(list.begin(), list.end(), name), list.end());
	world.injectedScripts.erase(name);
	return n;
}

void CobManager::update() {
	objects.clear();

	for (auto cob : findCobFiles("*.cob")) {
		if (engine.version == 1) {
			c1cobfile cobfile = read_c1cobfile(cob);
			objects.emplace_back(cobfile.name, cob);
			auto rcb = cob;
			rcb.replace_extension("rcb");
			if (!findCobFile(rcb).empty()) {
				objects.back().is_removable = true;
			}
		} else if (engine.version == 2) {
			c2cobfile cobfile(cob);

			for (auto b : cobfile.blocks) {
				if (b->type != "agnt")
					continue;
				cobAgentBlock a(b);
				objects.emplace_back(a.name, cob);
				auto& o = objects.back();
				if (a.removescript.size() > 0) {
					o.is_removable = true;
				}
				o.description = a.description;
				// the first classified object the install script makes
				std::string lower = to_ascii_lowercase(a.installscript);
				for (size_t at = lower.find("setv cls2"); at != std::string::npos; at = lower.find("setv cls2", at + 9)) {
					int f = 0, g = 0, sp = 0;
					if (sscanf(lower.c_str() + at + 9, "%d %d %d", &f, &g, &sp) != 3)
						continue;
					int cat = world.findCategory((unsigned char)f, (unsigned char)g, (unsigned short)sp);
					if (cat < 0)
						continue;
					o.category = cat;
					o.family = f, o.genus = g, o.species = sp;
					break;
				}
				std::error_code ec;
				auto t = fs::last_write_time(cob, ec);
				if (!ec)
					o.added = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::file_clock::to_sys(t).time_since_epoch()).count();
				std::string userdir = getUserDataDir().lexically_normal().string();
				o.imported = cob.lexically_normal().string().rfind(userdir, 0) == 0;
			}
		}
	}

	std::sort(objects.begin(), objects.end(), [](auto& a, auto& b) { return a.name < b.name; });
}

Image CobManager::getPicture(const CobFileInfo& info) {
	if (engine.version == 1) {
		c1cobfile cobfile = read_c1cobfile(info.filename);

		if (cobfile.picture.width > 0 && cobfile.picture.height > 0) {
			// TODO: don't require casting
			return cobfile.picture;
		}
	} else if (engine.version == 2) {
		c2cobfile cobfile(info.filename);

		for (auto b : cobfile.blocks) {
			if (b->type != "agnt")
				continue;
			cobAgentBlock a(b);
			if (a.name != info.name)
				continue;
			return a.thumbnail;
		}
	}
	return {};
}

// the CAOS for a C2 agent block: its scripts, then (withInstall) its install
// script; images and sounds it needs are written to the user data folder
static std::string c2AgentCaos(const std::filesystem::path& filename, const std::string& name, bool withInstall);
static std::vector<std::string> c2AgentScripts(const std::filesystem::path& filename, const std::string& name);

static fs::path usedFile() {
	return getUserDataDir() / "injector-used.txt";
}

// injector-used.txt: "seconds<TAB>name" lines, the latest for each name last
long long CobManager::lastUsed(const std::string& name) {
	std::ifstream in(usedFile());
	std::string line;
	long long when = 0;
	while (std::getline(in, line)) {
		auto tab = line.find('\t');
		if (tab != std::string::npos && line.substr(tab + 1) == name)
			when = atoll(line.c_str());
	}
	return when;
}

void CobManager::noteUsed(const std::string& name) {
	std::ofstream out(usedFile(), std::ios::app);
	out << (long long)std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count() << "\t" << name << "\n";
}

void CobManager::inject(const CobFileInfo& info, bool byHand) {
	std::string idata;
	if (engine.version == 1) {
		c1cobfile cobfile = read_c1cobfile(info.filename);
		for (auto s : cobfile.object_scripts) {
			idata += s + "\n";
		}
		for (auto s : cobfile.install_scripts) {
			idata += "iscr," + s + "\n";
		}
	} else if (engine.version == 2) {
		idata = c2AgentCaos(info.filename, info.name, true);
	}

	// by hand: the objects the install script makes are marked as injected
	// (and so, through NEW:, is anything they make later)
	std::set<Agent*> before;
	if (byHand)
		for (auto& a : world.agents)
			before.insert(a.get());

	// idata += "rscr\n";
	std::string result = engine.executeNetwork(idata);
	if (result.size()) {
		fmt::print("warning: injection returned data (error?): {}\n", result);
	}
	// the project's patches to scripts it just installed (the Anteater's,
	// 2026-11-13), as at a world load
	engine.applyScriptPatches(true);
	if (byHand && engine.version == 2) {
		for (auto& a : world.agents)
			if (a && !before.count(a.get()) && a->injectedFrom.empty())
				a->injectedFrom = info.name;
		if (std::find(world.injectedCobs.begin(), world.injectedCobs.end(), info.name) == world.injectedCobs.end())
			world.injectedCobs.push_back(info.name);
		world.injectedScripts[info.name] = c2AgentScripts(info.filename, info.name);
	}
}

static std::string c2AgentCaos(const std::filesystem::path& filename, const std::string& name, bool withInstall) {
	std::string idata;
	c2cobfile cobfile(filename);
	auto block = find_if(cobfile.blocks, [&](auto& b) {
		return b->type == "agnt" && cobAgentBlock(b).name == name;
	});
	if (!block) {
		throw Exception(fmt::format("Couldn't find agent {}", name));
	}
	cobAgentBlock a(*block);

	// dependencies
	assert(a.deptypes.size() == a.depnames.size());
	for (size_t i = 0; i < a.deptypes.size(); i++) {
		int deptype = a.deptypes[i];
		std::string depname = a.depnames[i];

		fs::path resourcedir;
		if (deptype == 0) {
			if (!findImageFile(depname).empty())
				continue; // TODO: update file if necessary?
		} else if (deptype == 1) {
			if (!findSoundFile(depname).empty())
				continue; // TODO: update file if necessary?
		} else {
			throw Exception("Unknown dependency type " + std::to_string(deptype));
		}

		auto depBlock = find_if(cobfile.blocks, [&](auto& b) {
			return b->type == "file" && cobFileBlock(b).filetype == deptype && cobFileBlock(b).filename == depname;
		});
		if (!depBlock) {
			throw Exception(fmt::format("Couldn't find dependency {} (type {})", depname, deptype));
		}
		cobFileBlock f(*depBlock);

		FileWriter output = [&] {
			if (deptype == 0) {
				return createUserImageFile(depname);
			} else if (deptype == 1) {
				return createUserSoundFile(depname);
			} else {
				throw Exception("Unknown dependency type " + std::to_string(deptype));
			}
		}();

		output.write((char*)f.getFileContents(), f.filesize);
	}

	for (auto s : a.scripts) {
		idata += s + "\n";
	}
	if (withInstall)
		idata += "iscr," + a.installscript + "\n";
	return idata;
}

// the scripts of a C2 COB agent, one per entry (no install script)
static std::vector<std::string> c2AgentScripts(const std::filesystem::path& filename, const std::string& name) {
	c2cobfile cobfile(filename);
	auto block = find_if(cobfile.blocks, [&](auto& b) {
		return b->type == "agnt" && cobAgentBlock(b).name == name;
	});
	if (!block)
		return {};
	cobAgentBlock a(*block);
	return a.scripts;
}

bool CobManager::reinstallScripts(const std::string& agentname, std::string& error) {
	// the copy saved with the world first: the COB file may be gone
	if (auto saved = world.injectedScripts.find(agentname); saved != world.injectedScripts.end() && !saved->second.empty()) {
		std::string idata;
		for (auto& s : saved->second)
			idata += s + "\n";
		std::string result = engine.executeNetwork(idata);
		if (!result.empty()) {
			error = result;
			return false;
		}
		return true;
	}
	CobManager cobs;
	cobs.update();
	auto it = std::find_if(cobs.objects.begin(), cobs.objects.end(), [&](auto& o) { return o.name == agentname; });
	if (it == cobs.objects.end()) {
		error = "no COB has it";
		return false;
	}
	try {
		std::string result = engine.executeNetwork(c2AgentCaos(it->filename, agentname, false));
		if (!result.empty()) {
			error = result;
			return false;
		}
		world.injectedScripts[agentname] = c2AgentScripts(it->filename, agentname);
	} catch (std::exception& e) {
		error = e.what();
		return false;
	}
	return true;
}

void CobManager::remove(const CobFileInfo& info, bool byHand) {
	std::string rdata;
	if (engine.version == 1) {
		auto rcbpath = findCobFile(with_extension(info.filename.filename(), "rcb"));
		c1cobfile cobfile = read_c1cobfile(rcbpath);
		for (auto s : cobfile.install_scripts) {
			rdata += s + "\n";
		}

	} else if (engine.version == 2) {
		c2cobfile cobfile(info.filename);
		auto block = find_if(cobfile.blocks, [&](auto& b) {
			return b->type == "agnt" && cobAgentBlock(b).name == info.name;
		});
		if (!block) {
			throw Exception(fmt::format("Couldn't find agent {}", info.name));
		}
		cobAgentBlock a(*block);

		rdata = a.removescript;
	}

	rdata += "\nrscr\n";
	std::string result = engine.executeNetwork(rdata);
	if (result.size()) {
		fmt::print("warning: removal returned data (error?): {}\n", result);
	}
	// removed: its scripts are no longer installed when the world loads
	if (byHand) {
		auto& list = world.injectedCobs;
		list.erase(std::remove(list.begin(), list.end(), info.name), list.end());
	}
}