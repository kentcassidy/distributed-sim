# ADR-0012: Ubuntu 22.04 + GCC 11 as the Container Base Image and Toolchain

- **Status:** Accepted
- **Date:** 2026-09-01
- **Deciders:** Kent

## Context

The federated driver builds and runs in a container, and the same image is used
for local development, CI, and the experiment so results are reproducible
(charter success criterion: a stranger can clone, build, and reproduce). The
base OS fixes the C library, the available compiler, and how cleanly the RTI
installs. The RTI is Portico (ADR-0001), which is Java-based: its C++ interface
wraps a JVM, and its releases ship as **prebuilt binaries linked against glibc**.
The choice must be made at bootstrap (M0), before the Dockerfile and CI are
written, because every later build inherits it.

Alpine Linux was considered specifically for its small image size.

## Decision

Use **Ubuntu 22.04 LTS (Jammy)** as the container base image and **GCC 11**
(`g++`, C++17) as the compiler, for development, CI, and the experiment alike.
The toolchain is `build-essential`, `cmake`, and Portico wired through CMake.
Portico bundles its own JRE (Java 8), so no system JDK is installed — C++
federates load Portico's own `libjvm.so` from `$RTI_HOME/jre/lib/server`. Editing is done in VS Code with the WSL2 / Dev Containers
extensions so the development environment is the same Linux image as CI.

## Alternatives Considered

- **Ubuntu 22.04 + GCC 11.** *Buys:* glibc, and the exact platform Portico is
  documented to build against (ADR-0001: "modern GCC on recent Ubuntu") — so the
  JVM-backed, glibc-linked Portico binaries install without shims; GCC 11 is the
  distro default; `iproute2` (netem) is one `apt` away for the post-MVP harness.
  *Costs:* a larger image than a musl distro, which does not matter for this
  project.
- **Debian stable + GCC.** *Buys:* also glibc, slightly leaner, near-identical to
  Ubuntu. *Costs:* diverges from the platform wording in ADR-0001 for no
  practical gain.
- **Alpine + musl libc.** *Buys:* the smallest image. *Costs:* musl vs Portico's
  glibc/JVM binaries requires `gcompat`/glibc shims and a musl JDK coexisting
  with glibc-built native libraries — a fragile setup that spends scarce sprint
  days on infrastructure that proves nothing about the competency being shown.
  Rejected.

## Consequences

- The Dockerfile, CI, and experiment share one image, so "works on my machine"
  cannot diverge from "works in CI."
- Portico installs against its tested platform, removing a class of
  loader/linker failures from the schedule's critical path.
- Image size is larger than a musl base; this is judged irrelevant — nothing in
  the project is graded on image size.
- For the MVP, latency is injected with an in-federate delay buffer (ADR-0008),
  so a single build/dev container suffices; the multi-container `tc netem`
  harness (adds `iproute2`, needs `NET_ADMIN`) is a post-9/13 addition on this
  same base image.
- If a genuinely minimal image later matters, revisiting the base is a contained
  change — the toolchain and CMake wiring are base-agnostic — and would supersede
  this ADR.
