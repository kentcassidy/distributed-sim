// ─────────────────────────────────────────────────────────────────────────────
// aircraft_federate — the RTI-facing simulator process (M1 and on).
//
// Responsibilities to build here (this stub does none of them yet):
//   * create/join the federation; publish & subscribe the Aircraft object class
//     declared in your hand-authored FOM (foms/)
//   * own the aircraft inside THIS federate's assigned sector, and advance them
//     each step via dff_core (the RTI-free model)
//   * hold read-only ghosts of neighbours; maintain a halo band just past the
//     sector boundary so approaching traffic is visible before it crosses
//   * hand off an aircraft (ownership transfer, or delete-here / create-there)
//     when it leaves this sector
//   * drive HLA time management — regulating + constrained, with lookahead —
//     so the split run advances in lockstep with a co-located run
//
// The design boundary: THIS file (and its siblings) is the only place allowed
// to include Portico headers. All physics stays behind dff_core.
//
// Right now this stub just proves the wiring: it links Portico and dff_core and
// runs, so the build graph is green before any real logic exists.
// ─────────────────────────────────────────────────────────────────────────────
#include "AircraftFederate.hpp"
#include <iostream>

int main(int argc, char** argv) {
	#if 0
	try {
		dff::AircraftFederate fed(argc > 1 ? argv[1] : "config/scenario.example.json");
		fed.run();
	} catch (const std::exception& e) {
		std::cerr << "fatal: " << e.what() << "\n";
		return 1;
	}
	return 0;
	#endif



    std::cout << "aircraft_federate skeleton — linked against "
              << dff::core_version() << "\n";
    std::cout << "TODO(M1): join federation, pub/sub the FOM, ownership, "
                 "time management.\n";
    return 0;
}
