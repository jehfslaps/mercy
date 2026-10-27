#include "../src/MercyBake.hpp"
#include <cstdio>
using namespace mercy;
int main(int argc, char** argv) {
    for (int a = 1; a < argc; ++a) {
        std::vector<float> L, R; double sr; std::string err;
        const bool ok = loadAudioFile(argv[a], L, R, sr, err);
        float pk = 0; for (size_t i = 0; i < L.size(); ++i) pk = std::max(pk, std::max(std::fabs(L[i]), std::fabs(R[i])));
        printf("%-28s %s  %zu frames @ %.0f Hz  peak %.3f  %s\n", fileName(argv[a]).c_str(), ok ? "OK " : "ERR", L.size(), sr, pk, err.c_str());
    }
}
