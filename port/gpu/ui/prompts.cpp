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
    const std::string path = std::string(XSG_PROMPTS_DIR) + "/" + rel + ".png";
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
