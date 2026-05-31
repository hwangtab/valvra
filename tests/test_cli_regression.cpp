#include <catch2/catch_test_macros.hpp>

#include <cstdlib>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <array>

#ifndef _WIN32
#include <sys/wait.h>
#include <unistd.h>
#else
#include <process.h>
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
    static std::atomic<std::uint64_t> seq { 0 };
    const auto base = std::filesystem::temp_directory_path();
    const auto ticks = static_cast<std::uint64_t>(
        std::chrono::high_resolution_clock::now().time_since_epoch().count());
#ifdef _WIN32
    const auto pid = static_cast<std::uint64_t>(_getpid());
#else
    const auto pid = static_cast<std::uint64_t>(::getpid());
#endif
    const auto uniq = seq.fetch_add(1, std::memory_order_relaxed);
    return base / ("valvra_cli_" + name
                   + "_" + std::to_string(pid)
                   + "_" + std::to_string(ticks)
                   + "_" + std::to_string(uniq));
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

TEST_CASE("valvra_process: --expansion-mix out of range is rejected",
          "[cli][args]")
{
    const std::filesystem::path exe = VALVRA_PROCESS_PATH;
    const auto outPath = makeTempPath("out");
    const auto errPath = makeTempPath("err");

    const std::string cmd =
        quote(exe) + " --sr=48000 --os=1 --expansion=tape --expansion-amount=0.8 "
        "--expansion-mix=1.5 > " + quote(outPath) + " 2> " + quote(errPath);
    const int rc = runShellCommand(cmd);

    REQUIRE(rc != 0);

    std::filesystem::remove(outPath);
    std::filesystem::remove(errPath);
}

TEST_CASE("valvra_process: invalid expansion string is rejected", "[cli][args]")
{
    const std::filesystem::path exe = VALVRA_PROCESS_PATH;
    const auto outPath = makeTempPath("out");
    const auto errPath = makeTempPath("err");

    const std::string cmd =
        quote(exe) + " --sr=48000 --os=1 --expansion=unknown > "
        + quote(outPath) + " 2> " + quote(errPath);
    const int rc = runShellCommand(cmd);

    REQUIRE(rc != 0);

    std::filesystem::remove(outPath);
    std::filesystem::remove(errPath);
}

TEST_CASE("valvra_process: unknown option is rejected", "[cli][args]")
{
    const std::filesystem::path exe = VALVRA_PROCESS_PATH;
    const auto outPath = makeTempPath("out");
    const auto errPath = makeTempPath("err");

    const std::string cmd =
        quote(exe) + " --sr=48000 --os=1 --not-a-real-option=1 > "
        + quote(outPath) + " 2> " + quote(errPath);
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

TEST_CASE("valvra_process: hifi preset short stdin smoke succeeds",
          "[cli][smoke]")
{
    const std::filesystem::path exe = VALVRA_PROCESS_PATH;
    const auto inPath  = makeTempPath("in_hifi.raw");
    const auto outPath = makeTempPath("out_hifi.raw");
    const auto errPath = makeTempPath("err_hifi.txt");

    {
        std::ofstream in(inPath, std::ios::binary | std::ios::trunc);
        const float x = 0.11f;
        in.write(reinterpret_cast<const char*>(&x), sizeof(x));
    }

    const std::string cmd =
        quote(exe)
        + " --preset=hifi --sr=48000 --os=1 < " + quote(inPath)
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

TEST_CASE("valvra_process: documented sample rates all process successfully",
          "[cli][smoke][sample-rate]")
{
    const std::filesystem::path exe = VALVRA_PROCESS_PATH;
    constexpr std::array<int, 6> kSampleRates {
        44100, 48000, 88200, 96000, 176400, 192000
    };

    for (int sr : kSampleRates)
    {
        INFO("sr = " << sr);

        const auto inPath  = makeTempPath("in_sr.raw");
        const auto outPath = makeTempPath("out_sr.raw");
        const auto errPath = makeTempPath("err_sr.txt");

        {
            std::ofstream in(inPath, std::ios::binary | std::ios::trunc);
            REQUIRE(in.good());
            for (int n = 0; n < 64; ++n)
            {
                const float x = 0.19f * static_cast<float>((n % 7) - 3) / 3.0f;
                in.write(reinterpret_cast<const char*>(&x), sizeof(x));
            }
        }

        const std::string cmd =
            quote(exe)
            + " --preset=marshall --sr=" + std::to_string(sr)
            + " --os=2 --expansion=tape --expansion-amount=0.75 --expansion-mix=1.0"
            + " < " + quote(inPath)
            + " > " + quote(outPath)
            + " 2> " + quote(errPath);
        const int rc = runShellCommand(cmd);

        REQUIRE(rc == 0);
        REQUIRE(std::filesystem::exists(outPath));
        REQUIRE(std::filesystem::file_size(outPath) == sizeof(float) * 64);

        std::filesystem::remove(inPath);
        std::filesystem::remove(outPath);
        std::filesystem::remove(errPath);
    }
}
