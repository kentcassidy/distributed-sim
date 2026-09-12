// ─────────────────────────────────────────────────────────────────────────────
// controller — the federation manager / sector-assignment federate.
//
// SOLE creator of the federation and coordinator of config-driven partitioning
// (ADR-0015). Start it FIRST, then the aircraft federates. It waits for you to press
// ENTER once they've joined, then divides the world into K = (number joined) slabs,
// assigns each aircraft by its start position, disseminates the assignments, and
// broadcasts the go signal. All physics stays behind dff_core; this target, like the
// aircraft federate, is one of the only places allowed to include Portico headers.
//
// Run from the repo root (the FOM and scenario paths are resolved against the CWD):
//     ./controller                              # default scenarios/two_aircraft.csv
//     ./controller scenarios/two_aircraft.csv
// ─────────────────────────────────────────────────────────────────────────────
#include <iostream>
#include <string>
#include "ControllerFederate.hpp"

using namespace std;

int main(int argc, char* argv[]) {
    wstring scenarioPath = L"scenarios/two_aircraft.csv";
    if (argc > 1) {
        string a(argv[1]);
        scenarioPath.assign(a.begin(), a.end());   // ASCII narrow -> wide
    }

    try {
        ControllerFederate controller;
        controller.run(scenarioPath);
    } catch (const rti1516e::Exception& e) {
        wcerr << L"[controller] RTI exception: " << e.what() << endl;
        return 1;
    } catch (const std::exception& e) {
        // loadScenario / assignEntities config errors surface here (std::runtime_error).
        cerr << "[controller] error: " << e.what() << endl;
        return 1;
    }

    return 0;
}
