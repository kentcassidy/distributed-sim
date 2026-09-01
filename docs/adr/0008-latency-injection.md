# ADR-0008: Inject Network Impairment at the Network Layer (tc netem)

- **Status:** Proposed
- **Date:** 2026-08-31
- **Deciders:** Kent

## Context

The experiment sweeps network delay (and later jitter and loss) against a
physical stability metric. That impairment can be faked inside the federate with
a delay buffer, or applied for real at the network layer beneath the RTI. The
choice affects how credible the result is as a stand-in for real network
conditions, and it interacts with how the federation is deployed for the
experiment (localhost, containers, or VMs).

## Decision

Apply impairment at the **network layer using Linux `tc netem`** on the virtual
interfaces of **containerized federates**. Development happens on localhost;
the experiment runs in containers with netem shaping the links; a single
full-VM run is done at the end to demonstrate the federation crosses real OS
network boundaries.

## Alternatives Considered

- **In-federate delay buffer.** *Buys:* trivial to implement, no infrastructure.
  *Costs:* it is a simulation of delay, not delay; a reviewer rightly discounts
  it, and it can't produce realistic jitter or loss.
- **`tc netem` on containers.** *Buys:* genuine link impairment — latency,
  jitter, reordering, loss — applied exactly as a real test would; per-container
  CPU pinning; fast iteration. *Costs:* requires a container network setup and
  netem familiarity.
- **Full VMs with impairment.** *Buys:* the strongest transparency claim.
  *Costs:* heavy — RAM and cores per guest, JVM-per-federate on top — and slow
  to iterate; unsuitable as the primary experiment harness.

## Consequences

- The latency-stability result rests on real shaped links, which is materially
  more defensible than an internal delay variable.
- The batch ensemble driver is explicitly **not** virtualized or containerized —
  it is a single multi-threaded process — so this decision touches only the
  federated driver.
- A late one-off VM run provides a transparency screenshot and paragraph without
  being the iteration environment.
- The container-vs-VM split, and the netem parameters per sweep point, are
  recorded so the experiment is reproducible.
