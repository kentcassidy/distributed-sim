#pragma once

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>
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

    // The run is gated by the controller's StartRun broadcast -- no local ENTER barrier.
    void run(wstring federateName);

private:
    void connectToRti();
    void joinFederation(wstring federateName);   // join-only: the controller is sole creator
    void cacheHandles();
    void publishAndSubscribe();
    void sendEnroll(wstring federateName);        // announce this federate to the controller
    void waitForStart();                          // pump callbacks until StartRun latches
    void buildWorld();                            // adopt assignments + full map -> world + meta
    void runLoop();                               // serve loop: advance/log/hand off until Shutdown
    void emitRecord(const Aircraft& ac, long long step);      // one NDJSON line for ONE aircraft
    void emitEvent(const char* kind, EntityId id, long long step,
                   const Vec3& pos, const wstring& to);       // NDJSON event line (handoff / out-of-bounds)
    void drainHandoffs();                         // adopt any peer handoffs addressed to us
    bool tryDepart(size_t k);                     // hand off / lose owned[k] if it left my sector
    void sendHandoff(const wstring& dest, const Aircraft& ac, long long step);
    void releaseAircraft(EntityId id, size_t k);  // delete instance + drop from owned_ / maps
    bool inMySector(const Vec3& p) const;         // inside any sector I own (inclusive)
    wstring ownerOf(const Vec3& p) const;         // owning federate for p (half-open); L"" = the void
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
    InteractionClassHandle assignClass, startClass, shutdownClass, assignSectorClass, handoffClass;
    ParameterHandle        assignTarget, assignId, assignPos, assignVel, assignOrient, assignAngV;
    ParameterHandle        startDt, startWorldMin, startWorldMax, startNumSteps;
    ParameterHandle        sectorTarget, sectorId, sectorMin, sectorMax;
    ParameterHandle        handoffTarget, handoffId, handoffPos, handoffVel, handoffOrient, handoffAngV, handoffStep;

    // One HLA object instance per owned aircraft (id -> instance handle).
    map<EntityId, ObjectInstanceHandle> ownedObjects;

    std::map<EntityId, long long> acStep_;         // per-aircraft logical step (# of dt advances)
    std::vector<RegionOwner>      partitionMap_;    // whole partition (all sectors + owners)
    std::set<EntityId>            finishedPrinted_; // aircraft already announced as complete
    size_t                        handoffsAdopted_ = 0;  // drain cursor into fedamb.incomingHandoffs
    bool                          idleAnnounced_ = false; // printed "no pending work"? (re-armed when work returns)

    double               dt_ = 0.1;
    int                  numSteps_ = 100;   // run length, set from the controller's StartRun
    LinearLongitudinal   model_;   // declared BEFORE world_ so it outlives borrowing aircraft
    World                world_;
    ofstream             log_;     // NDJSON frames for the browser viewer
    wstring              federateName_;
};
