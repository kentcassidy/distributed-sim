#include "dff_core.hpp0"

void AircraftFederate::run() {
	join();
        while(t < t_end) {
            // ghosts refresh asynchronously via reflectAttributeValues callbacks
            requestTimeAdvance(t + dt);     // RTI grands only when LBTS allows (all federates ready)
            // ... blocks until timeAdvanceGrant ...
            world.advance(dt);              // integrate owned_ + resolve collisions (owned-owned; owned-ghost at seam)
            doHandoffs();                   // planes that left this sector
            publishOwned();                 // updateAttributeValues -> refreshes everyone else's ghosts
            t += dt;
        }
}
        

