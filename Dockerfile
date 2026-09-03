# syntax=docker/dockerfile:1
# ─────────────────────────────────────────────────────────────────────────────
# Distributed Flight-Dynamics Federation — build / dev image
# Base + toolchain locked by ADR-0012: Ubuntu 22.04 LTS + GCC 11 (glibc).
# Portico RTI vendored in (ADR-0001). Portico is self-contained: the RTI, its
# Java dependencies, AND its own bundled JRE (Java 1.8.0_66) ship in the release,
# so NO system JDK is installed — C++ federates load Portico's own libjvm.so.
# One image for development, CI, and the experiment so results reproduce.
# ─────────────────────────────────────────────────────────────────────────────
FROM ubuntu:22.04

# Non-interactive apt, predictable locale.
ENV DEBIAN_FRONTEND=noninteractive \
    LANG=C.UTF-8 \
    LC_ALL=C.UTF-8

# --- Compiler + build system + debug/network tooling ------------------------
#   build-essential : g++ 11, gcc 11, make, libc6-dev (glibc headers)
#   cmake / ninja   : the project build system (charter M0 "CMake skeleton")
#   gdb / valgrind  : local debugging of the C++ federates and model core
#   iproute2        : provides `tc` for netem — POST-MVP; harmless to preinstall
#   curl / ca-certs / git : fetch dependencies, clone, TLS
RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential \
        cmake \
        ninja-build \
        gdb \
        valgrind \
        git \
        curl \
        ca-certificates \
        iproute2 \
        vim \
        tmux \
    && rm -rf /var/lib/apt/lists/*

# --- Portico RTI (vendored) --------------------------------------------------
# The official Portico 2.1.0 linux64 release is vendored in the repo at
# ./third_party/portico-2.1.0-linux64.tar.gz (~80 MB, tracked in git). It is
# self-contained: RTI + Java deps + a bundled JRE (Java 1.8.0_66). The archive's
# top-level dir is portico-2.1.0/, so --strip-components=1 lands it at /opt/portico.
# sha256: 55eaffc11e08e1ad4abc20d58048558ccac96f6f40818271e620f27796416188
ARG PORTICO_VERSION=2.1.0
COPY third_party/portico-2.1.0-linux64.tar.gz /tmp/portico.tar.gz
RUN mkdir -p /opt \
    && tar xzf /tmp/portico.tar.gz -C /opt --strip-components=1 \
    && rm /tmp/portico.tar.gz \
    && mv /opt/portico-2.1.0 /opt/portico

# Portico runtime + build environment for lin64 — taken from the shipped C++
# example's compile/run script, which is the authoritative source for 2.1.0:
#   lib/gcc4              -> native C++ wrapper libs: librti1516e64, libfedtime1516e64
#   jre/lib/amd64/server  -> bundled JVM: libjvm.so + libjsig.so (Java 8 layout;
#                            NOT jre/lib/server, which is the Java 9+ layout)
#   include/ieee1516e     -> HLA 1516e headers (dir is ieee1516e, NOT hla1516e);
#                            used at COMPILE time via -I in CMake, not an env var
#
# Federate build contract for Portico 2.1.0 (belongs in CMake; recorded here so
# the toolchain agreement is visible):
#   g++ -std=gnu++14 -fPIC -I$RTI_HOME/include/ieee1516e -DRTI_USES_STD_FSTREAM \
#       <sources> -L$RTI_HOME/lib/gcc4 -lrti1516e64 -lfedtime1516e64 \
#       -L$RTI_HOME/jre/lib/amd64/server -ljvm -ljsig
#   gnu++14 is REQUIRED: the 2.1.0 headers use throw()-specs and std::auto_ptr,
#   which ISO C++17 removed; GCC 11 defaults to C++17, so C++17 fails to compile.
ENV RTI_HOME=/opt/portico \
    PORTICO_VERSION=${PORTICO_VERSION}
ENV PATH="${RTI_HOME}/bin:${PATH}" \
    LD_LIBRARY_PATH="${RTI_HOME}/lib/gcc4:${RTI_HOME}/jre/lib/amd64/server" \
    CLASSPATH="${RTI_HOME}/lib/portico.jar"

# Image provenance — surfaces in `docker inspect` and SBOM tooling.
LABEL org.opencontainers.image.title="dff-dev" \
      dff.base="ubuntu:22.04" \
      dff.compiler="gcc-11" \
      dff.rti="portico ${PORTICO_VERSION}"

# Source is mounted at runtime (see docker-compose.yml), not baked in — this is a
# development/build image, so edits on the host are picked up without a rebuild.
WORKDIR /work

CMD ["/bin/bash"]
