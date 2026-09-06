#include "AircraftFederate.hpp"

#if 0
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
#endif        

CLASS AircraftFederate
    STATE: rtiAmbassador, an AircraftFedAmb instance, cached handles
           (Aircraft class handle; Position/Velocity/Orientation/EntityId attr
           handles), our own ObjectInstanceHandle, our federate name.
      // WHY cache handles: getXHandle() is a lookup; you resolve names to handles
      // ONCE after join and reuse the handles in the hot loop. Passing string
      // names every update would be slower and is not how HLA is meant to be used.

    METHOD run(federateName, fedFile):
      // ---- 1. Connect ----
      create RTIambassador (via the factory)
      rtiAmb.connect(fedAmb, EVOKED)
        // WHY EVOKED not IMMEDIATE: EVOKED means callbacks fire only when WE call
        // evokeCallback() — single-threaded, deterministic, and the model time
        // management needs later. IMMEDIATE spawns an RTI thread that calls back
        // whenever — easier for toys, wrong for a reproducible federation.
        // (1516-2010 requires this connect() step; 1516-2000 had no connect.)

      // ---- 2. Create federation (idempotent) ----
      TRY rtiAmb.createFederationExecution(federationName, fomModulePath)
      CATCH FederationExecutionAlreadyExists: ignore
        // WHY swallow it: whichever federate starts first creates it; the rest
        // find it already there. This is the normal race, not an error.

      // ---- 3. Join ----
      ourFederateHandle = rtiAmb.joinFederationExecution(
                              federateName, "Aircraft", federationName)

      // ---- 4. Resolve + cache handles ----
      AircraftClass = getObjectClassHandle("HLAobjectRoot.Aircraft")
      Position handle = getAttributeHandle(AircraftClass, "Position")
      ... (EntityId, Velocity, Orientation similarly, even if unused this slice)
      hand the Position handle to the FedAmb so it can match in reflect()

      // ---- 5. Declare interest: publish + subscribe ----
      build an AttributeHandleSet containing (at least) Position
      rtiAmb.publishObjectClassAttributes(AircraftClass, thatSet)
      rtiAmb.subscribeObjectClassAttributes(AircraftClass, thatSet)
        // WHY both in one binary: proves pub AND sub together, and mirrors the
        // real design where every federate both owns and observes aircraft.

      // ---- 6. Register our owned object ----
      ourAircraft = rtiAmb.registerObjectInstance(AircraftClass)
        // this is the moment other federates get discoverObjectInstance().
        // Later: also send the Static attrs (EntityId/Mass/Radius) right here, once.

      // ---- 7. Main loop (no time management yet) ----
      REPEAT some fixed number of ticks:
          compute a dummy Position (e.g. move x forward a bit each tick)
          encode Position -> bytes
          build an AttributeHandleValueMap { Position handle -> bytes }
          rtiAmb.updateAttributeValues(ourAircraft, thatMap, userTag)
          rtiAmb.evokeMultipleCallbacks(minSeconds, maxSeconds)
            // WHY evoke here: in EVOKED mode this is what actually delivers the
            // other federate's discover/reflect callbacks. No evoke = deaf federate.
          wait a small wall-clock interval   // pacing only; real pacing is TM later

      // ---- 8. Tear down ----
      rtiAmb.resignFederationExecution(DELETE_OBJECTS_THEN_DIVEST)
      TRY rtiAmb.destroyFederationExecution(federationName)
      CATCH FederatesCurrentlyJoined: ignore   // someone else still in; they'll destroy
      CATCH FederationExecutionDoesNotExist: ignore
      rtiAmb.disconnect()