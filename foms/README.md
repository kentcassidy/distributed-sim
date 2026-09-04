# FOM — Federation Object Model

**This is your hand-authored artifact.** The FOM is the file that most directly
shows the object model is understood, so it is written by hand, not generated
(see the charter's AI-tooling boundary and ADR-0003). This note is guidance for
what it must declare — not the FOM itself.

Portico 2.1.0 reads IEEE-1516e object models. You can start from the `.fed`
form the shipped example uses (`examples/portico-example/testfom.fed`) or author
the XML FOM module; keep whichever you pick under this directory
(e.g. `foms/dff-fom.xml`) and point the federate at it.

## What the MVP FOM needs to declare

- **An `Aircraft` object class** with the attributes the federates publish and
  subscribe — position, velocity, orientation (quaternion), angular velocity,
  plus an identity/id. These are the ghost-able state fields.
- **Datatypes** for those attributes (fixed-size arrays / records of doubles).
  Decide encoding deliberately — this is where partition invariance can silently
  break if two federates encode/decode a double differently.
- **The controller's assignment channel** — the sector→federate assignment the
  controller publishes. This can be an object class (persistent, re-readable by
  late joiners) or an interaction (transient). Persistent is the simpler mental
  model for the MVP; decide and record why.
- **(Later) a boundary/handoff signal** if you model ownership transfer through
  the FOM rather than through HLA Ownership Management services directly.

## Things to keep straight

- Attribute **ownership** is per-attribute, one owner at a time — that is the
  RTI primitive the sector model rides on. The FOM declares the attributes; the
  federates decide who publishes them and when ownership moves.
- Keep the FOM minimal for M1: the very first handshake can publish a single
  trivial attribute to prove pub/sub/lifecycle before the real state fields go in.
