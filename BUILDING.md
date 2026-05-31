# Building Valvra from source

## Prerequisites

- CMake 3.25+
- C++20 compiler (Clang 15+, GCC 12+, MSVC 2022+)
- Git (for CMake's FetchContent to pull JUCE)
- 2–3 GB free disk (JUCE + chowdsp_utils are large)

### macOS
```sh
brew install cmake ninja
xcode-select --install
```

### Ubuntu / Debian
```sh
sudo apt-get install -y cmake ninja-build build-essential \
    libasound2-dev libx11-dev libxcomposite-dev libxcursor-dev \
    libxext-dev libxinerama-dev libxrandr-dev libxrender-dev \
    libfreetype-dev libglu1-mesa-dev libjack-jackd2-dev
```

### Windows
- Install Visual Studio 2022 (Community edition is fine)
- Enable "Desktop development with C++" workload
- Install CMake from https://cmake.org

## Configure and build

```sh
# Fresh clone
git clone https://github.com/<user>/valvra.git
cd valvra

# Configure (first run downloads JUCE, chowdsp_utils, Catch2 — ~5 min)
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build everything
cmake --build build --config Release --parallel
```

## Build options

```sh
# DSP + tests only (no JUCE download, fast)
cmake -B build -DVALVRA_BUILD_PLUGIN=OFF

# Plugin only, no tests
cmake -B build -DVALVRA_BUILD_TESTS=OFF
```

## Run tests

```sh
cd build
ctest --output-on-failure -C Release
```

Tests validate DSP components against academic benchmarks (Dempwolf 2011 for
the triode model, Jiles-Atherton 1986 for the transformer).

## Plugin output location

After a successful build, the plugin binaries are under:

- `build/src/plugin/Valvra_artefacts/Release/VST3/Valvra.vst3`
- `build/src/plugin/Valvra_artefacts/Release/AU/Valvra.component` (macOS)
- `build/src/plugin/Valvra_artefacts/Release/Standalone/Valvra.app` (macOS)

Copy to your DAW's plugin folder, or add the parent directory to your DAW's
plugin search path.

## Troubleshooting

**"JUCE not found"**: Ensure `git` is in PATH. CMake's FetchContent uses it
to clone JUCE 8.0.4 from GitHub on first configure.

**"C++20 not supported"**: Upgrade your compiler. Apple Clang 14+, GCC 12+,
or MSVC 19.34+ (VS 2022 17.4).

**"libasound2-dev: could not find"** (Linux): You probably have an older
Ubuntu. Use 22.04 LTS or newer.

## Benchmarking (optional)

For tracking CPU performance across patches, build with the benchmark
target enabled:

```bash
cmake -S . -B build -DVALVRA_BUILD_BENCHES=ON
cmake --build build --target valvra_bench -j
./build/bench/valvra_bench
```

Output is a markdown table sorted by CPU cost descending (the heaviest
scenario first).  See [docs/27-bench-baseline-2026-04-27.md](docs/27-bench-baseline-2026-04-27.md)
for the current baseline, regression guidelines, and known hotspots.
