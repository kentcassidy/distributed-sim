#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#include <RTI/RTI1516.h>
#include <RTI/RTIambassadorFactory.h>
#include "AircraftFederate.hpp"

using namespace std;

// Fixed identity. The controller CREATES the federation (sole creator); this federate only
// joins, so it no longer needs the FOM path.
static const wstring FEDERATION     = L"DffFederation";
static const wstring AIRCRAFT_CLASS = L"ObjectRoot.Aircraft";   // .fed root is ObjectRoot

// Wall-clock stamp for each NDJSON frame: microseconds since the Unix epoch (system_clock,
// so it is comparable ACROSS federates on the same host). METADATA ONLY -- nondeterministic,
// never part of the truth/invariance check. It exists to show that federates computed the
// same LOGICAL step at different REAL times / interleavings, while the (id, t)-keyed truth
// stays bit-identical. Integer microseconds (not a double) so log setprecision can't crush it.
static long long wallMicros() {
    using namespace std::chrono;
    return duration_cast<microseconds>(system_clock::now().time_since_epoch()).count();
}

// unique_ptr<RTIambassador> needs the complete type where it's destroyed, so the
// destructor lives here (in the .cpp) rather than being implicit in the header.
AircraftFederate::AircraftFederate() {}
AircraftFederate::~AircraftFederate() {}

////////////////////
// Public lifecycle
//////////
void AircraftFederate::run(wstring federateName, bool /*interactive*/) {
    this->federateName_ = federateName;      // needed by cacheHandles (fedamb.myName)

    connectToRti();
    joinFederation(federateName);
    cacheHandles();
    publishAndSubscribe();
    sendEnroll(federateName);
    waitForStart();       // block (pumping) until the controller broadcasts StartRun
    buildWorld();         // adopt exactly what we were assigned
    runLoop();            // integrate + publish + log, for all owned aircraft
    serveUntilShutdown(); // hold until the controller declares the sim complete
    resignAndDestroy();
}

////////////////////
// 1. Connect
//////////
void AircraftFederate::connectToRti() {
    RTIambassadorFactory factory;
    this->rtiamb.reset( factory.createRTIambassador().release() );
    this->rtiamb->connect(this->fedamb, HLA_EVOKED);   // callbacks fire only inside evoke
    wcout << L"Connected to RTI" << endl;
}

////////////////////
// 2. Join (JOIN ONLY -- the controller is the sole creator)
//////////
void AircraftFederate::joinFederation(wstring federateName) {
    // The controller creates the federation (SOLE creator -- removes the concurrent-create
    // race that split federates into isolated executions). So START THE CONTROLLER FIRST;
    // this federate only JOINS, retrying until it exists. FederationExecutionDoesNotExist =
    // controller hasn't created it yet; RTIinternalError = Portico's transient mid-create.
    const int  maxAttempts = 60;
    const auto backoff      = std::chrono::milliseconds(500);
    for (int attempt = 1; ; ++attempt) {
        try {
            this->rtiamb->joinFederationExecution(federateName, L"Aircraft", FEDERATION);
            wcout << L"Joined as " << federateName << endl;
            return;
        } catch (FederationExecutionDoesNotExist&) {
            if (attempt == 1)
                wcout << L"Waiting for the controller to create the federation..." << endl;
            if (attempt >= maxAttempts) throw;
        } catch (RTIinternalError&) {
            if (attempt >= maxAttempts) throw;
        }
        std::this_thread::sleep_for(backoff);
    }
}

////////////////////
// 3. Resolve + cache handles (only valid once joined)
//////////
void AircraftFederate::cacheHandles() {
    this->aircraftClass = rtiamb->getObjectClassHandle(AIRCRAFT_CLASS);

    this->entityIdHandle    = rtiamb->getAttributeHandle(aircraftClass, L"EntityId");
    this->massHandle        = rtiamb->getAttributeHandle(aircraftClass, L"Mass");
    this->radiusHandle      = rtiamb->getAttributeHandle(aircraftClass, L"Radius");
    this->positionHandle    = rtiamb->getAttributeHandle(aircraftClass, L"Position");
    this->velocityHandle    = rtiamb->getAttributeHandle(aircraftClass, L"Velocity");
    this->orientationHandle = rtiamb->getAttributeHandle(aircraftClass, L"Orientation");
    this->fedamb.positionHandle = this->positionHandle;

    // Enroll (federate -> controller)
    this->enrollClass        = rtiamb->getInteractionClassHandle(L"InteractionRoot.Enroll");
    this->enrollFederateName = rtiamb->getParameterHandle(enrollClass, L"FederateName");

    // AssignEntity (controller -> federate)
    this->assignClass  = rtiamb->getInteractionClassHandle(L"InteractionRoot.AssignEntity");
    this->assignTarget = rtiamb->getParameterHandle(assignClass, L"TargetFederate");
    this->assignId     = rtiamb->getParameterHandle(assignClass, L"EntityId");
    this->assignPos    = rtiamb->getParameterHandle(assignClass, L"Position");
    this->assignVel    = rtiamb->getParameterHandle(assignClass, L"Velocity");
    this->assignOrient = rtiamb->getParameterHandle(assignClass, L"Orientation");

    // StartRun (controller -> all)
    this->startClass    = rtiamb->getInteractionClassHandle(L"InteractionRoot.StartRun");
    this->startDt       = rtiamb->getParameterHandle(startClass, L"Dt");
    this->startWorldMin = rtiamb->getParameterHandle(startClass, L"WorldMin");
    this->startWorldMax = rtiamb->getParameterHandle(startClass, L"WorldMax");

    this->shutdownClass = rtiamb->getInteractionClassHandle(L"InteractionRoot.Shutdown");

    // Give the ambassador what it needs to filter + decode the control interactions.
    fedamb.myName        = federateName_;
    fedamb.assignClass   = assignClass;
    fedamb.startClass    = startClass;
    fedamb.assignTarget  = assignTarget;
    fedamb.assignId      = assignId;
    fedamb.assignPos     = assignPos;
    fedamb.assignVel     = assignVel;
    fedamb.assignOrient  = assignOrient;
    fedamb.startDt       = startDt;
    fedamb.startWorldMin = startWorldMin;
    fedamb.startWorldMax = startWorldMax;
    fedamb.shutdownClass = shutdownClass;

    wcout << L"[handles] aircraft.isValid=" << aircraftClass.isValid()
          << L" assign.isValid=" << assignClass.isValid()
          << L" start.isValid="  << startClass.isValid() << endl;
}

