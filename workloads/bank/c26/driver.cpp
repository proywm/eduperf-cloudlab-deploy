// Faithful reconstruction of ArduPilot NavEKF2_core::calcOutputStates()
// IMU position-offset correction region, commit c93c3d54f301.
// Trivial stubs for Vector3f / Matrix3f mimic ArduPilot's AP_Math types:
//   %  = cross product, * (vector,scalar) = scale, * (matrix,vector) = mat-vec.
#include <cstdio>
#include <cmath>
#include <chrono>
#include <random>
#include <vector>
#include <algorithm>

struct Vector3f {
    float x, y, z;
    Vector3f() : x(0), y(0), z(0) {}
    Vector3f(float a, float b, float c) : x(a), y(b), z(c) {}
    // scalar multiply (ArduPilot: v * scalar)
    Vector3f operator*(float s) const { return Vector3f(x*s, y*s, z*s); }
    // cross product (ArduPilot overloads % as cross)
    Vector3f operator%(const Vector3f& r) const {
        return Vector3f(y*r.z - z*r.y, z*r.x - x*r.z, x*r.y - y*r.x);
    }
    // unary minus
    Vector3f operator-() const { return Vector3f(-x, -y, -z); }
    bool is_zero() const { return x==0.0f && y==0.0f && z==0.0f; }
    void zero() { x = y = z = 0.0f; }
    bool operator==(const Vector3f& o) const { return x==o.x && y==o.y && z==o.z; }
};

struct Matrix3f {
    Vector3f a, b, c; // rows
    Matrix3f() {}
    Matrix3f(Vector3f a_, Vector3f b_, Vector3f c_) : a(a_), b(b_), c(c_) {}
    // matrix * vector
    Vector3f operator*(const Vector3f& v) const {
        return Vector3f(a.x*v.x + a.y*v.y + a.z*v.z,
                        b.x*v.x + b.y*v.y + b.z*v.z,
                        c.x*v.x + c.y*v.y + c.z*v.z);
    }
};

struct IMUData { Vector3f delAng; float delAngDT; };

// ----- BEFORE -----
namespace v_before {
void compute(const Matrix3f& Tbn_temp, const IMUData& imuDataNew,
             const Vector3f& accelPosOffset,
             Vector3f& velOffsetNED, Vector3f& posOffsetNED) {
    // calculate the average angular rate across the last IMU update
    // note delAngDT is prevented from being zero in readIMUData()
    Vector3f angRate = imuDataNew.delAng * (1.0f/imuDataNew.delAngDT);

    // Calculate the velocity of the body frame origin relative to the IMU in body frame
    // and rotate into earth frame. Note % operator has been overloaded to perform a cross product
    Vector3f velBodyRelIMU = angRate % (- accelPosOffset);
    velOffsetNED = Tbn_temp * velBodyRelIMU;

    // calculate the earth frame position of the body frame origin relative to the IMU
    posOffsetNED = Tbn_temp * (- accelPosOffset);
}
}

// ----- AFTER -----
namespace v_after {
void compute(const Matrix3f& Tbn_temp, const IMUData& imuDataNew,
             const Vector3f& accelPosOffset,
             Vector3f& velOffsetNED, Vector3f& posOffsetNED) {
    // If the IMU accelerometer is offset from the body frame origin, then calculate corrections
    if (!accelPosOffset.is_zero()) {
        // calculate the average angular rate across the last IMU update
        Vector3f angRate = imuDataNew.delAng * (1.0f/imuDataNew.delAngDT);

        Vector3f velBodyRelIMU = angRate % (- accelPosOffset);
        velOffsetNED = Tbn_temp * velBodyRelIMU;

        posOffsetNED = Tbn_temp * (- accelPosOffset);
    } else {
        velOffsetNED.zero();
        posOffsetNED.zero();
    }
}
}

