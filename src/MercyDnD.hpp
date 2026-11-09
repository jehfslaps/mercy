// MERCY - native drag and drop (drop files onto the plugin, drag baked files out)
// Windows: MercyDnD.cpp (OLE). Mac: MercyMac.mm (AppKit). Linux (tests only): stubs in MercyDnD.cpp.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace mercy {

struct DropCallbacks {
    virtual ~DropCallbacks() {}
    virtual bool dropAccepting() = 0;                                  // is the bake panel open?
    virtual void dropHover(bool over) = 0;                             // files being dragged over the window
    virtual void dropFiles(const std::vector<std::string>& paths) = 0; // UTF-8 paths
};

void* dndInstall(uintptr_t nativeWindow, DropCallbacks* cb); // returns a handle (or nullptr)
void dndRemove(void* handle);
bool dndStartFileDrag(uintptr_t nativeWindow, const std::vector<std::string>& paths); // Windows: blocks until the drop
bool dndSupported();
// take keyboard focus for typing (true), or hand it back to the host window (false)
void keyboardFocus(uintptr_t nativeWindow, bool grab);
bool osReduceMotion();   // the OS accessibility setting "reduce motion" / "show animations" off

} // namespace mercy
