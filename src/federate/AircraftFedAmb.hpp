#ifndef AIRCRAFTFEDAMB_H_ // H or HPP? or use #pragma once?
#define AIRCRAFTFEDAMB_H_

#include <map>
#include <tuple>
#include <RTI/NullFederateAmbassador.h>
#include "../core/Math.hpp"
#include "../core/State.hpp"

using namespace rti1516e;
using namespace std;

class GhostRecord {
public:
    wstring name;
    unsigned int id = 0; //
    State state;

    GhostRecord();
    ~GhostRecord();
}

class AircraftFedAmb : public NullFederateAmbassador {
public:
    // variables //
    double federateTime;
    double federateLookahead; // create some config for this? Or is it dynamic?

    bool isRegulating;
    bool isConstrained;
    bool isAdvancing;
    bool isAnnounced;
    bool isReadyToRun;

/*
    STATE: // Is the state here different than the State.hpp in core?
      a map from ObjectInstanceHandle -> a small "GhostRecord" { has EntityId,
          last Position, maybe name }.  // the federate reads this after callbacks
      // WHY the map lives here: callbacks arrive asynchronously from the RTI;
      // the FedAmb is where the RTI hands us data, so it's the natural landing
      // zone. Alternative: give the FedAmb a pointer back to the Federate/World
      // and write straight into it — cleaner long-term, but couples the two;
      // for the handshake a local map is simpler and we can inject the World later.

    I am sort of tracking what this part means, but I need more clarification. What really is an Ambassador? Is there only one running per computer instance? Is it the primary? Does this only hold one STATE or vector of STATEs?
    What is the difference between EntityId, ObjectInstanceHandle, and Name? Some are Enumerated, some have special methods? Why do I need all?
    Below is my best guess implementation. 
    // I'm thinking this may be too primitive and I should perhaps define a GhostRecords class?
    */
    // Handle --> Name (or other), Physical State 
    map<ObjectInstanceHandle, tuple<wstring, State>> ghostRecords;
    // Alternatively
    map<ObjectInstanceHandle, GhostRecord> ghostRecordList;
    // Even Alternatively
    vector<GhostRecord> ghostDirectory; // Assuming GhostRecord subsumes the handler?
    
    // methods//
    AircraftFedAmb();
    virtual ~AircraftFedAmb() throw();

    ////////////////////
    // synchronization point methods unnecessary?
    //////////

    ////////////////////
    // Time Related Methods
    //////////
    virtual void timeAdvanceGrant(const LogicalTime& theFederateTime) override;

    ////////////////////
    // Object Management Methods
    //////////
    // object discovery
    virtual void discoverObjectInstance(ObjectInstanceHandle theObject,
                                        ObjectClassHandle theObjectClass,
                                        const std::wstring& theObjectName) override;
        // throw(FederateInternalError); was advised to use override instead

    virtual void discoverObjectInstance(ObjectInstanceHandle theObject,
                                        ObjectClassHandle theObjectClass,
                                        const std::wstring& theObjectName,
                                        FederateHandle producingFederate) override;

    // attribute reflection
    virtual void reflectAttributeValues(ObjectInstanceHandle theObject, // Assuming we want determinism so we get rid of this one?
                                        const AttributeHandleValueMap& theAttributes,
                                        const VariableLengthData& tag,
                                        OrderType sentOrder,
                                        TransportationType theType,
                                        SupplementalReflectInfo theReflectInfo) override;

    virtual void reflectAttributeValues(ObjectInstanceHandle theObject, // the one we probably like to use
                                        const AttributeHandleValueMap& theAttributes,
                                        const VariableLengthData& tag,
                                        OrderType sentOrder,
                                        TransportationType theType,
                                        const LogicalTime& theTime,
                                        OrderType receivedOrder,
                                        SupplementalReflectInfo theReflectInfo) override;
    
    virtual void reflectAttributeValues(ObjectInstanceHandle theObject, // keeping this around in case we face contentious collision calcs?
                                        const AttributeHandleValueMap& theAttributes,
                                        const VariableLengthData& tag,
                                        OrderType sentOrder,
                                        TransportationType theType,
                                        const LogicalTime& theTime,
                                        OrderType receivedOrder,
                                        MessageRetractionHandle theHandle,
                                        SupplementalReflectInfo theReflectInfo) override;            
                                        
private:
        double convertTime(const LogicalTime& theTime);
};

#endif //AIRCRAFTFEDAMB_H_