cmake_minimum_required(VERSION 3.16)

project(DummyDir VERSION 1.0.3.0 LANGUAGES CXX)

#Default to release build so that users can build on location
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

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
    DDReplay.h DDReplay.cpp
)
target_link_libraries(dummydir PRIVATE Threads::Threads)

# We want to make the build timestamp available
string(TIMESTAMP BUILD_TIMESTAMP "%m/%d/%Y %H:%M:%S")

# Generate a header file with the version information filled in by cmake
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake_vals.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/generated/cmake_vals.h
    @ONLY
)
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/docs/dummydir.1.in
    ${CMAKE_CURRENT_BINARY_DIR}/generated/dummydir.1
    @ONLY
)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/generated)

# Generate an rc file with the version information filled in by cmake
set(APP_ICON "${CMAKE_CURRENT_SOURCE_DIR}/windows/DD.ico")
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/windows/DD.rc.in
    ${CMAKE_CURRENT_BINARY_DIR}/generated/DD.rc
    @ONLY
)

if(WIN32)
    target_sources(dummydir PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated/DD.rc)
endif()

include(GNUInstallDirs)
install(TARGETS dummydir DESTINATION bin)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/generated/dummydir.1 DESTINATION share/man/man1)

# Tests. Run them with "ctest --test-dir build --output-on-failure".
enable_testing()
find_package(Python3 COMPONENTS Interpreter)
if(Python3_FOUND)
    add_test(NAME lifecycle
        COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/lifecycle_test.py
                $<TARGET_FILE:dummydir>)
    # Each case reproduces a bug that was found and fixed, so a reintroduced fault
    # is reported by name rather than as an unexplained change in behaviour.
    add_test(NAME regression
        COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/regression_test.py
                $<TARGET_FILE:dummydir>)
    # The sequence builds a couple of hundred files twice over, so give it room on a
    # slow or loaded machine without letting a genuine hang run forever.
    set_tests_properties(lifecycle regression PROPERTIES TIMEOUT 600)
else()
    message(WARNING "Python3 not found - the lifecycle and regression tests will not be registered.")
endif()

#CPACK stuff
set(CPACK_PACKAGE_NAME "DummyDir")
set(CPACK_PACKAGE_VENDOR "Chris Bartle")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "A utility to generate a directory of dummy data")
set(CPACK_PACKAGE_VERSION_MAJOR "${PROJECT_VERSION_MAJOR}")
set(CPACK_PACKAGE_VERSION_MINOR "${PROJECT_VERSION_MINOR}")
set(CPACK_PACKAGE_VERSION_PATCH "${PROJECT_VERSION_PATCH}")
set(CPACK_PACKAGE_CONTACT "software@chrisbartle.com")

if(WIN32)
    set(CPACK_GENERATOR "ZIP")
elseif(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
    set(CPACK_GENERATOR "TGZ")
else() # Linux
    set(CPACK_GENERATOR "DEB;RPM;TGZ")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Chris Bartle <software@chrisbartle.com>")
    set(CPACK_RPM_PACKAGE_LICENSE "BSD3")
endif()

# 3. Include CPack
include(CPack)
