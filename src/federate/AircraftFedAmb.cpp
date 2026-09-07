#include <iostream>
#include "RTI/time/HLAfloat64Time.h"
#include "AircraftFedAmb.hpp"
#include "Encoding.hpp"

using namespace std;

////////////////////
// Constructors
//////////
AircraftFedAmb::AircraftFedAmb() {
    this->federateTime      = 0.0;
    this->federateLookahead = 1.0;

    this->isRegulating      = false;
    this->isConstrained     = false;
    this->isAdvancing       = false;
    this->isAnnounced       = false;
    this->isReadyToRun      = false;
}

AircraftFedAmb::~AircraftFedAmb() throw() {}

////////////////////
// Helpers
//////////
// LogicalTime is an abstract interface; the concrete type in this federation is
// HLAfloat64Time, so we downcast to read the double back out. dynamic_cast is the
// correct tool here — we genuinely must recover a concrete type the interface hides.
double AircraftFedAmb::convertTime(const LogicalTime& theTime) {
    const HLAfloat64Time& castTime = dynamic_cast<const HLAfloat64Time&>(theTime);
    return castTime.getTime();
}

////////////////////
// Time Callbacks
//////////
// Not exercised yet (no time management this slice), but a declared virtual must
// be defined or the vtable is unresolved. Sets the flag run() spins on later.
void AircraftFedAmb::timeAdvanceGrant(const LogicalTime& theFederateTime)
    throw(FederateInternalError) {
    this->isAdvancing  = false;
    this->federateTime = convertTime(theFederateTime);
}

////////////////////
// Object Management Callbacks
//////////
// Called once when another federate registers a new Aircraft. We create the ghost
// record here (operator[] default-constructs it) and stamp its name.
void AircraftFedAmb::discoverObjectInstance(ObjectInstanceHandle theObject,
                                            ObjectClassHandle theObjectClass,
                                            const std::wstring& theObjectName)
    throw(FederateInternalError) {
    wcout << L"Discovered Object: handle=" << theObject
          << L", classHandle=" << theObjectClass
          << L", name=" << theObjectName << endl;
    this->ghosts[theObject].name = theObjectName;
}

// Called every time a subscribed attribute changes on a remote Aircraft. We look
// up (or create) the ghost, then decode any attribute we recognize into it.
void AircraftFedAmb::reflectAttributeValues(ObjectInstanceHandle theObject,
                                            const AttributeHandleValueMap& theAttributes,
                                            const VariableLengthData& tag,
                                            OrderType sentOrder,
                                            TransportationType theType,
                                            SupplementalReflectInfo theReflectInfo)
    throw(FederateInternalError) {
    GhostRecord& ghost = this->ghosts[theObject];

    AttributeHandleValueMap::const_iterator it;
    for (it = theAttributes.begin(); it != theAttributes.end(); ++it) {
        if (it->first == this->positionHandle) {
            ghost.state.position = decodeVec3(it->second);
            wcout << L"Reflected Position for " << ghost.name
                  << L" = " << ghost.state.position << endl;
        }
    }
}
