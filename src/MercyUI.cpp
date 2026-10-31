#include "DistrhoUI.hpp"
#include "MercyView.hpp"
#include "MercyDnD.hpp"
#include <atomic>
#include <mutex>
#ifdef __APPLE__
#include <pthread.h>
#endif

START_NAMESPACE_DISTRHO

class MercyUI : public UI, public mercy::ViewHost, public mercy::DropCallbacks
{
public:
    MercyUI()
        : UI(DISTRHO_UI_DEFAULT_WIDTH, DISTRHO_UI_DEFAULT_HEIGHT, true),
          fView(this, mercy::sessionFromPluginInstance(getPluginInstancePointer()))
    {
        installDnD();
    }

    ~MercyUI() override
    {
        mercy::dndRemove(fDnD);
        fDnD = nullptr;
    }

protected:
    // host -> UI
    // The AU wrapper calls these from whatever thread changed the value (the audio thread for the meters,
    // any thread for automation). The window may only be touched on the main thread (macOS crashes
    // otherwise, rarely), so off the main thread the change is queued and applied in uiIdle().
    static bool onMainThread()
    {
#ifdef __APPLE__
        return pthread_main_np() != 0;
#else
        return true; // Windows/VST3: DPF already calls these on the UI thread
#endif
    }
    void parameterChanged(uint32_t index, float value) override
    {
        if (onMainThread()) { fView.paramChanged(index, value); return; }
        if (index >= mercy::kNumParams) return;
        fPending[index].store(value, std::memory_order_relaxed);
        fPendingSet[index].store(true, std::memory_order_release);
        fAnyPending.store(true, std::memory_order_release);
    }
    void stateChanged(const char* key, const char* value) override
    {
        if (!onMainThread()) {
            std::lock_guard<std::mutex> lk(fStateMutex);
            fPendingState.emplace_back(key ? key : "", value ? value : "");
            fAnyPending.store(true, std::memory_order_release);
            return;
        }
        applyState(key, value);
    }
    void applyState(const char* key, const char* value)
    {
        if (std::strcmp(key, "order") == 0) fView.orderChanged(value);
        else if (std::strcmp(key, "ui") == 0) fView.uiStateChanged(value);
    }
    void applyPending()
    {
        if (!fAnyPending.exchange(false, std::memory_order_acq_rel)) return;
        for (uint32_t p = 0; p < mercy::kNumParams; ++p)
            if (fPendingSet[p].exchange(false, std::memory_order_acq_rel))
                fView.paramChanged(p, fPending[p].load(std::memory_order_relaxed));
        std::vector<std::pair<std::string, std::string>> st;
        { std::lock_guard<std::mutex> lk(fStateMutex); st.swap(fPendingState); }
        for (const auto& kv : st) applyState(kv.first.c_str(), kv.second.c_str());
    }
    void uiIdle() override
    {
        applyPending();
        if (fDnD == nullptr && ++fDnDTries < 50) installDnD();
        fView.idle();
    }

    // drawing
    void onNanoDisplay() override { fView.draw(*this, (float)getWidth(), (float)getHeight()); }

    // input
    bool onMouse(const MouseEvent& ev) override
    {
        const int button = ev.button == kMouseButtonLeft ? 1 : (ev.button == kMouseButtonRight ? 2 : 3);
        return fView.mouse(button, ev.press, (float)ev.pos.getX(), (float)ev.pos.getY(), ev.mod, ev.time);
    }
    bool onMotion(const MotionEvent& ev) override
    {
        return fView.motion((float)ev.pos.getX(), (float)ev.pos.getY(), ev.mod);
    }
    bool onScroll(const ScrollEvent& ev) override
    {
        return fView.scroll((float)ev.pos.getX(), (float)ev.pos.getY(), (float)ev.delta.getY(), ev.mod);
    }
    bool onKeyboard(const KeyboardEvent& ev) override { return fView.key(ev.key, ev.press); }
    bool onCharacterInput(const CharacterInputEvent& ev) override { return fView.character(ev.character); }

    // ViewHost
    void hostBeginEdit(uint32_t p) override { editParameter(p, true); }
    void hostSetParam(uint32_t p, float v) override { setParameterValue(p, v); }
    void hostEndEdit(uint32_t p) override { editParameter(p, false); }
    void hostSetOrder(const char* s) override { setState("order", s); }
    void hostRepaint() override { repaint(); }
    void hostResize(unsigned w, unsigned h) override { setSize(w, h); }
    void hostBrowseFile() override
    {
        FileBrowserOptions opts;
        opts.title = "Mercy - choose audio to bake";
        openFileBrowser(opts);
    }
    bool hostStartDrag(const std::vector<std::string>& paths) override { return mercy::dndStartFileDrag(getWindow().getNativeWindowHandle(), paths); }
    void hostSetUiState(const char* s) override { setState("ui", s); }
    void hostKeyboardFocus(bool grab) override
    {
        const uintptr_t h = getWindow().getNativeWindowHandle();
#if defined(_WIN32) || defined(__APPLE__)
        mercy::keyboardFocus(h, grab);
#else
        if (grab) getWindow().focus();
        (void)h;
#endif
    }

    void uiFileBrowserSelected(const char* filename) override
    {
        if (filename != nullptr && filename[0] != '\0')
            fView.filesDropped({std::string(filename)});
    }

    // DropCallbacks (called on the UI thread by the OS drag-and-drop loop)
    bool dropAccepting() override { return fView.dropAccepting(); }
    void dropHover(bool over) override { fView.setDropHover(over); }
    void dropFiles(const std::vector<std::string>& paths) override { fView.filesDropped(paths); }

    void installDnD()
    {
        const uintptr_t h = getWindow().getNativeWindowHandle();
        if (h != 0) fDnD = mercy::dndInstall(h, this);
    }

private:
    mercy::MercyView fView;
    std::atomic<float> fPending[mercy::kNumParams] {};
    std::atomic<bool> fPendingSet[mercy::kNumParams] {};
    std::atomic<bool> fAnyPending {false};
    std::mutex fStateMutex;
    std::vector<std::pair<std::string, std::string>> fPendingState;
    void* fDnD = nullptr;
    int fDnDTries = 0;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MercyUI)
};

UI* createUI() { return new MercyUI(); }

END_NAMESPACE_DISTRHO
