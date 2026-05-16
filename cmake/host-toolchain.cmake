# Host (desktop) toolchain for Mist++ when ESP-IDF is sourced in the shell.
# ESP-IDF prepends esp32ulp-elf and other cross tools to PATH; g++ then links
# with the wrong ld. Pin the system compiler and linker from /usr/bin.

# todo: make this dynamic based on OS
set(CMAKE_SYSTEM_NAME Linux)

foreach(_tool IN ITEMS c++ g++ cc gcc ld ar ranlib nm)
  find_program(_host_${_tool} NAMES ${_tool}
    HINTS /usr/bin /usr/local/bin
    NO_CMAKE_PATH
    NO_CMAKE_ENVIRONMENT_PATH
  )
endforeach()

if(NOT _host_c++ AND NOT _host_g++)
  message(FATAL_ERROR "host-toolchain.cmake: no host C++ compiler found in /usr/bin")
endif()
if(NOT _host_ld)
  message(FATAL_ERROR "host-toolchain.cmake: no host linker (ld) found in /usr/bin")
endif()

set(CMAKE_CXX_COMPILER "${_host_c++}" CACHE FILEPATH "Host C++ compiler" FORCE)
if(_host_g++ AND NOT _host_c++)
  set(CMAKE_CXX_COMPILER "${_host_g++}" CACHE FILEPATH "Host C++ compiler" FORCE)
endif()
if(_host_cc)
  set(CMAKE_C_COMPILER "${_host_cc}" CACHE FILEPATH "Host C compiler" FORCE)
elseif(_host_gcc)
  set(CMAKE_C_COMPILER "${_host_gcc}" CACHE FILEPATH "Host C compiler" FORCE)
endif()
set(CMAKE_LINKER "${_host_ld}" CACHE FILEPATH "Host linker" FORCE)

# g++ still picks esp32ulp-elf ld from PATH unless we pass -B or -fuse-ld.
set(_host_bin_dir "/usr/bin")
get_filename_component(_host_bin_dir "${_host_ld}" DIRECTORY)
set(CMAKE_CXX_FLAGS_INIT "-B${_host_bin_dir}")
set(CMAKE_C_FLAGS_INIT "-B${_host_bin_dir}")
set(CMAKE_EXE_LINKER_FLAGS_INIT "-B${_host_bin_dir}")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "-B${_host_bin_dir}")

if(_host_ar)
  set(CMAKE_AR "${_host_ar}" CACHE FILEPATH "Host archiver" FORCE)
endif()
if(_host_ranlib)
  set(CMAKE_RANLIB "${_host_ranlib}" CACHE FILEPATH "Host ranlib" FORCE)
endif()
if(_host_nm)
  set(CMAKE_NM "${_host_nm}" CACHE FILEPATH "Host nm" FORCE)
endif()

message(STATUS "Mist++: using host toolchain (ESP cross-tools detected on PATH)")
