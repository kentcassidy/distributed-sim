# dff_diff — the partition-invariance V&V tool. RTI-free and standalone (reads NDJSON only),
# so it links nothing and compiles as C++17. Given two run directories (out_K1/ out_K2/), it
# parses the per-federate NDJSON, compares the (entity id, logical step)-keyed truth for
# bit-exact equality, and reports the ownership / real-time divergence the invariance holds
# across. Exit code 0 iff invariance holds -- usable in CI.
add_executable(dff_diff main.cpp)

set_target_properties(dff_diff PROPERTIES
    CXX_STANDARD          17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS        OFF)
