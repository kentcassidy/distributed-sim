# controller — RTI-facing (it joins as a federate to CREATE the federation and disseminate
# partition assignments), so gnu++14 + the project-wide old ABI. Links Portico and
# dff_core (Scenario + Partition), and shares the aircraft federate's wire Encoding.
#
# NOTE (layering wart): the controller compiles ../federate/Encoding.cpp and includes that
# directory for Encoding.hpp. Both RTI-facing targets need the same wire format; extracting
# a small shared "rti_common" library is the cleaner home, deferred to keep this slice
# focused. dff_core stays RTI-free, so Encoding cannot live there.
add_executable(controller
    main.cpp
    ControllerFederate.cpp
    ControllerFedAmb.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../federate/Encoding.cpp)

target_include_directories(controller PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../federate)

target_link_libraries(controller PRIVATE Portico::Portico dff_core)

set_target_properties(controller PROPERTIES
    CXX_STANDARD          14
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS        ON)

target_compile_options(controller PRIVATE -Wno-deprecated)
