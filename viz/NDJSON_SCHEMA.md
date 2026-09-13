# DFF NDJSON schema (v1)

The seam between the C++ federates (which only *emit*) and the viewer (which only
*reads*). **One file per federate.** An optional `meta` line comes first; every other
line is one simulation step.

## meta line (first line, optional)

```json
{"meta":{"federate":"F1","dt":0.1,"sectors":[]}}
```

- `federate` — this file's federate name. Falls back to the filename without
  `.ndjson` (so `F1.ndjson` → `"F1"`) when absent.
- `dt` — fixed timestep, informational.
- `sectors` — the world partition, each `{"id":N,"owner":"<federate>","min":[x,y,z],"max":[x,y,z]}`.
  The **controller** emits the authoritative full list (see below); aircraft federates emit
  `[]` (they don't infer geometry).
- `world` — *(controller file only)* the overall volume `{"min":[x,y,z],"max":[x,y,z]}`.

### controller descriptor file (`controller.ndjson`)

The controller writes one **meta-only** file (no frames) giving the authoritative partition, so
the viewer draws the world + sector boxes directly instead of inferring them from federate truth:

```json
{"meta":{"federate":"controller","dt":0.1,"world":{"min":[0,0,-500],"max":[20000,1500,500]},"sectors":[{"id":0,"owner":"fed1","min":[0,0,-500],"max":[20000,750,500]},{"id":1,"owner":"fed2","min":[0,750,-500],"max":[20000,1500,500]}]}}
```

Because it has no aircraft frames, `dff_diff` ignores it (it contributes no `(id, step)` points),
and the viewer treats it as the partition descriptor rather than a source of trajectories.

## frame line (one per step)

```json
{"t":0.1,"wt":1694531200123456,"aircraft":[{"id":1,"role":"owned","pos":[x,y,z],"vel":[x,y,z],"quat":[x,y,z,w]}]}
```

- `wt` — wall-clock time this frame was computed: integer **microseconds since the Unix
  epoch** (`system_clock`, comparable across federates on one host). **Metadata only** —
  nondeterministic, and never part of the truth/invariance check. It exists to show that the
  same LOGICAL step `t` was produced at different REAL times / interleavings (and by
  different owners) across federates and partitionings, while the `(id, t)`-keyed truth stays
  bit-identical. Optional; readers ignore it if absent.
- `role` — `"owned"` (this federate computes this aircraft's exact truth) or `"ghost"`
  (a dead-reckoned copy of *another* federate's aircraft — the **Live** experiment only).
  **Defaults to `"owned"`** when absent. Ghost entries may also carry `"age"` (seconds
  since the last update) for staleness styling later.

## Back-compatibility

Files with **no meta line and no `role` field load unchanged**: federate = filename,
role = owned. The current `sim_out/F1.ndjson` / `F2.ndjson` are exactly this case.

## Reference C++ emitter change (apply in the main library later)

In `AircraftFederate.cpp`:

- Once, right after opening the log (in `initWorld`):
  ```cpp
  log_ << "{\"meta\":{\"federate\":\"" << fname
       << "\",\"dt\":" << 0.1 << ",\"sectors\":[]}}\n";
  ```
- In `step()`, add the role to each aircraft entry:
  ```cpp
  << ",\"role\":\"owned\""
  ```

Constructive runs emit only `role:"owned"`. `ghost` rows appear only once the Live
experiment (dead reckoning + latency) is built.
