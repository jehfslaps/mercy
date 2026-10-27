// MERCY - native drag and drop (Windows). The Mac version is MercyMac.mm; Linux (tests only) gets no-op stubs.
#include "MercyDnD.hpp"

#ifdef _WIN32
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <ole2.h>
#include <shlobj.h>
#include <shellapi.h>

namespace mercy {

static std::wstring widenUtf8(const std::string& s) {
    if (s.empty()) return std::wstring();
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    std::wstring w(n, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &w[0], n);
    return w;
}
static std::string narrowUtf8(const wchar_t* w) {
    const int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
    if (n <= 1) return std::string();
    std::string s(n - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, w, -1, &s[0], n, nullptr, nullptr);
    return s;
}

// ------------------------------------------------------------------------------------------------ drop target
class MercyDropTarget : public IDropTarget {
public:
    explicit MercyDropTarget(DropCallbacks* c) : cb(c) {}
    // IUnknown
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (riid == IID_IUnknown || riid == IID_IDropTarget) { *ppv = static_cast<IDropTarget*>(this); AddRef(); return S_OK; }
        *ppv = nullptr; return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return (ULONG)InterlockedIncrement(&refs); }
    ULONG STDMETHODCALLTYPE Release() override {
        const LONG r = InterlockedDecrement(&refs);
        if (r == 0) delete this;
        return (ULONG)r;
    }
    // IDropTarget
    HRESULT STDMETHODCALLTYPE DragEnter(IDataObject* data, DWORD, POINTL, DWORD* effect) override {
        hasFiles = false;
        if (data != nullptr) {
            FORMATETC fe = {CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
            hasFiles = data->QueryGetData(&fe) == S_OK;
        }
        const bool ok = hasFiles && cb != nullptr && cb->dropAccepting();
        *effect = ok ? DROPEFFECT_COPY : DROPEFFECT_NONE;
        if (ok) cb->dropHover(true);
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE DragOver(DWORD, POINTL, DWORD* effect) override {
        const bool ok = hasFiles && cb != nullptr && cb->dropAccepting();
        *effect = ok ? DROPEFFECT_COPY : DROPEFFECT_NONE;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE DragLeave() override {
        if (cb != nullptr) cb->dropHover(false);
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE Drop(IDataObject* data, DWORD, POINTL, DWORD* effect) override {
        *effect = DROPEFFECT_NONE;
        if (cb == nullptr || data == nullptr) return S_OK;
        cb->dropHover(false);
        if (!cb->dropAccepting()) return S_OK;
        FORMATETC fe = {CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
        STGMEDIUM sm = {};
        if (data->GetData(&fe, &sm) != S_OK) return S_OK;
        std::vector<std::string> paths;
        HDROP hd = (HDROP)GlobalLock(sm.hGlobal);
        if (hd != nullptr) {
            const UINT n = DragQueryFileW(hd, 0xFFFFFFFF, nullptr, 0);
            for (UINT i = 0; i < n; ++i) {
                const UINT len = DragQueryFileW(hd, i, nullptr, 0);
                std::wstring w(len + 1, 0);
                DragQueryFileW(hd, i, &w[0], len + 1);
                w.resize(len);
                paths.push_back(narrowUtf8(w.c_str()));
            }
            GlobalUnlock(sm.hGlobal);
        }
        ReleaseStgMedium(&sm);
        if (!paths.empty()) { cb->dropFiles(paths); *effect = DROPEFFECT_COPY; }
        return S_OK;
    }
    DropCallbacks* cb;
private:
    LONG refs = 1;
    bool hasFiles = false;
};

struct DnDHandle {
    HWND hwnd = nullptr;
    MercyDropTarget* target = nullptr;
    bool oleInit = false;
};

void* dndInstall(uintptr_t nativeWindow, DropCallbacks* cb) {
    if (nativeWindow == 0) return nullptr;
    DnDHandle* h = new DnDHandle();
    h->hwnd = (HWND)nativeWindow;
    h->oleInit = SUCCEEDED(OleInitialize(nullptr));
    h->target = new MercyDropTarget(cb);
    if (RegisterDragDrop(h->hwnd, h->target) != S_OK) {
        // a drop target may already exist on this window: replace it with ours
        RevokeDragDrop(h->hwnd);
        if (RegisterDragDrop(h->hwnd, h->target) != S_OK) {
            h->target->Release();
            h->target = nullptr;
            // last resort: classic WM_DROPFILES is not handled here; drops just won't work
        }
    }
    return h;
}

void dndRemove(void* handle) {
    DnDHandle* h = static_cast<DnDHandle*>(handle);
    if (h == nullptr) return;
    if (h->target != nullptr) {
        RevokeDragDrop(h->hwnd);
        h->target->cb = nullptr;
        h->target->Release();
    }
    if (h->oleInit) OleUninitialize();
    delete h;
}

// ------------------------------------------------------------------------------------------------ drag source
class MercyDropSource : public IDropSource {
public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (riid == IID_IUnknown || riid == IID_IDropSource) { *ppv = static_cast<IDropSource*>(this); AddRef(); return S_OK; }
        *ppv = nullptr; return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return (ULONG)InterlockedIncrement(&refs); }
    ULONG STDMETHODCALLTYPE Release() override {
        const LONG r = InterlockedDecrement(&refs);
        if (r == 0) delete this;
        return (ULONG)r;
    }
    HRESULT STDMETHODCALLTYPE QueryContinueDrag(BOOL escape, DWORD keys) override {
        if (escape) return DRAGDROP_S_CANCEL;
        if (!(keys & (MK_LBUTTON | MK_RBUTTON))) return DRAGDROP_S_DROP;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GiveFeedback(DWORD) override { return DRAGDROP_S_USEDEFAULTCURSORS; }
private:
    LONG refs = 1;
};

bool dndStartFileDrag(uintptr_t, const std::vector<std::string>& paths) {
    if (paths.empty()) return false;
    // Build a shell data object, exactly like dragging the files out of Explorer (CF_HDROP + shell IDLists).
    std::vector<PIDLIST_ABSOLUTE> full;
    for (const std::string& p : paths) {
        PIDLIST_ABSOLUTE pidl = nullptr;
        if (SUCCEEDED(SHParseDisplayName(widenUtf8(p).c_str(), nullptr, &pidl, 0, nullptr)) && pidl != nullptr)
            full.push_back(pidl);
    }
    if (full.empty()) return false;
    // all baked files live in the same folder: use the first file's parent as the common folder
    PIDLIST_ABSOLUTE folder = ILClone(full[0]);
    ILRemoveLastID(folder);
    std::vector<PCUITEMID_CHILD> children;
    for (PIDLIST_ABSOLUTE p : full) children.push_back(ILFindLastID(p));
    IDataObject* data = nullptr;
    HRESULT hr = SHCreateDataObject(folder, (UINT)children.size(), children.data(), nullptr, IID_IDataObject, (void**)&data);
    bool ok = false;
    if (SUCCEEDED(hr) && data != nullptr) {
        MercyDropSource* src = new MercyDropSource();
        DWORD effect = DROPEFFECT_NONE;
        hr = DoDragDrop(data, src, DROPEFFECT_COPY | DROPEFFECT_LINK, &effect);
        ok = hr == DRAGDROP_S_DROP;
        src->Release();
        data->Release();
    }
    ILFree(folder);
    for (PIDLIST_ABSOLUTE p : full) ILFree(p);
    return ok;
}

bool dndSupported() { return true; }

// Clicking a plugin window doesn't give it keyboard focus on Windows, and hosts (FL Studio) don't
// reliably forward keys, so while MERCY wants typing it takes focus itself and gives it back after.
void keyboardFocus(uintptr_t nativeWindow, bool grab) {
    HWND h = (HWND)nativeWindow;
    if (h == nullptr) return;
    if (grab) { SetFocus(h); return; }
    if (GetFocus() == h) { HWND p = GetParent(h); SetFocus(p); }
}

} // namespace mercy

#elif !defined(__APPLE__) // ----------------------------------------------------------------------------- Linux (tests)

namespace mercy {
void* dndInstall(uintptr_t, DropCallbacks*) { return nullptr; }
void dndRemove(void*) {}
bool dndStartFileDrag(uintptr_t, const std::vector<std::string>&) { return false; }
bool dndSupported() { return false; }
void keyboardFocus(uintptr_t, bool) {}
}

#endif
