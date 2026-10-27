// MERCY - checks MercyMath.hpp against the system math library (accuracy) and prints a checksum
// of the results (the checksum must be the same on every platform).
#include "MercyMath.hpp"
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstring>

static uint64_t rng = 0x9E3779B97F4A7C15ull;
static double uni() { rng ^= rng << 13; rng ^= rng >> 7; rng ^= rng << 17; return (double)(rng >> 11) * (1.0 / 9007199254740992.0); }
static double ulpDiff(double a, double b) {
    if (a == b) return 0;
    if (std::isnan(a) && std::isnan(b)) return 0;
    const double u = std::fabs(std::nextafter(b, 2 * b + 1) - b);
    return std::fabs(a - b) / (u > 0 ? u : 1e-300);
}
static uint64_t hash = 1469598103934665603ull;
static void mix(double v) { uint64_t u; std::memcpy(&u, &v, 8); hash = (hash ^ u) * 1099511628211ull; }
static void mixf(float v) { uint32_t u; std::memcpy(&u, &v, 4); hash = (hash ^ u) * 1099511628211ull; }

template <class F, class G>
static int check(const char* name, F mine, G ref, double lo, double hi, double maxUlp, bool logScale = false) {
    double worst = 0, at = 0;
    for (int i = 0; i < 2000000; ++i) {
        double x = logScale ? std::exp(std::log(lo) + (std::log(hi) - std::log(lo)) * uni()) : lo + (hi - lo) * uni();
        if (logScale && (i & 1)) x = -x;
        const double a = mine(x), b = ref(x);
        mix(a);
        const double d = ulpDiff(a, b);
        if (d > worst) { worst = d; at = x; }
    }
    const bool ok = worst <= maxUlp;
    std::printf("%-8s worst %.2f ulp at %.17g %s\n", name, worst, at, ok ? "ok" : "FAIL");
    return ok ? 0 : 1;
}
template <class F, class G>
static int checkf(const char* name, F mine, G ref, float lo, float hi, double maxUlp) {
    double worst = 0, at = 0;
    int differ = 0;
    for (int i = 0; i < 2000000; ++i) {
        const float x = (float)(lo + (hi - lo) * uni());
        const float a = mine(x), b = ref(x);
        mixf(a);
        if (a != b) ++differ;
        const float u = std::fabs(std::nextafterf(b, 2 * b + 1) - b);
        const double d = a == b ? 0 : std::fabs((double)a - (double)b) / (u > 0 ? u : 1e-38);
        if (d > worst) { worst = d; at = x; }
    }
    const bool ok = worst <= maxUlp;
    std::printf("%-8s worst %.2f ulp at %.9g, %d of 2000000 differ from libm %s\n", name, worst, at, differ, ok ? "ok" : "FAIL");
    return ok ? 0 : 1;
}

int main() {
    using namespace mercy;
    int bad = 0;
    bad += check("exp", [](double x) { return dm::exp(x); }, [](double x) { return std::exp(x); }, -740, 709, 1.0);
    bad += check("exp-s", [](double x) { return dm::exp(x); }, [](double x) { return std::exp(x); }, -2, 2, 1.0);
    bad += check("log", [](double x) { return dm::log(x); }, [](double x) { return std::log(x); }, 1e-300, 1e300, 1.0, true);
    bad += check("log-1", [](double x) { return dm::log(x); }, [](double x) { return std::log(x); }, 0.5, 2.0, 1.0);
    bad += check("sin", [](double x) { return dm::sin(x); }, [](double x) { return std::sin(x); }, -1000, 1000, 1.0);
    bad += check("cos", [](double x) { return dm::cos(x); }, [](double x) { return std::cos(x); }, -1000, 1000, 1.0);
    bad += check("sin-s", [](double x) { return dm::sin(x); }, [](double x) { return std::sin(x); }, -4, 4, 1.0);
    bad += check("cos-s", [](double x) { return dm::cos(x); }, [](double x) { return std::cos(x); }, -4, 4, 1.0);
    bad += check("tan", [](double x) { return dm::tan(x); }, [](double x) { return std::tan(x); }, 0, 0.785, 3.0);
    bad += check("atan", [](double x) { return dm::atan(x); }, [](double x) { return std::atan(x); }, 1e-10, 1e10, 1.0, true);
    bad += check("tanh", [](double x) { return dm::tanh(x); }, [](double x) { return std::tanh(x); }, -25, 25, 4096.0);
    bad += check("pow10", [](double x) { return dm::pow(10.0, x); }, [](double x) { return std::pow(10.0, x); }, -8, 8, 64.0); // exp(y*log(x)): fine for coefficients
    bad += checkf("expf", [](float x) { return dm::exp(x); }, [](float x) { return std::exp(x); }, -80, 80, 1.0);
    bad += checkf("logf", [](float x) { return dm::log(x); }, [](float x) { return std::log(x); }, 1e-6f, 1e6f, 1.0);
    bad += checkf("log10f", [](float x) { return dm::log10(x); }, [](float x) { return (float)std::log10((long double)x); }, 1e-9f, 100.f, 1.0);
    bad += checkf("tanhf", [](float x) { return dm::tanh(x); }, [](float x) { return (float)std::tanh((long double)x); }, -12, 12, 1.0);
    bad += checkf("atanf", [](float x) { return dm::atan(x); }, [](float x) { return (float)std::atan((long double)x); }, -20, 20, 1.0);
    bad += checkf("pow10f", [](float x) { return dm::pow(10.f, x); }, [](float x) { return std::pow(10.f, x); }, -6, 6, 1.0);
    bad += checkf("pow2f", [](float x) { return dm::pow(2.f, x); }, [](float x) { return std::pow(2.f, x); }, -16, 16, 1.0);
    std::printf("checksum %016llx\n", (unsigned long long)hash);
    return bad;
}
