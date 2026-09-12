#pragma once

#include <string>
#include <vector>
#include <RTI/NullFederateAmbassador.h>

using namespace rti1516e;
using namespace std;

// The controller's ears. It cares about exactly ONE callback -- receiveInteraction, and
// only for Enroll ("federate X has joined the federation"). Every other callback falls
// through to NullFederateAmbassador's empty defaults. The controller fills in enrollClass
// + federateNameParam after cacheHandles() (handles don't exist until joined) so this can
// recognize the interaction and decode its one parameter.
class ControllerFedAmb : public NullFederateAmbassador {
public:
    ControllerFedAmb();
    virtual ~ControllerFedAmb() throw();

    // Set by the controller after join, so the callback can match + decode Enroll.
    InteractionClassHandle enrollClass;
    ParameterHandle        federateNameParam;

    // The roster: names of aircraft federates that have enrolled, in arrival order,
    // deduplicated. The controller freezes this at start; K = roster.size().
    vector<wstring> roster;

    // Only the non-timestamped overload is needed: the control interactions are 'receive'
    // order (no time management), so this is the one Portico calls.
    virtual void receiveInteraction(
        InteractionClassHandle theInteraction,
        ParameterHandleValueMap const& theParameterValues,
        VariableLengthData const& theUserSuppliedTag,
        OrderType sentOrder,
        TransportationType theType,
        SupplementalReceiveInfo theReceiveInfo)
        throw(FederateInternalError);
};