////////////////////
// 4. Declare interest
//////////
void AircraftFederate::publishAndSubscribe() {
    AttributeHandleSet attributes;
    attributes.insert(this->positionHandle);
    rtiamb->publishObjectClassAttributes(this->aircraftClass, attributes);
    // No SUBSCRIBE to Aircraft: the constructive core is INDEPENDENT aircraft -- no ghosts,
    // no reflection. Truth is consolidated via NDJSON; ghosts belong to the Live experiment.

    rtiamb->publishInteractionClass(this->enrollClass);      // announce ourselves
    rtiamb->subscribeInteractionClass(this->assignClass);    // receive our assignments
    rtiamb->subscribeInteractionClass(this->startClass);     // receive the go signal
    rtiamb->subscribeInteractionClass(this->shutdownClass);  // receive the stop signal
    wcout << L"Published Aircraft.Position + Enroll; subscribed AssignEntity + StartRun + Shutdown" << endl;
}

////////////////////
// 5. Announce ourselves to the controller (Enroll interaction)
//////////
void AircraftFederate::sendEnroll(wstring federateName) {
    // Let declaration management SETTLE first: a freshly-joined federate may not yet know
    // the controller SUBSCRIBES to Enroll, and an interaction sent before that subscription
    // is known is routed to nobody. Pump a few callbacks so the pub/sub picture propagates,
    // THEN send.
    for (int i = 0; i < 5; ++i) this->rtiamb->evokeMultipleCallbacks(0.05, 0.1);

    ParameterHandleValueMap params;
    params[this->enrollFederateName] = encodeString(federateName);
    VariableLengthData tag((void*)"enroll", 7);
    this->rtiamb->sendInteraction(this->enrollClass, params, tag);
    wcout << L"Enrolled with the controller as " << federateName << endl;

    for (int i = 0; i < 3; ++i) this->rtiamb->evokeMultipleCallbacks(0.02, 0.05);
}

////////////////////
// 6. Wait for the controller: pump callbacks until StartRun latches
//////////
void AircraftFederate::waitForStart() {
    wcout << L"Waiting for assignments + StartRun from the controller..." << endl;
    // AssignEntity messages arrive BEFORE StartRun (the controller sends them first), so
    // once startReceived latches, our assignments are already collected. Drain a little
    // extra afterward to catch any straggler.
    const int maxSpins = 6000;   // generous upper bound (~ up to 10 min at 0.1s)
    int spins = 0;
    while (!fedamb.startReceived && spins < maxSpins) {
        rtiamb->evokeMultipleCallbacks(0.1, 0.2);
        ++spins;
    }
    for (int i = 0; i < 5; ++i) rtiamb->evokeMultipleCallbacks(0.05, 0.1);

    if (!fedamb.startReceived)
        throw std::runtime_error("timed out waiting for StartRun from the controller");

    this->dt_ = fedamb.dt;
}

