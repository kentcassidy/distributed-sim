# dff_core — the model. RTI-FREE by rule: no target here may #include a Portico
# / ieee1516e header. That rule is what lets this library compile as clean C++17
# and be unit-tested with no RTI, and what makes it reusable behind any driver
# (the aircraft federate today; a batch/replay driver later).
add_library(dff_core STATIC
    dff_core.cpp
    Aircraft.cpp
    LinearLongitudinal.cpp
    Integrator.cpp
    Collision.cpp
    World.cpp)

# PUBLIC so anything linking dff_core (the federate, the controller, tests) can
# #include its headers by plain name.
target_include_directories(dff_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Full C++17 here — the core never touches Portico's C++14-only headers, so it
# is free of the gnu++14 constraint the RTI-facing targets carry. Strict (no GNU
# extensions) keeps the core portable and honest.
set_target_properties(dff_core PROPERTIES
    CXX_STANDARD          17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS        OFF)
