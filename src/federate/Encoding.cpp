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

// Quat: same idea as Vec3 but four slots (x, y, z, w).
VariableLengthData encodeQuat(const Quat& q) {
    HLAfixedArray array(HLAfloat64BE(), 4);
    array.set(0, HLAfloat64BE(q.x));
    array.set(1, HLAfloat64BE(q.y));
    array.set(2, HLAfloat64BE(q.z));
    array.set(3, HLAfloat64BE(q.w));
    return array.encode();
}

Quat decodeQuat(const VariableLengthData& data) {
    HLAfixedArray array(HLAfloat64BE(), 4);
    array.decode(data);

    Quat q;
    q.x = static_cast<const HLAfloat64BE&>(array.get(0)).get();
    q.y = static_cast<const HLAfloat64BE&>(array.get(1)).get();
    q.z = static_cast<const HLAfloat64BE&>(array.get(2)).get();
    q.w = static_cast<const HLAfloat64BE&>(array.get(3)).get();
    return q;
}

// A bare big-endian float64 (no array wrapper) — StartRun.Dt.
VariableLengthData encodeDouble(double v) {
    return HLAfloat64BE(v).encode();
}

double decodeDouble(const VariableLengthData& data) {
    HLAfloat64BE x;
    x.decode(data);
    return x.get();
}

// EntityId on the wire as a big-endian int32. Our ids are small positive integers, so the
// signed/unsigned round-trip is lossless.
VariableLengthData encodeUint32(uint32_t v) {
    return HLAinteger32BE(static_cast<int32_t>(v)).encode();
}

uint32_t decodeUint32(const VariableLengthData& data) {
    HLAinteger32BE x;
    x.decode(data);
    return static_cast<uint32_t>(x.get());
}

// Names: ASCII on the wire, wstring at the boundary. Our names are pure ASCII, so the
// narrow<->wide copy is safe (this is NOT a general Unicode transcode).
VariableLengthData encodeString(const std::wstring& s) {
    std::string narrow(s.begin(), s.end());
    return HLAASCIIstring(narrow).encode();
}

std::wstring decodeString(const VariableLengthData& data) {
    HLAASCIIstring s;
    s.decode(data);
    std::string narrow = s.get();
    return std::wstring(narrow.begin(), narrow.end());
}
