#include <catch2/catch_test_macros.hpp>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#ifndef _WIN32
#include <sys/wait.h>
#endif

#ifndef VALVRA_PROCESS_PATH
#error "VALVRA_PROCESS_PATH is not defined"
#endif

namespace {

std::string quote(const std::filesystem::path& p)
{
    return "\"" + p.string() + "\"";
}

int normaliseExitCode(int raw) noexcept
{
#ifdef _WIN32
    return raw;
#else
    if (raw == -1) return -1;
    if (WIFEXITED(raw)) return WEXITSTATUS(raw);
    if (WIFSIGNALED(raw)) return 128 + WTERMSIG(raw);
    return raw;
#endif
}

int runShellCommand(const std::string& cmd)
{
    return normaliseExitCode(std::system(cmd.c_str()));
}

std::filesystem::path makeTempPath(const std::string& name)
{
    const auto base = std::filesystem::temp_directory_path();
    const auto uniq = std::to_string(std::rand());
    return base / ("valvra_cli_" + name + "_" + uniq);
}

} // namespace

TEST_CASE("valvra_process: missing numeric value returns non-zero", "[cli][args]")
{
    const std::filesystem::path exe = VALVRA_PROCESS_PATH;
    const auto outPath = makeTempPath("out");
    const auto errPath = makeTempPath("err");

    const std::string cmd =
        quote(exe) + " --sr > " + quote(outPath) + " 2> " + quote(errPath);
    const int rc = runShellCommand(cmd);

    REQUIRE(rc != 0);

    std::filesystem::remove(outPath);
    std::filesystem::remove(errPath);
}

TEST_CASE("valvra_process: --sr=0 is rejected", "[cli][args][nan]")
{
    const std::filesystem::path exe = VALVRA_PROCESS_PATH;
    const auto outPath = makeTempPath("out");
    const auto errPath = makeTempPath("err");

    const std::string cmd =
        quote(exe) + " --sr=0 --os=4 > " + quote(outPath) + " 2> " + quote(errPath);
    const int rc = runShellCommand(cmd);

    REQUIRE(rc != 0);

    std::filesystem::remove(outPath);
    std::filesystem::remove(errPath);
}

TEST_CASE("valvra_process: valid short stdin smoke succeeds", "[cli][smoke]")
{
    const std::filesystem::path exe = VALVRA_PROCESS_PATH;
    const auto inPath  = makeTempPath("in.raw");
    const auto outPath = makeTempPath("out.raw");
    const auto errPath = makeTempPath("err.txt");

    {
        std::ofstream in(inPath, std::ios::binary | std::ios::trunc);
        const float x = 0.25f;
        in.write(reinterpret_cast<const char*>(&x), sizeof(x));
    }

    const std::string cmd =
        quote(exe)
        + " --sr=48000 --os=1 < " + quote(inPath)
        + " > " + quote(outPath)
        + " 2> " + quote(errPath);
    const int rc = runShellCommand(cmd);

    REQUIRE(rc == 0);
    REQUIRE(std::filesystem::exists(outPath));
    REQUIRE(std::filesystem::file_size(outPath) == sizeof(float));

    std::filesystem::remove(inPath);
    std::filesystem::remove(outPath);
    std::filesystem::remove(errPath);
}
