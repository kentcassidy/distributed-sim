# ADR-0014: Build the Whole Project Against Portico's Old C++ ABI (and gnu++14 for RTI-facing code)

- **Status:** Accepted
- **Date:** 2026-09-02
- **Deciders:** Kent

## Context

The RTI is Portico 2.1.0 (the only downloadable binary; ADR-0001), consumed on
Ubuntu 22.04 / GCC 11 (ADR-0012). Portico 2.1.0 predates that toolchain, which
surfaces two independent friction points when building federates:

1. **C++ ABI.** Portico's native libraries live in `lib/gcc4` and were built with
   the pre-C++11 `std::string` / `std::wstring` ABI (`_GLIBCXX_USE_CXX11_ABI=0`).
   GCC 11 defaults to the newer ABI (`=1`). Because Portico's API passes
   `std::wstring` (federation/federate names), a mismatch **compiles and links
   cleanly but crashes at runtime** with `SIGSEGV` inside the `std::wstring` copy
   constructor when an object crosses the boundary — observed exactly this way
   building the shipped C++ example.
2. **Language standard.** The 2.1.0 headers use dynamic exception specifications
   (`throw(...)`) and `std::auto_ptr`, both removed in C++17. GCC 11 defaults to
   C++17, so RTI-facing translation units fail to compile as C++17.

The ABI point is a whole-program property; the standard point is per-file.

## Decision

Adapt the build to Portico rather than change the RTI or the OS:

- Compile the **entire project** with **`-D_GLIBCXX_USE_CXX11_ABI=0`**, set once
  globally in CMake (`add_compile_definitions`). The ABI must be uniform across
  every translation unit, so this is intentionally not per-target.
- Compile **RTI-facing targets** (anything that includes Portico headers) as
  **`gnu++14`** (`CXX_STANDARD 14`, extensions on). Targets that never include
  Portico headers — notably the physics **model core** — remain free to use
  **C++17**; the ABI define does not restrict language version.

## Alternatives Considered

- **Old ABI project-wide + gnu++14 for RTI files (chosen).** *Buys:* one define
  and a per-target standard; the example runs; modern C++ still available in the
  core. *Costs:* the whole project is old-ABI, so a future new-ABI C++
  dependency that exchanges `std::string` would clash.
- **Façade: isolate all Portico contact behind an adapter whose public header
  exposes no `std::wstring`.** *Buys:* only the adapter is old-ABI/gnu++14; the
  rest could be pristine modern C++. *Costs:* real boilerplate and a conversion
  seam, protecting against a dependency the project does not have. Over-
  engineering for a 13-day MVP; kept as a documented future option.
- **Rebuild Portico 2.1.0 from source with the new ABI.** *Buys:* no ABI flag.
  *Costs:* a Gradle/Java build with no bundled JRE — a schedule rabbit hole.
- **Move to Portico 2.2.x (new-ABI binary).** Not available for download; source
  only. Rejected on availability.

## Consequences

- `CMakeLists.txt` carries `add_compile_definitions(_GLIBCXX_USE_CXX11_ABI=0)` as
  the single source of truth; the Dockerfile comment records the full build
  contract; nothing in the vendored Portico tarball is modified.
- `std::string` uses the copy-on-write layout rather than short-string
  optimization — irrelevant to this project's performance.
- The physics core can and does stay C++17; only RTI-facing files are gnu++14.
- **Future risk, stated plainly:** linking a C++ library built with the default
  new ABI that trades `std::string` with our code would reintroduce the crash.
  None is used today (physics + RTI + our own CSV; plotting is separate Python).
  If one becomes necessary, introduce the façade rather than flip the global ABI.
- Relates to ADR-0001 (Portico) and ADR-0012 (base image / toolchain).
