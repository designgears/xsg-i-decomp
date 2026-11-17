/* Rebuilt UI: input prompt icons.  See prompts.h. */
#include "prompts.h"

#include "../menu/cfg.h"
#include "../menu/menu_render.h"

#include <cstdio>
#include <string>
#include <unordered_map>

#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include "stb_image.h"

#include <cstdlib>
#include <sys/stat.h>
#ifdef XSG_GPU_SDL
#include <SDL3/SDL_filesystem.h>
#endif

#ifndef XSG_PROMPTS_DIR
#define XSG_PROMPTS_DIR "assets/prompts"
#endif

namespace xsgui {
namespace {
const char* const kFamily[] = {"xbox", "playstation", "nintendo", "keyboard"};
const char* const kButton[] = {"south", "east", "west", "north", "lb", "rb", "lt", "rt", "back", "start"};
bool g_have_pad;
std::unordered_map<std::string, ImTextureID> g_cache;   // "family/style/button" -> texture (0: missing)

ImTextureID Load(const std::string& rel) {
    auto it = g_cache.find(rel);
    if (it != g_cache.end()) return it->second;
    ImTextureID id = 0;
    // the icons: the source tree's (a build run in place; empty in packaged builds), else an
    // AppImage's copy, else next to the executable: <dir of xsg-gpu>/../share/xsg/prompts (the
    // Windows zip: gpu\xsg-gpu.exe, share\xsg\prompts) or <dir>/prompts (port/packaging)
    static const std::string dir = [] {
        struct stat st;
        if (*XSG_PROMPTS_DIR && stat(XSG_PROMPTS_DIR, &st) == 0) return std::string(XSG_PROMPTS_DIR);
        if (const char* a = std::getenv("APPDIR"); a && *a) return std::string(a) + "/usr/share/xsg/prompts";
#ifdef XSG_GPU_SDL
        if (const char* b = SDL_GetBasePath(); b && *b) {
            for (const char* rel : {"../share/xsg/prompts", "share/xsg/prompts", "prompts"}) {
                const std::string d = std::string(b) + rel;
                if (stat(d.c_str(), &st) == 0) return d;
            }
        }
#endif
        return std::string(XSG_PROMPTS_DIR);
    }();
    const std::string path = dir + "/" + rel + ".png";
    int w = 0, h = 0, n = 0;
    if (unsigned char* px = stbi_load(path.c_str(), &w, &h, &n, 4)) {
        id = ImTextureID(xsgmenu::RenderTexture(0, reinterpret_cast<const uint32_t*>(px), uint32_t(w), uint32_t(h)));
        stbi_image_free(px);
    }
    g_cache[rel] = id;
    return id;
}
}  // namespace

void PromptsFrame(bool have_pad) { g_have_pad = have_pad; }

int PromptFamily() {
    const std::string p = xsgcfg::Get("display.prompts");
    for (int i = 0; i < 4; ++i)
        if (p == kFamily[i]) return i;
    if (!g_have_pad) return 3;
    const std::string l = xsgcfg::Get("gamepad.layout");
    return l == "playstation" ? 1 : l == "nintendo" ? 2 : 0;
}

ImTextureID PromptIcon(int button) {
    if (button < 0 || button >= kPromptCount) return 0;
    std::string style = xsgcfg::Get("display.prompt_style");
    if (style == "drawn") return 0;
    if (style.empty()) style = "default";
    const int f = PromptFamily();
    if (f == 1 && (button == kBack || button == kStart)) return 0;   // PlayStation's Share / Options art is unreadable small
    const std::string fam = kFamily[f];
    if (ImTextureID id = Load(fam + "/" + style + "/" + kButton[button])) return id;
    return Load(fam + "/default/" + kButton[button]);   // a style without that icon
}

bool PromptDraw(ImDrawList* dl, int button, ImVec2 c, float h, float alpha) {
    const ImTextureID id = PromptIcon(button);
    if (!id) return false;
    const float a = alpha < 0 ? 0 : alpha > 1 ? 1 : alpha;
    dl->AddImage(id, ImVec2(c.x - h * 0.5f, c.y - h * 0.5f), ImVec2(c.x + h * 0.5f, c.y + h * 0.5f), ImVec2(0, 0), ImVec2(1, 1),
                 IM_COL32(255, 255, 255, int(255 * a)));
    return true;
}

}  // namespace xsgui

/* stb_image for the rest of the UI (art.cpp overrides): compiled once, here. */
unsigned char* xsgui_stbi_load(const char* path, int* w, int* h, int* n, int want) { return stbi_load(path, w, h, n, want); }
void xsgui_stbi_free(unsigned char* p) { stbi_image_free(p); }
