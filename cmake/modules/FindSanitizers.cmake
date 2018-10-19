if(NOT Sanitizers_FIND_COMPONENTS)
  set(Sanitizers_FIND_COMPONENTS
    address undefined_behavior)
endif()
if(HAVE_JEMALLOC)
  message(WARNING "JeMalloc does not work well with sanitizers")
endif()

set(Sanitizers_OPTIONS)

foreach(component ${Sanitizers_FIND_COMPONENTS})
  if(component STREQUAL "address")
    set(Sanitizers_address_COMPILE_OPTIONS "address")
  elseif(component STREQUAL "leak")
    set(Sanitizers_leak_COMPILE_OPTIONS "leak")
  elseif(component STREQUAL "thread")
    if ("address" IN_LIST ${Sanitizers_FIND_COMPONENTS} OR
        "leak" IN_LIST ${Sanitizers_FIND_COMPONENTS})
      message(SEND_ERROR "Cannot combine -fsanitize-leak w/ -fsanitize-thread")
    elseif(NOT CMAKE_POSITION_INDEPENDENT_CODE)
      message(SEND_ERROR "TSan requires all code to be position independent")
    endif()
    set(Sanitizers_Thread_COMPILE_OPTIONS "thread")
  elseif(component STREQUAL "undefined_behavior")
    set(Sanitizers_undefined_behavior_COMPILE_OPTIONS "undefined")
  else()
    message(SEND_ERROR "Unsupported sanitizer: ${component}")
  endif()
  list(APPEND Sanitizers_OPTIONS "${Sanitizers_${component}_COMPILE_OPTIONS}")
endforeach()

if(Sanitizers_OPTIONS)
  string(REPLACE ";" ","
    Sanitizers_COMPILE_OPTIONS
    "${Sanitizers_OPTIONS}")
  set(Sanitizers_COMPILE_OPTIONS
    "-fsanitize=${Sanitizers_COMPILE_OPTIONS} -fno-omit-frame-pointer")
endif()

include(CheckCXXSourceCompiles)
set(CMAKE_REQUIRED_FLAGS ${Sanitizers_COMPILE_OPTIONS})
set(CMAKE_REQUIRED_LIBRARIES ${Sanitizers_COMPILE_OPTIONS})
check_cxx_source_compiles("int main() {}"
  Sanitizers_ARE_SUPPORTED)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Sanitizers
  REQUIRED_VARS
    Sanitizers_ARE_SUPPORTED
    Sanitizers_COMPILE_OPTIONS)

if(Sanitizers_FOUND)
  if(NOT TARGET Sanitizers::Sanitizers)
    add_library(Sanitizers::Sanitizers INTERFACE IMPORTED)
    set_target_properties(Sanitizers::Sanitizers PROPERTIES
      INTERFACE_COMPILE_OPTIONS ${Sanitizers_COMPILE_OPTIONS}
      INTERFACE_LINK_LIBRARIES ${Sanitizers_COMPILE_OPTIONS})
  endif()
  foreach(component ${Sanitizers_FIND_COMPONENTS})
    if(NOT TARGET Sanitizers::${component})
      set(target Sanitizers::${component})
      set(compile_option "-fsanitize=${Sanitizers_${component}_COMPILE_OPTIONS}")
      add_library(${target} INTERFACE IMPORTED)
      set_target_properties(${target} PROPERTIES
        INTERFACE_COMPILE_OPTIONS ${compile_option}
        INTERFACE_LINK_LIBRARIES ${compile_option})
    endif()
  endforeach()
endif()
