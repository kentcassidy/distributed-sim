#ifndef AIRCRAFTFEDAMB_H_ // H or HPP? or use #pragma once?
#define AIRCRAFTFEDAMB_H_

#include <RTI/NullFederateAmbassador.h>

using namespace rti1516e;
using namespace std;


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

    // methods//
    AircraftFedAmb();
    virtual ~AircraftFedAmb() throw();

    //////////
    // synchronization point methods unnecessary?
    //////////

    //////////
    // time related methods
    //////////
    virtual void timeAdvanceGrant(const LogicalTime& theFederateTime) override;

    //////////
    // object management methods
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