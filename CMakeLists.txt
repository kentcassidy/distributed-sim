# ═════════════════════════════════════════════════════════════════════════════
# Distributed Flight-Dynamics Federation — root build script
#
# CMake is a build-system *generator*: you describe TARGETS (things to build) and
# their REQUIREMENTS (include dirs, libs, flags), and CMake writes the actual
# Makefiles/Ninja files. You then build those. You never hand-write compiler
# command lines — you attach requirements to targets and CMake composes them.
# ═════════════════════════════════════════════════════════════════════════════
cmake_minimum_required(VERSION 3.16)          # ships with Ubuntu 20.04+/22.04

project(dff
    VERSION     0.1.0
    DESCRIPTION "Distributed Flight-Dynamics Federation"
    LANGUAGES   CXX)

# ─────────────────────────────────────────────────────────────────────────────
# Project-wide C++ ABI (ADR-0014)
# Portico 2.1.0's native libs live in lib/gcc4 and use the PRE-C++11 std::string
# / std::wstring ABI. Everything we build must match, or a std::string crossing
# the boundary between our .o files (or into Portico) crashes at runtime in the
# string copy constructor. This define is global on purpose — the ABI is a
# whole-program property, NOT a per-file choice. It does not limit which C++
# standard any target uses.
# ─────────────────────────────────────────────────────────────────────────────
add_compile_definitions(_GLIBCXX_USE_CXX11_ABI=0)

# ─────────────────────────────────────────────────────────────────────────────
# Locate Portico via RTI_HOME (baked into the dev image — ADR-0012)
# ─────────────────────────────────────────────────────────────────────────────
if(NOT DEFINED ENV{RTI_HOME})
    message(FATAL_ERROR
        "RTI_HOME is not set. Build inside the dff/dev container (docker compose run --rm dev).")
endif()
set(RTI_HOME "$ENV{RTI_HOME}" CACHE PATH "Portico install root")
message(STATUS "Using Portico at: ${RTI_HOME}")

# ─────────────────────────────────────────────────────────────────────────────
# Portico as a reusable INTERFACE target.
# An INTERFACE library builds nothing itself; it just carries "usage
# requirements" that propagate to anything that links it. So every federate we
# ever write links `Portico::Portico` and inherits the include dir, the four
# libs, the search paths, and the RTI_USES_STD_FSTREAM define — the whole build
# contract from Portico's example script, in one place.
# ─────────────────────────────────────────────────────────────────────────────
add_library(portico INTERFACE)
target_include_directories(portico INTERFACE "${RTI_HOME}/include/ieee1516e")
target_compile_definitions(portico INTERFACE RTI_USES_STD_FSTREAM)
target_link_directories(portico INTERFACE
    "${RTI_HOME}/lib/gcc4"                 # native C++ wrapper libs
    "${RTI_HOME}/jre/lib/amd64/server")    # bundled JVM (Java 8 layout)
target_link_libraries(portico INTERFACE
    rti1516e64 fedtime1516e64              # the RTI itself
    jvm jsig)                              # Portico boots this JVM behind the scenes
add_library(Portico::Portico ALIAS portico)

# ─────────────────────────────────────────────────────────────────────────────
# First federate target.
# RTI-facing → must be gnu++14 (see ADR / Portico 2.1.0 headers). CXX_EXTENSIONS
# ON gives gnu++14 rather than strict c++14, matching the example's environment.
# Replace/extend these sources as the real federates take shape (M1+).
# ─────────────────────────────────────────────────────────────────────────────
add_executable(example_federate
    src/federate/main.cpp
    src/federate/ExampleCPPFederate.cpp
    src/federate/ExampleFedAmb.cpp)

target_link_libraries(example_federate PRIVATE Portico::Portico)

set_target_properties(example_federate PROPERTIES
    CXX_STANDARD          14
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS        ON)              # ON → -std=gnu++14

# Cosmetic: silence the "dynamic exception specifications are deprecated" noise
# from Portico's headers. -Wno-deprecated is the correct category (not
# -Wno-deprecated-declarations). Real errors are unaffected.
target_compile_options(example_federate PRIVATE -Wno-deprecated)

# ── Later: your physics core, RTI-free, can be full C++17 ────────────────────
# add_library(dff_core STATIC src/core/aircraft.cpp ...)
# target_compile_features(dff_core PUBLIC cxx_std_17)   # 17 is fine here
# target_link_libraries(example_federate PRIVATE dff_core)
