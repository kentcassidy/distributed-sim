# ADR-0001: Use Portico as the RTI

- **Status:** Accepted
- **Date:** 2026-08-31
- **Deciders:** Kent

## Context

The federation needs an HLA Run-Time Infrastructure. The choice is effectively
irreversible: federate code binds to the RTI's interface libraries and its
lifecycle semantics, so switching mid-project means rewriting the RTI-facing
layer of every federate. The decision must be made before the first federate is
written.

The candidates are all open source, since commercial RTIs restrict their free
tiers to a small number of federates per federation — a poor fit for a project
whose subject is federation behaviour.

## Decision

Use **Portico** (IEEE 1516e C++ interface) for the federated driver.

## Alternatives Considered

- **Portico.** Actively maintained; current line builds against modern GCC on
  recent Ubuntu. Java-based — the C++ interface wraps a core Java library, so a
  JVM loads behind each C++ federate. *Buys:* the smoothest install and the
  fastest path to a working federation, leaving more of the four weeks for the
  physics and the experiment. *Costs:* a JVM in each federate process and the
  memory that implies, which matters when running federates inside VMs.
- **OpenRTI.** Native C++, CMake-based, 1516 and 1516e. *Buys:* a pure-C++
  process tree with no JVM. *Costs:* more build friction, spent on setup that
  has nothing to do with the competency being demonstrated.
- **CERTI (ONERA).** Native C++, full HLA 1.3, partial 1516-2000/2010. *Buys:*
  native C++ and an established pedigree. *Costs:* the partial 1516e coverage
  must be checked against the specific services this project uses before
  committing.

## Consequences

- A working federation is reachable in week one, protecting the schedule for the
  time-management and experiment milestones.
- The JVM-per-federate footprint constrains how many federates can run inside
  full VMs on one host; the container and localhost paths are unaffected.
- The evidentiary value of the project rests on the C++ **model core** and the
  federation behaviour, not on the absence of Java in the process tree, so the
  JVM cost is judged acceptable.
- If a genuinely native-C++ artifact later becomes important, OpenRTI is the
  documented migration target and this ADR would be superseded.
