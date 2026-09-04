# ADR-0003: C++ for Model, Federates, and Ensemble; Scripting Only for Plots

- **Status:** Accepted
- **Date:** 2026-08-31
- **Deciders:** Kent

## Context

A prior review recorded doubt about C++ depth. The language in which the
substantive work is written is therefore itself part of the argument. At the
same time, plotting and light statistical post-processing are far faster in a
scripting language, and no reviewer assesses C++ ability from a plotting script.

## Decision

The **model core, the federates, and the Monte Carlo ensemble driver are C++**.
Figure generation and light post-processing may use a scripting language
(Python). No component that carries the technical argument is written in
anything but C++.

## Alternatives Considered

- **Pure C++ everywhere.** *Buys:* every artifact is the language in question.
  *Costs:* hand-rolling plotting and stats in C++ spends days on work that
  proves nothing.
- **C++ core, scripting for analysis and stats.** *Buys:* fast analysis while
  keeping the physics and ensemble — the parts that demonstrate the skill — in
  C++. *Costs:* two languages in the repo; discipline required to keep the
  boundary where it belongs.
- **Scripting-heavy with a thin C++ shell.** *Buys:* fastest development.
  *Costs:* fails the project's core purpose entirely; rejected outright.

## Consequences

- The physics and the ensemble statistics are computed in C++, so the
  evidentiary claim holds.
- A clear line divides "computation" (C++) from "presentation" (script). The
  line is documented in the README so the split reads as intentional.
- CSV is the interchange format between the C++ side and the plotting side,
  which also serves the reproducibility requirement (figures regenerate from
  committed data).
