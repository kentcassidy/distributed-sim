#pragma once

#include <map>
#include <memory>
#include <string>
#include <fstream>
#include <RTI/RTI1516.h>
#include "AircraftFedAmb.hpp"
#include "Encoding.hpp"
#include "../core/World.hpp"
#include "../core/LinearLongitudinal.hpp"

using namespace rti1516e;
using namespace std;

// The orchestrator. It no longer invents its own aircraft: it JOINS (the controller is the
// sole creator), ENROLLS with the controller, waits for its AssignEntity messages + the
// StartRun broadcast, builds EXACTLY the aircraft it was assigned, then integrates and
// publishes their truth (one NDJSON file per federate). This is what makes the K=1-vs-K=2
// partition-invariance diff possible: the same scenario, partitioned differently, must
// produce identical per-aircraft truth.
class AircraftFederate {
public:
    AircraftFederate();
    ~AircraftFederate();

    // `interactive` is now vestigial: the run is gated by the controller's StartRun, not a
    // local ENTER barrier. Kept for launcher compatibility.
    void run(wstring federateName, bool interactive = false);

private:
    void connectToRti();
    void joinFederation(wstring federateName);   // join-only: the controller is sole creator
    void cacheHandles();
    void publishAndSubscribe();
    void sendEnroll(wstring federateName);        // announce this federate to the controller
    void waitForStart();                          // pump callbacks until StartRun latches
    void buildWorld();                            // adopt assignments -> world + objects + meta
    void runLoop();                               // integrate + publish + NDJSON, all owned
    void logFrame(double simTime);                // publish + write one NDJSON frame (all owned)
    void serveUntilShutdown();                    // hold (keep pumping) until the controller stops us
    void resignAndDestroy();

    unique_ptr<RTIambassador> rtiamb;
    AircraftFedAmb            fedamb;

    // Object-class (Aircraft) handles
    ObjectClassHandle aircraftClass;
    AttributeHandle   entityIdHandle, massHandle, radiusHandle;
    AttributeHandle   positionHandle, velocityHandle, orientationHandle;

    // Interaction handles: Enroll (we publish), AssignEntity + StartRun (we subscribe)
    InteractionClassHandle enrollClass;
    ParameterHandle        enrollFederateName;
    InteractionClassHandle assignClass, startClass, shutdownClass;
    ParameterHandle        assignTarget, assignId, assignPos, assignVel, assignOrient;
    ParameterHandle        startDt, startWorldMin, startWorldMax;

    // One HLA object instance per owned aircraft (id -> instance handle).
    map<EntityId, ObjectInstanceHandle> ownedObjects;

    double               dt_ = 0.1;
    LinearLongitudinal   model_;   // declared BEFORE world_ so it outlives borrowing aircraft
    World                world_;
    ofstream             log_;     // NDJSON frames for the browser viewer
    wstring              federateName_;
};
