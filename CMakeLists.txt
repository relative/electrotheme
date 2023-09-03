cmake_minimum_required(VERSION 3.22)

function(embed_resource resource_file_name source_file_name variable_name)
  if(EXISTS "${source_file_name}")
    if("${source_file_name}" IS_NEWER_THAN "${resource_file_name}")
      return()
    endif()
  endif()

  file(READ "${resource_file_name}" hex_content HEX)

  string(REPEAT "[0-9a-f]" 32 pattern)
  string(REGEX REPLACE "(${pattern})" "\\1\n  " content "${hex_content}")

  string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1, " content "${content}")

  string(REGEX REPLACE ", $" "" content "${content}")

  set(array_definition "static const unsigned char ${variable_name}[] = {\n  ${content}\n};")
  set(size_definition "static const unsigned ${variable_name}_size = sizeof(${variable_name});")
  set(source "// Auto generated file.\n${array_definition}\n${size_definition}\n")

  file(WRITE "${source_file_name}" "${source}")
endfunction()

project (electrotheme)

find_package(CLI11 CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(efsw CONFIG REQUIRED)
find_package(curl CONFIG REQUIRED)
find_package(ZLIB REQUIRED)
find_package(libuv CONFIG REQUIRED)

find_path(UWEBSOCKETS_INCLUDE_DIRS "uwebsockets/App.h")

find_program(CMAKE_MC_COMPILER mc.exe windmc.exe HINTS "${sdk_bindir}" "${kit_bindir}" "${kit81_bindir}" ${kit10_bindir}
     DOC "path to message compiler")
MESSAGE(STATUS "CMAKE_MC_COMPILER:${CMAKE_MC_COMPILER}")

add_executable(electrotheme
  src/main.cpp
  src/cli/cli.hpp
  src/cli/appcmd.cpp
  src/cli/editconfigcmd.cpp
  src/cli/openfoldercmd.cpp
  src/cli/startservicecmd.cpp

  src/log.hpp
  src/config.cpp src/config.hpp
  src/util.cpp src/util.hpp
  src/watcher.cpp src/watcher.hpp
  src/js/bundle.hpp

  src/dependencies/easywsclient/easywsclient.cpp src/dependencies/easywsclient/easywsclient.hpp

  src/service/service.cpp src/service/service.hpp
  src/service/loader.cpp src/service/loader.hpp
  src/service/server.cpp src/service/server.hpp
  src/service/eventsink.cpp src/service/eventsink.hpp
  src/service/node.cpp src/service/node.hpp
)

embed_resource("client/dist/index.js" "src/js/bundle.hpp" "jsbundle")

find_library(USOCKETS_LIB usockets)

MESSAGE(STATUS "LIBUV_LIB:${LIBUV_LIB}")
MESSAGE(STATUS "USOCKETS_LIB:${USOCKETS_LIB}")

target_link_libraries(electrotheme PRIVATE
  nlohmann_json::nlohmann_json
  CURL::libcurl
  CLI11::CLI11
  efsw::efsw
  ZLIB::ZLIB
  $<IF:$<TARGET_EXISTS:uv_a>,uv_a,uv> ${USOCKETS_LIB}
  Shlwapi.lib wbemuuid.lib iphlpapi.lib Kernel32.lib Psapi.lib Userenv.lib)
target_include_directories(electrotheme PRIVATE ${UWEBSOCKETS_INCLUDE_DIRS})

set_property(TARGET electrotheme PROPERTY 
  CXX_STANDARD 23
  MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")