// ─────────────────────────────────────────────────────────────────────────────
// aircraft_federate — the RTI-facing simulator process (M1 handshake).
//
// Thin launcher: pick a federate name from argv, construct the federate, run it.
// All simulation/HLA logic lives in AircraftFederate — main just wires argv to it
// and turns any RTI exception into a non-zero exit. Run two of these (with
// different names) from the repo root to see two federates exchange Position:
//     ./aircraft_federate aircraft-1
//     ./aircraft_federate aircraft-2
//
// The design boundary: this target is the only place allowed to include Portico
// headers. All physics stays behind dff_core (the RTI-free model).
// ─────────────────────────────────────────────────────────────────────────────
#include <iostream>
#include <string>
#include "AircraftFederate.hpp"

using namespace std;

int main(int argc, char* argv[]) {
    wstring federateName = L"aircraft-1";
    if (argc > 1) {
        string arg(argv[1]);
        federateName.assign(arg.begin(), arg.end());   // ASCII narrow -> wide
    }

    try {
        AircraftFederate federate;
        federate.run(federateName);
    } catch (const rti1516e::Exception& e) {
        wcerr << L"RTI exception: " << e.what() << endl;
        return 1;
    }

    return 0;
}
