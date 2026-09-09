#include "LinearLongitudinal.hpp"
#include <cmath>

// derivative() -- equations of motion for ONE aircraft: a linearized longitudinal
// model about a steady cruise. MINIMAL FILLER for a distributed-systems PoC: the
// coefficients are arbitrary-but-stable (see AircraftParams), not a real aircraft.
// What matters is that it is (1) STABLE/bounded and (2) DETERMINISTIC -- same input,
// same output -- because the real V&V is partition invariance (identical result
// whether the world runs on one machine or split across several).
//
// Shape: reduce the full State to a reduced perturbation [u, w, q, theta] about
// trim, apply the 4x4 A-matrix, expand the rates back into a StateDot. Exactly
// linear (small-angle embedding), so a stable A can never diverge. Position
// advances at the FULL world velocity (the constant cruise term), so the aircraft
// actually travels across sectors -- which is what exercises the distributed code.
StateDot LinearLongitudinal::derivative(const State& x, const AircraftParams& p) const {
    const AircraftParams::LonDerivs& d = p.lon;

    // --- reduce: State -> [u, w, q, theta] about trim (small-angle) ---
    // A pure pitch quaternion is [0, sin(th/2), 0, cos(th/2)] ~ [0, th/2, 0, 1],
    // so theta ~ 2*attitude.y -- an exactly-linear read-out of pitch.
    const double u  = x.velocity.x - p.trimSpeed;      // forward-speed perturbation
    const double w  = x.velocity.z - p.trimW;          // vertical-speed perturbation
    const double q  = x.angularV.y;                    // pitch rate
    const double th = 2.0 * x.attitude.y - p.trimPitch;

    // --- fold in the standard Mw_dot corrections (Mu*, Mw*, Mq*) ---
    const double Mu = d.Mu + d.Mw_dot * d.Zu;
    const double Mw = d.Mw + d.Mw_dot * d.Zw;
    const double Mq = d.Mq + d.Mw_dot * (p.trimSpeed + d.Zq);

    const double c = std::cos(p.trimPitch);
    const double s = std::sin(p.trimPitch);
    const double g = p.gravity;

    // --- apply the 4x4 A-matrix: [u,w,q,theta] -> [udot,wdot,qdot,thetadot] ---
    const double udot  = d.Xu*u + d.Xw*w + (d.Xq - p.trimW)*q     - g*c*th;
    const double wdot  = d.Zu*u + d.Zw*w + (d.Zq + p.trimSpeed)*q - g*s*th;
    const double qdot  = Mu*u   + Mw*w   + Mq*q                   - d.Mw_dot*g*s*th;
    const double thdot = q;

    // --- expand rates back into a StateDot (inverse of the reduction) ---
    StateDot dot;
    dot.dPosition = x.velocity;                          // travel through the world at full velocity
    dot.dVelocity = Vec3{ udot, 0.0, wdot };             // longitudinal only; lateral untouched
    dot.dAngularV = Vec3{ 0.0, qdot, 0.0 };              // pitch axis only
    dot.dAttitude = Quat{ 0.0, 0.5 * thdot, 0.0, 0.0 };  // quaternion RATE; integrator renormalizes
    return dot;
}
