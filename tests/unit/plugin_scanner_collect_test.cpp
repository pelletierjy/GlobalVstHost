// tests/unit/plugin_scanner_collect_test.cpp
//
// Unit tests for collectVst3Candidates — the .vst3 candidate enumeration used
// by PluginScanner. Verifies the two-level scan (top level + one vendor
// subdirectory level), that scan roots are absolute-normalized (MSIX packaged
// launches can have an arbitrary working directory), and that .vst3 bundles
// are never descended into.

#include "../../src/audio-engine/vst-host/plugin_scanner.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <set>
#include <string>

namespace jyglobalvst::testing {

namespace {

namespace fs = std::filesystem;

class CollectVst3CandidatesTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        root_ = fs::temp_directory_path() / "jyglobalvst_collect_test";
        fs::remove_all(root_);
        fs::create_directories(root_ / "TopLevel.vst3");
        fs::create_directories(root_ / "Vendor" / "Nested.vst3");
        // Inside a bundle: module binary tree, not a separate plugin.
        fs::create_directories(root_ / "TopLevel.vst3" / "Contents" / "x86_64-win" / "Inner.vst3");
        // Deeper than the one-level vendor rule.
        fs::create_directories(root_ / "DeepA" / "DeepB" / "TooDeep.vst3");
        fs::create_directories(root_ / "PlainDir");
    }

    void TearDown() override
    {
        std::error_code ec;
        fs::remove_all(root_, ec);
    }

    static std::set<std::string> filenames(const std::vector<fs::path>& paths)
    {
        std::set<std::string> names;
        for (const auto& p : paths)
        {
            names.insert(p.filename().string());
        }
        return names;
    }

    fs::path root_;
};

}  // namespace

TEST_F(CollectVst3CandidatesTest, FindsTopLevelAndOneVendorLevelOnly)
{
    const auto candidates = engine::collectVst3Candidates(root_);
    const auto names = filenames(candidates);

    EXPECT_EQ(names, (std::set<std::string>{"TopLevel.vst3", "Nested.vst3"}))
        << "inner bundle modules and >1-level nests must not be scanned";
}

TEST_F(CollectVst3CandidatesTest, ReturnsAbsolutePathsEvenForRelativeRoot)
{
    // Simulate a packaged-launch working directory mismatch: pass a relative
    // root and require absolute results.
    std::error_code ec;
    const auto cwd = fs::current_path(ec);
    ASSERT_FALSE(ec);
    fs::current_path(root_.parent_path(), ec);
    ASSERT_FALSE(ec);

    const auto candidates = engine::collectVst3Candidates(root_.filename());

    fs::current_path(cwd, ec);
    ASSERT_FALSE(candidates.empty());
    for (const auto& p : candidates)
    {
        EXPECT_TRUE(p.is_absolute()) << p.string();
    }
}

TEST_F(CollectVst3CandidatesTest, MissingRootYieldsEmpty)
{
    const auto candidates = engine::collectVst3Candidates(root_ / "does_not_exist");
    EXPECT_TRUE(candidates.empty());
}

}  // namespace jyglobalvst::testing
