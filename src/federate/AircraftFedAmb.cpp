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
// HLAfloat64Time, so we downcast to read the double back out.
double AircraftFedAmb::convertTime(const LogicalTime& theTime) {
    const HLAfloat64Time& castTime = dynamic_cast<const HLAfloat64Time&>(theTime);
    return castTime.getTime();
}

////////////////////
// Time Callbacks
//////////
// Not exercised yet (no time management this slice), but a declared virtual must be
// defined or the vtable is unresolved.
void AircraftFedAmb::timeAdvanceGrant(const LogicalTime& theFederateTime)
    throw(FederateInternalError) {
    this->isAdvancing  = false;
    this->federateTime = convertTime(theFederateTime);
}

////////////////////
// Object Management Callbacks (Live experiment; unused in the constructive core)
//////////
void AircraftFedAmb::discoverObjectInstance(ObjectInstanceHandle theObject,
                                            ObjectClassHandle theObjectClass,
                                            const std::wstring& theObjectName)
    throw(FederateInternalError) {
    wcout << L"Discovered Object: handle=" << theObject
          << L", name=" << theObjectName << endl;
    this->ghosts[theObject].name = theObjectName;
}

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
        }
    }
}

////////////////////
// Interaction Callback -- the control plane (AssignEntity + StartRun)
//////////
void AircraftFedAmb::receiveInteraction(InteractionClassHandle theInteraction,
                                        const ParameterHandleValueMap& theParameterValues,
                                        const VariableLengthData& tag,
                                        OrderType sentOrder,
                                        TransportationType theType,
                                        SupplementalReceiveInfo theReceiveInfo)
    throw(FederateInternalError) {
    ParameterHandleValueMap::const_iterator it;

    // AssignEntity: "you own EntityId, starting from this state" -- but only if it's for us.
    if (theInteraction == this->assignClass) {
        it = theParameterValues.find(this->assignTarget);
        if (it == theParameterValues.end()) return;
        if (decodeString(it->second) != this->myName) return;    // not addressed to me

        EntitySpec spec;
        if ((it = theParameterValues.find(assignId))     != theParameterValues.end()) spec.id = decodeUint32(it->second);
        if ((it = theParameterValues.find(assignPos))    != theParameterValues.end()) spec.initial.position = decodeVec3(it->second);
        if ((it = theParameterValues.find(assignVel))    != theParameterValues.end()) spec.initial.velocity = decodeVec3(it->second);
        if ((it = theParameterValues.find(assignOrient)) != theParameterValues.end()) spec.initial.attitude = decodeQuat(it->second);
        this->assignments.push_back(spec);
        wcout << L"[" << myName << L"] assigned entity " << spec.id << endl;
        return;
    }

    // StartRun: "begin," carrying the shared run config. Latch it -- the federate spins on
    // startReceived. (AssignEntity messages are sent BEFORE StartRun, so by the time this
    // latches, our assignments are already in.)
    if (theInteraction == this->startClass) {
        if ((it = theParameterValues.find(startDt))       != theParameterValues.end()) this->dt       = decodeDouble(it->second);
        if ((it = theParameterValues.find(startWorldMin)) != theParameterValues.end()) this->worldMin = decodeVec3(it->second);
        if ((it = theParameterValues.find(startWorldMax)) != theParameterValues.end()) this->worldMax = decodeVec3(it->second);
        this->startReceived = true;
        wcout << L"[" << myName << L"] StartRun received (dt=" << this->dt << L")" << endl;
        return;
    }

    // Shutdown: the controller has declared the sim complete. Latch it -- the federate's
    // serve loop spins on this and then resigns.
    if (theInteraction == this->shutdownClass) {
        this->shutdownReceived = true;
        wcout << L"[" << myName << L"] Shutdown received" << endl;
        return;
    }
}
