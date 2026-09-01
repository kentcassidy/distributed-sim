# ADR-0009: Strict Isolation From Other Independent Work

- **Status:** Accepted
- **Date:** 2026-08-31
- **Deciders:** Kent

## Context

A separate, independently-developed body of work exists whose clean provenance —
independent development, sole ownership, no entanglement — is valuable and must
be preserved. This project is public (ADR-0002) and will be shared with a
government hiring official. Any bleed between the two would create provenance
questions about the separate work and muddy the story this project is meant to
tell. Unlike most ADRs here, this one is Accepted from the outset: it is a
constraint, not an open choice.

## Decision

This project shares **nothing** with any other independent work: no source, no
repository, no license, no dependency, no design lineage, and no cross-reference
in commits, issues, or documentation. Concept transfer is permitted and
expected — general techniques learned in one inform the other — but no file,
identifier, or text crosses in either direction.

## Alternatives Considered

- **Strict isolation (this decision).** *Buys:* the separate work's clean
  provenance is preserved intact; this project stands entirely on its own.
  *Costs:* some techniques must be re-derived and re-expressed from scratch
  rather than reused.
- **Shared utilities or tooling.** *Buys:* minor time savings on common code.
  *Costs:* creates exactly the entanglement and provenance ambiguity the
  constraint exists to prevent. Rejected outright.

## Consequences

- Any generic utility needed here is written fresh for this repository.
- Reviews of commits and documentation include an explicit check that no
  cross-reference or shared artifact has leaked in.
- Because the repo is public, this constraint is load-bearing rather than
  cosmetic; a violation would be visible and costly.
- Nothing work-derived or classified enters either, for the same reasons.
