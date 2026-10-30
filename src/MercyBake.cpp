// MERCY - bake implementation
#define DR_WAV_IMPLEMENTATION
#define DR_FLAC_IMPLEMENTATION
#define DR_MP3_IMPLEMENTATION
#define DR_WAV_NO_STDIO_WARNINGS
#include "dr_wav.h"
#include "dr_flac.h"
#include "dr_mp3.h"
#include "MercyBake.hpp"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <algorithm>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shlobj.h>
#include <shellapi.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#endif
#include <ctime>
#ifdef __APPLE__
#include <pwd.h>
#include <spawn.h>
extern char** environ;
#endif

#ifdef MERCY_TEASER
#define MERCY_BAKE_FOLDER "000 Bakes"
#define MERCY_BAKE_FOLDER_W L"000 Bakes"
#define MERCY_BAKE_TAG "_000_"
#else
#define MERCY_BAKE_FOLDER "Mercy Bakes"
#define MERCY_BAKE_FOLDER_W L"Mercy Bakes"
#define MERCY_BAKE_TAG "_mercy_"
#endif

namespace mercy {

#ifdef _WIN32
static std::wstring widen(const std::string& s) {
    if (s.empty()) return std::wstring();
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    std::wstring w(n, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &w[0], n);
    return w;
}
static std::string narrow(const std::wstring& w) {
    if (w.empty()) return std::string();
    const int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s(n, 0);
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), &s[0], n, nullptr, nullptr);
    return s;
}
#endif

static std::string lowerExt(const std::string& path) {
    const size_t dot = path.find_last_of('.');
    const size_t sep = path.find_last_of("/\\");
    if (dot == std::string::npos || (sep != std::string::npos && dot < sep)) return "";
    std::string e = path.substr(dot + 1);
    for (char& c : e) c = (char)std::tolower((unsigned char)c);
    return e;
}

bool isAudioFileName(const std::string& path) {
    const std::string e = lowerExt(path);
    return e == "wav" || e == "wave" || e == "aif" || e == "aiff" || e == "aifc" || e == "flac" || e == "mp3";
}

std::string fileName(const std::string& path) {
    const size_t sep = path.find_last_of("/\\");
    return sep == std::string::npos ? path : path.substr(sep + 1);
}

std::string fileBaseName(const std::string& path) {
    std::string n = fileName(path);
    const size_t dot = n.find_last_of('.');
    if (dot != std::string::npos && dot > 0) n = n.substr(0, dot);
    return n;
}

bool fileExists(const std::string& path) {
#ifdef _WIN32
    return GetFileAttributesW(widen(path).c_str()) != INVALID_FILE_ATTRIBUTES;
#else
    struct stat st; return stat(path.c_str(), &st) == 0;
#endif
}

#ifdef __APPLE__
std::string macHomeFolder() {
    if (const passwd* pw = getpwuid(getuid())) if (pw->pw_dir != nullptr && pw->pw_dir[0]) return pw->pw_dir;
    const char* home = std::getenv("HOME");
    return home ? home : "/tmp";
}
#endif

std::string bakeFolder(bool create) {
#ifdef _WIN32
    wchar_t docs[MAX_PATH] = {};
    if (FAILED(SHGetFolderPathW(nullptr, CSIDL_PERSONAL, nullptr, 0, docs))) {
        const DWORD n = GetEnvironmentVariableW(L"USERPROFILE", docs, MAX_PATH);
        if (n == 0 || n >= MAX_PATH) return "";
    }
    std::wstring dir = std::wstring(docs) + L"\\" MERCY_BAKE_FOLDER_W;
    if (create) CreateDirectoryW(dir.c_str(), nullptr);
    return narrow(dir);
#elif defined(__APPLE__)
    // ~/Documents/Mercy Bakes, using the real home folder (a sandboxed host's HOME points into its
    // container); if the host's sandbox won't let us write there, fall back to the host's own Documents
    std::string dir = macHomeFolder() + "/Documents/" MERCY_BAKE_FOLDER;
    if (create) {
        mkdir(dir.c_str(), 0755);
        if (access(dir.c_str(), W_OK) != 0) {
            const char* home = std::getenv("HOME");
            if (home != nullptr) {
                const std::string alt = std::string(home) + "/Documents/" MERCY_BAKE_FOLDER;
                mkdir((std::string(home) + "/Documents").c_str(), 0755);
                mkdir(alt.c_str(), 0755);
                if (access(alt.c_str(), W_OK) == 0) dir = alt;
            }
        }
    }
    return dir;
#else
    const char* home = std::getenv("HOME");
    std::string dir = std::string(home ? home : "/tmp") + "/" MERCY_BAKE_FOLDER;
    if (create) mkdir(dir.c_str(), 0755);
    return dir;
#endif
}

