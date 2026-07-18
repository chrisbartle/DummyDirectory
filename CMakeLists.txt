cmake_minimum_required(VERSION 3.16)

project(DummyDir VERSION 0.9.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(dummydir main.cpp
    DDDeterministicPCGPRNG.h DDDeterministicPCGPRNG.cpp
    DDParameters.h DDParameters.cpp
    DDManifest.h DDManifest.cpp
    DDFile.h DDFile.cpp
    DDDirectory.h DDDirectory.cpp
    DDOperation.h DDOperation.cpp
    BS_thread_pool.hpp
    DDMD5Hasher.h DDMD5Hasher.cpp
    DDOperationAdd.h DDOperationAdd.cpp
    DDOperationAddDirectory.h DDOperationAddDirectory.cpp
    DDOperationVerify.h DDOperationVerify.cpp
    DDOperationClean.h DDOperationClean.cpp
    DDOperationDelete.h DDOperationDelete.cpp
    DDOperationRebuild.h DDOperationRebuild.cpp
    DDOperationModify.h DDOperationModify.cpp
    DDOperationRename.h DDOperationRename.cpp
    DDOperationMove.h DDOperationMove.cpp
    DDOperationRenameDirectory.h DDOperationRenameDirectory.cpp
    DDOperationDeleteDirectory.cpp DDOperationDeleteDirectory.h
    DDOperationMoveDirectory.cpp DDOperationMoveDirectory.h
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
install(TARGETS dummydir
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
