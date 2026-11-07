#include "common/readfile.h"
#include "openc2e/caosScript.h"

#include <fmt/core.h>
#include <string>

int main(int argc, char** argv) {
	std::string dialect = "c3";
	int arg = 1;
	if (argc == 4 && std::string(argv[1]) == "--dialect") {
		dialect = argv[2];
		arg = 3;
	} else if (argc != 2) {
		fmt::print(stderr, "USAGE: {} [--dialect c1|c2|cv|c3|sm] FILE\n", argv[0]);
		return 1;
	}

	auto filename = argv[arg];

	caosScript script(dialect, filename);
	script.parse(readfile(filename));

	if (script.installer) {
		fmt::print("Install Script:\n");
		fmt::print("{}\n", script.installer->dump());
	}
	for (auto& s : script.scripts) {
		fmt::print("Script {} {} {} {}\n", s->fmly, s->gnus, s->spcs, s->scrp);
		fmt::print("{}\n", s->dump());
	}
	if (script.removal) {
		fmt::print("Remove Script:\n");
		fmt::print("{}\n", script.removal->dump());
	}

	return 0;
}