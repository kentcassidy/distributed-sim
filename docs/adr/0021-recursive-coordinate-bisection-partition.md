# ADR-0021: Partition the World by Recursive Coordinate Bisection

- **Status:** Accepted
- **Date:** 2026-09-13
- **Deciders:** Kent

## Context

ADR-0015 divides the world into K sectors owned one-per-federate, but not *how*. The first
implementation tiled the world into K equal slabs along a single fixed axis (Y). That was
enough to prove invariance and migration across a seam, but it has two limits worth removing
now that the partition is a headline visual and the scenarios are 3D:

- **Long thin slabs.** A single-axis split of a wide box gives cells with a bad aspect ratio;
  their seams are large, and a plane crossing tests only one direction of handoff.
- **Only equal K.** Equal slabs say nothing about uneven federate counts, and a fixed axis
  can't demonstrate that the handoff logic is *general* (computing the true destination cell,
  not guessing a 2D nearest neighbour).

The operator asked specifically for an *unequal, non-global* split that leans cubic: K=2 splits
the box in two; K=3 splits one half again (1:1:2); K=4 evens out (1:1:1:1); K=5 → 1:1:2:2:2;
and so on — with cells kept as close to cubic as possible.

## Decision

**Tile the world by Recursive Coordinate Bisection (RCB).** Start with the world as one cell;
until there are K cells, take the **largest-volume** cell and split it at the **geometric
midpoint of its longest axis** into two halves. This is a standard kd-tree / RCB partition
(parallel computing, mesh and N-body codes): splitting the *longest* axis drives cells toward a
cubic aspect ratio, and splitting the *largest* cell yields exactly the operator's unequal
ratios for non-power-of-two K.

- **Geometric midpoint, not median-of-aircraft.** The tiling is a pure function of
  `(worldMin, worldMax, K)` and is **independent of where the aircraft are** — which is what
  keeps it a clean, reproducible partition and preserves the invariance argument (the split is
  not data-dependent).
- **Deterministic tie-breaks:** largest cell by volume, ties to the lowest index; longest axis,
  ties X → Y → Z; the split face is exactly the midpoint so `[min, mid)` / `[mid, max)` tile
  with no gap or overlap.
- **Ownership is half-open** (`cellOf`), the same rule the federates apply (`ownerOf`), so a
  point on a seam falls to the upper cell and a point past the outer max face is *outside the
  world* (−1). `slabOf` / the single `Axis` split are removed.

## Alternatives Considered

- **Single-axis equal slabs (the first implementation).** *Buys:* trivial arithmetic; a clean
  first invariance/handoff proof. *Costs:* long thin cells, only equal K, and handoff exercised
  in one direction. Superseded here.
- **A regular kx×ky×kz grid.** *Buys:* simple, symmetric. *Costs:* forces K to factor into three
  integers and can't express the operator's unequal 1:1:2 ratios; less cubic when K isn't a
  perfect cube. Rejected.
- **Median-of-aircraft (load-balancing) RCB.** *Buys:* equal aircraft count per cell — the usual
  reason RCB is used. *Costs:* makes the partition depend on the scenario, muddying the
  "partition is independent of the data" story the invariance claim leans on. Deferred; if
  load-balancing is ever wanted it belongs to the Live/scale work, not the constructive core.

## Consequences

- **Refines ADR-0015.** "K sectors" is now specifically an RCB tiling; the sector list the
  controller disseminates is the RCB cell set, ids 0..K−1 in split order, mapped onto the sorted
  roster.
- **The federate needed no change.** `ownerOf` / `inMySector` already scan an arbitrary cell
  list with the half-open rule, so RCB dropped in behind the existing peer-handoff code
  (ADR-0020). The viewer draws the 3D cells directly from the controller descriptor.
- **Handoff is now exercised in 3D and in every direction**, and a fast aircraft that skips a
  cell is routed to the cell it actually enters (destination is computed, not neighbour-passed).
- **K=2 now splits on X** (the world's longest side, 2500 m), not Y — a visible behaviour change
  from the slab era; scenarios that assumed a Y-seam (e.g. the old `crossing.csv`) move their
  seam accordingly. The seeded random scenario is the intended driver now.
- The tiling stays cheap and deterministic, so invariance across K=1 vs K=N is unaffected: the
  partition changes *who* owns *where*, never the physics.
