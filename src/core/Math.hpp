#pragma once
#include <iostream>
#include <cmath>

struct Vec3 {
    double x;
    double y;
    double z;

    // --- Constructors ---
    Vec3() : x(0.0), y(0.0), z(0.0) {}
    
    Vec3(double nx, double ny, double nz)
        : x(nx), y(ny), z(nz) {}
    
    // --- Compound assignment operators ---
    Vec3& operator+=(const Vec3& other) {
        x += other.x;
        y += other.y;
        z += other.z;
        return *this;
    }
    Vec3& operator-=(const Vec3& other) {
        x -= other.x;
        y -= other.y;
        z -= other.z;
        return *this;
    }
    Vec3& operator*=(double scalar) {
        x *= scalar;
        y *= scalar;
        z *= scalar;
        return *this;
    }
    Vec3& operator/=(double scalar) {
        // assuming scalar is not 0
        double inv = 1.0 / scalar;
        x *= inv;
        y *= inv;
        z *= inv;
        return *this;
    }
    Vec3 operator-() const {
        return {-x, -y, -z};
    }
};
// --- Binary Arithmetic Operators ---
inline Vec3 operator+(Vec3 lhs, const Vec3& rhs) {
    return lhs += rhs; // reuses operator
}
inline Vec3 operator-(Vec3 lhs, const Vec3& rhs) {
    return lhs -= rhs;
}
inline Vec3 operator*(double scalar, Vec3 rhs) {
    return rhs *= scalar;
}
inline Vec3 operator*(Vec3 lhs, double scalar) {
    return lhs *= scalar;
}
inline Vec3 operator/(Vec3 lhs, double scalar) {
    return lhs /= scalar;
}
inline std::ostream& operator<<(std::ostream& os, const Vec3& v) {
    return os << "(" << v.x << ", " << v.y << ", " << v.z << ")";
}
// Wide-stream twin, for the RTI layer (all Portico I/O is wcout / wstring).
inline std::wostream& operator<<(std::wostream& os, const Vec3& v) {
    return os << L"(" << v.x << L", " << v.y << L", " << v.z << L")";
}

inline Vec3 cross(const Vec3& a, const Vec3& b) {
    return {
        a.y * b.z  -  a.z * b.y,
        a.z * b.x  -  a.x * b.z,
        a.x * b.y  -  a.y * b.x
    };
}
inline double dot(const Vec3& a, const Vec3& b) {
    return a.x * b.x  +  a.y * b.y  +  a.z * b.z;
}

struct Quat {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;  
    double w = 1.0;

    Vec3 vec() const { return {x, y, z}; }

    // --- Constructors ---
    Quat() : x(0.0), y(0.0), z(0.0), w(1.0) {}
    
    Quat(double nx, double ny, double nz, double nw)
        : x(nx), y(ny), z(nz), w(nw) {}
    
    Quat(const Vec3& v, double nw)
        : x(v.x), y(v.y), z(v.z), w(nw) {}   // vec() is an accessor now, not a member

    Quat(const Quat& other) = default;
    ~Quat() = default;

    Quat& operator=(const Quat& other) = default;
        
    // --- Quaternion Multiplication Assignment (Combining Rotations) ---
    // Note: Quaternion multiplication is NOT commutative. Q1 *= Q2 means apply rotation Q1, then Q2 in order
    Quat& operator*=(const Quat& q) {
        double ow = w, ox = x, oy = y, oz = z;
        
        w = ow * q.w  -  ox * q.x  -  oy * q.y  -  oz*q.z;
        x = ow * q.x  +  ox * q.w  +  oy * q.z  -  oz*q.y;
        y = ow * q.y  -  ox * q.z  +  oy * q.w  +  oz*q.x;
        z = ow * q.z  +  ox * q.y  -  oy * q.x  +  oz*q.w;
        
        return *this;
    }
};

// --- Binary Multiplication Operator ---
inline Quat operator*(Quat lhs, const Quat& rhs) {
    return lhs *= rhs;
}

// --- Component-wise ops, for INTEGRATION ONLY (not quaternion algebra) ---
// A quaternion RATE (qdot = 0.5 * omega (x) q) is integrated numerically as
// q += qdot*dt, then renormalized -- which needs plain component-wise add and
// scalar-multiply. These are NOT Hamilton operations: keep them separate from
// operator*(Quat,Quat) above (composition) and operator*(Quat,Vec3) below (rotation).
inline Quat operator+(const Quat& a, const Quat& b) {
    return { a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w };
}
inline Quat operator*(const Quat& q, double s) {
    return { q.x * s, q.y * s, q.z * s, q.w * s };
}
inline Quat operator*(double s, const Quat& q) { return q * s; }

// --- Vector Rotation Operator (Quat * Vec3) ----
// This rotates a 3D point or direction vector by the quaternion's orientation.
inline Vec3 operator*(const Quat& q, const Vec3& v) {
    // Optimized standard formula: v' = v + 2 * q.xyz x (q.xyz x v + q.w * v)
    Vec3 t = 2.0 * cross(q.vec(), v);
    return v + q.w * t + cross(q.vec(), t);
}