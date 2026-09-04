// ─────────────────────────────────────────────────────────────────────────────
// controller — the sector-assignment federate.
//
// It joins the federation as a dedicated coordinator. For the MVP its job is
// deliberately small: LOAD a scenario/partition config file and DELEGATE —
// compute (or simply read) the sector -> federate assignment once at startup,
// publish it, and let each aircraft federate adopt its sector and run. That is
// the whole MVP behaviour; it does not rebalance while the run is live.
//
// Why a real federate and not just a config file every process reads: it gives
// dynamic rebalancing (reassigning sectors by live capability) a home LATER
// without changing the topology, and it matches the "central controller" model
// — the seam is here even though the MVP only walks through it statically.
//
// Note on authority: the controller ASSIGNS who owns each sector; it is NOT an
// arbiter of physics. Once a sector is owned by exactly one federate, HLA
// ownership already guarantees a single authoritative computation there — the
// convergence comes from that singularity, not from the controller ranking sims.
//
// Right now this stub just proves the wiring.
// ─────────────────────────────────────────────────────────────────────────────
#include <iostream>

#include "dff_core.hpp"

int main() {
    std::cout << "controller skeleton — linked against "
              << dff::core_version() << "\n";
    std::cout << "TODO(MVP): load scenario config, assign sectors -> federates, "
                 "publish assignment.\n";
    return 0;
}
