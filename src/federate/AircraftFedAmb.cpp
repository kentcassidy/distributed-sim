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

AircraftFedAmb::~AircraftFedAmb() {} // was using override here because example had throw()... removal was proper?

GhostRecord::GhostRecord(const std::wstring theObjectName, 
                        ) {
    this->name = theObjectName; // Wondering, does a string copy work like this? I remember strncpy was necessary in C or it's lost
    this->id = 0;
    //this->state.position already initialized to 0.0s
}

////////////////////
// Instance methods
//////////
// perhaps this helper should be placed into Encoding? Thoughts?
double AircraftFedAmb::convertTime(const LogicalTime& theTime) { // I would like some assistance understanding this, copied from example. I glanced online to see this dynamic_cast isn't recommended?
    const HLAfloat64Time& castTime = dynamic_cast<const HLAfloat64Time&>(theTime);
    return castTime.getTime();
}

////////////////////
// Time Callbacks
//////////
// The example uses the word Callback, and you have used it many times as well. What does this really mean? Fundamentally, simply...
void timeAdvanceGrant(const LogicalTime& theFederateTime) {
    this->isAdvancing = false;
    this->federateTime = convertTime(theFederateTime);
}

////////////////////
// Object Management Callbacks
//////////
// discover object methods
void AircraftFedAmb::discoverObjectInstance(ObjectInstanceHandle theObject,
                                            ObjectClassHandle theObjectClass,
                                            const std::wrtring& theObjectName) {
    // Called when a NEW remote Aircraft is registered by another federate.
    wcout << L"Discovered Object: handle=" << theObject
        << L", classHandle=" << theObjectClass
        << L", name=" << theObjectName << endl;
}
// Still not sure if this set of functions counts as an "update" or one time initializer... This would greatly change how this is implemented. 
void AircraftFedAmb::discoverObjectInstance(ObjectInstanceHandle theObject,
                                            ObjectClassHandle theObjectClass,
                                            const std::wstring& theObjectName,
                                            FederateHandle producingFederate) {
    // Called when a New remote Aircraft is registered by another federate.
    wcout << L"Discovered Object: handle=" << theObject
        << L", classHandle=" << theObjectClass
        << L", name=" << theObjectName
        << L", createdBy=" << producingFederate << endl;
    this->ghostRecordList.emplace(theObject, GhostRecord(theObjectName));
}

/* TODO
    OVERRIDE discoverObjectInstance(theObject, theClass, objectName):
      // Called when a NEW remote Aircraft is registered by another federate.
      create an empty GhostRecord, insert into the map keyed by theObject
      log "discovered <objectName>"

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