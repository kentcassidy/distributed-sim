#pragma once

#include <memory>
#include <string>
#include <RTI/RTI1516.h>
#include "AircraftFedAmb.hpp"
#include "Encoding.hpp"

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
    void step(double simTime);
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
};
