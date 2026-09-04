# Portico's shipped IEEE-1516e C++ example, relocated here verbatim as a
# TOOLCHAIN PROOF. It is NOT part of the DFF design — it exists so that
# `example_federate` keeps building and joining a federation, which is the
# end-to-end evidence that the container + build contract (gnu++14, old ABI,
# Portico link, bundled JVM) is intact. If this target ever stops building,
# the environment regressed, not your code.
add_executable(example_federate
    main.cpp
    ExampleCPPFederate.cpp
    ExampleFedAmb.cpp)

target_link_libraries(example_federate PRIVATE Portico::Portico)

set_target_properties(example_federate PROPERTIES
    CXX_STANDARD          14
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS        ON)              # ON → -std=gnu++14

target_compile_options(example_federate PRIVATE -Wno-deprecated)

# The example opens testfom.fed relative to its working directory, so drop a
# copy next to the binary after each build.
add_custom_command(TARGET example_federate POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/testfom.fed"
        "$<TARGET_FILE_DIR:example_federate>")
