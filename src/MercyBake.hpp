// MERCY - bake: offline rendering of audio files with the current settings
#pragma once
#include "MercyDSP.hpp"
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <deque>

namespace mercy {

struct BakeSettings {
    float v[kFirstOutputParam];
    int order[kNumStages];
    std::string presetName; // used in the output file name
};

// ---- file helpers (UTF-8 paths everywhere; converted to UTF-16 on Windows)
bool loadAudioFile(const std::string& path, std::vector<float>& L, std::vector<float>& R, double& sampleRate, std::string& err);
bool writeWavFile(const std::string& path, const std::vector<float>& L, const std::vector<float>& R, double sampleRate, bool asFloat, std::string& err);
std::string bakeFolder(bool create);
std::string fileBaseName(const std::string& path);   // "C:\x\amen.wav" -> "amen"
std::string fileName(const std::string& path);       // -> "amen.wav"
bool fileExists(const std::string& path);
bool isAudioFileName(const std::string& path);
void openFolderInShell(const std::string& path);
bool isDirectory(const std::string& path);
// audio files from a drop: files as they are, folders opened up (subfolders too), max 500
std::vector<std::string> expandAudioPaths(const std::vector<std::string>& paths, bool* hadFolder = nullptr);
// "export to desktop": a new folder on the desktop with copies of the baked files
std::string desktopFolder();
std::string exportFolderName(const std::string& app, const std::string& preset); // "Mercy – gold throne – Oct 30"
bool exportToDesktop(const std::vector<std::string>& files, const std::string& folderName, std::string& outDir, int& copied, std::string& err);
#ifdef __APPLE__
std::string macHomeFolder(); // the real home folder, even inside a sandboxed host
#endif

// ---- the render itself (no threads, no UI): returns false if cancelled
// Loops are rendered seamlessly: the engine is warmed up on the end of the file first, the look-ahead
// is flushed with the start of the file, and the output keeps the exact length and timing of the input.
bool renderBake(const BakeSettings& s, const std::vector<float>& inL, const std::vector<float>& inR, double sr,
                std::vector<float>& outL, std::vector<float>& outR,
                std::atomic<float>* progress, const std::atomic<bool>* cancel);

// ---- background worker
class Baker {
public:
    enum State { kWaiting = 0, kBaking, kDone, kError };
    struct Item {
        int id = 0;
        std::string inPath, name;
        double seconds = 0;
        State state = kWaiting;
        float progress = 0;
        std::string outPath, outName, error;
        float peakDb = -100;
        bool asFloat = false;
    };

    Baker();
    ~Baker();

    // replace = single mode (keeps just the newest file)
    void addFiles(const std::vector<std::string>& paths, bool replace);
    void clear();
    // bake every item in the list with these settings (re-bakes finished ones)
    void bakeAll(const BakeSettings& s);
    std::vector<Item> snapshot();
    bool busy();
    int numItems();

private:
    std::vector<Item> items;
    std::mutex m;
    std::condition_variable cv;
    std::deque<int> queue;       // item ids
    BakeSettings settings;
    std::thread worker;
    std::atomic<bool> quit {false};
    std::atomic<bool> cancelCurrent {false};
    std::atomic<float> curProgress {0};
    int curId = -1;
    int nextId = 1;
    void run();
};

} // namespace mercy
