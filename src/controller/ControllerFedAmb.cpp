#include <algorithm>
#include <iostream>
#include "ControllerFedAmb.hpp"
#include "Encoding.hpp"     // shared wire encoding (src/federate) -- decodeString

using namespace std;

ControllerFedAmb::ControllerFedAmb() {}
ControllerFedAmb::~ControllerFedAmb() throw() {}

// One aircraft federate has announced itself. Record its name (deduplicated, in arrival
// order). Anything that isn't Enroll is ignored -- the controller subscribes only to
// Enroll, but we guard on the class handle anyway.
void ControllerFedAmb::receiveInteraction(
    InteractionClassHandle theInteraction,
    ParameterHandleValueMap const& theParameterValues,
    VariableLengthData const& theUserSuppliedTag,
    OrderType sentOrder,
    TransportationType theType,
    SupplementalReceiveInfo theReceiveInfo)
    throw(FederateInternalError) {
    if (theInteraction != this->enrollClass) return;

    ParameterHandleValueMap::const_iterator it = theParameterValues.find(this->federateNameParam);
    if (it == theParameterValues.end()) return;   // malformed enroll; ignore

    wstring name = decodeString(it->second);

    // A federate may enroll more than once (e.g. if it retried its send). Keep one entry.
    if (find(roster.begin(), roster.end(), name) == roster.end()) {
        roster.push_back(name);
        wcout << L"[controller] enrolled: " << name
              << L"  (roster size = " << roster.size() << L")" << endl;
    }
}
