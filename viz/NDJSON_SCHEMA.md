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
- `sectors` — regions this federate owns, each `{"id":N,"min":[x,y,z],"max":[x,y,z]}`.
  Empty/omitted for now; drawn later as the real federate/sector boxes.

## frame line (one per step)

```json
{"t":0.1,"aircraft":[{"id":1,"role":"owned","pos":[x,y,z],"vel":[x,y,z],"quat":[x,y,z,w]}]}
```

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
