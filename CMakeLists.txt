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

find_library(USOCKETS_LIB usockets)
find_path(UWEBSOCKETS_INCLUDE_DIRS "uwebsockets/App.h")

file(GLOB_RECURSE SRC CONFIGURE_DEPENDS src/*.cpp src/*.hpp src/*.c src/*.h)
add_executable(electrotheme ${SRC})

embed_resource("client/dist/index.js" "src/js/bundle.hpp" "jsbundle")

target_link_libraries(electrotheme PRIVATE
  nlohmann_json::nlohmann_json
  CURL::libcurl
  CLI11::CLI11
  efsw::efsw
  ZLIB::ZLIB
  $<IF:$<TARGET_EXISTS:uv_a>,uv_a,uv> ${USOCKETS_LIB}
  Shlwapi.lib wbemuuid.lib iphlpapi.lib Kernel32.lib Psapi.lib Userenv.lib)
target_include_directories(electrotheme PRIVATE ${UWEBSOCKETS_INCLUDE_DIRS})
target_compile_definitions(electrotheme PRIVATE
  WIN32_LEAN_AND_MEAN
  VC_EXTRALEAN
)
set_target_properties(electrotheme PROPERTIES 
  CXX_STANDARD 23
  MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:DEBUG>:Debug>"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/out"
)
