cmake_minimum_required(VERSION 3.16)

project(DummyDir VERSION 0.1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(DummyDir main.cpp
    DDDeterministicPCGPRNG.h DDDeterministicPCGPRNG.cpp
    DDParameters.h DDParameters.cpp
    DDManifest.h DDManifest.cpp
    DDFile.h DDFile.cpp
    DDDirectory.h DDDirectory.cpp
    DDOperation.h DDOperation.cpp
    BS_thread_pool.hpp
    DDMD5Hasher.h DDMD5Hasher.cpp
)

# We want to make the build timestamp available
string(TIMESTAMP BUILD_TIMESTAMP "%m/%d/%Y %H:%M:%S")

# Generate a header file with the version information filled in by cmake
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake_vals.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/generated/cmake_vals.h
    @ONLY
)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/generated)

include(GNUInstallDirs)
install(TARGETS DummyDir
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
