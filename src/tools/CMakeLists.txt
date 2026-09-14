# gen_scenario -- seeded random IC generator. Standalone C++17, no RTI, no dff_core (links
# nothing, like dff_diff). Writes a scenario CSV the controller loads; same seed => same file.
add_executable(gen_scenario gen_scenario.cpp)

set_target_properties(gen_scenario PROPERTIES
    CXX_STANDARD          17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS        OFF)
