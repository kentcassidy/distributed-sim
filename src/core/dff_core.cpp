#include "dff_core.hpp"
#include <vector>

namespace dff {

std::string core_version() {
    return "dff_core 0.1.0 (skeleton)";
}

}  // namespace dff

/*
// Define your aliases BEFORE the class so the compiler recognizes them
using EntityId       = int;          // Alias to a standard int
using State          = std::string;  // Alias to a standard string
using AircraftParams = double;       // Alias to a standard double

// Forward declaration of your pointer's type
struct DynamicsModel; 
*/

class Sector {
    double xmin;
    double xmax;
    double ymin;
    double ymax;
}

class World {
    std::vector<Aircraft> owned_;
    std::vector<Aircraft> ghosts_;
    std::vector<Sector>                sectors_;
public:
    void advance(double dt) {
        for (auto& ac : self->owned_) ac.advance(dt); // integrate
        resolveCollisions();                    // owned-owned; owned-ghost at seam
    }
};


class DynamicsModel {
public:
        virtual ~DynamicsModel() = default;
        virtual State derivative(const State&, const AircraftParams&) const = 0;
};
class LinearLongitudinal : public DynamicsModel { /* stability derivatives */};

class Aircraft {
    EntityId       id_;
    State          state_;
    AircraftParams params_;
    DynamicsModel* model_;
public:
    void advance(double dt);
};