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
        if ((it = theParameterValues.find(assignAngV))   != theParameterValues.end()) spec.initial.angularV = decodeVec3(it->second);
        this->assignments.push_back(spec);
        wcout << L"[" << myName << L"] assigned entity " << spec.id << endl;
        return;
    }

    // AssignSector: one per slab, tagged with its owner, BROADCAST to every federate. Keep
    // them ALL as the partition map (so we can compute a handoff destination ourselves,
    // peer-to-peer, with no controller round-trip); the slab(s) owned by ME are ALSO
    // installed into World for leave-detection.
    if (theInteraction == this->assignSectorClass) {
        it = theParameterValues.find(this->sectorTarget);
        if (it == theParameterValues.end()) return;
        wstring owner = decodeString(it->second);

        Sector s;
        if ((it = theParameterValues.find(sectorId))  != theParameterValues.end()) s.id  = decodeUint32(it->second);
        if ((it = theParameterValues.find(sectorMin)) != theParameterValues.end()) s.min = decodeVec3(it->second);
        if ((it = theParameterValues.find(sectorMax)) != theParameterValues.end()) s.max = decodeVec3(it->second);

        RegionOwner ro; ro.owner = owner; ro.sector = s;
        this->partitionMap.push_back(ro);
        if (owner == this->myName) {
            this->assignedSectors.push_back(s);
            wcout << L"[" << myName << L"] assigned sector " << s.id << L" (mine)" << endl;
        }
        return;
    }

    // Handoff: a peer is transferring an aircraft to US -- adopt only if addressed to me.
    // Queue it (id + exact state + the logical step the state is valid at); the federate
    // drains the queue in its serve loop and continues integrating from step+1.
    if (theInteraction == this->handoffClass) {
        it = theParameterValues.find(this->handoffTarget);
        if (it == theParameterValues.end()) return;
        if (decodeString(it->second) != this->myName) return;   // not addressed to me

        HandoffIn h;
        if ((it = theParameterValues.find(handoffId))     != theParameterValues.end()) h.spec.id               = decodeUint32(it->second);
        if ((it = theParameterValues.find(handoffPos))    != theParameterValues.end()) h.spec.initial.position = decodeVec3(it->second);
        if ((it = theParameterValues.find(handoffVel))    != theParameterValues.end()) h.spec.initial.velocity = decodeVec3(it->second);
        if ((it = theParameterValues.find(handoffOrient)) != theParameterValues.end()) h.spec.initial.attitude = decodeQuat(it->second);
        if ((it = theParameterValues.find(handoffAngV))   != theParameterValues.end()) h.spec.initial.angularV = decodeVec3(it->second);
        if ((it = theParameterValues.find(handoffStep))   != theParameterValues.end()) h.step                  = decodeUint32(it->second);
        this->incomingHandoffs.push_back(h);
        wcout << L"[" << myName << L"] received handoff of entity " << h.spec.id
              << L" at step " << h.step << endl;
        return;
    }

    // StartRun: "begin," carrying the shared run config. Latch it -- the federate spins on
    // startReceived. (AssignEntity messages are sent BEFORE StartRun, so by the time this
    // latches, our assignments are already in.)
    if (theInteraction == this->startClass) {
        if ((it = theParameterValues.find(startDt))       != theParameterValues.end()) this->dt       = decodeDouble(it->second);
        if ((it = theParameterValues.find(startWorldMin)) != theParameterValues.end()) this->worldMin = decodeVec3(it->second);
        if ((it = theParameterValues.find(startWorldMax)) != theParameterValues.end()) this->worldMax = decodeVec3(it->second);
        if ((it = theParameterValues.find(startNumSteps)) != theParameterValues.end()) this->numSteps = decodeUint32(it->second);
        this->startReceived = true;
        wcout << L"[" << myName << L"] StartRun received (dt=" << this->dt
              << L", numSteps=" << this->numSteps << L")" << endl;
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
