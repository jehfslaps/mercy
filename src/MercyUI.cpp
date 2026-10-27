#include "DistrhoUI.hpp"
#include "MercyView.hpp"
#include "MercyDnD.hpp"

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
    void parameterChanged(uint32_t index, float value) override { fView.paramChanged(index, value); }
    void stateChanged(const char* key, const char* value) override
    {
        if (std::strcmp(key, "order") == 0) fView.orderChanged(value);
        else if (std::strcmp(key, "ui") == 0) fView.uiStateChanged(value);
    }
    void uiIdle() override
    {
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
    void* fDnD = nullptr;
    int fDnDTries = 0;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MercyUI)
};

UI* createUI() { return new MercyUI(); }

END_NAMESPACE_DISTRHO
