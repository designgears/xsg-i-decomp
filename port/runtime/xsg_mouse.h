/* Native mouse, the game's side (port/docs/MOUSE.md).
 *
 * xsg-gpu publishes the pointer (gpu_ipc.h GpuMouse); this turns it into a per-frame
 * state the game's menus and field read directly.  Hooks (#ifdef PORT_HOST blocks in
 * src/) hit-test the pointer against the game's own geometry (512 x 448), set their
 * cursor, and claim the click; a click nothing claims falls back to a pad button
 * (left: CIRCLE when [mouse] click_advances, right: CROSS, middle: TRIANGLE, side
 * buttons: L1 / R1, wheel: UP / DOWN). */
#ifndef XSG_MOUSE_H
#define XSG_MOUSE_H

#ifdef __cplusplus
extern "C" {
#endif

enum { XSG_MB_LEFT = 1, XSG_MB_RIGHT = 2, XSG_MB_MIDDLE = 4, XSG_MB_BACK = 8, XSG_MB_FORWARD = 16 };
/* DS2 buttons (SCE_PAD masks) the mouse taps */
enum {
    XSG_PAD_L1 = 0x0004, XSG_PAD_R1 = 0x0008, XSG_PAD_TRIANGLE = 0x0010, XSG_PAD_CIRCLE = 0x0020,
    XSG_PAD_CROSS = 0x0040, XSG_PAD_SQUARE = 0x0080, XSG_PAD_UP = 0x1000, XSG_PAD_RIGHT = 0x2000,
    XSG_PAD_DOWN = 0x4000, XSG_PAD_LEFT = 0x8000
};
/* XsgMouse.ui_kind: what the rebuilt UI reports under the pointer (gpu_ipc.h GPU_MOUSE_UI_*) */
enum { XSG_UI_TITLE = 1, XSG_UI_BATTLE_CMD = 2, XSG_UI_PROMPT = 3 };

typedef struct XsgMouse {
    int active;      /* mouse play on and the pointer in the window */
    int x, y;        /* the pointer, game px (centre-anchored x) */
    int held;        /* XSG_MB_* held now */
    int pressed;     /* pressed this frame (or a recent press nobody claimed yet) */
    int released;    /* released this frame */
    int moved;       /* the pointer moved lately (and no pad direction since) */
    int wheel;       /* notches this frame, up positive */
    int hover;       /* [mouse] hover: hovering selects */
    int advances;    /* [mouse] click_advances: a left click on nothing is CIRCLE */
    int field;       /* [mouse] field: 0 off, 1 hold, 2 click */
    int run_radius;  /* game px */
    int ui_kind, ui_index;   /* the rebuilt UI's own hit (XSG_UI_*) */
    int over_ui;     /* over a rebuilt UI panel: the scene under the pointer is not pickable */
    int pick;        /* the native renderer: the object under the pointer (actor + 1), 0 none, -1 unknown */
} XsgMouse;

/* This frame's state (updated once per game vblank, on the first call). */
const XsgMouse *xsg_mouse(void);
/* The pointer's x for an element spanning game x0..x1 (the rebuilt UI's anchors). */
int xsg_mouse_x(int x0, int x1);
/* The pointer is inside (x, y, w, h), game px. */
int xsg_mouse_in(int x, int y, int w, int h);
/* A press of `button` this frame not yet claimed by another hook: claims it, returns 1. */
int xsg_mouse_click(int button);
/* Claims this frame's press of `button` without acting on it (no fallback). */
void xsg_mouse_claim(int button);
/* An open menu has the pointer this frame: a left click nobody takes does nothing (no CIRCLE
   fallback) and the field does not walk; hooks still take the click with xsg_mouse_click. */
void xsg_mouse_capture(void);
/* Taps a DS2 button mask through the pad (pressed for two reads, then released). */
void xsg_mouse_tap(unsigned mask);

/* A vertical list's mouse handling: rows (x, y + r * pitch, w, pitch) for r in
   0..visible-1 show items first + r (of count).  Hover (when the pointer moved) and a
   left click select; returns the new selection (sel when none), and *activate = 1 on a
   click on an item (the caller confirms as for CIRCLE; *activate may be 0).  The wheel
   is left to the pad fallback (UP / DOWN: the game scrolls as usual).  A list seen this
   frame captures the pointer (xsg_mouse_capture). */
int xsg_mouse_list(int x, int y, int w, int pitch, int first, int visible, int count, int sel, int *activate);
/* The same for a grid of `cols` columns (w / cols wide): row r shows items
   (first_row + r) * cols + c. */
int xsg_mouse_grid(int x, int y, int w, int pitch, int cols, int first_row, int visible, int count, int sel, int *activate);
/* xsg_mouse_grid for a game cursor: a new selection plays the cursor sound (SE 3), a click on an
   item also taps CIRCLE (the caller's own confirm). */
int xsg_mouse_grid_pick(int x, int y, int w, int pitch, int cols, int first_row, int visible, int count, int sel);
/* Choices laid out as rects {x, y, w, h} (game px; w <= 0: skipped): hover (when the pointer
   moved) and a left click select, a click also taps CIRCLE (the caller's own confirm).  Returns
   the new selection (sel when none); the caller plays its cursor sound on a change.  Captures
   the pointer like a list. */
int xsg_mouse_rects(const short (*rects)[4], int n, int sel);

/* The hand cursor (eCursolMain): where the game drew it this frame (the brightest, on screen;
   of equal ones the frontmost, the lowest sprite depth `work`). */
void xsg_mouse_hand(int x, int y, int alpha, int depth);
/* Last frame's hand, 1 when there was one. */
int xsg_mouse_last_hand(int *x, int *y);

/* The pointer is over something clickable this frame (the hand cursor). */
void xsg_mouse_hot(void);

/* Native renderer object picking: the actor index under the pointer's pixels comes back as
   xsg_mouse()->pick - 1; this names the actors to outline this frame (-1 none). */
void xsg_mouse_outline(int selected_actor, int hovered_actor);
/* The index of an actor record in actor[] (act_1.h), -1 for anything else. */
int xsg_mouse_actor_index(const void *actor);
/* An actor the renderer may outline under the pointer this frame (no read-back delay). */
void xsg_mouse_hoverable(int actor_index);
/* Object ids: the actor whose models are entered next (0 none; act_2.c), the model slot just
   entered (nml_model_set.c), and the actor + 1 of a slot (nml_packet_add.c). */
void xsg_mouse_model_actor(const void *actor);
void xsg_mouse_model_slot(int slot);
int xsg_mouse_model_tag(int slot);

/* Tagging your own 3D objects (a card, a part, a reel): ids XSG_OBJ_FIRST..XSG_OBJ_LAST, above the
   actors.  Bracket the object's nml model entries (nmlModelEntry / nmlModelEntryCard ...) with
   begin(id) / end(); the native renderer then writes the id under its opaque pixels, picks and
   outlines it like an actor (xsg_mouse_outline / xsg_mouse_hoverable take id - 1). */
enum { XSG_OBJ_FIRST = 65, XSG_OBJ_LAST = 255 };
void xsg_mouse_object_begin(int id);
void xsg_mouse_object_end(void);
/* Choices drawn as tagged objects (ids[i], 0: not shown): as battle units.  The object under the
   pointer is outlined (cyan), the current one gold; pointing selects (hover, once the pointer
   moves), a click selects, a click on the current one confirms (taps CIRCLE).  Returns the new
   selection (sel when none).  Needs the native renderer (xsg_mouse()->pick >= 0); captures the
   pointer like a list. */
int xsg_mouse_object_pick(const int *ids, int n, int sel);

/* sdk.c scePadRead: buttons (SCE_PAD mask, active high) the mouse presses this read. */
unsigned xsg_mouse_pad_buttons(void);
/* sdk.c: the keyboard / gamepad moved a cursor: hover waits for the pointer to move again. */
void xsg_mouse_pad_used(void);
/* Field walking (look_at_player.c GameCfPlayerMove): the left stick the mouse asks for,
   1 when it drives (axis -128..127, the game's stick convention). */
int xsg_mouse_field_stick(const float *player_pos, signed char *sx, signed char *sy);
/* The pad walks (or the controls do not take a direction): a click-walk ends. */
void xsg_mouse_field_cancel(void);

#ifdef __cplusplus
}
#endif

#endif
