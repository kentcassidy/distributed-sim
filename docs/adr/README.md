# Architecture Decision Records

This directory records the significant decisions behind the project, one file
per decision, following the scheme established in
[ADR-0000](0000-record-architecture-decisions.md).

Records are immutable once **Accepted**. A decision is never rewritten; a later
ADR supersedes it, and the superseded record is marked accordingly.

| # | Decision | Status | Resolve by |
|---|----------|--------|-----------|
| [0000](0000-record-architecture-decisions.md) | Record architecture decisions | Accepted | — |
| [0001](0001-rti-selection.md) | Use Portico as the RTI | Proposed | M0 |
| [0002](0002-public-repository.md) | Public repository from the first commit | Proposed | M0 |
| [0003](0003-language-discipline.md) | C++ for model/federates/ensemble; scripting only for plots | Proposed | M0 |
| [0004](0004-project-framing.md) | Frame as validation study, present as result | Proposed | M0 |
| [0005](0005-model-fidelity.md) | Linearized longitudinal model as fidelity floor | Proposed | M2 |
| [0006](0006-model-core-and-drivers.md) | Separate model core from execution drivers | Proposed | M1 |
| [0007](0007-coupled-formation-scenario.md) | Couple the aircraft via formation-keeping | Proposed | M3 |
| [0008](0008-latency-injection.md) | Inject impairment at the network layer (tc netem) | Proposed | M4 |
| [0009](0009-isolation-constraint.md) | Strict isolation from other independent work | Accepted | — |
| [0010](0010-storm-and-turbulence.md) | Rankine vortex storm with Dryden turbulence | Proposed | M2 |
| [0011](0011-constructive-scope-virtual-accommodated.md) | Constructive scope; virtual accommodated by design | Proposed | M1 |
| [0012](0012-container-base-image-and-toolchain.md) | Ubuntu 22.04 + GCC 11 base image and toolchain | Accepted | — |
| [0013](0013-config-driven-entity-partition-model.md) | Config-driven entity/partition model — N aircraft over K containers | Accepted | — |
| [0014](0014-portico-old-cxx-abi.md) | Build against Portico's old C++ ABI; gnu++14 for RTI-facing code | Accepted | — |

**Status key.** *Proposed* — the decision is recorded but not yet locked; it
moves to *Accepted* when resolved at or before the noted milestone. *Accepted* —
locked; supersede rather than edit. *Superseded by ADR-XXXX* — replaced.
