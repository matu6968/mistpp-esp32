# Detect whether ESP-IDF cross tools on PATH would break a host C++ link.
# IDF_PATH / ESP_IDF_VERSION may remain in the environment after a shell
# was closed or from profile scripts; only enable the host toolchain when
# the active compiler or linker is actually an ESP cross-tool.

function(mist_detect_idf_toolchain_pollution out_var)
  set(_polluted FALSE)

  foreach(_ev IN ITEMS CC CXX LD)
    if(DEFINED ENV{${_ev}})
      if("$ENV{${_ev}}" MATCHES "(xtensa|riscv32)-esp|esp32ulp|\\.espressif/")
        set(_polluted TRUE)
        break()
      endif()
    endif()
  endforeach()

  if(NOT _polluted)
    find_program(_mist_ld NAMES ld)
    if(_mist_ld AND _mist_ld MATCHES "(esp32ulp|xtensa-esp|riscv32-esp|esp-clang|\\.espressif/)")
      set(_polluted TRUE)
    endif()
  endif()

  set(${out_var} ${_polluted} PARENT_SCOPE)
endfunction()
