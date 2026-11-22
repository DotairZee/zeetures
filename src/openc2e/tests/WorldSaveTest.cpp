#include "openc2e/WorldSave.h"

#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <iterator>

namespace fs = std::filesystem;

// A save this version cannot read must never be lost: it is moved aside at
// the load, and can be put back unless the world has been saved since.
class WorldSaveTest : public ::testing::Test {
  protected:
	fs::path dir;
	void SetUp() override {
		dir = fs::temp_directory_path() / ("zeetures-savetest-" + std::to_string(::testing::UnitTest::GetInstance()->random_seed()) + "-" + ::testing::UnitTest::GetInstance()->current_test_info()->name());
		fs::remove_all(dir);
		write(dir / "save" / "state.txt", "openc2e-world-save 99\ncreature\n");
	}
	void TearDown() override {
		std::error_code ec;
		fs::remove_all(dir, ec);
	}
	static void write(const fs::path& p, const std::string& text) {
		fs::create_directories(p.parent_path());
		std::ofstream(p, std::ios::binary) << text;
	}
	static std::string read(const fs::path& p) {
		std::ifstream in(p, std::ios::binary);
		return {std::istreambuf_iterator<char>(in), {}};
	}
};

TEST_F(WorldSaveTest, UnknownFormatIsKeptAside) {
	EXPECT_FALSE(WorldSave::LoadWorldState(dir));
	EXPECT_FALSE(fs::exists(dir / "save"));
	auto& aside = WorldSave::LastSetAside();
	ASSERT_TRUE(aside.happened);
	ASSERT_FALSE(aside.kept.empty());
	EXPECT_EQ(read(aside.kept / "state.txt"), "openc2e-world-save 99\ncreature\n");
}

TEST_F(WorldSaveTest, PutBackRestoresIt) {
	EXPECT_FALSE(WorldSave::LoadWorldState(dir));
	std::string error;
	EXPECT_TRUE(WorldSave::PutBackSetAsideSave(error)) << error;
	EXPECT_EQ(read(dir / "save" / "state.txt"), "openc2e-world-save 99\ncreature\n");
}

TEST_F(WorldSaveTest, PutBackNeverReplacesANewerSave) {
	EXPECT_FALSE(WorldSave::LoadWorldState(dir));
	write(dir / "save" / "state.txt", "newer");
	std::string error;
	EXPECT_FALSE(WorldSave::PutBackSetAsideSave(error));
	EXPECT_FALSE(error.empty());
	EXPECT_EQ(read(dir / "save" / "state.txt"), "newer");
	EXPECT_EQ(read(WorldSave::LastSetAside().kept / "state.txt"), "openc2e-world-save 99\ncreature\n");
}

TEST_F(WorldSaveTest, NoSaveIsNotSetAside) {
	fs::remove_all(dir / "save");
	EXPECT_FALSE(WorldSave::LoadWorldState(dir));
	EXPECT_EQ(std::distance(fs::directory_iterator(dir), fs::directory_iterator{}), 0);
}
