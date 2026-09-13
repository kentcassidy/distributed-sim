#pragma once

#include <map>
#include <string>
#include <vector>
#include <RTI/NullFederateAmbassador.h>
#include "../core/Math.hpp"
#include "../core/State.hpp"
#include "../core/Sector.hpp"       // sector AABB assigned to this federate
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

// One entry of the broadcast partition map: a sector and the federate that owns it. Every
// federate keeps the WHOLE map (not just its own slab) so it can compute, on its own, which
// peer to hand a departing aircraft to -- no controller round-trip (peer-to-peer migration).
struct RegionOwner {
    wstring owner;
    Sector  sector;
};

// A received Handoff, queued for the federate to adopt: the transferred aircraft (id + exact
// state) and the logical step that state is valid at. The adopter continues from step+1.
struct HandoffIn {
    EntitySpec spec;
    long long  step = 0;
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
    InteractionClassHandle assignSectorClass;  // controller's "your sector is ..." message
    InteractionClassHandle handoffClass;       // peer-to-peer ownership transfer (fed -> fed)
    ParameterHandle        assignTarget, assignId, assignPos, assignVel, assignOrient, assignAngV;
    ParameterHandle        startDt, startWorldMin, startWorldMax, startNumSteps;
    ParameterHandle        sectorTarget, sectorId, sectorMin, sectorMax;
    ParameterHandle        handoffTarget, handoffId, handoffPos, handoffVel, handoffOrient, handoffAngV, handoffStep;

    // --- received from the controller ---
    vector<EntitySpec> assignments;      // entities assigned to ME (id + initial State)
    vector<Sector>     assignedSectors;  // sector(s) assigned to ME (installed into World)
    vector<RegionOwner> partitionMap;    // the WHOLE partition (all sectors + owners)
    vector<HandoffIn>  incomingHandoffs; // peer handoffs addressed to ME, awaiting adoption
    bool  startReceived = false;         // latched when StartRun arrives
    bool  shutdownReceived = false;      // latched when Shutdown arrives
    double dt = 0.1;                   // from StartRun
    unsigned int numSteps = 100;       // from StartRun (controller owns the run length)
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