void openFolderInShell(const std::string& path) {
#ifdef _WIN32
    ShellExecuteW(nullptr, L"open", widen(path).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#elif defined(__APPLE__)
    // reveal the folder in Finder (no shell, so any characters in the path are safe)
    pid_t pid;
    char* argv[] = {(char*)"/usr/bin/open", (char*)path.c_str(), nullptr};
    if (posix_spawn(&pid, "/usr/bin/open", nullptr, nullptr, argv, environ) == 0) {}
#else
    const std::string cmd = "xdg-open \"" + path + "\" >/dev/null 2>&1 &";
    if (std::system(cmd.c_str()) != 0) {}
#endif
}

// ------------------------------------------------------------------------------------------------ folders
bool isDirectory(const std::string& path) {
#ifdef _WIN32
    const DWORD a = GetFileAttributesW(widen(path).c_str());
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
#else
    struct stat st; return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
#endif
}

static bool lessNoCase(const std::string& a, const std::string& b) {
    const size_t n = std::min(a.size(), b.size());
    for (size_t i = 0; i < n; ++i) {
        const int x = std::tolower((unsigned char)a[i]), y = std::tolower((unsigned char)b[i]);
        if (x != y) return x < y;
    }
    return a.size() < b.size();
}

// every audio file in a folder (and its subfolders, a few levels deep), sorted by name
static void scanFolder(const std::string& dir, int depth, std::vector<std::string>& out, size_t maxFiles) {
    std::vector<std::string> files, dirs;
#ifdef _WIN32
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(widen(dir + "\\*").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        const std::string name = narrow(fd.cFileName);
        if (name.empty() || name[0] == '.') continue;
        const std::string full = dir + "\\" + name;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) dirs.push_back(full);
        else if (isAudioFileName(name)) files.push_back(full);
    } while (FindNextFileW(h, &fd));
    FindClose(h);
#else
    DIR* d = opendir(dir.c_str());
    if (d == nullptr) return;
    while (const dirent* e = readdir(d)) {
        const std::string name = e->d_name;
        if (name.empty() || name[0] == '.') continue; // also skips macOS "._" files
        const std::string full = dir + "/" + name;
        if (isDirectory(full)) dirs.push_back(full);
        else if (isAudioFileName(name)) files.push_back(full);
    }
    closedir(d);
#endif
    std::sort(files.begin(), files.end(), lessNoCase);
    std::sort(dirs.begin(), dirs.end(), lessNoCase);
    for (const auto& f : files) { if (out.size() >= maxFiles) return; out.push_back(f); }
    if (depth > 0) for (const auto& sd : dirs) { if (out.size() >= maxFiles) return; scanFolder(sd, depth - 1, out, maxFiles); }
}

std::vector<std::string> expandAudioPaths(const std::vector<std::string>& paths, bool* hadFolder) {
    const size_t kMax = 500;
    std::vector<std::string> out;
    if (hadFolder) *hadFolder = false;
    for (const auto& p : paths) {
        if (out.size() >= kMax) break;
        if (isDirectory(p)) { if (hadFolder) *hadFolder = true; scanFolder(p, 3, out, kMax); }
        else if (isAudioFileName(p)) out.push_back(p);
    }
    return out;
}

// ------------------------------------------------------------------------------------------------ export to desktop
std::string desktopFolder() {
#ifdef _WIN32
    wchar_t buf[MAX_PATH] = {};
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_DESKTOPDIRECTORY, nullptr, 0, buf))) return narrow(buf);
    const DWORD n = GetEnvironmentVariableW(L"USERPROFILE", buf, MAX_PATH);
    return (n > 0 && n < MAX_PATH) ? narrow(buf) + "\\Desktop" : std::string();