int main() {
    std::mt19937 rng(12345);
    std::uniform_real_distribution<float> ud(-10.0f, 10.0f);
    std::uniform_real_distribution<float> dt(0.001f, 0.02f);

    auto rndV = [&](){ return Vector3f(ud(rng), ud(rng), ud(rng)); };

    struct Case { Matrix3f T; IMUData imu; Vector3f off; };
    std::vector<Case> cases;

    // Diverse random cases, half with zero offset (common path), half nonzero
    for (int i = 0; i < 200000; ++i) {
        Case c;
        c.T = Matrix3f(rndV(), rndV(), rndV());
        c.imu.delAng = rndV();
        c.imu.delAngDT = dt(rng);
        if (i % 2 == 0) c.off = Vector3f(0,0,0);          // zero offset
        else c.off = rndV();                               // nonzero
        cases.push_back(c);
    }
    // Boundary / adversarial cases
    cases.push_back({Matrix3f(Vector3f(1,0,0),Vector3f(0,1,0),Vector3f(0,0,1)), {Vector3f(0,0,0),0.004f}, Vector3f(0,0,0)});
    cases.push_back({Matrix3f(Vector3f(1,0,0),Vector3f(0,1,0),Vector3f(0,0,1)), {Vector3f(1,1,1),0.004f}, Vector3f(0,0,0)});
    cases.push_back({Matrix3f(Vector3f(1,0,0),Vector3f(0,1,0),Vector3f(0,0,1)), {Vector3f(0,0,0),0.004f}, Vector3f(1e-9f,0,0)}); // tiny nonzero
    cases.push_back({Matrix3f(rndV(),rndV(),rndV()), {Vector3f(1e6f,-1e6f,1e6f),1e-3f}, Vector3f(0,0,0)});

    // ---- Differential correctness ----
    bool equivalent = true;
    for (size_t i = 0; i < cases.size() && equivalent; ++i) {
        Vector3f vB, pB, vA, pA;
        v_before::compute(cases[i].T, cases[i].imu, cases[i].off, vB, pB);
        v_after::compute(cases[i].T, cases[i].imu, cases[i].off, vA, pA);
        if (!(vB == vA) || !(pB == pA)) {
            equivalent = false;
            printf("DIVERGENCE at case %zu: off=(%g,%g,%g)\n", i,
                   cases[i].off.x, cases[i].off.y, cases[i].off.z);
            printf("  velB=(%g,%g,%g) velA=(%g,%g,%g)\n", vB.x,vB.y,vB.z, vA.x,vA.y,vA.z);
            printf("  posB=(%g,%g,%g) posA=(%g,%g,%g)\n", pB.x,pB.y,pB.z, pA.x,pA.y,pA.z);
        }
    }
    printf("EQUIVALENT: %s\n", equivalent ? "YES" : "NO");

    // ---- Interleaved timing ----
    const int REP = 300;
    std::vector<double> ratios;
    volatile float sink = 0;
    for (int r = 0; r < REP; ++r) {
        auto t0 = std::chrono::high_resolution_clock::now();
        for (auto& c : cases) {
            Vector3f v,p;
            v_before::compute(c.T, c.imu, c.off, v, p);
            sink += v.x + p.y;
        }
        auto t1 = std::chrono::high_resolution_clock::now();
        for (auto& c : cases) {
            Vector3f v,p;
            v_after::compute(c.T, c.imu, c.off, v, p);
            sink += v.x + p.y;
        }
        auto t2 = std::chrono::high_resolution_clock::now();
        double bns = std::chrono::duration<double,std::nano>(t1-t0).count();
        double ans = std::chrono::duration<double,std::nano>(t2-t1).count();
        if (ans > 0) ratios.push_back(bns/ans);
    }
    std::sort(ratios.begin(), ratios.end());
    double med = ratios[ratios.size()/2];
    printf("MEDIAN_SPEEDUP: %.4f\n", med);
    printf("sink=%g\n", (float)sink);
    return 0;
}
