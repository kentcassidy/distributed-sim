# ═════════════════════════════════════════════════════════════════════════════
# Distributed Flight-Dynamics Federation — root build script
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
# whole-program property, NOT a per-file choice. It applies to every target
# added below (dff_core included), and it does not limit which C++ standard a
# target uses.
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
# Subprojects. Order matters: dff_core is RTI-free and is linked by both the
# aircraft federate and the controller, so it is added first.
#
#   src/core        dff_core           — the model. C++17, MUST NOT touch Portico.
#   src/federate    aircraft_federate  — RTI-facing sim process. gnu++14.
#   src/controller  controller         — sector-assignment federate. gnu++14.
#   examples/…      example_federate   — Portico's shipped demo, kept green as
#                                        the toolchain proof (not part of DFF).
#
# The standard split (C++17 core vs gnu++14 RTI code) lives per-target inside
# each subdirectory's CMakeLists.txt; the ABI define above is shared by all.
# ─────────────────────────────────────────────────────────────────────────────
add_subdirectory(src/core)
add_subdirectory(src/federate)
add_subdirectory(src/controller)
add_subdirectory(examples/portico-example)
