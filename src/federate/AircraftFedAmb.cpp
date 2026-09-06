#include <iostream>
#include "RTI/time/HLAfloat64Time.h"
#include "AircraftFedAmb.hpp"

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
// Object Management Callbacks
//////////
// discover object methods
void AircraftFedAmb::discoverObjectInstance(ObjectInstanceHandle theObject,
                                            ObjectClassHandle theObjectClass,
                                            const std::wstring& theObjectName) {
    // Called when a New remote Aircraft is registered by another federate.
    wcout << L"Discovered Object: handle=" << theObject
        << L", classHandle=" << theObjectClass
        << L", name=" << theObjectName << endl;
    this->ghosts[theObject].name = theObjectName;
}

/* TODO

    OVERRIDE reflectAttributeValues(theObject, theAttributeValues, tag, ...):
      // Called when a subscribed attribute we care about is updated remotely.
      look up the GhostRecord for theObject (ignore if unknown)
      FOR each (attributeHandle -> encodedBytes) in theAttributeValues:
          IF attributeHandle == Position handle:
              decode bytes -> (x,y,z)   // see Encoding notes
              store into the GhostRecord
      log the reflected position
      // NOTE the FedAmb needs the Position attribute HANDLE to compare against.
      // Options: (a) the Federate sets the handles on the FedAmb after joining,
      // (b) the FedAmb re-queries the RTI. (a) is cleaner — pass them in.

*/