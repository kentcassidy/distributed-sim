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
- `scenario` — *(controller file only)* the scenario name (CSV basename without extension,
  e.g. `"random"`), so the viewer can label the run.

### controller descriptor file (`controller.ndjson`)

The controller writes one **meta-only** file (no frames) giving the authoritative partition, so
the viewer draws the world + sector boxes directly instead of inferring them from federate truth:

```json
{"meta":{"federate":"controller","scenario":"random","dt":0.1,"world":{"min":[0,0,-500],"max":[20000,1500,500]},"sectors":[{"id":0,"owner":"fed1","min":[0,0,-500],"max":[20000,750,500]},{"id":1,"owner":"fed2","min":[0,750,-500],"max":[20000,1500,500]}]}}
```

Because it has no aircraft frames, `dff_diff` ignores it (it contributes no `(id, step)` points),
and the viewer treats it as the partition descriptor rather than a source of trajectories.

## frame line (one aircraft per line)

```json
{"t":0.1,"wt":1694531200123456,"aircraft":[{"id":1,"owner":"A","role":"owned","pos":[x,y,z],"vel":[x,y,z],"quat":[x,y,z,w]}]}
```

The federate emits **one line per (aircraft, logical step)** — the `aircraft` array holds a
single entry, and `t = step*dt` is *that aircraft's* own step. (The array is kept, so a
multi-aircraft line still parses.) One-aircraft-per-line is what keeps the truth exactly-once
under migration: a handed-off aircraft's steps are contiguous across two files (owner does
`0..s`, adopter does `s+1..N`), each row labeled by the aircraft's own step regardless of how
the federates' wall-clock loops interleave. The viewer already merges by `id` across files and
sorts by time, so trajectories reconstruct without change.

- `owner` — the federate that COMPUTED this step (explicit, not inferred from the file). Under
  migration it **changes across an aircraft's track** at the crossover step: the previous owner
  logs up to the transfer step, the new owner logs from the next step on. The viewer honors this
  per frame: `timeline.js` carries owner-per-step, stamps the home federate as the earliest
  owner, and the scene recolors each aircraft (and moves it between per-federate panes) live at
  the handoff. Defaults to the file's federate when absent.
- `wt` — wall-clock time this record was computed: integer **microseconds since the Unix
  epoch** (`system_clock`, comparable across federates on one host). **Metadata only** —
  nondeterministic, and never part of the truth/invariance check. It exists to show that the
  same LOGICAL step `t` was produced at different REAL times / interleavings (and by
  different owners) across federates and partitionings, while the `(id, t)`-keyed truth stays
  bit-identical. Optional; readers ignore it if absent.
- `role` — `"owned"` (this federate computes this aircraft's exact truth) or `"ghost"`
  (a dead-reckoned copy of *another* federate's aircraft — the **Live** experiment only).
  **Defaults to `"owned"`** when absent. Ghost entries may also carry `"age"` (seconds
  since the last update) for staleness styling later.

## event line (departures)

An aircraft leaving its owner's region emits an **event** line at the step it was detected,
so the viewer can annotate the moment (a handoff crossover, or a plane flying out of the
world) instead of only inferring it from the `owner` change:

```json
{"t":5.9,"wt":1694531200123456,"event":"handoff","id":1,"from":"A","to":"B","pos":[x,y,z]}
{"t":5.9,"wt":1694531200123456,"event":"out_of_bounds","id":3,"from":"A","pos":[x,y,z]}
```

- `event` — `"handoff"` (transferred to a peer; `to` = the new owner) or `"out_of_bounds"`
  (left the world entirely and was dropped — "lost in the void"; no `to`).
- `from` — the federate that owned it up to this step. `to` — the receiving federate (handoff
  only). `pos` — where it was when it left.
- An event line carries `t` + `id` but **no `{"id":...}` aircraft entry**, so `dff_diff`
  ignores it (it contributes no `(id, step)` truth point) and it never affects the invariance
  check. Readers that don't care can skip any line with an `event` key.

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
