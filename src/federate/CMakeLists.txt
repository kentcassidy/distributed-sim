# aircraft_federate — RTI-facing, so gnu++14 + the project-wide old ABI. Links
# BOTH Portico (the transport) and dff_core (the model). This is the ONLY kind
# of target permitted to include Portico / ieee1516e headers.
add_executable(aircraft_federate
    main.cpp)

target_link_libraries(aircraft_federate PRIVATE Portico::Portico dff_core)

# gnu++14: Portico 2.1.0's headers use throw()-specs / std::auto_ptr, removed in
# C++17. EXTENSIONS ON → -std=gnu++14, matching the environment the RTI expects.
set_target_properties(aircraft_federate PROPERTIES
    CXX_STANDARD          14
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS        ON)

# Silence the "dynamic exception specifications are deprecated" noise from
# Portico's headers (correct category is -Wno-deprecated). Real errors unaffected.
target_compile_options(aircraft_federate PRIVATE -Wno-deprecated)
