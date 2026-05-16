# Mist++ (ESP32 port)

A ESP32 port of [TurboWarp/mist](https://github.com/TurboWarp/mist) to C++.

This project was made specifically for
[Scratch Everywhere!](https://github.com/ScratchEverywhere/ScratchEverywhere).

## Installation

### PC

```sh
cmake -B build -DCMAKE_BUILD_TYPE="Release" -DBUILD_TEST=off
cmake --build build
sudo cmake --install build
```

If ESP-IDF is sourced in the same shell, CMake uses `cmake/host-toolchain.cmake` only when ESP cross-tools (e.g. `esp32ulp-elf` or `ld`) are ahead of the system linker on `PATH`. Force with `-DMIST_FORCE_HOST_TOOLCHAIN=ON`. (or replace ON with OFF to disable)

### ESP32

Option A/B are only meant to build the test project in `test/esp-project`.

**Option A (from repo root (same `CMakeLists.txt`, requires sourced ESP-IDF)):**

```sh
source /path/to/idf_source_script.sh
cmake -B build-esp -DESP32=ON
cmake --build build-esp          # runs idf.py build in test/esp-project
cmake --build build-esp --target flash
cmake --build build-esp --target monitor
```

**Option B (ESP-IDF project directory directly):**

```sh
source /path/to/idf_source_script.sh
cd test/esp-project
idf.py build flash monitor
```

**Option C (use as a dependency in your own ESP-IDF app):**

```sh
idf.py add-dependency --git https://github.com/matu6968/mistpp-esp32
idf.py update-dependencies
```

### Homebrew

Use the packages in
[the mistpp-packages repo](https://github.com/ScratchEverywhere/mistpp-packages).
