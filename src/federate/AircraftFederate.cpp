#include <iostream>
#include <iomanip>
#include <vector>
#include <thread>       // std::this_thread::sleep_for -- join-race backoff
#include <chrono>
#include <RTI/RTI1516.h>
#include <RTI/RTIambassadorFactory.h>
#include "AircraftFederate.hpp"

using namespace std;

// Fixed identity. The controller CREATES the federation (sole creator); this federate
// only joins, so it no longer needs the FOM path.
static const wstring FEDERATION     = L"DffFederation";
static const wstring AIRCRAFT_CLASS = L"ObjectRoot.Aircraft";   // .fed root is ObjectRoot, not HLAobjectRoot

// unique_ptr<RTIambassador> needs the complete type where it's destroyed, so the
// destructor lives here (in the .cpp) rather than being implicit in the header.
AircraftFederate::AircraftFederate() {}
AircraftFederate::~AircraftFederate() {}

////////////////////
// Public lifecycle
//////////
void AircraftFederate::run(wstring federateName, bool interactive) {
    connectToRti();
    joinFederation(federateName);
    cacheHandles();
    publishAndSubscribe();
    sendEnroll(federateName);
    registerOwnAircraft();
    initWorld(federateName);

    // Barrier for the two-federate demo: hold here until BOTH federates have
    // registered, so their publish loops overlap and discovery/reflection cross.
    // Skipped when non-interactive (CI smoke test) so it doesn't block.
    if (interactive)
        waitForUser();

    // Main loop: no time management yet. Advance the physics by dt, publish our real
    // Position, then evoke callbacks so the RTI delivers the OTHER federate's
    // discover/reflect.
    const double dt = 0.1;
    for (int i = 0; i < 100; i++) {
        step(i * dt, dt);
        rtiamb->evokeMultipleCallbacks(0.1, 0.2);
    }

    resignAndDestroy();
}

////////////////////
// 1. Connect
//////////
void AircraftFederate::connectToRti() {
    RTIambassadorFactory factory;
    // createRTIambassador() hands back a smart pointer; release() moves ownership
    // into our unique_ptr (works whether Portico hands back auto_ptr or unique_ptr).
    this->rtiamb.reset( factory.createRTIambassador().release() );

    // EVOKED: callbacks fire only inside evokeMultipleCallbacks(), single-threaded.
    this->rtiamb->connect(this->fedamb, HLA_EVOKED);
    wcout << L"Connected to RTI" << endl;
}

