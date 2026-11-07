/* Stage 10: keyboard/gamepad input in xsg-gpu, published to the game as a
   DualShock 2 state (GpuPad in port/runtime/gpu_ipc.h).  See
   port/docs/STAGE10_INPUT.md. */
#ifndef XSG_INPUT_H
#define XSG_INPUT_H

#include "gpu_ipc.h"

#include <string>
#include <utility>
#include <vector>

#ifdef XSG_GPU_SDL
union SDL_Event;
#endif

namespace xsginput {

/* Video options from the [video] section of input.ini (env vars win). */
std::string Option(const char* key, const char* env, const char* def);

/* Loads the config; `sdl` = SDL events are available (window, or the
   XSG_GPU_INPUT_TEST hook).  Without it the pad stays neutral. */
void Init(GpuPad* pad, bool sdl);
#ifdef XSG_GPU_SDL
/* Every SDL event; returns true when it was consumed as pad input. */
bool Event(const SDL_Event& e);
#endif
/* Once per game vblank, before the event pump: runs XSG_GPU_INPUT_TEST. */
void BeginFrame(uint32_t frame);
/* After the event pump: publishes the DS2 state to the shared block. */
void Publish();
/* SDL_PollEvent loop for runs without a window (test hook only). */
void PumpHeadless();

/* In-game menu (port/docs/MENU.md). While open the game gets a neutral pad. */
void SetMenuOpen(bool open);
bool MenuOpen();
/* "CROSS+UP" <-> SCE_PAD mask (plus the LS_ / RS_ stick targets). */
uint32_t TargetMask(const char* names);
std::string TargetNames(uint32_t mask);
void SetDeadzone(int deadzone, int trigger_threshold);
#ifdef XSG_GPU_SDL
/* Raw SDL gamepad state for menu navigation: bit i = SDL_GamepadButton i,
   left stick axes.  False when no gamepad is open. */
bool MenuPad(uint32_t* buttons, int* lx, int* ly);
/* Remapping: (scancode | SDL_GamepadButton, mask) pairs.  Gamepad ids
   SDL_GAMEPAD_BUTTON_COUNT / +1 are the left / right trigger axes. */
std::vector<std::pair<int, uint32_t>> KeyBinds();
std::vector<std::pair<int, uint32_t>> PadBinds();
void SetKeyBind(int scancode, uint32_t mask);   // 0 unbinds
void SetPadBind(int button, uint32_t mask);
const char* PadButtonName(int button);
void SaveBinds();    // into settings.ini [keyboard] / [gamepad] (diff to the defaults)
void ResetBinds();
#endif

}  // namespace xsginput

#endif
