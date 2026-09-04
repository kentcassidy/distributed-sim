#include "dff_core.hpp"

namespace dff {

std::string core_version() {
    return "dff_core 0.1.0 (skeleton)";
}

}  // namespace dff

class World {
    std::vector<Aircraft> owned_;
    std::vector<Aircraft> ghosts_;
    Sector                sector_;
public:
    void advance(double dt) {
        for (auto& ac : owned_) ac.advance(dt); // integrate
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