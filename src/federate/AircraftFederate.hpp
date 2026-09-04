/*
The orchestrator. Owns the RTI ambassador, the AircraftFedAmb, a World, and the handle->Aircraft map. run() is the event loop.
*/

#include "dff_core.hpp"
namespace dff{
class AircraftFederate {
public:
    explicit AircraftFederate(std::string configPath);
    void run();
private:
    void join(); void publishOwned(); void doHandoffs(); void resign();

    std::unique_ptr<rti1516e::RTIambassador>    rti_;    // Portico
    AircraftFedAmb                              fedAmb_; // callbacks (back-ref to *this)
    World                                       world_;  // the physics <- composition
    std::map<rti1516e::ObjectInstanceHandle, Aircraft*> instances_; // HLA <-> C++
    double t_ = 0.0, dt_ = 0.01, tEnd_ = 60.0;
};
}