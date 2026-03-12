cmake_minimum_required(VERSION 3.20)

project(urbis C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin)

option(URBIS_BUILD_SHARED "Build shared library" ON)
option(URBIS_BUILD_STATIC "Build static library" ON)
option(URBIS_BUILD_CLI "Build CLI tool" ON)
option(URBIS_BUILD_TESTS "Build tests" ON)
option(URBIS_USE_MMAP "Enable mmap on supported platforms" OFF)

set(URBIS_PUBLIC_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)

file(GLOB URBIS_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c)

if (URBIS_BUILD_SHARED)
  add_library(urbis_shared SHARED ${URBIS_SOURCES})
  target_include_directories(urbis_shared PUBLIC ${URBIS_PUBLIC_INCLUDE_DIR})
  target_compile_definitions(urbis_shared PUBLIC URBIS_SHARED)
endif()

if (URBIS_BUILD_STATIC)
  add_library(urbis_static STATIC ${URBIS_SOURCES})
  target_include_directories(urbis_static PUBLIC ${URBIS_PUBLIC_INCLUDE_DIR})
endif()

if (TARGET urbis_shared)
  add_library(urbis::urbis ALIAS urbis_shared)
elseif (TARGET urbis_static)
  add_library(urbis::urbis ALIAS urbis_static)
endif()

foreach(target IN LISTS urbis_shared urbis_static)
  if (TARGET ${target})
    if (MSVC)
      target_compile_options(${target} PRIVATE /W4 /WX)
    else()
      target_compile_options(${target} PRIVATE -Wall -Wextra -Werror)
      target_compile_definitions(${target} PRIVATE _POSIX_C_SOURCE=200809L)
    endif()
    if (URBIS_USE_MMAP)
      target_compile_definitions(${target} PRIVATE URBIS_USE_MMAP=1)
    endif()
    target_link_libraries(${target} PUBLIC m)
  endif()
endforeach()

if (URBIS_BUILD_CLI)
  add_executable(urbis_cli ${CMAKE_CURRENT_SOURCE_DIR}/src/urbis_cli.c)
  target_link_libraries(urbis_cli PRIVATE urbis::urbis)
  set_target_properties(urbis_cli PROPERTIES OUTPUT_NAME urbis)
endif()

if (URBIS_BUILD_TESTS)
  enable_testing()
  file(GLOB URBIS_TEST_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_*.c)
  foreach(test_src IN LISTS URBIS_TEST_SOURCES)
    get_filename_component(test_name ${test_src} NAME_WE)
    add_executable(${test_name} ${test_src})
    target_link_libraries(${test_name} PRIVATE urbis::urbis)
    add_test(NAME ${test_name} COMMAND ${test_name})
  endforeach()
endif()

include(GNUInstallDirs)

if (TARGET urbis_shared)
  install(TARGETS urbis_shared
    EXPORT urbisTargets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  )
endif()

if (TARGET urbis_static)
  install(TARGETS urbis_static
    EXPORT urbisTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  )
endif()

install(DIRECTORY ${URBIS_PUBLIC_INCLUDE_DIR}/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(EXPORT urbisTargets
  FILE urbisTargets.cmake
  NAMESPACE urbis::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/urbis
)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/urbis.pc.in
  ${CMAKE_CURRENT_BINARY_DIR}/urbis.pc @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/urbis.pc DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
