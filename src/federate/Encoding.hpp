#pragma once

#include <RTI/VariableLengthData.h>            // full type — the encoding headers only forward-declare it
#include <RTI/encoding/BasicDataElements.h>
#include <RTI/encoding/HLAfixedArray.h>
#include <cstdint>
#include <string>
#include "../core/Math.hpp"

using namespace rti1516e;

// Encoding — the one place the on-wire byte layout is defined, for BOTH the object
// attributes (Position/Velocity/Orientation) and the control-interaction parameters
// (Enroll/AssignEntity/StartRun). Using the standard encoding helpers (instead of
// hand-packing bytes) is what guarantees every federate agrees byte-for-byte, including
// endianness — and encoding is the one place partition invariance can silently break, so
// it lives behind these named functions rather than being scattered around.

// Vec3 <-> the FOM's Vector3D: a fixed array of three big-endian float64. Position and
// Velocity both ride on this.
VariableLengthData encodeVec3(const Vec3& v);
Vec3 decodeVec3(const VariableLengthData& data);

// Quat <-> a fixed array of four big-endian float64 (x, y, z, w). The Orientation attr
// and the AssignEntity.Orientation parameter.
VariableLengthData encodeQuat(const Quat& q);
Quat decodeQuat(const VariableLengthData& data);

// Scalars for the control interactions.
VariableLengthData encodeDouble(double v);          // StartRun.Dt
double decodeDouble(const VariableLengthData& data);

VariableLengthData encodeUint32(uint32_t v);        // AssignEntity.EntityId
uint32_t decodeUint32(const VariableLengthData& data);

// Names on the wire. ASCII is enough (our names are "F1", "F2", "controller"); this
// converts to/from wstring at the boundary since the federate speaks wstring throughout.
VariableLengthData encodeString(const std::wstring& s);
std::wstring decodeString(const VariableLengthData& data);
