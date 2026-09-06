// Ready for review by Claude.
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
    
    Vec3(const Vec3& other) = default;
    ~Vec3() = default; // not sure if needed at all if this stays on stack

    Vec3& operator=(const Quat& other) = default; // This might give compilation error based on placement?? I do wonder how this potentially circular declaration goes, as Quat uses Vec3

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
    Vec3& operator/=(double other) {
        // assuming scalar is not 0
        double inv = 1.0 / scalar;
        x *= scalar;
        y *= scalar;
        z *= scalar;
        return *this;
    }
    Vec3 operator-() const {
        return {-x, -y, -z};
    }
}
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
    union {
        struct {
            double x = 0.0;
            double y = 0.0;
            double z = 0.0;
        };
        
        Vec3 vec;
    };
    
    double w = 1.0; // Default constructor yields an Identity Quaternion (zero rotation)

    // Allegedly Standard ISO C++17 way to get x, y, z accessors without compiler extensions. I would like elaboration please...
    /*
    double& x;
    double& y;
    double& z;
    */

    // --- Constructors ---
    Quat() : x(0.0), y(0.0), z(0.0), w(1.0) {}
    
    Quat(double nx, double ny, double nz, double nw)
        : x(nx), y(ny), z(nz), w(nw) {}
    
    Quat(const Vec3& v, double nw)
        : vec(v), w(nw) {}

    Quat(const Quat& other) = default;
    ~Quat() = default;

    Quat& operator=(const Quat& other) = default;
        
    // --- Quaternion Multiplication Assignment (Combining Rotations) ---
    // Note: Quaternion multiplication is NOT commutative. Q1 *= Q2 means apply rotation Q1, then Q2 in order
    Quat& operator*=(const Quat& q) {
        float ow = w, ox = x, oy = y, oz = z;
        
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

// --- Vector Rotation Operator (Quat * Vec3) ----
// This rotates a 3D point or direction vector by the quaternion's orientation.
inline Vec3 operator*(const Quat& q, const Vec3& v) {
    // Optimized standard formula: v' = v + 2 * q.xyz x (q.xyz x v + q.w * v)
    Vec3 w_t = q.w * cross(q.vec, v) * 2.0;
    Vec3 cross_q_t = cross(q.vec, t);

    return v + w_t + cross_q_t;
}