////////////////////
// 2-3. Join (JOIN ONLY -- the controller is the sole creator)
//////////
void AircraftFederate::joinFederation(wstring federateName) {
    // The controller creates the federation (SOLE creator -- this removes the concurrent-
    // create race that used to split federates into isolated executions). So START THE
    // CONTROLLER FIRST; this federate only JOINS, retrying with backoff until the
    // federation exists. FederationExecutionDoesNotExist = the controller hasn't created it
    // yet; RTIinternalError = Portico's transient mid-creation race.
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
// 4. Resolve + cache handles (only valid once joined)
//////////
void AircraftFederate::cacheHandles() {
    this->aircraftClass = rtiamb->getObjectClassHandle(AIRCRAFT_CLASS);

    this->entityIdHandle    = rtiamb->getAttributeHandle(aircraftClass, L"EntityId");
    this->massHandle        = rtiamb->getAttributeHandle(aircraftClass, L"Mass");
    this->radiusHandle      = rtiamb->getAttributeHandle(aircraftClass, L"Radius");
    this->positionHandle    = rtiamb->getAttributeHandle(aircraftClass, L"Position");
    this->velocityHandle    = rtiamb->getAttributeHandle(aircraftClass, L"Velocity");
    this->orientationHandle = rtiamb->getAttributeHandle(aircraftClass, L"Orientation");

    // hand Position to the ambassador so reflect() can match it in the value map
    this->fedamb.positionHandle = this->positionHandle;

    // Control-plane: Enroll (federate -> controller).
    this->enrollClass        = rtiamb->getInteractionClassHandle(L"InteractionRoot.Enroll");
    this->enrollFederateName = rtiamb->getParameterHandle(enrollClass, L"FederateName");

    // DIAGNOSTIC: bisects the failure. If class is valid but Position is not, the
    // class loaded without its attributes (FOM attribute parse). If BOTH are
    // invalid, the FOM/object model didn't load at all (stale federation or path).
    wcout << L"[handles] aircraftClass.isValid=" << this->aircraftClass.isValid()
          << L"  position.isValid="              << this->positionHandle.isValid()
          << L"  entityId.isValid="              << this->entityIdHandle.isValid() << endl;
}

////////////////////
// 5. Declare interest
//////////
void AircraftFederate::publishAndSubscribe() {
    // Only Position is wired for the handshake; the rest join this set as the
    // model comes online. Publishing AND subscribing in one binary proves both
    // directions and mirrors the real design (every federate owns and observes).
    AttributeHandleSet attributes;
    attributes.insert(this->positionHandle);

    rtiamb->publishObjectClassAttributes(this->aircraftClass, attributes);
    rtiamb->subscribeObjectClassAttributes(this->aircraftClass, attributes);
    rtiamb->publishInteractionClass(this->enrollClass);
    wcout << L"Published and subscribed Aircraft.Position; publishing Enroll" << endl;
}

////////////////////
// 5b. Announce ourselves to the controller (Enroll interaction)
//////////
void AircraftFederate::sendEnroll(wstring federateName) {
    ParameterHandleValueMap params;
    params[this->enrollFederateName] = encodeString(federateName);
    VariableLengthData tag((void*)"enroll", 7);
    this->rtiamb->sendInteraction(this->enrollClass, params, tag);
    wcout << L"Enrolled with the controller as " << federateName << endl;
    // Nudge the callback pump so the enroll actually leaves before we go quiet.
    this->rtiamb->evokeMultipleCallbacks(0.02, 0.05);
}

////////////////////
// 6. Register our owned aircraft
//////////
void AircraftFederate::registerOwnAircraft() {
    // The moment other federates receive discoverObjectInstance() for us.
    this->ownAircraft = rtiamb->registerObjectInstance(this->aircraftClass);
    wcout << L"Registered own Aircraft, handle=" << this->ownAircraft << endl;
}

////////////////////
// Demo barrier: block until the user has started both federates
//////////
void AircraftFederate::waitForUser() {
    wcout << L">>> Press ENTER once BOTH federates print 'Registered' <<<" << endl;
    string line;
    getline(cin, line);
}

////////////////////
// 6b. Build this federate's physics world (one owned aircraft) + open the log
//////////
void AircraftFederate::initWorld(wstring federateName) {
    this->federateName_ = federateName;

    // One aircraft per federate for the MVP. Derive a stable id + a lane offset from
    // the last character of the name (aircraft-1 -> 1) so the two federates' planes
    // fly parallel, visibly distinct lanes.
    unsigned int tail = 1;
    
    wchar_t c = federateName.empty() ? L'1' : federateName.back();
    if (c >= L'0' && c <= L'9') tail = (unsigned int)(c - L'0');

    AircraftParams params;          // arbitrary-but-stable filler (see AircraftParams.hpp)
    params.id = tail;


    // Create random coordinates + starting attitude + maybe speed, using federateName as a seed (for now)
    // Will evolve to randomly distributed point cloud generator?

    // Cruise straight down +x at trim speed (so the u,w perturbations start at 0), in
    // a lane offset on y, with a small initial pitch so the linear dynamics visibly
    // oscillate -- proof the integrator + derivative are actually running.
    State s0;
    s0.position = Vec3(0.0, tail * 100.0, 0.0);
    s0.velocity = Vec3(params.trimSpeed, 0.0, 0.0);
    s0.attitude = Quat(0.0, 0.02, 0.0, 1.0);   // ~0.04 rad pitch; renormalized on first step

    world_.owned().push_back(Aircraft(params.id, params, &model_));
    world_.owned().back().state() = s0;

    // NDJSON viewer log, one file per federate (the viewer merges by timestamp).
    string fname(federateName.begin(), federateName.end());
    log_.open("sim_out/" + fname + ".ndjson");
    log_ << setprecision(9);

    wcout << L"World ready: 1 aircraft, id=" << tail
          << L", logging to " << federateName << L".ndjson" << endl;
}

////////////////////
// 7. One step: advance real physics, publish Position, log an NDJSON frame
//////////
void AircraftFederate::step(double simTime, double dt) {
    world_.advance(dt);                                 // RK4 over the owned aircraft
    const State& s = world_.owned()[0].state();

    // Put the REAL position on the wire (replaces the old dummy ramp). Velocity and
    // Orientation join the published set in the next increment (ghost + dead reckoning).
    AttributeHandleValueMap attributes;
    attributes[this->positionHandle] = encodeVec3(s.position);
    VariableLengthData tag((void*)"pos", 4);
    rtiamb->updateAttributeValues(this->ownAircraft, attributes, tag);

    // NDJSON frame for the viewer: this federate's full local state (pos + vel + quat).
    unsigned int id = world_.owned()[0].id();
    log_ << "{\"t\":" << simTime
         << ",\"aircraft\":[{\"id\":" << id
         << ",\"pos\":["  << s.position.x << "," << s.position.y << "," << s.position.z << "]"
         << ",\"vel\":["  << s.velocity.x << "," << s.velocity.y << "," << s.velocity.z << "]"
         << ",\"quat\":[" << s.attitude.x << "," << s.attitude.y << "," << s.attitude.z << "," << s.attitude.w << "]}]"
         << "}\n";

    wcout << L"t=" << simTime << L"  Position = " << s.position << endl;
}

////////////////////
// 8. Tear down
//////////
void AircraftFederate::resignAndDestroy() {
    VariableLengthData tag((void*)"bye", 4);
    rtiamb->deleteObjectInstance(this->ownAircraft, tag);
    rtiamb->resignFederationExecution(NO_ACTION);
    wcout << L"Resigned from federation" << endl;

    // Only the last federate out succeeds here; the others are expected to fail.
    try {
        rtiamb->destroyFederationExecution(FEDERATION);
        wcout << L"Destroyed federation" << endl;
    } catch (FederatesCurrentlyJoined&) {
        wcout << L"Others still joined; leaving federation for them to destroy" << endl;
    } catch (FederationExecutionDoesNotExist&) {
        wcout << L"Federation already gone" << endl;
    }

    rtiamb->disconnect();
    wcout << L"Disconnected from RTI" << endl;
}
