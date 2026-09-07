#include "Encoding.hpp"

// We build the FOM's Vector3D exactly: a fixed array of three big-endian float64s.
// Using the standard encoding helpers (instead of hand-packing bytes) is what
// guarantees the two federates agree byte-for-byte, including endianness.

VariableLengthData encodeVec3(const Vec3& v) {
    HLAfloat64BE x(v.x);
    HLAfloat64BE y(v.y);
    HLAfloat64BE z(v.z);

    // prototype element + fixed length 3; set() copies each value into its slot
    HLAfixedArray array(HLAfloat64BE(), 3);
    array.set(0, x);
    array.set(1, y);
    array.set(2, z);

    return array.encode();
}

Vec3 decodeVec3(const VariableLengthData& data) {
    // reconstruct the same shape, then decode the bytes into it
    HLAfixedArray array(HLAfloat64BE(), 3);
    array.decode(data);

    Vec3 v;
    v.x = static_cast<const HLAfloat64BE&>(array.get(0)).get();
    v.y = static_cast<const HLAfloat64BE&>(array.get(1)).get();
    v.z = static_cast<const HLAfloat64BE&>(array.get(2)).get();
    return v;
}
