cmake_minimum_required(VERSION 3.15)

###################################################################################################
## These variables are passed to oatpp-module-install.cmake script
## use these variables to configure module installation

set(OATPP_THIS_MODULE_NAME oatpp-flatbuffers) ## name of the module (also name of folders in installation dirs)
set(OATPP_THIS_MODULE_VERSION "1.4.0") ## version of the module (also sufix of folders in installation dirs)
set(OATPP_THIS_MODULE_LIBRARIES oatpp-flatbuffers) ## list of libraries to find when find_package is called
set(OATPP_THIS_MODULE_TARGETS oatpp-flatbuffers) ## list of targets to install
set(OATPP_THIS_MODULE_DIRECTORIES oatpp-flatbuffers) ## list of directories to install

###################################################################################################

project(${OATPP_THIS_MODULE_NAME}
  VERSION ${OATPP_THIS_MODULE_VERSION}
  LANGUAGES CXX
)

option(BUILD_SHARED_LIBS "Build shared libraries" OFF)
option(OATPP_DIR_SRC "Path to oatpp module directory (sources)")
option(OATPP_DIR_LIB "Path to directory with liboatpp (directory containing ex: liboatpp.so or liboatpp.dynlib)")
option(OATPP_BUILD_TESTS "Build tests for this module" ON)
option(OATPP_INSTALL "Install module binaries" ON)

set(OATPP_MODULES_LOCATION "INSTALLED" CACHE STRING "Location where to find oatpp modules. can be [INSTALLED|EXTERNAL|CUSTOM]")

###################################################################################################
## get oatpp main module in specified location

set(OATPP_MODULES_LOCATION_INSTALLED INSTALLED)
set(OATPP_MODULES_LOCATION_EXTERNAL EXTERNAL)
set(OATPP_MODULES_LOCATION_CUSTOM CUSTOM)

if(OATPP_MODULES_LOCATION STREQUAL OATPP_MODULES_LOCATION_INSTALLED)

  message("Finding oatpp in location=INSTALLED")

  find_package(oatpp ${OATPP_THIS_MODULE_VERSION} REQUIRED)

  get_target_property(OATPP_INCLUDE oatpp::oatpp INTERFACE_INCLUDE_DIRECTORIES)
  message("OATPP_INCLUDE=${OATPP_INCLUDE}")

  get_target_property(OATPP_TEST_INCLUDE oatpp::oatpp-test INTERFACE_INCLUDE_DIRECTORIES)
  message("OATPP_TEST_INCLUDE=${OATPP_TEST_INCLUDE}")

elseif(OATPP_MODULES_LOCATION STREQUAL OATPP_MODULES_LOCATION_EXTERNAL)

  message("Finding oatpp in location=EXTERNAL")

  include(ExternalProject)

  set(MODULE_WAIT_DEPS ON)

  set(LIB_OATPP_EXTERNAL "lib_oatpp_external")
  ExternalProject_Add(${LIB_OATPP_EXTERNAL}
    GIT_REPOSITORY "https://github.com/oatpp/oatpp.git"
    GIT_TAG origin/master
    CMAKE_ARGS -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF -DOATPP_INSTALL=OFF -DOATPP_BUILD_TESTS=OFF
    INSTALL_COMMAND cmake -E echo "SKIP INSTALL '${LIB_OATPP_EXTERNAL}'"
  )

  ExternalProject_Get_Property(${LIB_OATPP_EXTERNAL} BINARY_DIR)
  set(OATPP_DIR_LIB ${BINARY_DIR}/src)

  ExternalProject_Get_Property(${LIB_OATPP_EXTERNAL} SOURCE_DIR)
  set(OATPP_DIR_SRC ${SOURCE_DIR}/src)

  message("OATPP_DIR_SRC --> '${OATPP_DIR_SRC}'")
  message("OATPP_DIR_LIB --> '${OATPP_DIR_LIB}'")

elseif(OATPP_MODULES_LOCATION STREQUAL OATPP_MODULES_LOCATION_CUSTOM)

  message("Finding oatpp in location=CUSTOM")

  message("OATPP_DIR_SRC --> '${OATPP_DIR_SRC}'")
  message("OATPP_DIR_LIB --> '${OATPP_DIR_LIB}'")

else()
  message("FATAL_ERROR Unknown location to find oatpp '${OATPP_MODULES_LOCATION}'")
endif()

if(OATPP_DIR_LIB)
  link_directories(${OATPP_DIR_LIB})
endif()

###################################################################################################
## get dependencies

message("\n############################################################################")
message("## ${OATPP_THIS_MODULE_NAME} module. Resolving dependencies...\n")

#list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake/module")
find_package(flatbuffers REQUIRED)

message("\n############################################################################\n")

###################################################################################################
## define targets

# 设置全局编译选项
# 保留警告级别，但禁用"将警告当作错误"，避免第三方库编译问题
if(MSVC)
  add_compile_options(/W0)  # 使用最高警告级别，但不将警告当作错误
  add_compile_options(/wd4996)  # 禁用C++98兼容性警告
  # 解决Windows下min/max宏冲突问题
  add_compile_definitions(NOMINMAX)
  # add_compile_definitions("UNICODE")
  # add_compile_definitions("_UNICODE")
  # add_compile_definitions("$<$<C_COMPILER_ID:MSVC>:/execution-charset:utf-8>")
else()
  # add_compile_options(-Wall -Wextra -Wpedantic)  # 使用严格警告，但不将警告当作错误
  add_compile_options(-w)  # 禁用所有警告
  add_compile_options(-Wno-c++98-compat)  # 禁用C++98兼容性警告
endif()

add_definitions("-DUNICODE")
add_definitions("-D_UNICODE")

if (MSVC AND MSVC_VERSION GREATER 1913)
  string(APPEND CMAKE_CXX_FLAGS " /Zc:__cplusplus")
  add_compile_options("$<$<C_COMPILER_ID:MSVC>:/utf-8>")
  add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
  message(STATUS "MSVC_VERSION: ${MSVC_VERSION}")
endif()

# 设置构建类型
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# 调试信息配置
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  if(MSVC)
    add_compile_options(/Zi /Od)  # 生成调试信息，禁用优化
    add_link_options(/DEBUG)      # 链接调试信息
  else()
    add_compile_options(-g -O0)   # 生成调试信息，禁用优化
  endif()
endif()

# 设置输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

include(cmake/module-utils.cmake)

add_subdirectory("src")

if(OATPP_BUILD_TESTS)
  add_subdirectory("test")
endif()