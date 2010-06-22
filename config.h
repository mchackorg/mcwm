/* User configurable stuff. */

/*
 * Move this many pixels when moving or resizing with keyboard unless
 * the window has hints saying otherwise.
 */
#define MOVE_STEP 32

/*
 * Use this modifier combined with other keys to control wm from
 * keyboard. Default is Mod2, which on my keyboard is the Alt/Windows
 * key.
 */
#define MODKEY XCB_MOD_MASK_2

/* Extra modifier for resizing. Default is Shift. */
#define SHIFTMOD XCB_MOD_MASK_SHIFT

/*
 * Modifier key to use with mouse buttons. Default Mod1, Meta on my
 * keyboard.
 */
#define MOUSEMODKEY XCB_MOD_MASK_1

/*
 * Start this program when pressing MODKEY + USERKEY_TERMINAL. Needs
 * to be in $PATH.
 *
 * Change to "xterm" if you're feeling conservative.
 */
#define TERMINAL "urxvt"

/* Colour on border for focused windows. */

/*
 * FIXME: We blatantly ignore displays that doesn't handle direct
 * colour values. Sorry.
 */

/* very light gray. */
/* #define FOCUSCOL 0xe5e5e5 */
/* amber */
#define FOCUSCOL 0xff7f24

/* Ditto for unfocused. */
#define UNFOCUSCOL 0x666666

/* Width of border window, in pixels. */
#define BORDERWIDTH 1

/*
 * Keysym codes for window operations. Look in X11/keysymdefs.h for
 * actual symbols.
 */
#define USERKEY_MOVE_LEFT 	XK_H
#define USERKEY_MOVE_DOWN 	XK_J
#define USERKEY_MOVE_UP 	XK_K
#define USERKEY_MOVE_RIGHT 	XK_L
#define USERKEY_MAXVERT 	XK_M
#define USERKEY_RAISE 		XK_R
#define USERKEY_TERMINAL 	XK_Return
#define USERKEY_MAX 		XK_X
#define USERKEY_CHANGE 		XK_Tab
