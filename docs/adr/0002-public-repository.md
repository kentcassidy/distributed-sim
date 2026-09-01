# ADR-0002: Public Repository From the First Commit

- **Status:** Proposed
- **Date:** 2026-08-31
- **Deciders:** Kent

## Context

The project exists in part to make visible a body of C++ and distributed-systems
work that a prior review could not see. The commit history itself — its
cadence, its conventional-commit discipline, the accreting ADRs and closing
issues — is part of the evidence, not just scaffolding around it. The
visibility choice locks early: a history made public only at the end, squashed
into existence, reads as manufactured.

## Decision

The repository is **public from the first commit**, with full history preserved
and never squashed or rewritten.

## Alternatives Considered

- **Public from commit one.** *Buys:* a genuine, timestamped development record
  that substantiates the process claims a reviewer cannot otherwise verify.
  *Costs:* the work is visible while rough; early mistakes are part of the
  record.
- **Private until presentable.** *Buys:* freedom to be messy without an
  audience. *Costs:* the history loses its evidentiary weight — a clean repo
  that appeared fully-formed proves far less than one a reviewer watched grow.

## Consequences

- The development history becomes a first-class deliverable.
- Early roughness is accepted as the price of authenticity; it is not a defect
  to hide but part of what makes the record credible.
- The isolation constraint (ADR-0009 territory) becomes load-bearing: because
  the repo is public, it must contain no reference to, or artifact of, any
  separate independent work.
- Nothing sensitive, work-derived, or classified can ever enter the repository,
  since it is public from the outset.