////////////////////
// 7. Build our world from EXACTLY what the controller assigned
//////////
void AircraftFederate::buildWorld() {
    // Sort assignments by id so owned_ order is deterministic regardless of the order the
    // AssignEntity interactions happened to arrive -- partition invariance requires the
    // per-aircraft truth to be independent of who/what order computes it.
    std::vector<EntitySpec> specs = fedamb.assignments;
    std::sort(specs.begin(), specs.end(),
              [](const EntitySpec& a, const EntitySpec& b){ return a.id < b.id; });

    for (size_t i = 0; i < specs.size(); ++i) {
        const EntitySpec& spec = specs[i];
        AircraftParams params;          // shared filler defaults (fidelity is out of scope)
        params.id = spec.id;
        world_.owned().push_back(Aircraft(spec.id, params, &model_));
        world_.owned().back().state() = spec.initial;

        // one HLA object instance per owned aircraft
        ObjectInstanceHandle h = rtiamb->registerObjectInstance(aircraftClass);
        ownedObjects[spec.id] = h;
    }

    // NDJSON log: one file per federate. meta line first. (sectors:[] for now -- 5c will
    // populate it from the disseminated partition so the viewer can draw the boxes.)
    string fname(federateName_.begin(), federateName_.end());
    log_.open("sim_out/" + fname + ".ndjson");
    // 17 significant digits = max_digits10 for double, so the truth round-trips to the exact
    // same bits -- required for the diff to verify BIT-EXACT invariance (not just ~9 digits).
    log_ << std::setprecision(17);
    log_ << "{\"meta\":{\"federate\":\"" << fname << "\",\"dt\":" << dt_
         << ",\"sectors\":[]}}\n";

    wcout << L"World ready: " << world_.owned().size()
          << L" owned aircraft; logging to " << federateName_ << L".ndjson" << endl;
}

////////////////////
// 8. Main loop: advance, publish each owned aircraft, log one NDJSON frame
//////////
void AircraftFederate::runLoop() {
    const int STEPS = 100;

    // t=0 is the INITIAL state, before any integration. Every later frame is the state
    // JUST CALCULATED by that step's advance, labeled with its true logical time -- so
    // frame t=i*dt genuinely holds the post-advance state at that time (no off-by-one).
    logFrame(0.0);
    for (int i = 1; i <= STEPS; ++i) {
        world_.advance(dt_);        // RK4 over all owned aircraft, in order
        logFrame(i * dt_);          // the just-calculated state at t = i*dt
        rtiamb->evokeMultipleCallbacks(0.05, 0.1);
    }

    wcout << L"Run complete: " << STEPS << L" steps (+ initial frame), "
          << world_.owned().size() << L" aircraft." << endl;
}

// Publish each owned aircraft's real Position and write one NDJSON frame (all owned
// aircraft) at logical time simTime. wt is the wall-clock stamp (metadata only).
void AircraftFederate::logFrame(double simTime) {
    log_ << "{\"t\":" << simTime << ",\"wt\":" << wallMicros() << ",\"aircraft\":[";
    const std::vector<Aircraft>& owned = world_.owned();
    for (size_t k = 0; k < owned.size(); ++k) {
        const Aircraft& ac = owned[k];
        const State&    s  = ac.state();

        // put this aircraft's real Position on the wire
        AttributeHandleValueMap attrs;
        attrs[positionHandle] = encodeVec3(s.position);
        VariableLengthData tag((void*)"pos", 4);
        rtiamb->updateAttributeValues(ownedObjects[ac.id()], attrs, tag);

        // NDJSON entry -- role "owned": this federate computes this aircraft's truth
        if (k) log_ << ",";
        log_ << "{\"id\":" << ac.id() << ",\"role\":\"owned\""
             << ",\"pos\":["  << s.position.x << "," << s.position.y << "," << s.position.z << "]"
             << ",\"vel\":["  << s.velocity.x << "," << s.velocity.y << "," << s.velocity.z << "]"
             << ",\"quat\":[" << s.attitude.x << "," << s.attitude.y << "," << s.attitude.z << "," << s.attitude.w << "]}";
    }
    log_ << "]}\n";
}

////////////////////
// 8b. Hold after our steps until the controller declares the sim complete
//////////
void AircraftFederate::serveUntilShutdown() {
    // Do NOT disconnect when our steps finish. Keep the callback pump running so the
    // controller can still reach us (later: reassign an aircraft mid-run) until it
    // broadcasts Shutdown -- there can always be more work until the sim is complete.
    wcout << L"Steps done; holding for controller Shutdown..." << endl;
    while (!fedamb.shutdownReceived) {
        rtiamb->evokeMultipleCallbacks(0.1, 0.2);
    }
    wcout << L"Shutdown received; resigning." << endl;
}

////////////////////
// 9. Tear down. The CONTROLLER (creator) destroys the federation; we only resign.
//////////
void AircraftFederate::resignAndDestroy() {
    VariableLengthData tag((void*)"bye", 4);
    for (map<EntityId, ObjectInstanceHandle>::iterator it = ownedObjects.begin();
         it != ownedObjects.end(); ++it) {
        rtiamb->deleteObjectInstance(it->second, tag);
    }

    try {
        rtiamb->resignFederationExecution(NO_ACTION);
        wcout << L"Resigned from federation" << endl;
    } catch (const rti1516e::Exception& e) {
        // Should not happen now that the controller pumps callbacks during the run, but if
        // resign coordination ever times out, still disconnect cleanly rather than crash.
        wcerr << L"Resign failed (" << e.what() << L"); disconnecting anyway" << endl;
    }

    rtiamb->disconnect();
    wcout << L"Disconnected from RTI" << endl;
}
