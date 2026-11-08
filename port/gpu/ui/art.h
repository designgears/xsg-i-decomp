/* Rebuilt UI: the game's own art (portraits, icons, gauges), drawn by ImGui.
 *
 * Captured sprites name a texture by its TEX0 (base, width, format, CLUT).  The
 * page is decoded from GS memory (native.h DecodeVramTexture) into an ImGui
 * texture, cached per TEX0, and re-checked every few frames while it is in use
 * (menus swap pages: a changed page is uploaded again). */
#pragma once
#include <cstdint>

#include "imgui.h"

namespace ParallelGS { class GSInterface; }

namespace xsgui {

void ArtInit(ParallelGS::GSInterface* gs);
/* GPU_MSG_UI_TEX: a texture set (key) arrives: its uploads go into a private GS memory. */
void ArtTexset(uint32_t key, uint32_t entries, const void* payload, uint32_t len);
/* A rebuilt-UI marker reached the GS stream: copy the menu texture region of the GS-memory
   mirror into texture set 0x5A000000 + slot (re-decoded only when it changed). */
void ArtSnapshot(uint32_t slot);
/* The same for one texture: the pages of tex0's image and CLUT, from a 4 MB GS image, into set `key`. */
void ArtSnapshotTex(uint32_t key, uint64_t tex0, const uint8_t* mem);
/* Once per frame, before drawing (counts frames for the re-check). */
void ArtFrame();
/* The ImGui texture for a TEX0 (0: not available); *w / *h: its size in texels.  texset:
   decode from that texture set's private memory instead of GS memory. */
ImTextureID ArtTexture(uint64_t tex0, uint64_t texa, uint32_t* w, uint32_t* h, uint32_t texset = 0);
/* Draw texels (u, v, tw, th) of tex0 into the screen rectangle a..b, tinted by
   rgba (PS2 colour: 0x80 = 1.0 per channel). additive: GS additive blend. */
void ArtSprite(ImDrawList* dl, uint64_t tex0, uint64_t texa, float u, float v, float tw, float th, ImVec2 a, ImVec2 b,
               uint32_t rgba, float alpha, bool additive, uint32_t texset = 0);

}  // namespace xsgui
