# ADR-0004: Frame the Project as a Validation Study, Present It as a Result

- **Status:** Accepted
- **Date:** 2026-08-31
- **Deciders:** Kent

## Context

How the project is described determines how it is read. The same codebase can
present as a student exercise or as work adjacent to a test organization's
actual mission. This is a framing decision, not a technical one, but it governs
the README's first paragraph, the report's abstract, and any note that
accompanies the work — so it is recorded like any other decision.

## Decision

Frame the work, underneath, as a **validation study**: whether federated
execution preserves the analytical results of a monolithic run. **Present** it,
on the surface, as a **concrete result** — how network degradation shifts the
statistics of a stochastically-forced two-aircraft scenario. Lead with the
result; let the validation significance be what the reader concludes.

## Alternatives Considered

- **"A distributed flight simulator."** *Buys:* accurate, simple. *Costs:*
  reads as a class project; invites the "like a videogame" dismissal.
- **"How network degradation shifts scenario statistics."** *Buys:* frames the
  work as an analysis question of the kind the target organization exists to
  answer. *Costs:* none of note; this is the presentation layer.
- **"A validation study of federated-vs-monolithic analytical equivalence."**
  *Buys:* the most senior framing, mapping directly onto a V&V mission. *Costs:*
  as an opening line it can read as overclaiming if the result doesn't back it —
  so it belongs underneath, earned by the data, not asserted up front.

## Consequences

- The README opens with the concrete question and its answer, not with a mission
  statement.
- The report's structure follows V&V doctrine, so the validation framing is
  demonstrated by organization rather than announced.
- Any accompanying note stays concrete and free of self-assessment; the framing
  is allowed to land on its own.
