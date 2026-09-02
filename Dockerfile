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
    && rm -rf /var/lib/apt/lists/*

# --- Portico RTI (vendored) --------------------------------------------------
# Place your Portico release at ./third_party/portico.tgz in the build context.
# Any tarball with a single top-level directory works (--strip-components=1
# normalizes it into /opt/portico regardless of that dir's name). To produce one
# from your working install:  tar czf third_party/portico.tgz -C /opt portico
#
# Record provenance for the SBOM: set PORTICO_VERSION and keep the tarball's
# sha256 in docs (see the SBOM note).
ARG PORTICO_VERSION=unknown
COPY third_party/portico.tgz /tmp/portico.tgz
RUN mkdir -p /opt/portico \
    && tar xzf /tmp/portico.tgz -C /opt/portico --strip-components=1 \
    && rm /tmp/portico.tgz

# Portico runtime + build environment for lin64 (per Portico's README):
#   lib/gcc4        -> native C++ wrapper libs  ([compiler] = gcc4; VERIFY with
#                      `ls /opt/portico/lib` and change this token if different)
#   jre/lib/server  -> Portico's bundled JVM (libjvm.so), loaded behind federates
#   include/hla1516e-> HLA 1516e headers, used at COMPILE time via -I (in CMake,
#                      not an env var)
ENV RTI_HOME=/opt/portico \
    PORTICO_VERSION=${PORTICO_VERSION}
ENV PATH="${RTI_HOME}/bin:${PATH}" \
    LD_LIBRARY_PATH="${RTI_HOME}/lib/gcc4:${RTI_HOME}/jre/lib/server" \
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
