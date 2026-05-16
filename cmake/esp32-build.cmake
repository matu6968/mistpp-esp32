# ESP-IDF firmware build invoked from the repo root via: cmake -B build-esp -DESP32=ON
# Plain CMake cannot cross-compile ESP-IDF like a normal target; this drives idf.py instead.

if(NOT DEFINED ENV{IDF_PATH})
  message(FATAL_ERROR
    "ESP32=ON requires a sourced ESP-IDF environment.\n"
    "  source /path/to/activate_idf_v6.0.1.sh\n"
    "Then: cmake -B build-esp -DESP32=ON && cmake --build build-esp")
endif()

set(MIST_ESP_PROJECT_DIR "${CMAKE_SOURCE_DIR}/test/esp-project" CACHE PATH
    "ESP-IDF application project directory")

if(NOT EXISTS "${MIST_ESP_PROJECT_DIR}/CMakeLists.txt")
  message(FATAL_ERROR "ESP-IDF project not found at ${MIST_ESP_PROJECT_DIR}")
endif()

if(NOT DEFINED ENV{IDF_PYTHON_ENV_PATH})
  message(FATAL_ERROR "IDF_PYTHON_ENV_PATH is not set (re-source the ESP-IDF activate script)")
endif()

set(_idf_python "$ENV{IDF_PYTHON_ENV_PATH}/bin/python")
set(_idf_py "$ENV{IDF_PATH}/tools/idf.py")

if(NOT EXISTS "${_idf_python}")
  message(FATAL_ERROR "IDF Python not found: ${_idf_python}")
endif()
if(NOT EXISTS "${_idf_py}")
  message(FATAL_ERROR "idf.py not found: ${_idf_py}")
endif()

# No host compiler probe; firmware is built by idf.py in test/esp-project.
project(mistpp_esp32 NONE)

set(_idf_cmd "${_idf_python}" "${_idf_py}" -C "${MIST_ESP_PROJECT_DIR}")

add_custom_target(firmware ALL
  COMMAND ${_idf_cmd} build
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  USES_TERMINAL
  COMMENT "Building ESP-IDF firmware (test/esp-project)"
)

add_custom_target(flash
  COMMAND ${_idf_cmd} flash
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  USES_TERMINAL
  COMMENT "Flashing ESP-IDF firmware"
)

add_custom_target(monitor
  COMMAND ${_idf_cmd} monitor
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  USES_TERMINAL
  COMMENT "Serial monitor for ESP-IDF firmware"
)

add_custom_target(menuconfig
  COMMAND ${_idf_cmd} menuconfig
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  USES_TERMINAL
  COMMENT "ESP-IDF menuconfig (test/esp-project)"
)

message(STATUS "ESP32=ON: build firmware with: cmake --build ${CMAKE_BINARY_DIR}")
message(STATUS "  Project: ${MIST_ESP_PROJECT_DIR}")
