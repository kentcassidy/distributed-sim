#pragma once

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

// The orchestrator. Owns the RTIambassador (you -> RTI), the AircraftFedAmb
// (RTI -> you), and the cached FOM handles. run() walks the whole HLA lifecycle.
// No physics/time management yet — this is the M1 handshake: publish Position,
// discover + reflect the other federate's aircraft.
class AircraftFederate {
public:
    AircraftFederate();
    ~AircraftFederate();

    void run(wstring federateName, bool interactive = false);

private:
    // lifecycle steps, called in order by run()
    void connectToRti();
    void createAndJoin(wstring federateName);
    void cacheHandles();
    void publishAndSubscribe();
    void registerOwnAircraft();
    void waitForUser();          // demo barrier; skipped in non-interactive (CI) runs
    void initWorld(wstring federateName);   // build this federate's physics + open the NDJSON log
    void step(double simTime, double dt);
    void resignAndDestroy();

    unique_ptr<RTIambassador> rtiamb;   // ctor via factory.createRTIambassador()
    AircraftFedAmb            fedamb;    // by value; passed to connect() by ref

    // FOM handles, resolved once after join and reused in the loop
    ObjectClassHandle aircraftClass;
    AttributeHandle   entityIdHandle;
    AttributeHandle   massHandle;
    AttributeHandle   radiusHandle;
    AttributeHandle   positionHandle;
    AttributeHandle   velocityHandle;
    AttributeHandle   orientationHandle;

    ObjectInstanceHandle ownAircraft;   // the single aircraft this federate owns

    // dff_core physics. model_ is declared BEFORE world_ so it OUTLIVES the aircraft
    // that borrow it (class members are destroyed in reverse declaration order).
    LinearLongitudinal   model_;
    World                world_;
    ofstream             log_;            // NDJSON frames for the browser viewer
    wstring              federateName_;
};
