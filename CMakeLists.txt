cmake_minimum_required(VERSION 3.16)

project(DummyDir LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(DummyDir main.cpp
    DeterministicPCGPRNG.h DeterministicPCGPRNG.cpp
)

include(GNUInstallDirs)
install(TARGETS DummyDir
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
