// MERCY - deterministic math: the same bits on every compiler, OS and CPU.
//
// The system math libraries (Windows, macOS, Linux) each round exp/log/sin/... a little differently,
// so the same plugin code would sound a hair different on each platform. MERCY uses these functions
// instead, compiled from this one source, with strict floating point (-fno-fast-math -ffp-contract=off),
// so Windows and Mac render bit-identical audio.
//
// The double-precision kernels are from FreeBSD msun (fdlibm):
//   Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
//   Developed at SunPro, a Sun Microsystems, Inc. business.
//   Permission to use, copy, modify, and distribute this software is freely granted,
//   provided that this notice is preserved.
// Float versions are computed in double and rounded once.
#pragma once
#include <cstdint>
#include <cstring>

namespace mercy {
namespace dm {

inline uint64_t bitsOf(double x) { uint64_t u; std::memcpy(&u, &x, 8); return u; }
inline double fromBits(uint64_t u) { double x; std::memcpy(&x, &u, 8); return x; }
inline int32_t hiWord(double x) { return (int32_t)(bitsOf(x) >> 32); }
inline uint32_t loWord(double x) { return (uint32_t)bitsOf(x); }
inline double withHiWord(double x, uint32_t hi) { return fromBits(((uint64_t)hi << 32) | (bitsOf(x) & 0xffffffffull)); }
inline double fromWords(uint32_t hi, uint32_t lo) { return fromBits(((uint64_t)hi << 32) | lo); }

// ------------------------------------------------------------------------------------------------ exp
inline double exp(double x) {
    static const double halF[2] = {0.5, -0.5};
    static const double o_threshold = 7.09782712893383973096e+02;
    static const double u_threshold = -7.45133219101941108420e+02;
    static const double ln2HI[2] = {6.93147180369123816490e-01, -6.93147180369123816490e-01};
    static const double ln2LO[2] = {1.90821492927058770002e-10, -1.90821492927058770002e-10};
    static const double invln2 = 1.44269504088896338700e+00;
    static const double P1 = 1.66666666666666019037e-01, P2 = -2.77777777770155933842e-03,
                        P3 = 6.61375632143793436117e-05, P4 = -1.65339022054652515390e-06,
                        P5 = 4.13813679705723846039e-08;
    double y, hi = 0.0, lo = 0.0, c, t, twopk;
    int32_t k = 0;
    uint32_t hx = (uint32_t)hiWord(x);
    const int xsb = (int)((hx >> 31) & 1);
    hx &= 0x7fffffff;
    if (hx >= 0x40862E42) {                      // |x| >= 709.78
        if (hx >= 0x7ff00000) {
            if (((hx & 0xfffff) | loWord(x)) != 0) return x + x; // NaN
            return xsb == 0 ? x : 0.0;           // exp(+-inf) = {inf, 0}
        }
        if (x > o_threshold) return 1.0e300 * 1.0e300;
        if (x < u_threshold) return 0.0;
    }
    if (hx > 0x3fd62e42) {                       // |x| > 0.5 ln2
        if (hx < 0x3FF0A2B2) {                   // and |x| < 1.5 ln2
            hi = x - ln2HI[xsb]; lo = ln2LO[xsb]; k = 1 - xsb - xsb;
        } else {
            k = (int32_t)(invln2 * x + halF[xsb]);
            t = k;
            hi = x - t * ln2HI[0];
            lo = t * ln2LO[0];
        }
        x = hi - lo;
    } else if (hx < 0x3e300000) {                // |x| < 2^-28
        return 1.0 + x;
    } else {
        k = 0;
    }
    t = x * x;
    if (k >= -1021) twopk = fromWords((uint32_t)(0x3ff00000 + (k << 20)), 0);
    else twopk = fromWords((uint32_t)(0x3ff00000 + ((k + 1000) << 20)), 0);
    c = x - t * (P1 + t * (P2 + t * (P3 + t * (P4 + t * P5))));
    if (k == 0) return 1.0 - ((x * c) / (c - 2.0) - x);
    y = 1.0 - ((lo - (x * c) / (2.0 - c)) - hi);
    if (k >= -1021) {
        if (k == 1024) return y * 2.0 * 8.98846567431157953865e+307; // 2^1023
        return y * twopk;
    }
    return y * twopk * 9.33263618503218878990e-302; // 2^-1000
}

// ------------------------------------------------------------------------------------------------ log
inline double log(double x) {
    static const double ln2_hi = 6.93147180369123816490e-01, ln2_lo = 1.90821492927058770002e-10,
                        two54 = 1.80143985094819840000e+16,
                        Lg1 = 6.666666666666735130e-01, Lg2 = 3.999999999940941908e-01,
                        Lg3 = 2.857142874366239149e-01, Lg4 = 2.222219843214978396e-01,
                        Lg5 = 1.818357216161805012e-01, Lg6 = 1.531383769920937332e-01,
                        Lg7 = 1.479819860511658591e-01;
    double hfsq, f, s, z, R, w, t1, t2, dk;
    int32_t k = 0, hx = hiWord(x), i, j;
    const uint32_t lx = loWord(x);
    if (hx < 0x00100000) {                       // x < 2^-1022
        if (((hx & 0x7fffffff) | (int32_t)lx) == 0) return -1.0e300 * 1.0e300; // log(0) = -inf
        if (hx < 0) return (x - x) / (x - x);    // log(-#) = NaN
        k -= 54; x *= two54;                     // subnormal: scale up
        hx = hiWord(x);
    }
    if (hx >= 0x7ff00000) return x + x;
    k += (hx >> 20) - 1023;
    hx &= 0x000fffff;
    i = (hx + 0x95f64) & 0x100000;
    x = withHiWord(x, (uint32_t)(hx | (i ^ 0x3ff00000)));
    k += (i >> 20);
    f = x - 1.0;
    if ((0x000fffff & (2 + hx)) < 3) {           // -2^-20 <= f < 2^-20
        if (f == 0.0) {
            if (k == 0) return 0.0;
            dk = (double)k;
            return dk * ln2_hi + dk * ln2_lo;
        }
        R = f * f * (0.5 - 0.33333333333333333 * f);
        if (k == 0) return f - R;
        dk = (double)k;
        return dk * ln2_hi - ((R - dk * ln2_lo) - f);
    }
    s = f / (2.0 + f);
    dk = (double)k;
    z = s * s;
    i = hx - 0x6147a;
    w = z * z;
    j = 0x6b851 - hx;
    t1 = w * (Lg2 + w * (Lg4 + w * Lg6));
    t2 = z * (Lg1 + w * (Lg3 + w * (Lg5 + w * Lg7)));
    i |= j;
    R = t2 + t1;
    if (i > 0) {
        hfsq = 0.5 * f * f;
        if (k == 0) return f - (hfsq - s * (hfsq + R));
        return dk * ln2_hi - ((hfsq - (s * (hfsq + R) + dk * ln2_lo)) - f);
    }
    if (k == 0) return f - s * (f - R);
    return dk * ln2_hi - ((s * (f - R) - dk * ln2_lo) - f);
}

// ------------------------------------------------------------------------------------------------ sin / cos
inline double kSin(double x, double y, int iy) {
    static const double S1 = -1.66666666666666324348e-01, S2 = 8.33333333332248946124e-03,
                        S3 = -1.98412698298579493134e-04, S4 = 2.75573137070700676789e-06,
                        S5 = -2.50507602534068634195e-08, S6 = 1.58969099521155010221e-10;
    const double z = x * x, w = z * z;
    const double r = S2 + z * (S3 + z * S4) + z * w * (S5 + z * S6);
    const double v = z * x;
    if (iy == 0) return x + v * (S1 + z * r);
    return x - ((z * (0.5 * y - v * r) - y) - v * S1);
}
inline double kCos(double x, double y) {
    static const double C1 = 4.16666666666666019037e-02, C2 = -1.38888888888741095749e-03,
                        C3 = 2.48015872894767294178e-05, C4 = -2.75573143513906633035e-07,
                        C5 = 2.08757232129817482790e-09, C6 = -1.13596475577881948265e-11;
    const double z = x * x;
    double w = z * z;
    const double r = z * (C1 + z * (C2 + z * C3)) + w * w * (C4 + z * (C5 + z * C6));
    const double hz = 0.5 * z;
    w = 1.0 - hz;
    return w + (((1.0 - w) - hz) + (z * r - x * y));
}
// x = n*pi/2 + (y0 + y1), for |x| < 2^20 * pi/2 (MERCY only ever needs small angles)
inline int remPio2(double x, double& y0, double& y1) {
    static const double invpio2 = 6.36619772367581382433e-01,
                        pio2_1 = 1.57079632673412561417e+00, pio2_1t = 6.07710050650619224932e-11,
                        pio2_2 = 6.07710050630396597660e-11, pio2_2t = 2.02226624879595063154e-21,
                        pio2_3 = 2.02226624871116645580e-21, pio2_3t = 8.47842766036889956997e-32;
    const int32_t ix = hiWord(x) & 0x7fffffff;
    const double toint = 6755399441055744.0; // 1.5 * 2^52: rounds to nearest integer
    const double fn = (x * invpio2 + toint) - toint;
    const int n = (int)fn;
    double r = x - fn * pio2_1;
    double w = fn * pio2_1t;
    const int32_t j = ix >> 20;
    y0 = r - w;
    int32_t i = j - ((hiWord(y0) >> 20) & 0x7ff);
    if (i > 16) {
        double t = r;
        w = fn * pio2_2;
        r = t - w;
        w = fn * pio2_2t - ((t - r) - w);
        y0 = r - w;
        i = j - ((hiWord(y0) >> 20) & 0x7ff);
        if (i > 49) {
            t = r;
            w = fn * pio2_3;
            r = t - w;
            w = fn * pio2_3t - ((t - r) - w);
            y0 = r - w;
        }
    }
    y1 = (r - y0) - w;
    return n;
}
inline double sin(double x) {
    const int32_t ix = hiWord(x) & 0x7fffffff;
    if (ix <= 0x3fe921fb) {                      // |x| ~<= pi/4
        if (ix < 0x3e500000) return x;           // |x| < 2^-26
        return kSin(x, 0.0, 0);
    }
    if (ix >= 0x7ff00000) return x - x;          // inf or NaN
    if (ix >= 0x413921fb) return 0.0;            // |x| >= 2^20 * pi/2: never used by MERCY
    double y0, y1;
    switch (remPio2(x, y0, y1) & 3) {
    case 0: return kSin(y0, y1, 1);
    case 1: return kCos(y0, y1);
    case 2: return -kSin(y0, y1, 1);
    default: return -kCos(y0, y1);
    }
}
inline double cos(double x) {
    const int32_t ix = hiWord(x) & 0x7fffffff;
    if (ix <= 0x3fe921fb) {
        if (ix < 0x3e46a09e) return 1.0;         // |x| < 2^-27 * sqrt(2)
        return kCos(x, 0.0);
    }
    if (ix >= 0x7ff00000) return x - x;
    if (ix >= 0x413921fb) return 1.0;
    double y0, y1;
    switch (remPio2(x, y0, y1) & 3) {
    case 0: return kCos(y0, y1);
    case 1: return -kSin(y0, y1, 1);
    case 2: return -kCos(y0, y1);
    default: return kSin(y0, y1, 1);
    }
}
inline double tan(double x) { return sin(x) / cos(x); }

// ------------------------------------------------------------------------------------------------ atan
inline double atan(double x) {
    static const double atanhi[] = {4.63647609000806093515e-01, 7.85398163397448278999e-01,
                                    9.82793723247329054082e-01, 1.57079632679489655800e+00};
    static const double atanlo[] = {2.26987774529616870924e-17, 3.06161699786838301793e-17,
                                    1.39033110312309984516e-17, 6.12323399573676603587e-17};
    static const double aT[] = {3.33333333333329318027e-01, -1.99999999998764832476e-01,
                                1.42857142725034663711e-01, -1.11111104054623557880e-01,
                                9.09088713343650656196e-02, -7.69187620504482999495e-02,
                                6.66107313738753120669e-02, -5.83357013379057348645e-02,
                                4.97687799461593236017e-02, -3.65315727442169155270e-02,
                                1.62858201153657823623e-02};
    double w, s1, s2, z;
    int id;
    const int32_t hx = hiWord(x), ix = hx & 0x7fffffff;
    if (ix >= 0x44100000) {                      // |x| >= 2^66
        if (ix > 0x7ff00000 || (ix == 0x7ff00000 && loWord(x) != 0)) return x + x;
        return hx > 0 ? atanhi[3] + atanlo[3] : -atanhi[3] - atanlo[3];
    }
    if (ix < 0x3fdc0000) {                       // |x| < 0.4375
        if (ix < 0x3e400000) return x;           // |x| < 2^-27
        id = -1;
    } else {
        x = x < 0 ? -x : x;
        if (ix < 0x3ff30000) {                   // |x| < 1.1875
            if (ix < 0x3fe60000) { id = 0; x = (2.0 * x - 1.0) / (2.0 + x); }
            else { id = 1; x = (x - 1.0) / (x + 1.0); }
        } else {
            if (ix < 0x40038000) { id = 2; x = (x - 1.5) / (1.0 + 1.5 * x); }
            else { id = 3; x = -1.0 / x; }
        }
    }
    z = x * x;
    w = z * z;
    s1 = z * (aT[0] + w * (aT[2] + w * (aT[4] + w * (aT[6] + w * (aT[8] + w * aT[10])))));
    s2 = w * (aT[1] + w * (aT[3] + w * (aT[5] + w * (aT[7] + w * aT[9]))));
    if (id < 0) return x - x * (s1 + s2);
    z = atanhi[id] - ((x * (s1 + s2) - atanlo[id]) - x);
    return hx < 0 ? -z : z;
}

// ------------------------------------------------------------------------------------------------ derived
inline double log10(double x) { return log(x) * 4.34294481903251827651e-01; }
inline double pow(double x, double y) {
    if (y == 0.0) return 1.0;
    if (x == 1.0) return 1.0;
    if (x > 0.0) return exp(y * log(x));
    if (x == 0.0) return y > 0.0 ? 0.0 : 1.0e300 * 1.0e300;
    // negative base: only integer exponents are defined
    const double yi = (y + 6755399441055744.0) - 6755399441055744.0;
    if (yi != y) return (x - x) / (x - x);
    const double m = exp(y * log(-x));
    const int64_t n = (int64_t)yi;
    return (n & 1) ? -m : m;
}
inline double tanh(double x) {
    const double ax = x < 0 ? -x : x;
    if (!(ax == ax)) return x;                   // NaN
    if (ax > 22.0) return x < 0 ? -1.0 : 1.0;
    double t;
    if (ax < 1e-4) {                             // tanh x = x - x^3/3 + 2x^5/15
        const double a2 = ax * ax;
        t = ax * (1.0 - a2 * (1.0 / 3.0 - a2 * (2.0 / 15.0)));
    } else {
        const double e = exp(2.0 * ax) - 1.0;    // exp(2|x|)-1 >= 2e-4: loses at most ~12 of 53 bits
        t = e / (e + 2.0);
    }
    return x < 0 ? -t : t;
}

// float versions: computed in double, rounded once (matches a correctly rounded libm almost always)
inline float exp(float x) { return (float)exp((double)x); }
inline float log(float x) { return (float)log((double)x); }
inline float log10(float x) { return (float)log10((double)x); }
inline float sin(float x) { return (float)sin((double)x); }
inline float cos(float x) { return (float)cos((double)x); }
inline float tan(float x) { return (float)tan((double)x); }
inline float atan(float x) { return (float)atan((double)x); }
inline float tanh(float x) { return (float)tanh((double)x); }
inline float pow(float x, float y) { return (float)pow((double)x, (double)y); }

} // namespace dm
} // namespace mercy
