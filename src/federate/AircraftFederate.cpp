#include <iostream>
#include <vector>
#include <RTI/RTI1516.h>
#include <RTI/RTIambassadorFactory.h>
#include "AircraftFederate.hpp"

using namespace std;

// Fixed for the MVP handshake. FOM_MODULE is resolved against the process working
// directory, so run both federates from the repo root (or pass an absolute path).
static const wstring FEDERATION     = L"DffFederation";
static const wstring FOM_MODULE     = L"foms/dff-fom.fed";      // 1.3 .fed for bring-up; XML is the eventual deliverable
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
    createAndJoin(federateName);
    cacheHandles();
    publishAndSubscribe();
    registerOwnAircraft();

    // Barrier for the two-federate demo: hold here until BOTH federates have
    // registered, so their publish loops overlap and discovery/reflection cross.
    // Skipped when non-interactive (CI smoke test) so it doesn't block.
    if (interactive)
        waitForUser();

    // Main loop: no time management yet. Publish our Position each tick, then
    // evoke callbacks so the RTI delivers the OTHER federate's discover/reflect.
    for (int i = 0; i < 50; i++) {
        step(i * 1.0);
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
// 2-3. Create (idempotent) + join
//////////
void AircraftFederate::createAndJoin(wstring federateName) {
    // Whichever federate starts first creates the execution; the rest just join.
    // Pass the FOM as a MODULE LIST (what both shipped examples do) rather than a
    // bare string — the single-string overload doesn't load it the same way.
    try {
        vector<wstring> fomModules;
        fomModules.push_back(FOM_MODULE);
        this->rtiamb->createFederationExecution(FEDERATION, fomModules);
        wcout << L"Created federation " << FEDERATION << L" (fresh, from " << FOM_MODULE << L")" << endl;
    } catch (FederationExecutionAlreadyExists&) {
        wcout << L"Federation already existed; joining it (NOTE: my FOM edits are NOT reloaded)" << endl;
    }

    this->rtiamb->joinFederationExecution(federateName, L"Aircraft", FEDERATION);
    wcout << L"Joined as " << federateName << endl;
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
    wcout << L"Published and subscribed Aircraft.Position" << endl;
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
// 7. One step: dummy kinematics + publish Position
//////////
void AircraftFederate::step(double simTime) {
    // Placeholder until dff_core's flight model lands: drift along +x over time.
    Vec3 position(simTime * 10.0, 0.0, 0.0);

    AttributeHandleValueMap attributes;
    attributes[this->positionHandle] = encodeVec3(position);

    VariableLengthData tag((void*)"pos", 4);
    rtiamb->updateAttributeValues(this->ownAircraft, attributes, tag);

    wcout << L"Published Position = (" << position.x << L", "
          << position.y << L", " << position.z << L")" << endl;
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
