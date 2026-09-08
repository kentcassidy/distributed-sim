#include "LinearLongitudinal.hpp"

// TODO(M2-2, M2-4): populate the A-matrix from published stability derivatives,
// reduce the full State to [u, w, q, theta] about trim, apply the matrix, and
// expand the rates back into a StateDot. Stub returns zero rates (no motion).
StateDot LinearLongitudinal::derivative(const State& x, const AircraftParams& p) const {
    return StateDot{};   // TODO
}
