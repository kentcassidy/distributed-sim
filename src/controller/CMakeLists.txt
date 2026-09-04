# controller — RTI-facing (it joins as a federate to publish sector assignments),
# so gnu++14 + the project-wide old ABI. Links Portico and shares core types
# (the Sector geometry and ids will live in dff_core / a shared header).
add_executable(controller
    main.cpp)

target_link_libraries(controller PRIVATE Portico::Portico dff_core)

set_target_properties(controller PROPERTIES
    CXX_STANDARD          14
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS        ON)

target_compile_options(controller PRIVATE -Wno-deprecated)