#elif defined(__APPLE__)
    const std::string d = macHomeFolder() + "/Desktop";
    if (access(d.c_str(), W_OK) == 0) return d;
    const char* home = std::getenv("HOME"); // sandboxed host: its own Desktop
    return home ? std::string(home) + "/Desktop" : d;
#else
    const char* home = std::getenv("HOME");
    const std::string d = std::string(home ? home : "/tmp") + "/Desktop";
    mkdir(d.c_str(), 0755);
    return d;
#endif
}

static bool makeDir(const std::string& path) {
#ifdef _WIN32
    return CreateDirectoryW(widen(path).c_str(), nullptr) != 0;
#else
    return mkdir(path.c_str(), 0755) == 0;
#endif
}

static bool copyFile(const std::string& from, const std::string& to) {
#ifdef _WIN32
    return CopyFileW(widen(from).c_str(), widen(to).c_str(), TRUE) != 0;
#else
    FILE* a = std::fopen(from.c_str(), "rb");
    if (!a) return false;
    FILE* b = std::fopen(to.c_str(), "wb");
    if (!b) { std::fclose(a); return false; }
    std::vector<char> buf(1 << 16);
    bool ok = true;
    size_t n;
    while ((n = std::fread(buf.data(), 1, buf.size(), a)) > 0) if (std::fwrite(buf.data(), 1, n, b) != n) { ok = false; break; }
    std::fclose(a);
    if (std::fclose(b) != 0) ok = false;
    return ok;
#endif
}

