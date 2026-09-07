#pragma once

#include <RTI/VariableLengthData.h>            // full type — the encoding headers only forward-declare it
#include <RTI/encoding/BasicDataElements.h>
#include <RTI/encoding/HLAfixedArray.h>
#include "../core/Math.hpp"

using namespace rti1516e;

// Turns a Vec3 <-> the on-wire bytes for the FOM's Vector3D (an HLAfixedArray of
// 3 HLAfloat64BE). Position and Velocity both ride on this. Encoding is the one
// place partition invariance can silently break, so it lives behind two named
// functions rather than being scattered through the federate/ambassador.
VariableLengthData encodeVec3(const Vec3& v);
Vec3 decodeVec3(const VariableLengthData& data);
