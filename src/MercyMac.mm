// MERCY - Mac (AppKit) versions of the Windows-only pieces in MercyDnD.cpp:
//   - drop audio files onto the plugin (bake panel)
//   - drag baked files out of the plugin into Logic / GarageBand / any DAW or Finder
//   - take keyboard focus while a text box is open, and hand it back afterwards
// Built only on macOS (see Makefile). Manual retain/release (DPF builds without ARC).
#include "MercyDnD.hpp"

#import <Cocoa/Cocoa.h>
#include <objc/runtime.h>
#include <map>
#include <cstdio>

namespace mercy {

// the plugin's NSView (pugl's wrapper view) -> its callbacks; only touched on the main thread
static std::map<NSView*, DropCallbacks*>& dropViews() {
    static std::map<NSView*, DropCallbacks*> m;
    return m;
}
static DropCallbacks* callbacksFor(id view) {
    std::map<NSView*, DropCallbacks*>& m = dropViews();
    const auto it = m.find((NSView*)view);
    return it == m.end() ? nullptr : it->second;
}

static NSArray* fileURLsIn(id<NSDraggingInfo> info) {
    NSPasteboard* pb = [info draggingPasteboard];
    NSDictionary* opts = @{ NSPasteboardURLReadingFileURLsOnlyKey : @YES };
    return [pb readObjectsForClasses:@[ [NSURL class] ] options:opts];
}

// ------------------------------------------------------------------------------------------------ drop target
// These are added to pugl's view class at run time (it doesn't implement NSDraggingDestination itself).
static NSDragOperation mercyDraggingEntered(id self, SEL, id<NSDraggingInfo> info) {
    DropCallbacks* cb = callbacksFor(self);
    if (cb == nullptr || !cb->dropAccepting() || [fileURLsIn(info) count] == 0) return NSDragOperationNone;
    cb->dropHover(true);
    return NSDragOperationCopy;
}
static NSDragOperation mercyDraggingUpdated(id self, SEL, id<NSDraggingInfo> info) {
    DropCallbacks* cb = callbacksFor(self);
    if (cb == nullptr || !cb->dropAccepting() || [fileURLsIn(info) count] == 0) return NSDragOperationNone;
    return NSDragOperationCopy;
}
static void mercyDraggingExited(id self, SEL, id<NSDraggingInfo>) {
    if (DropCallbacks* cb = callbacksFor(self)) cb->dropHover(false);
}
static BOOL mercyPrepareForDrag(id self, SEL, id<NSDraggingInfo>) {
    DropCallbacks* cb = callbacksFor(self);
    return cb != nullptr && cb->dropAccepting();
}
static BOOL mercyPerformDrag(id self, SEL, id<NSDraggingInfo> info) {
    DropCallbacks* cb = callbacksFor(self);
    if (cb == nullptr) return NO;
    cb->dropHover(false);
    if (!cb->dropAccepting()) return NO;
    std::vector<std::string> paths;
    for (NSURL* url in fileURLsIn(info))
        if ([url isFileURL] && [url path] != nil) paths.push_back([[url path] UTF8String]);
    if (paths.empty()) return NO;
    cb->dropFiles(paths);
    return YES;
}

static void addMethod(Class cls, SEL sel, IMP imp, const char* types) {
    // class_addMethod leaves an existing implementation alone, which is what we want
    class_addMethod(cls, sel, imp, types);
}

void* dndInstall(uintptr_t nativeWindow, DropCallbacks* cb) {
    if (nativeWindow == 0 || cb == nullptr) return nullptr;
    NSView* view = (NSView*)nativeWindow;
    Class cls = object_getClass(view);
    const std::string op = std::string(@encode(NSDragOperation)) + "@:@";
    const std::string vd = std::string(@encode(void)) + "@:@";
    const std::string bl = std::string(@encode(BOOL)) + "@:@";
    addMethod(cls, @selector(draggingEntered:), (IMP)mercyDraggingEntered, op.c_str());
    addMethod(cls, @selector(draggingUpdated:), (IMP)mercyDraggingUpdated, op.c_str());
    addMethod(cls, @selector(draggingExited:), (IMP)mercyDraggingExited, vd.c_str());
    addMethod(cls, @selector(prepareForDragOperation:), (IMP)mercyPrepareForDrag, bl.c_str());
    addMethod(cls, @selector(performDragOperation:), (IMP)mercyPerformDrag, bl.c_str());
    dropViews()[view] = cb;
    [view registerForDraggedTypes:@[ NSPasteboardTypeFileURL ]];
    return view;
}

void dndRemove(void* handle) {
    if (handle == nullptr) return;
    NSView* view = (NSView*)handle;
    dropViews().erase(view);
    [view unregisterDraggedTypes];
}

// ------------------------------------------------------------------------------------------------ drag source
// A tiny NSDraggingSource class, created at run time with a name unique to this copy of the plugin
// (the VST3 and the AU can be loaded into the same host at once).
static NSDragOperation mercySourceMask(id, SEL, NSDraggingSession*, NSDraggingContext) {
    return NSDragOperationCopy | NSDragOperationGeneric;
}
static BOOL mercyIgnoreModifiers(id, SEL, NSDraggingSession*) { return YES; }

static id dragSource() {
    static id source = nil;
    if (source != nil) return source;
    char name[96];
    std::snprintf(name, sizeof name, "SlapworxMercyDragSource_%p", (void*)&dragSource);
    Class cls = objc_getClass(name);
    if (cls == Nil) {
        cls = objc_allocateClassPair([NSObject class], name, 0);
        if (cls == Nil) return nil;
        const std::string mask = std::string(@encode(NSDragOperation)) + "@:@" + @encode(NSDraggingContext);
        const std::string ign = std::string(@encode(BOOL)) + "@:@";
        class_addMethod(cls, @selector(draggingSession:sourceOperationMaskForDraggingContext:), (IMP)mercySourceMask, mask.c_str());
        class_addMethod(cls, @selector(ignoreModifierKeysForDraggingSession:), (IMP)mercyIgnoreModifiers, ign.c_str());
        class_addProtocol(cls, @protocol(NSDraggingSource));
        objc_registerClassPair(cls);
    }
    source = [[cls alloc] init]; // kept for the life of the plugin binary
    return source;
}

bool dndStartFileDrag(uintptr_t nativeWindow, const std::vector<std::string>& paths) {
    if (nativeWindow == 0 || paths.empty()) return false;
    NSView* view = (NSView*)nativeWindow;
    NSEvent* ev = [NSApp currentEvent]; // the mouse-drag event we're being called from
    if (ev == nil || [view window] == nil) return false;
    id src = dragSource();
    if (src == nil) return false;
    const NSPoint at = [view convertPoint:[ev locationInWindow] fromView:nil];
    NSMutableArray* items = [NSMutableArray array];
    int i = 0;
    for (const std::string& p : paths) {
        NSString* path = [NSString stringWithUTF8String:p.c_str()];
        if (path == nil) continue;
        NSURL* url = [NSURL fileURLWithPath:path isDirectory:NO];
        NSDraggingItem* item = [[[NSDraggingItem alloc] initWithPasteboardWriter:url] autorelease];
        NSImage* icon = [[NSWorkspace sharedWorkspace] iconForFile:path];
        const NSRect frame = NSMakeRect(at.x - 16 + i * 6, at.y - 16 - i * 6, 32, 32);
        [item setDraggingFrame:frame contents:icon];
        [items addObject:item];
        ++i;
    }
    if ([items count] == 0) return false;
    NSDraggingSession* session = [view beginDraggingSessionWithItems:items event:ev source:src];
    if (session == nil) return false;
    [session setAnimatesToStartingPositionsOnCancelOrFail:YES];
    return true; // the drag carries on asynchronously; the files stay where they are
}

bool dndSupported() { return true; }

// ------------------------------------------------------------------------------------------------ keyboard
// Typing a preset name or a value: make the plugin view the first responder so key presses come to it
// instead of the host's shortcuts (space = play, etc.); give it back when the text box closes.
void keyboardFocus(uintptr_t nativeWindow, bool grab) {
    if (nativeWindow == 0) return;
    NSView* view = (NSView*)nativeWindow;
    NSWindow* win = [view window];
    if (win == nil) return;
    if (grab) {
        [win makeKeyWindow];
        [win makeFirstResponder:view];
    } else if ([win firstResponder] == view) {
        [win makeFirstResponder:nil];
    }
}

// System Settings > Accessibility > Display > Reduce motion
bool osReduceMotion() {
    return [[NSWorkspace sharedWorkspace] accessibilityDisplayShouldReduceMotion];
}

} // namespace mercy
