// folder drop + "export to desktop": scan a folder of loops, export copies to a new desktop folder (twice)
#include "../src/MercyBake.hpp"
#include <cstdio>
using namespace mercy;
int main(int argc, char** argv) {
    bool hadFolder = false;
    const std::vector<std::string> files = expandAudioPaths({argv[1]}, &hadFolder);
    std::printf("folder=%d, %zu audio files:\n", (int)hadFolder, files.size());
    for (const auto& f : files) std::printf("  %s\n", fileName(f).c_str());
    for (int round = 0; round < 2; ++round) {
        std::string dir, err; int copied = 0;
        const bool ok = exportToDesktop(files, exportFolderName("Mercy", "gold throne"), dir, copied, err);
        std::printf("export %d: ok=%d copied=%d dir=%s %s\n", round + 1, (int)ok, copied, dir.c_str(), err.c_str());
    }
    std::printf("not a folder: %zu files\n", expandAudioPaths({std::string(argv[1]) + "/notes.txt"}).size());
    return 0;
}
