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
 *
 * Can be set from command line with "-t program".
 */
#define TERMINAL "urxvt"

/*
 * Default colour on border for focused windows. Can be set from
 * command line with "-f color".
 */
#define FOCUSCOL "chocolate1"

/* Ditto for unfocused. Use "-u color". */
#define UNFOCUSCOL "grey40"

#define FIXEDCOL "steelblue"

/* Width of border window, in pixels. */
#define BORDERWIDTH 1

/*
 * Keysym codes for window operations. Look in X11/keysymdefs.h for
 * actual symbols.
 */
#define USERKEY_FIX 		XK_F
#define USERKEY_MOVE_LEFT 	XK_H
#define USERKEY_MOVE_DOWN 	XK_J
#define USERKEY_MOVE_UP 	XK_K
#define USERKEY_MOVE_RIGHT 	XK_L
#define USERKEY_MAXVERT 	XK_M
#define USERKEY_RAISE 		XK_R
#define USERKEY_TERMINAL 	XK_Return
#define USERKEY_MAX 		XK_X
#define USERKEY_CHANGE 		XK_Tab
#define USERKEY_WS1		XK_1
#define USERKEY_WS2		XK_2
#define USERKEY_WS3		XK_3
#define USERKEY_WS4		XK_4
#define USERKEY_WS5		XK_5
#define USERKEY_WS6		XK_6
#define USERKEY_WS7		XK_7
#define USERKEY_WS8		XK_8
#define USERKEY_WS9		XK_9
#define USERKEY_WS10		XK_0
