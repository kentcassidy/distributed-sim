# CLAUDE.md — Operating context for the Distributed Flight-Dynamics Federation

This file primes any agent working on this repo. **Read it fully, then read the
sources it points to, before making changes or assumptions.** It exists because
past sessions drifted by *assuming* instead of *reading* — see "How to work here".

---

## What this project actually is

A **distributed-systems proof of concept**: an IEEE 1516e (HLA) federation in C++
where the simulation of N aircraft is partitioned across K federates/containers by
config. **The star is the distributed system, not the flight physics.** Judge every
decision by whether it serves the systems result.

## Read before acting — in this order

1. `docs/project_charter_v0_5_0.html` — the authoritative planning doc. The
   **Scope · MVP tab governs**; later tabs are the full vision/reference. Read the
   Scope tab *in full* — do not work from a summary of it.
2. The latest entry in `docs/devlog/Claude Generated/` — most recent session state.
3. The auto-memory files (via their `MEMORY.md` index) — durable decisions + the
   pickup pointer (`current-state-pickup`).

Do **not** run on stale memory notes alone. Memory is a point-in-time hint; the
charter and the "Current scope decisions" below are the source of truth. Verify
against code before asserting file/line facts.

## The two experiments — DO NOT CONFLATE (the #1 source of confusion)

The project has **two distinct simulations** (the charter's "two claims, in order").
Mixing them up is the mistake to avoid:

**1. Constructive — the MVP core (build this first).**
- Fully deterministic, **bit-exact**. Each federate computes the EXACT truth for its
  aircraft, every single step, and reports that truth to the controller over the RTI
  (streamed live, or flushed per sector/handoff — a delivery choice).
- The claim: assemble the truth; run the same seeded scenario partitioned different
  ways (K=1 co-located vs K=2 split); the result must be **identical to the bit**
  (bit-exact on one host/image; tight-tolerance only across heterogeneous hosts, per
  floating-point ordering).
- **Ghosts and dead reckoning are NOT part of this path.** No thresholds, no accepted
  error, no approximation.
- **Determinism is sacred:** fixed `dt`, fixed iteration order, conservative time
  management, same seed, turbulence off, no RNG in the truth path. Anything that makes
  truth vary between partitionings is a bug.

**2. Live — a later experiment (NOT the core).**
- Deliberately injects network latency; each federate **dead-reckons ghosts** of the
  others and accepts *bounded* error.
- Measures graceful degradation vs. delay. **Ghosts are load-bearing HERE, and only
  here.** This is the charter's "controlled breakdown / distribution drift", and it
  comes *after* the constructive result.

> If you reach for ghosts / dead reckoning / error thresholds while working on the
> core invariance result, stop — you've conflated the two experiments.

## Current scope decisions (live; supersede any stale doc text)

- **Physics is disposable stable filler.** Do NOT source real aerodynamic data or
  chase fidelity. Coefficients are arbitrary-but-stable and deterministic (see
  `src/core/AircraftParams.hpp`). Fidelity is deliberately out of scope.
- **The published-mode eigenvalue check is BACKLOGGED** (charter M2-6 / ADR-0005 /
  Success Criterion #3). With made-up numbers there is no external truth to verify
  against, so it's cut. *(This is a known deviation from the charter text — pending a
  doc-sync the user will approve.)*
- **V&V actually performed = (A) basic stable functionality + (B) partition
  invariance.** Nothing more for the MVP.
- **Logging/visualization seam = NDJSON** (not CSV). Browser Canvas 2D viewer; replay
  first, optional realtime via Server-Sent Events. C++ only **emits** NDJSON — no web
  server or DB in the federate. Viz is off the critical path; build it after the
  constructive MVP runs. (Memory: `viz-logging-ndjson-pipeline`.)

## How to work here (safeguards against assuming)

- **Do NOT build or compile on the host.** The user builds in their OWN container
  (Ubuntu 22.04 + GCC 11, WSL2 / Docker, ADR-0012). Hand over code and let them
  compile; do not hunt for host toolchains or run `cmake`/`g++` on the host.
- **Do NOT web-search for or invent external "truth"/reference data.** Not needed.
- **Do NOT run ahead of scope or assume the next step.** When intent is ambiguous,
  *ask*. The user prefers small, understandable, incremental steps.
- **Explain code patiently when implementing.** The user is learning the
  flight-dynamics / HLA domain and wants to follow along, line by line.
- **Keep the simulation surface minimal.**
- **Never edit the user's handwritten devlog** (`docs/devlog/Handwritten/`).
  Claude-generated logs go in `docs/devlog/Claude Generated/`.
- Respect the charter's hard rules: **consume the RTI — do not build a transport,
  serialization, or synchronization layer**; documentation ≤ 15% of effort; strict
  isolation from other independent work (ADR-0009).

## Where things stand (pickup)

See `current-state-pickup` (memory) and the latest `docs/devlog/Claude Generated/`
entry. As of **2026-09-10**: the RTI-free core is in (`LinearLongitudinal::derivative`
filler; `World::advance`; `Sector::contains`/`inHalo`; `World::outOfSector`) and the
aircraft federate publishes real `Position` and writes NDJSON. **Off the critical path,
a full 3D worldspace viewer was built** in `viz/` (Vue + Vite + Three.js) that replays
the NDJSON truth — synced per-federate viewpoints, playback, theming. It is NOT part of
the C++ build (`viz/` is walled off) and the federate still only *emits* NDJSON.

**Resolved:** the **live-coupling** question is closed — aircraft are **independent** in
the constructive core, so truth can be flushed per sector/handoff; the invariance's
teeth are in handoff/migration, not interaction. (Ghosts/DR/latency = the later *Live*
experiment only.)

**Next — the C++ library (in order):**
1. Emit the evolved NDJSON: patch `AircraftFederate.cpp` to write a `meta` line + a
   per-aircraft `role` (exact change specced in `viz/NDJSON_SCHEMA.md`). User compiles
   in their container.
2. **Partition-by-config** (the constructive core): replace the name-derived single
   aircraft with a config-driven **list** from a shared scenario (id→IC); tell each
   federate which ids it owns (K=1 co-located vs K=2 split). `src/controller/` is the
   home for the assignment. Then the **K=1-vs-K=2 truth diff** on fixed dt / order /
   seed. Static assignment first; migration/handoff second (the real teeth).