std::string exportFolderName(const std::string& app, const std::string& preset) {
    static const char* months[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    const std::time_t t = std::time(nullptr);
    const std::tm* lt = std::localtime(&t);
    std::string p;
    for (char c : preset) p += std::strchr("\\/:*?\"<>|", c) ? '-' : c; // not allowed in folder names
    while (!p.empty() && (p.back() == ' ' || p.back() == '.')) p.pop_back();
    char date[16] = "";
    if (lt) std::snprintf(date, sizeof date, "%s %d", months[lt->tm_mon], lt->tm_mday);
    return app + " \xE2\x80\x93 " + (p.empty() ? std::string("bakes") : p) + (date[0] ? std::string(" \xE2\x80\x93 ") + date : std::string());
}

bool exportToDesktop(const std::vector<std::string>& files, const std::string& folderName, std::string& outDir, int& copied, std::string& err) {
    copied = 0;
    const std::string desk = desktopFolder();
    if (desk.empty()) { err = "can't find your desktop"; return false; }
#ifdef _WIN32
    const std::string sep = "\\";
#else
    const std::string sep = "/";
#endif
    std::string dir = desk + sep + folderName;
    for (int k = 2; isDirectory(dir) || fileExists(dir); ++k) {
        if (k > 999) { err = "can't make the folder"; return false; }
        dir = desk + sep + folderName + " (" + std::to_string(k) + ")";
    }
    if (!makeDir(dir)) { err = "can't make a folder on your desktop"; return false; }
    for (const auto& f : files)
        if (copyFile(f, dir + sep + fileName(f))) ++copied;
    outDir = dir;
    if (copied < (int)files.size()) err = "some files couldn't be copied";
    return copied > 0;
}

static void toStereo(const float* inter, size_t frames, unsigned channels, std::vector<float>& L, std::vector<float>& R) {
    L.resize(frames); R.resize(frames);
    for (size_t i = 0; i < frames; ++i) {
        L[i] = inter[i * channels];
        R[i] = channels > 1 ? inter[i * channels + 1] : inter[i * channels];
    }
}

bool loadAudioFile(const std::string& path, std::vector<float>& L, std::vector<float>& R, double& sampleRate, std::string& err) {
    const std::string e = lowerExt(path);
    if (e == "wav" || e == "wave" || e == "aif" || e == "aiff" || e == "aifc") {
        drwav wav;
#ifdef _WIN32
        if (!drwav_init_file_w(&wav, widen(path).c_str(), nullptr)) { err = "can't read this file"; return false; }
#else
        if (!drwav_init_file(&wav, path.c_str(), nullptr)) { err = "can't read this file"; return false; }
#endif
        const size_t frames = (size_t)wav.totalPCMFrameCount;
        const unsigned ch = wav.channels;
        if (frames == 0 || ch == 0) { drwav_uninit(&wav); err = "empty file"; return false; }
        std::vector<float> inter(frames * ch);
        const size_t got = (size_t)drwav_read_pcm_frames_f32(&wav, frames, inter.data());
        sampleRate = wav.sampleRate;
        drwav_uninit(&wav);
        toStereo(inter.data(), got, ch, L, R);
        return got > 0;
    }
    if (e == "flac") {
#ifdef _WIN32
        drflac* f = drflac_open_file_w(widen(path).c_str(), nullptr);
#else
        drflac* f = drflac_open_file(path.c_str(), nullptr);
#endif
        if (f == nullptr) { err = "can't read this flac"; return false; }
        const size_t frames = (size_t)f->totalPCMFrameCount;
        const unsigned ch = f->channels;
        std::vector<float> inter(frames * ch);
        const size_t got = (size_t)drflac_read_pcm_frames_f32(f, frames, inter.data());
        sampleRate = f->sampleRate;
        drflac_close(f);
        if (got == 0) { err = "empty file"; return false; }
        toStereo(inter.data(), got, ch, L, R);
        return true;
    }
    if (e == "mp3") {
        drmp3 mp3;
#ifdef _WIN32
        if (!drmp3_init_file_w(&mp3, widen(path).c_str(), nullptr)) { err = "can't read this mp3"; return false; }
#else
        if (!drmp3_init_file(&mp3, path.c_str(), nullptr)) { err = "can't read this mp3"; return false; }
#endif
        const unsigned ch = mp3.channels;
        std::vector<float> inter, buf(4096 * ch);
        for (;;) {
            const size_t got = (size_t)drmp3_read_pcm_frames_f32(&mp3, 4096, buf.data());
            if (got == 0) break;
            inter.insert(inter.end(), buf.begin(), buf.begin() + got * ch);
        }
        sampleRate = mp3.sampleRate;
        drmp3_uninit(&mp3);
        if (inter.empty()) { err = "empty file"; return false; }
        toStereo(inter.data(), inter.size() / ch, ch, L, R);
        return true;
    }
    err = "unsupported file type";
    return false;
}

bool writeWavFile(const std::string& path, const std::vector<float>& L, const std::vector<float>& R, double sampleRate, bool asFloat, std::string& err) {
    drwav_data_format fmt;
    fmt.container = drwav_container_riff;
    fmt.format = asFloat ? DR_WAVE_FORMAT_IEEE_FLOAT : DR_WAVE_FORMAT_PCM;
    fmt.channels = 2;
    fmt.sampleRate = (drwav_uint32)std::lround(sampleRate);
    fmt.bitsPerSample = asFloat ? 32 : 24;
    drwav wav;
#ifdef _WIN32
    if (!drwav_init_file_write_w(&wav, widen(path).c_str(), &fmt, nullptr)) { err = "can't write to the bake folder"; return false; }
#else
    if (!drwav_init_file_write(&wav, path.c_str(), &fmt, nullptr)) { err = "can't write to the bake folder"; return false; }
#endif
    const size_t n = L.size();
    const size_t chunk = 8192;
    std::vector<unsigned char> buf(chunk * 2 * 4);
    bool ok = true;
    for (size_t i = 0; i < n && ok; i += chunk) {
        const size_t m = std::min(chunk, n - i);
        if (asFloat) {
            float* f = reinterpret_cast<float*>(buf.data());
            for (size_t k = 0; k < m; ++k) { f[2 * k] = L[i + k]; f[2 * k + 1] = R[i + k]; }
        } else {
            unsigned char* p = buf.data();
            for (size_t k = 0; k < m; ++k) {
                for (int c = 0; c < 2; ++c) {
                    const float x = clampf(c ? R[i + k] : L[i + k], -1.f, 1.f);
                    long s = std::lround(x * 8388607.0);
                    if (s > 8388607) s = 8388607;
                    if (s < -8388608) s = -8388608;
                    *p++ = (unsigned char)(s & 0xFF);
                    *p++ = (unsigned char)((s >> 8) & 0xFF);
                    *p++ = (unsigned char)((s >> 16) & 0xFF);
                }
            }
        }
        ok = drwav_write_pcm_frames(&wav, m, buf.data()) == m;
    }
    drwav_uninit(&wav);
    if (!ok) err = "disk full or write error";
    return ok;
}

bool renderBake(const BakeSettings& s, const std::vector<float>& inL, const std::vector<float>& inR, double sr,
                std::vector<float>& outL, std::vector<float>& outR,
                std::atomic<float>* progress, const std::atomic<bool>* cancel) {
    const size_t n = inL.size();
    outL.assign(n, 0.f); outR.assign(n, 0.f);
    if (n == 0) return true;

    Engine e;
    e.setSampleRate(sr);
    for (uint32_t p = 0; p < kFirstOutputParam; ++p) e.setParam(p, s.v[p]);
    e.setParam(pBypass, 0.f);   // bake what the settings do, even if bypassed for comparison
    e.setParam(pQuality, 1.f);  // always 2x
    char ord[kNumStages + 1];
    for (int i = 0; i < kNumStages; ++i) ord[i] = (char)('0' + s.order[i]);
    ord[kNumStages] = 0;
    e.setOrderString(ord);

    const size_t lat = e.latencySamples();
    // warm-up: at least 4 s, made of whole passes of the loop (or the last 4 s of a long file), ending on the
    // file's last sample, so compressor, auto gain and detectors are settled exactly as on a looping playback
    // at least 12 s (auto gain needs ~10 s to settle on a loop) and at least one full pass; capped at 60 s for long files
    const size_t want = (size_t)(12.0 * sr), cap = (size_t)(60.0 * sr);
    size_t warm = ((std::max(want, n) + n - 1) / n) * n;
    if (warm > cap) warm = std::max(want, std::min(n, cap));
    const size_t start = (n - (warm % n)) % n;
    const size_t total = warm + n + lat;

    const size_t B = 512;
    std::vector<float> bl(B), br(B), ol(B), orr(B);
    size_t fed = 0, src = start, written = 0;
    while (fed < total) {
        if (cancel && cancel->load()) return false;
        const size_t m = std::min(B, total - fed);
        for (size_t k = 0; k < m; ++k) {
            bl[k] = inL[src]; br[k] = inR[src];
            if (++src == n) src = 0;
        }
        e.process(bl.data(), br.data(), ol.data(), orr.data(), (uint32_t)m);
        for (size_t k = 0; k < m; ++k) {
            const size_t t = fed + k;              // engine time
            if (t >= warm + lat && written < n) { outL[written] = ol[k]; outR[written] = orr[k]; ++written; }
        }
        fed += m;
        if (progress) progress->store((float)fed / (float)total);
    }
    return true;
}

// ------------------------------------------------------------------------------------------------ Baker

Baker::Baker() { worker = std::thread([this] { run(); }); }

Baker::~Baker() {
    quit = true;
    cancelCurrent = true;
    cv.notify_all();
    if (worker.joinable()) worker.join();
}

static double probeSeconds(const std::string& path) {
    const std::string e = lowerExt(path);
    if (e == "wav" || e == "wave" || e == "aif" || e == "aiff" || e == "aifc") {
        drwav wav;
#ifdef _WIN32
        if (!drwav_init_file_w(&wav, widen(path).c_str(), nullptr)) return 0;
#else
        if (!drwav_init_file(&wav, path.c_str(), nullptr)) return 0;
#endif
        const double s = wav.sampleRate ? (double)wav.totalPCMFrameCount / wav.sampleRate : 0;
        drwav_uninit(&wav);
        return s;
    }
    if (e == "flac") {
#ifdef _WIN32
        drflac* f = drflac_open_file_w(widen(path).c_str(), nullptr);
#else
        drflac* f = drflac_open_file(path.c_str(), nullptr);
#endif
        if (!f) return 0;
        const double s = f->sampleRate ? (double)f->totalPCMFrameCount / f->sampleRate : 0;
        drflac_close(f);
        return s;
    }
    return 0; // mp3: known after decoding
}

void Baker::addFiles(const std::vector<std::string>& paths, bool replace) {
    std::vector<Item> fresh;
    for (const std::string& p : paths) {
        if (!isAudioFileName(p)) continue;
        Item it; it.inPath = p; it.name = fileName(p); it.seconds = probeSeconds(p);
        fresh.push_back(it);
    }
    if (fresh.empty()) return;
    std::lock_guard<std::mutex> lk(m);
    if (replace) {
        queue.clear(); cancelCurrent = true; items.clear();
        fresh.resize(1);
    }
    for (Item& it : fresh) {
        bool dup = false;
        for (const Item& o : items) if (o.inPath == it.inPath) dup = true;
        if (dup) continue;
        it.id = nextId++;
        items.push_back(it);
    }
}

void Baker::clear() {
    std::lock_guard<std::mutex> lk(m);
    queue.clear(); cancelCurrent = true; items.clear();
}

void Baker::bakeAll(const BakeSettings& s) {
    std::lock_guard<std::mutex> lk(m);
    settings = s;
    queue.clear();
    cancelCurrent = true; // restart anything in progress with the new settings
    for (Item& it : items) {
        it.state = kWaiting; it.progress = 0; it.error.clear();
        queue.push_back(it.id);
    }
    cv.notify_all();
}

std::vector<Baker::Item> Baker::snapshot() {
    std::lock_guard<std::mutex> lk(m);
    std::vector<Item> v = items;
    for (Item& it : v) if (it.id == curId && it.state == kBaking) it.progress = curProgress.load();
    return v;
}

bool Baker::busy() {
    std::lock_guard<std::mutex> lk(m);
    return curId >= 0 || !queue.empty();
}

int Baker::numItems() {
    std::lock_guard<std::mutex> lk(m);
    return (int)items.size();
}

static std::string slug(const std::string& s) {
    std::string o;
    for (char c : s) {
        const unsigned char u = (unsigned char)c;
        if (std::isalnum(u)) o.push_back((char)std::tolower(u));
        else if ((c == ' ' || c == '-' || c == '_') && !o.empty() && o.back() != '-') o.push_back('-');
    }
    while (!o.empty() && o.back() == '-') o.pop_back();
    return o.empty() ? "custom" : o;
}

void Baker::run() {
    for (;;) {
        int id = -1;
        BakeSettings s;
        std::string inPath, base;
        {
            std::unique_lock<std::mutex> lk(m);
            cv.wait(lk, [this] { return quit.load() || !queue.empty(); });
            if (quit) return;
            id = queue.front(); queue.pop_front();
            bool found = false;
            for (Item& it : items) if (it.id == id) { it.state = kBaking; it.progress = 0; inPath = it.inPath; found = true; }
            if (!found) continue;
            s = settings;
            curId = id; curProgress = 0; cancelCurrent = false;
        }
        std::vector<float> L, R, oL, oR;
        double sr = 44100;
        std::string err, outPath;
        bool ok = loadAudioFile(inPath, L, R, sr, err);
        float peak = 0;
        bool asFloat = false;
        if (ok) {
            ok = renderBake(s, L, R, sr, oL, oR, &curProgress, &cancelCurrent);
            if (!ok) err = "cancelled";
        }
        if (ok) {
            for (size_t i = 0; i < oL.size(); ++i) peak = std::max(peak, std::max(std::fabs(oL[i]), std::fabs(oR[i])));
            asFloat = peak > 1.0f; // never clip silently: overs are kept in a 32-bit float file
            const std::string dir = bakeFolder(true);
#ifdef _WIN32
            const std::string sep = "\\";
#else
            const std::string sep = "/";
#endif
            const std::string stem = dir + sep + fileBaseName(inPath) + MERCY_BAKE_TAG + slug(s.presetName);
            outPath = stem + ".wav";
            for (int k = 2; fileExists(outPath) && k < 1000; ++k) outPath = stem + "_" + std::to_string(k) + ".wav";
            ok = writeWavFile(outPath, oL, oR, sr, asFloat, err);
        }
        {
            std::lock_guard<std::mutex> lk(m);
            for (Item& it : items) if (it.id == id) {
                if (err == "cancelled") { /* re-queued or cleared by the UI */ if (it.state == kBaking) it.state = kWaiting; }
                else if (ok) {
                    it.state = kDone; it.progress = 1; it.outPath = outPath; it.outName = fileName(outPath);
                    it.peakDb = linToDb(peak); it.asFloat = asFloat; it.seconds = (double)L.size() / sr;
                } else { it.state = kError; it.error = err; }
            }
            curId = -1;
        }
    }
}

} // namespace mercy
