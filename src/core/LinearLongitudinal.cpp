#include "LinearLongitudinal.hpp"

// TODO(M2-2, M2-4): populate the A-matrix from published stability derivatives,
// reduce the full State to [u, w, q, theta] about trim, apply the matrix, and
// expand the rates back into a StateDot. Stub returns zero rates (no motion).
StateDot LinearLongitudinal::derivative(const State& x, const AircraftParams& p) const {
    return StateDot{};   // TODO
}

/*
Found online:

[ u_dot ]   [ X_u   X_w   (X_q - w0)       -g*cos(th0) ] [ u ]
[ w_dot ] = [ Z_u   Z_w   (Z_q + u0)       -g*sin(th0) ] [ w ]
[ q_dot ]   [ M_u   M_w   (M_q + M_w_dot)       0      ] [ q ]
[ th_dot]   [  0     0         1                0      ] [ th]

Where:
M_u     = M_u_raw + M_w_dot * Z_u
M_w     = M_w_raw + M_w_dot * Z_w
M_q_tot = M_q     + M_w_dot * (u0 + Z_q)

*/

/*
Unicode Ver:

┌ ̇u ┐   ┌ X_u   X_w   X_q - w₀        -g·cos(θ₀) ┐ ┌ u ┐
│ ̇w │   │ Z_u   Z_w   Z_q + u₀        -g·sin(θ₀) │ │ w │
│ ̇q │ = │ M_u*  M_w*  M_q*         -M_ẇ·g·sin(θ₀)│ │ q │
└ ̇θ ┘   └  0     0       1                 0     ┘ └ θ ┘

*Note: M_u*, M_w*, and M_q* include the standard M_ẇ correction terms:
 M_u* = M_u + M_ẇ·Z_u
 M_w* = M_w + M_ẇ·Z_w
 M_q* = M_q + M_ẇ·(u₀ + Z_q)

*/