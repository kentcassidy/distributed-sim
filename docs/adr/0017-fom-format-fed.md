# ADR-0017: Author the FOM in HLA 1.3 (.fed) format, not 1516e XML

- **Status:** Accepted
- **Date:** 2026-09-07
- **Deciders:** Kent

## Context

The federation needs a hand-authored FOM. The **interface** standard is fixed at
IEEE 1516e by ADR-0001, but that is a separate axis from how the FOM *file* is
serialized: Portico loads both the 1516e XML (OMT DIF) format and the older HLA
1.3 `.fed` S-expression format, and it does so even under the 1516e C++ binding
we use. The vendored C++ example (`examples/portico-example/`) is proven against
a `.fed` file.

The FOM was first authored as 1516e XML (`foms/dff-fom.xml`). Against Portico
2.1.0 the `Aircraft` **object class** loaded but its **attributes did not attach**
— `getAttributeHandle` returned an invalid handle and `publishObjectClassAttributes`
failed with `attribute [-1] not defined`. Reordering the XML to match Portico's
shipped example FOMs (element-form `<name>`, an empty `<dimensions/>` in each
attribute) did not resolve it; the residual cause lies somewhere in Portico's XML
DIF parser (most likely custom-datatype resolution for `Vector3D`/`Quaternion`).
Root-causing the vendored parser was judged not worth the risk against the M1
deadline, and ADR-0014's spirit — fit the RTI as vendored rather than fight it —
applies.

## Decision

Author the DFF FOM as **`foms/dff-fom.fed`** in HLA 1.3 format. The object class
is addressed as `ObjectRoot.Aircraft` (the 1.3 root is `ObjectRoot`, not
`HLAobjectRoot`). The **interface remains IEEE 1516e** (ADR-0001 unchanged) — only
the FOM serialization is 1.3. The `.fed` format declares **no datatypes**;
attributes are opaque byte buffers, and the wire encoding is owned entirely by the
federate's `Encoding` layer (`HLAfixedArray` of `HLAfloat64BE`) — which is where
partition-invariance byte-agreement is deliberately controlled anyway (ADR-0006).

## Alternatives Considered

- **HLA 1.3 `.fed` (chosen).** *Buys:* proven against the vendored example; no
  datatype-resolution surface to trip over; the fastest path to a running
  federation; puts the wire format in the one layer we already own and test.
  *Costs:* no FOM-level type declarations, so the FOM is less self-describing; a
  format one generation older than the interface we bind to; `Encoding.*` and the
  `.fed` attribute list must be kept in manual agreement.
- **1516e XML (deferred, retained in-tree).** *Buys:* matches the interface
  generation; declares datatypes in the model; the "modern" artifact. *Costs:*
  Portico 2.1.0 rejected our hand-authored attributes for reasons not yet
  root-caused; chasing it risked the deadline for no functional gain at MVP scale.
- **Debug the XML parser now.** *Buys:* keeps the XML. *Costs:* an open-ended
  time sink against a toolchain we have decided not to modify.

## Consequences

- The `.fed` file **is** the hand-authored FOM artifact the charter calls for; the
  "hand-authored FOM" deliverable stands, and the IEEE 1516e claim (interface) is
  intact.
- Attribute wire types live only in code (`Encoding.*`), making that layer the
  single source of truth for serialization — consistent with treating encoding as
  the place invariance is owned.
- `foms/dff-fom.xml` is **kept in-tree** as a reference and future option. Porting
  back to XML is now a contained task (only the parser mystery remains, everything
  else proven) and, if done, would be recorded as an ADR superseding this one.
- Reaffirms the no-library-modification stance: the FOM format is chosen to fit
  Portico as vendored, not the reverse (cf. ADR-0014).
- Supersedes nothing. ADR-0001 (1516e **interface**) is unaffected; this records a
  FOM **format** decision no prior ADR covered.
