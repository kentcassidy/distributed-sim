#pragma once

#include <map>
#include <string>
#include <vector>
#include <RTI/NullFederateAmbassador.h>
#include "../core/Math.hpp"
#include "../core/State.hpp"
#include "../core/Scenario.hpp"     // EntitySpec = { id, initial State } -- an assigned entity

using namespace rti1516e;
using namespace std;

// A read-only copy of a NEIGHBOUR aircraft (Live experiment only; unused in the
// constructive core, kept for when ghosts/dead-reckoning return).
struct GhostRecord {
    wstring name;
    unsigned int id = 0;
    State state;
};

// The federate's ears. Two jobs now:
//   1) (existing) object discovery/reflection -- kept for the later Live experiment.
//   2) (new) receiveInteraction for the control plane: collect the AssignEntity messages
//      addressed to THIS federate, and latch the StartRun broadcast (dt + world bounds).
// The federate fills in myName + the interaction/parameter handles after cacheHandles().
class AircraftFedAmb : public NullFederateAmbassador {
public:
    // --- time management (declared; not exercised yet) ---
    double federateTime;
    double federateLookahead;
    bool   isRegulating;
    bool   isConstrained;
    bool   isAdvancing;
    bool   isAnnounced;
    bool   isReadyToRun;

    // --- object reflection (Live experiment; unused in the constructive core) ---
    map<ObjectInstanceHandle, GhostRecord> ghosts;
    AttributeHandle positionHandle;   // set after join so reflect() can match Position

    // --- control plane: set by the federate after cacheHandles() ---
    wstring                myName;             // filter AssignEntity by TargetFederate
    InteractionClassHandle assignClass;
    InteractionClassHandle startClass;
    InteractionClassHandle shutdownClass;      // controller's "end the run" signal
    ParameterHandle        assignTarget, assignId, assignPos, assignVel, assignOrient;
    ParameterHandle        startDt, startWorldMin, startWorldMax;

    // --- received from the controller ---
    vector<EntitySpec> assignments;    // entities assigned to ME (id + initial State)
    bool  startReceived = false;       // latched when StartRun arrives
    bool  shutdownReceived = false;    // latched when Shutdown arrives
    double dt = 0.1;                   // from StartRun
    Vec3  worldMin, worldMax;          // from StartRun

    AircraftFedAmb();
    virtual ~AircraftFedAmb() throw();

    virtual void timeAdvanceGrant(const LogicalTime& theFederateTime)
        throw(FederateInternalError);

    virtual void discoverObjectInstance(ObjectInstanceHandle theObject,
                                        ObjectClassHandle theObjectClass,
                                        const std::wstring& theObjectName)
        throw(FederateInternalError);

    virtual void reflectAttributeValues(ObjectInstanceHandle theObject,
                                        const AttributeHandleValueMap& theAttributes,
                                        const VariableLengthData& tag,
                                        OrderType sentOrder,
                                        TransportationType theType,
                                        SupplementalReflectInfo theReflectInfo)
        throw(FederateInternalError);

    // Control interactions are 'receive' order, so the non-timestamped overload fires.
    virtual void receiveInteraction(InteractionClassHandle theInteraction,
                                    const ParameterHandleValueMap& theParameterValues,
                                    const VariableLengthData& tag,
                                    OrderType sentOrder,
                                    TransportationType theType,
                                    SupplementalReceiveInfo theReceiveInfo)
        throw(FederateInternalError);

private:
    double convertTime(const LogicalTime& theTime);
};
