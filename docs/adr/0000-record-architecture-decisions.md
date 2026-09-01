# ADR-0000: Record Architecture Decisions

- **Status:** Accepted
- **Date:** 2026-08-31
- **Deciders:** Kent

## Context

This project is judged as much on the reasoning behind its choices as on the
code. A reviewer opening the repository cold needs to see not only what was
built but why each significant fork was taken and what was given up. Decisions
made only in my head, or buried in commit messages, are invisible at review
time and impossible to reconstruct later.

## Decision

Every architecturally significant decision is recorded as a numbered Markdown
file in `docs/adr/`, using the template below. A decision is "architecturally
significant" if reversing it later would be costly, if it constrains other
decisions, or if a reviewer might reasonably ask "why did you do it that way?"

Records are immutable once Accepted. A decision is never edited to say something
different; instead a new ADR supersedes it, and the old one is marked
`Superseded by ADR-XXXX`. History is kept, not rewritten.

Numbering is sequential and permanent. `0000` is this record. Substantive
project decisions begin at `0001`.

## Template

```
# ADR-NNNN: <short title in the imperative>

- Status: Proposed | Accepted | Superseded by ADR-XXXX
- Date: YYYY-MM-DD
- Deciders: <names>

## Context
<The forces at play: the problem, the constraints, what makes this a real
choice rather than an obvious one. Written so someone with no prior context
understands why a decision was needed.>

## Decision
<The position taken, stated plainly and in the active voice: "We will ...">

## Alternatives Considered
<Each real option, and specifically what it would have bought and what it
would have cost. An ADR with no alternatives is a note, not a decision.>

## Consequences
<What becomes true now — the good, the bad, and the newly-constrained. What
this decision makes easy and what it makes hard.>
```

## Consequences

- The repository carries a visible decision trail from the first day.
- Writing a record costs a few minutes per decision; this is capped as part of
  the project's overall 15% documentation budget.
- The numbered set doubles as the backbone of the final V&V report's design
  section — the report references ADRs rather than re-arguing them.
- ADR-0001 through ADR-0011 are stubbed at project start with status `Proposed`,
  each capturing a decision that must be made before or during early
  development. They move to `Accepted` as each is resolved, so the board and the
  ADR set together show what is still open.
