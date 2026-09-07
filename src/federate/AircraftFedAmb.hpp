#pragma once

#include <map>
#include <tuple>
#include <vector>
#include <RTI/NullFederateAmbassador.h>
#include "../core/Math.hpp"
#include "../core/State.hpp"

using namespace rti1516e;
using namespace std;

struct GhostRecord { // changed to a struct, is that okay???
    wstring name;
    unsigned int id = 0; //
    State state;
};

class AircraftFedAmb : public NullFederateAmbassador {
public:
    // variables //
    double federateTime;
    double federateLookahead;

    bool isRegulating;
    bool isConstrained;
    bool isAdvancing;
    bool isAnnounced;
    bool isReadyToRun;

    map<ObjectInstanceHandle, GhostRecord> ghosts;

    // Set by the federate after join() so reflect() can recognize which entry in
    // the attribute map is Position. Handles don't exist until we've joined.
    AttributeHandle positionHandle;

    // methods//
    AircraftFedAmb();
    virtual ~AircraftFedAmb() throw();

    ////////////////////
    // Time Related Methods
    //////////
    virtual void timeAdvanceGrant(const LogicalTime& theFederateTime)
        throw(FederateInternalError);

    ////////////////////
    // Object Management Methods
    //////////
    // object discovery
    virtual void discoverObjectInstance(ObjectInstanceHandle theObject,
                                        ObjectClassHandle theObjectClass,
                                        const std::wstring& theObjectName)
        throw(FederateInternalError);

    // attribute reflection
    virtual void reflectAttributeValues(ObjectInstanceHandle theObject,
                                        const AttributeHandleValueMap& theAttributes,
                                        const VariableLengthData& tag,
                                        OrderType sentOrder,
                                        TransportationType theType,
                                        SupplementalReflectInfo theReflectInfo)
        throw(FederateInternalError);
                                        
private:
        double convertTime(const LogicalTime& theTime);
};