// user preset naming / rename / delete through the public input API (no window needed)
#include "../src/MercyView.hpp"
#include <cstdio>
#include <fstream>
#include <sstream>
using namespace mercy;
struct H : ViewHost {
    void hostBeginEdit(uint32_t) override {} void hostEndEdit(uint32_t) override {}
    void hostSetParam(uint32_t, float) override {} void hostSetOrder(const char*) override {}
    void hostRepaint() override {} void hostResize(unsigned, unsigned) override {}
};
static std::string fileText() { std::ifstream f(std::string(getenv("HOME")) + "/.config/mercy/user_presets.txt"); std::stringstream s; s << f.rdbuf(); return s.str(); }
static void names(const char* tag) {
    std::string t = fileText(), out; std::istringstream is(t); std::string line;
    while (std::getline(is, line)) out += "[" + line.substr(0, line.find('\t')) + "] ";
    printf("%-26s %s\n", tag, out.c_str());
}
int main() {
    std::remove((std::string(getenv("HOME")) + "/.config/mercy/user_presets.txt").c_str());
    H h; Session s; MercyView v(&h, &s);
    unsigned t = 1000;
    auto click = [&](float x, float y, int b = 1) { v.mouse(b, true, x, y, 0, t); v.mouse(b, false, x, y, 0, t + 5); t += 1000; };
    auto type = [&](const char* c) { for (; *c; ++c) v.character((unsigned)*c); };
    // 1. save with a typed name (typing replaces the suggested "user 01")
    click(260, 557); type("dark fantasy kick"); v.key(13, true); names("save typed:");
    // 2. save, accept the suggested name with the save button
    click(260, 557); click(212 + 320 - 20 - 38, 220 + 131); names("save default:");
    // 3. save, cancel with Esc
    click(260, 557); type("nope"); v.key(27, true); names("save cancelled:");
    // 4. save with backspace editing + junk characters filtered
    click(260, 557); type("throne   claps!!"); v.key(8, true); v.key(8, true); v.key(13, true); names("save edited:");
    // 5. rename "user 02" (the 2nd user preset) via right-click menu
    click(150, 557);                                   // open browser
    for (int i = 0; i < 10; ++i) v.scroll(300, 300, -1, 0);
    // user rows: header at k=12 (y 440), row k=13 y=462 -> [dark fantasy kick | user 02], k=14 y=484 -> [throne claps]
    click(450, 462, 2); click(470, 475);               // right-click "user 02" -> rename
    type("hotel snare"); v.key(13, true); names("rename:");
    // 6. delete "dark fantasy kick" with the x, then confirm yes
    click(348, 462); click(318, 462); names("delete (x, yes):");
    // 7. right-click delete then "no"
    click(150, 557); for (int i = 0; i < 10; ++i) v.scroll(300, 300, -1, 0);
    click(200, 462, 2); click(220, 462 + 34); click(356 - 6, 462); names("delete -> no:");
    printf("preset idx %d\n", s.presetIdx);
    return 0;
}
