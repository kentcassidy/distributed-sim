#include "WeathervaneFlight.hpp"
#include <cmath>

// derivative() -- rotational equations of motion for ONE aircraft. The aircraft flies at its
// current (world-frame) velocity; this model turns the NOSE to point along that velocity via a
// restoring torque (a linearized weathervane), plus damping, plus a dihedral bank into the
// turn. See WeathervaneFlight.hpp for the frame conventions and scope.
StateDot WeathervaneFlight::derivative(const State& x, const AircraftParams& p) const {
    const AircraftParams::AeroFiller& a = p.aero;

    StateDot sd;
    sd.dPosition = x.velocity;          // travel through the world at full velocity
    sd.dVelocity = Vec3{0.0, 0.0, 0.0}; // Option A: velocity holds; the nose turns to meet it

    // No defined heading at (near) zero speed -- freeze the attitude to avoid a 0/0 direction.
    const double V = std::sqrt(dot(x.velocity, x.velocity));   // dot() = Math.hpp dot product
    if (V < 1e-6) {
        sd.dAttitude = Quat{0.0, 0.0, 0.0, 0.0};
        sd.dAngularV = Vec3{0.0, 0.0, 0.0};
        return sd;
    }

    // Weathervane restoring torque, sign-safe via a cross product. noseWorld is the current
    // nose (body +X) in world space; vHat is the velocity direction. cross(noseWorld, vHat) is
    // the axis that rotates the nose TOWARD the velocity (magnitude ~ sin of the misalignment).
    // Rotate that error into the body frame with the conjugate (inverse of a unit quaternion),
    // where its y-component is the pitch error and its z-component the yaw error (the roll
    // component is ~0, being perpendicular to the nose).
    const Quat& q = x.attitude;
    const Vec3 noseWorld = q * Vec3{1.0, 0.0, 0.0};
    const Vec3 vHat      = x.velocity / V;
    const Vec3 errWorld  = cross(noseWorld, vHat);
    const Quat qConj{-q.x, -q.y, -q.z, q.w};      // unit q => inverse = conjugate
    const Vec3 errBody   = qConj * errWorld;

    // Body angular ACCELERATIONS = restoring (toward alignment) + damping (opposes rotation,
    // always stabilizing). angularV is (roll=x, pitch=y, yaw=z).
    const double rollAcc  = -a.kDihedral * errBody.z - a.kRollDamp  * x.angularV.x;
    const double pitchAcc =  a.kAlign    * errBody.y - a.kPitchDamp * x.angularV.y;
    const double yawAcc   =  a.kAlign    * errBody.z - a.kYawDamp   * x.angularV.z;
    sd.dAngularV = Vec3{rollAcc, pitchAcc, yawAcc};

    // Full quaternion kinematics: qdot = 0.5 * q (x) omega_body. Quat(vec, w) packs the body
    // rate as a pure quaternion; operator*(Quat,Quat) is the Hamilton product; the integrator
    // does q += qdot*dt then renormalizes.
    const Quat omega{x.angularV, 0.0};
    sd.dAttitude = 0.5 * (q * omega);
    return sd;
}
