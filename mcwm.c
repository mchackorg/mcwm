/*
 * mcwm, a small window manager for the X Window System using the X
 * protocol C Binding libraries.
 *
 * For 'user' configurable stuff, see config.h.
 *
 * MC, mc at the domain hack.org
 * http://hack.org/mc/
 *
 * Copyright (c) 2010 Michael Cardell Widerkrantz, mc at the domain hack.org.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>
#include <getopt.h>

#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>
#include <xcb/xcb_atom.h>
#include <xcb/xcb_icccm.h>
#include <X11/keysym.h>

/* Check here for user configurable parts: */
#include "config.h"

#ifdef DMALLOC
#include "dmalloc.h"
#endif

#ifdef DEBUG
#define PDEBUG(Args...) \
  do { fprintf(stderr, "mcwm: "); fprintf(stderr, ##Args); } while(0)
#define D(x) x
#else
#define PDEBUG(Args...)
#define D(x)
#endif


/* Internal Constants. */
#define MCWM_MOVE 2
#define MCWM_RESIZE 3


/* Types. */
typedef enum {
    KEY_H,
    KEY_J,
    KEY_K,
    KEY_L,
    KEY_M,
    KEY_R,
    KEY_RET,
    KEY_X,
    KEY_TAB,
    KEY_MAX
} key_enum_t;
    

/* Globals */
xcb_connection_t *conn;         /* Connection to X server. */
xcb_screen_t *screen;           /* Our current screen.  */
char *terminal = TERMINAL;      /* Path to terminal to start. */
xcb_drawable_t focuswin;        /* Current focus window. */

struct keys
{
    xcb_keysym_t keysym;
    xcb_keycode_t keycode;
} keys[KEY_MAX] =
{
    { USERKEY_MOVE_LEFT, 0 },
    { USERKEY_MOVE_DOWN, 0 },
    { USERKEY_MOVE_UP, 0 },
    { USERKEY_MOVE_RIGHT, 0 },
    { USERKEY_MAXVERT, 0 },
    { USERKEY_RAISE, 0 },
    { USERKEY_TERMINAL, 0 },
    { USERKEY_MAX, 0 },
    { USERKEY_CHANGE, 0 }    
};    

struct conf
{
    bool borders;
} conf;


/* Functions declerations. */
void newwin(xcb_window_t win);
void setupwin(xcb_window_t win);
xcb_keycode_t keysymtokeycode(xcb_keysym_t keysym, xcb_key_symbols_t *keysyms);
int setupkeys(void);
int setupscreen(void);
void raisewindow(xcb_drawable_t win);
void raiseorlower(xcb_drawable_t win);
void movewindow(xcb_drawable_t win, uint16_t x, uint16_t y);
void setunfocus(xcb_drawable_t win);
void setfocus(xcb_drawable_t win);
int start_terminal(void);
void resize(xcb_drawable_t win, uint32_t width, uint32_t height);
void resizestep(xcb_drawable_t win, char direction);
void mousemove(xcb_drawable_t win, int rel_x, int rel_y);
void mouseresize(xcb_drawable_t win, int rel_x, int rel_y);
void movestep(xcb_drawable_t win, char direction);
void maximize(xcb_drawable_t win);
void maxvert(xcb_drawable_t win);
void handle_keypress(xcb_drawable_t win, xcb_key_press_event_t *ev);


/* Function bodies. */

/*
 * Set position, geometry and attributes of a new window and show it
 * on the screen.
 */
void newwin(xcb_window_t win)
{
    xcb_query_pointer_reply_t *pointer;
    xcb_get_geometry_reply_t *geom;
    int x;
    int y;
    xcb_size_hints_t hints;
    int32_t width;
    int32_t height;

    /* Get pointer position so we can move the window to the cursor. */
    pointer = xcb_query_pointer_reply(
        conn, xcb_query_pointer(conn, screen->root), 0);

    if (NULL == pointer)
    {
        x = 0;
        y = 0;
    }
    else
    {
        x = pointer->root_x;
        y = pointer->root_y;

        free(pointer);
    }

    /*
     * Get window geometry so we can check if it fits on the screen
     * where the cursor is.
     */
    geom = xcb_get_geometry_reply(conn,
                                  xcb_get_geometry(conn, win),
                                  NULL);
    if (NULL == geom)
    {
        return;
    }

    /* Get geometry hints. */
    if (!xcb_get_wm_normal_hints_reply(
            conn, xcb_get_wm_normal_hints_unchecked(
                conn, win), &hints, NULL))
    {
        PDEBUG("Couldn't get size hints.");
    }

    if (hints.flags & XCB_SIZE_HINT_P_SIZE)
    {
        width = hints.width;
        height = hints.height;

        /*
         * If the hints don't agree with the window geometry, resize
         * the window to what the hints say.
         */
        if (width != geom->width || height != geom->height)
        {
            resize(win, width, height);
        }
    }
    else
    {
        width = geom->width;
        height = geom->height;
    }

    PDEBUG("Hints say initial size of window: %d x %d (geom: %d x %d)\n",
           width, height, geom->width, geom->height);
    
    /* FIXME: XCB_SIZE_HINT_BASE_SIZE */
    
    /*
     * If the window is larger than our screen, just place it in the
     * corner and resize.
     */
    if (width > screen->width_in_pixels)
    {
        x = 0;
        width = screen->width_in_pixels - BORDERWIDTH * 2;;
        resize(win, width, height);
    }
    else if (x + width + BORDERWIDTH * 2 > screen->width_in_pixels)
    {
        x = screen->width_in_pixels - (width + BORDERWIDTH * 2);
    }

    if (height > screen->height_in_pixels)
    {
        y = 0;
        height = screen->height_in_pixels - BORDERWIDTH * 2;
        resize(win, width, height);
    }
    else if (y + height + BORDERWIDTH * 2 > screen->height_in_pixels)
    {
        y = screen->height_in_pixels - (height + BORDERWIDTH * 2);
    }
    
    /* Move the window to cursor position. */
    movewindow(win, x, y);

    /* Set up stuff and raise the window. */
    setupwin(win);
    raisewindow(win);

    /* Show window on screen. */
    xcb_map_window(conn, win);

    /*
     * Move cursor into the middle of the window so we don't lose the
     * pointer to another window.
     */
    xcb_warp_pointer(conn, XCB_NONE, win, 0, 0, 0, 0,
                     geom->width / 2, geom->height / 2);

    free(geom);
    
    xcb_flush(conn);
}

/* set border colour, width and event mask for window. */
void setupwin(xcb_window_t win)
{
    uint32_t mask = 0;    
    uint32_t values[2];

    if (conf.borders)
    {
        /* Set border color. */
        values[0] = UNFOCUSCOL;
        xcb_change_window_attributes(conn, win, XCB_CW_BORDER_PIXEL, values);

        /* Set border width. */
        values[0] = BORDERWIDTH;
        mask = XCB_CONFIG_WINDOW_BORDER_WIDTH;
        xcb_configure_window(conn, win, mask, values);
    }
    
    mask = XCB_CW_EVENT_MASK;
    values[0] = XCB_EVENT_MASK_ENTER_WINDOW;
    xcb_change_window_attributes_checked(conn, win, mask, values);
    
    /* FIXME: set properties. */

    xcb_flush(conn);
}

xcb_keycode_t keysymtokeycode(xcb_keysym_t keysym, xcb_key_symbols_t *keysyms)
{
    xcb_keycode_t *keyp;
    xcb_keycode_t key;

    /* We only use the first keysymbol, even if there are more. */
    keyp = xcb_key_symbols_get_keycode(keysyms, keysym);
    if (NULL == keyp)
    {
        fprintf(stderr, "mcwm: Couldn't look up key. Exiting.\n");
        exit(1);
        return 0;
    }

    key = *keyp;
    free(keyp);
    
    return key;
}

int setupkeys(void)
{
    xcb_key_symbols_t *keysyms;
    int i;
    
    /* Get all the keysymbols. */
    keysyms = xcb_key_symbols_alloc(conn);

    for (i = KEY_H; i < KEY_MAX; i ++)
    {
        keys[i].keycode = keysymtokeycode(keys[i].keysym, keysyms);        
        if (0 == keys[i].keycode)
        {
            /* Couldn't set up keys! */
    
            /* Get rid of key symbols. */
            free(keysyms);

            return -1;
        }
    }
    
    /* Get rid of the key symbols table. */
    free(keysyms);

    return 0;
}

/* Walk through all existing windows and set them up. */
int setupscreen(void)
{
    xcb_query_tree_reply_t *reply;
    xcb_query_pointer_reply_t *pointer;
    int i;
    int len;
    xcb_window_t *children;
    
    /* Get all children. */
    reply = xcb_query_tree_reply(conn,
                                 xcb_query_tree(conn, screen->root), 0);
    if (NULL == reply)
    {
        return -1;
    }

    len = xcb_query_tree_children_length(reply);    
    children = xcb_query_tree_children(reply);
    
    /* Set up all windows. */
    for (i = 0; i < len; i ++)
    {
        setupwin(children[i]);
    }

    /*
     * Get pointer position so we can set focus on any window which
     * might be under it.
     */
    pointer = xcb_query_pointer_reply(
        conn, xcb_query_pointer(conn, screen->root), 0);

    if (NULL == pointer)
    {
        focuswin = screen->root;
    }
    else
    {
        setfocus(pointer->child);
        free(pointer);
    }

    xcb_flush(conn);
    
    free(reply);

    return 0;
}

void raisewindow(xcb_drawable_t win)
{
    uint32_t values[] = { XCB_STACK_MODE_ABOVE };

    if (screen->root == win || 0 == win)
    {
        return;
    }
    
    xcb_configure_window(conn, win,
                         XCB_CONFIG_WINDOW_STACK_MODE,
                         values);
    xcb_flush(conn);
}

void raiseorlower(xcb_drawable_t win)
{
    uint32_t values[] = { XCB_STACK_MODE_OPPOSITE };

    if (screen->root == win || 0 == win)
    {
        return;
    }
    
    xcb_configure_window(conn, win,
                         XCB_CONFIG_WINDOW_STACK_MODE,
                         values);
    xcb_flush(conn);
}

void movewindow(xcb_drawable_t win, uint16_t x, uint16_t y)
{
    uint32_t values[2];

    if (screen->root == win || 0 == win)
    {
        /* Can't move root. */
        return;
    }
    
    values[0] = x;
    values[1] = y;

    xcb_configure_window(conn, win, XCB_CONFIG_WINDOW_X
                         | XCB_CONFIG_WINDOW_Y, values);
    
    xcb_flush(conn);

}

void setunfocus(xcb_drawable_t win)
{
    uint32_t values[1];
    
    if (focuswin == screen->root || !conf.borders)
    {
        return;
    }

    /* Set new border colour. */
    values[0] = UNFOCUSCOL;
    xcb_change_window_attributes(conn, win, XCB_CW_BORDER_PIXEL, values);

    xcb_flush(conn);
}

void setfocus(xcb_drawable_t win)
{
    uint32_t values[1];

    /*
     * Don't bother focusing on the root window or on the same window
     * that already has focus.
     */
    if (win == screen->root || win == focuswin)
    {
        return;
    }

    if (conf.borders)
    {
        /* Set new border colour. */
        values[0] = FOCUSCOL;
        xcb_change_window_attributes(conn, win, XCB_CW_BORDER_PIXEL, values);

        /* Unset last focus. */
        setunfocus(focuswin);
    }

    /* Set new input focus. */
    focuswin = win;
    xcb_set_input_focus(conn, XCB_INPUT_FOCUS_POINTER_ROOT, win,
                        XCB_CURRENT_TIME);

    xcb_flush(conn);
}

int start_terminal(void)
{
    pid_t pid;

    pid = fork();
    if (-1 == pid)
    {
        perror("fork");
        return -1;
    }
    else if (0 == pid)
    {
        char *argv[2];
        
        /* In the child. */
        
        argv[0] = terminal;
        argv[1] = NULL;
        
        /*
         * Create new process leader, otherwise the terminal will die
         * when wm dies.
         */
        if (-1 == setsid())
        {
            perror("setsid");
            exit(1);
        }

        if (-1 == execvp(terminal, argv))
        {
            perror("execve");            
            exit(1);
        }        
    }

    return 0;
}

void resize(xcb_drawable_t win, uint32_t width, uint32_t height)
{
    uint32_t values[2];

    if (screen->root == win || 0 == win)
    {
        /* Can't resize root. */
        return;
    }
    
    values[0] = width;
    values[1] = height;
                                        
    xcb_configure_window(conn, win,
                         XCB_CONFIG_WINDOW_WIDTH
                         | XCB_CONFIG_WINDOW_HEIGHT, values);
    xcb_flush(conn);
}

void resizestep(xcb_drawable_t win, char direction)
{
    xcb_get_geometry_reply_t *geom;
    int width;
    int height;
    xcb_size_hints_t hints;
    int step_x = MOVE_STEP;
    int step_y = MOVE_STEP;
    
    if (0 == win)
    {
        /* Can't resize root. */
        return;
    }

    raisewindow(win);

    /* Get window geometry. */
    geom = xcb_get_geometry_reply(conn,
                                  xcb_get_geometry(conn, win),
                                  NULL);
    if (NULL == geom)
    {
        return;
    }

    /*
     * Get the window's incremental size step, if any, and use that
     * when resizing.
     */
    if (!xcb_get_wm_normal_hints_reply(
            conn, xcb_get_wm_normal_hints_unchecked(
                conn, win), &hints, NULL))
    {
        PDEBUG("Couldn't get size hints.");
    }

    if (hints.flags & XCB_SIZE_HINT_P_RESIZE_INC)
    {
        if (0 == hints.width_inc || 0 == hints.height_inc)
        {
            PDEBUG("Client lied. No size inc hints.\n");
            step_x = 1;
            step_y = 1;
        }
        else
        {
            step_x = hints.width_inc;
            step_y = hints.height_inc;
        }
    }
    
    switch (direction)
    {
    case 'h':
        width = geom->width - step_x;
        height = geom->height;
        if (width < 0)
        {
            width = 0;
        }
        break;

    case 'j':
        width = geom->width;
        height = geom->height + step_y;
        if (height + geom->y > screen->height_in_pixels)
        {
            goto bad;
        }
        break;

    case 'k':
        width = geom->width;
        height = geom->height - step_y;
        if (height < 0)
        {
            goto bad;
        }
        break;

    case 'l':
        width = geom->width + step_x;
        height = geom->height;
        if (width + geom->x > screen->width_in_pixels)
        {
            goto bad;
        }
        break;

    default:
        PDEBUG("resizestep in unknown direction.\n");
        break;
    } /* switch direction */
           
    resize(win, width, height);

    /*
     * Move cursor into the middle of the window so we don't lose the
     * pointer to another window.
     */
    xcb_warp_pointer(conn, XCB_NONE, win, 0, 0, 0, 0,
                     width / 2, height / 2);
    
    xcb_flush(conn);
    
bad:
    free(geom);
}

void mousemove(xcb_drawable_t win, int rel_x, int rel_y)
{
    xcb_get_geometry_reply_t *geom;    
    int x;
    int y;
    
    /* Get window geometry. */

    geom = xcb_get_geometry_reply(conn,
                                  xcb_get_geometry(conn, win),
                                  NULL);
    if (NULL == geom)
    {
        return;
    }
    
    x = rel_x;
    y = rel_y;
    
    if (x < BORDERWIDTH)
    {
        x = BORDERWIDTH;
    }
    if (y < BORDERWIDTH)
    {
        y = BORDERWIDTH;
    }
    if (y + geom->height + BORDERWIDTH * 2 > screen->height_in_pixels)
    {
        y = screen->height_in_pixels - (geom->height + BORDERWIDTH * 2);
    }
    if (x + geom->width + BORDERWIDTH * 2 > screen->width_in_pixels)
    {
        x = screen->width_in_pixels - (geom->width + BORDERWIDTH * 2);
    }
    
    movewindow(win, x, y);

    free(geom);
}

void mouseresize(xcb_drawable_t win, int rel_x, int rel_y)
{
    xcb_get_geometry_reply_t *geom;
    xcb_size_hints_t hints;
    uint32_t width;
    uint32_t height;
    uint32_t width_inc = 1;
    uint32_t height_inc = 1;

    raisewindow(win);

    /* Get window geometry. */
    geom = xcb_get_geometry_reply(conn,
                                  xcb_get_geometry(conn, win),
                                  NULL);
    if (NULL == geom)
    {
        return;
    }
    
    if (rel_x - geom->x <= 1)
    {
        return;
    }

    if (rel_y - geom->y <= 1)
    {
        return;
    }

    width = rel_x - geom->x;
    height = rel_y - geom->y;

    /*
     * Get the window's incremental size step, if any, and use that
     * when resizing.
     */
    if (!xcb_get_wm_normal_hints_reply(
            conn, xcb_get_wm_normal_hints_unchecked(conn, win), &hints, NULL))
    {
        PDEBUG("Couldn't get size hints.");

    }
    if (hints.flags & XCB_SIZE_HINT_P_RESIZE_INC)
    {
        if (0 == hints.width_inc || 0 == hints.height_inc)
        {
            PDEBUG("The client lied. There is no resize inc here.\n");

            hints.width_inc = 1;
            hints.height_inc = 1;
        }
        else
        {
            width_inc = hints.width_inc;
            height_inc = hints.height_inc;

            if (0 != width % width_inc)
            {
                width -= width % width_inc;
            }

            if (0 != height % height_inc)
            {        
                height -= height % height_inc;
            }
        }
    }

    if (width > screen->width_in_pixels)
    {
        width = (screen->width_in_pixels - geom->x) / width_inc;
    }
        
    if (height > screen->height_in_pixels)
    {
        height = (screen->height_in_pixels - geom->y) / height_inc;
    }

    PDEBUG("Resizing to %dx%d (%dx%d)\n", width, height,
           width / width_inc,
           height / height_inc);

    resize(win, width, height);

    free(geom);
}
    
void movestep(xcb_drawable_t win, char direction)
{
    xcb_get_geometry_reply_t *geom;
    int x;
    int y;
    int width;
    int height;

    if (0 == win)
    {
        /* Can't move root. */
        return;
    }

    raisewindow(win);
    
    /* Get window geometry. */
    geom = xcb_get_geometry_reply(conn,
                                  xcb_get_geometry(conn, win),
                                  NULL);

    width = geom->width + BORDERWIDTH * 2;
    height = geom->height + BORDERWIDTH * 2;

    if (NULL == geom)
    {
        return;
    }
    
    switch (direction)
    {
    case 'h':
        x = geom->x - MOVE_STEP;
        if (x < 0)
        {
            x = 0;
        }

        movewindow(win, x, geom->y);
        break;

    case 'j':
        y = geom->y + MOVE_STEP;
        if (y + height > screen->height_in_pixels)
        {
            y = screen->height_in_pixels - height;
        }
        movewindow(win, geom->x, y);
        break;

    case 'k':
        y = geom->y - MOVE_STEP;
        if (y < 0)
        {
            y = 0;
        }
        
        movewindow(win, geom->x, y);
        break;

    case 'l':
        x = geom->x + MOVE_STEP;
        if (x + width > screen->width_in_pixels)
        {
            x = screen->width_in_pixels - width;
        }

        movewindow(win, x, geom->y);
        break;

    default:
        PDEBUG("movestep: Moving in unknown direction.\n");
        break;
    } /* switch direction */

    /* Move cursor into the middle of the window after moving. */
    xcb_warp_pointer(conn, XCB_NONE, win, 0, 0, 0, 0,
                     width / 2, height / 2);
    
    xcb_flush(conn);
    
    free(geom);
}

void maximize(xcb_drawable_t win)
{
    xcb_get_geometry_reply_t *geom;
    uint32_t values[2];
    uint32_t mask = 0;    

    if (screen->root == win || 0 == win)
    {
        return;
    }
    
    /* FIXME: Check if maximized already. If so, revert to stored geometry. */

    /* Get window geometry. */
    geom = xcb_get_geometry_reply(conn,
                                  xcb_get_geometry(conn, win),
                                  NULL);
    if (NULL == geom)
    {
        return;
    }

    /* Raise first. Pretty silly to maximize below something else. */
    raisewindow(win);
    
    /* FIXME: Store original geom in property. */

    /* Remove borders. */
    values[0] = 0;
    mask = XCB_CONFIG_WINDOW_BORDER_WIDTH;
    xcb_configure_window(conn, win, mask, values);
    
    /* Move to top left. */
    values[0] = 0;
    values[1] = 0;

    xcb_configure_window(conn, win, XCB_CONFIG_WINDOW_X
                         | XCB_CONFIG_WINDOW_Y, values);

    /* Then resize. */
    resize(win, screen->width_in_pixels,
           screen->height_in_pixels);

    free(geom);    
}

void maxvert(xcb_drawable_t win)
{
    xcb_get_geometry_reply_t *geom;
    uint32_t values[2];

    if (screen->root == win || 0 == win)
    {
        return;
    }
    
    /*
     * FIXME: Check if maximized already. If so, revert to stored
     * geometry.
     */

    /* Raise first. Pretty silly to maximize below something else. */
    raisewindow(win);

    /* Get window geometry. */
    geom = xcb_get_geometry_reply(conn,
                                  xcb_get_geometry(conn, win),
                                  NULL);
    if (NULL == geom)
    {
        return;
    }
    
    /* FIXME: Store original geom in property. */

    /* Move to top of screen. */
    values[0] = geom->x;
    values[1] = 0;

    xcb_configure_window(conn, win, XCB_CONFIG_WINDOW_X
                         | XCB_CONFIG_WINDOW_Y, values);

    /* Then resize. */
    resize(win, geom->width, screen->height_in_pixels - BORDERWIDTH * 2);

    free(geom);
}

void handle_keypress(xcb_drawable_t win, xcb_key_press_event_t *ev)
{
    int i;
    key_enum_t key;
    
    for (key = KEY_MAX, i = KEY_H; i < KEY_MAX; i ++)
    {
        if (ev->detail == keys[i].keycode)
        {
            key = i;
        }
    }
    if (key == KEY_MAX)
    {
        PDEBUG("Unknown key pressed.\n");
        return;
    }

    /* Is it shifted? */
    if (ev->state & XCB_MOD_MASK_SHIFT)
    {
        switch (key)
        {
        case KEY_H: /* h */
            resizestep(focuswin, 'h');
            break;

        case KEY_J: /* j */
            resizestep(focuswin, 'j');
            break;

        case KEY_K: /* k */
            resizestep(focuswin, 'k');
            break;

        case KEY_L: /* l */
            resizestep(focuswin, 'l');
            break;
        }
    }
    else
    {
        switch (key)
        {
        case KEY_RET: /* return */
            start_terminal();
            break;

        case KEY_H: /* h */
            movestep(focuswin, 'h');
            break;

        case KEY_J: /* j */
            movestep(focuswin, 'j');
            break;

        case KEY_K: /* k */
            movestep(focuswin, 'k');
            break;

        case KEY_L: /* l */
            movestep(focuswin, 'l');
            break;

        case KEY_TAB: /* tab */
            break;

        case KEY_M: /* m */
            maxvert(focuswin);
            break;

        case KEY_R: /* r*/
            raiseorlower(focuswin);
            break;
                    
        case KEY_X: /* x */
            maximize(focuswin);
            break;
                
        } /* switch unshifted */
    }
} /* handle_keypress() */

void events(void)
{
    xcb_generic_event_t *ev;
    xcb_drawable_t win;
    xcb_get_geometry_reply_t *geom;
    int mode = 0;
    uint16_t mode_x;
    uint16_t mode_y;
    
    for (;;)
    {
        ev = xcb_wait_for_event(conn);
        if (NULL == ev)
        {
            fprintf(stderr, "mcwm: Couldn't get event. Exiting...\n");
            exit(1);
        }
        
        PDEBUG("Event: %d\n", ev->response_type);

        switch (ev->response_type & ~0x80)
        {
        case XCB_MAP_REQUEST:
        {
            xcb_map_request_event_t *e;

            PDEBUG("event: Map request.\n");
            e = (xcb_map_request_event_t *) ev;
            newwin(e->window);
        }
        break;
        
        case XCB_DESTROY_NOTIFY:
        {
            xcb_destroy_notify_event_t *e;

            PDEBUG("event: Destroy notify.\n");
            e = (xcb_destroy_notify_event_t *) ev;

            /* FIXME: Find the window in list of clients. */
        }
        break;
            
        case XCB_BUTTON_PRESS:
        {
            xcb_button_press_event_t *e;
                
            e = ( xcb_button_press_event_t *) ev;
            PDEBUG ("Button %d pressed in window %ld, subwindow %d "
                    "coordinates (%d,%d)\n",
                    e->detail, e->event, e->child, e->event_x, e->event_y);

            if (e->child != 0)
            {
                win = e->child; 
                
                /*
                 * If middle button was pressed, raise window or lower
                 * it if it was already on top.
                 */
                if (2 == e->detail)
                {
                    raiseorlower(win);                    
                }
                else
                {
                    /* We're moving or resizing. */

                    /* Raise window. */
                    raisewindow(win);
                    
                    /* Save the pointer coordinates when starting. */
                    mode_x = e->event_x;
                    mode_y = e->event_y;

                    /* Get window geometry. */
                    geom = xcb_get_geometry_reply(conn, xcb_get_geometry(conn,
                                                                         win),
                                                  NULL);
                    if (NULL == geom)
                    {
                        break;
                    }

                    if (1 == e->detail)
                    {
                        mode = MCWM_MOVE;

                        /*
                         * Warp pointer to upper left of window before
                         * starting move.
                         */
                        xcb_warp_pointer(conn, XCB_NONE, win, 0, 0, 0, 0,
                                         1, 1);
                    }
                    else
                    {
                        mode = MCWM_RESIZE;

                        /* Warp pointer to lower right. */
                        xcb_warp_pointer(conn, XCB_NONE, win, 0, 0, 0, 0,
                                         geom->width, geom->height);
                    }

                    /*
                     * Take control of the pointer in the root window
                     * and confine it to root.
                     *
                     * Give us events when the button is released or
                     * if any motion occurs with the button held down,
                     * but give us only hints about movement. We ask
                     * for the position ourselves later.
                     *
                     * Keep updating everything else.
                     *
                     * Don't use any new cursor.
                     */
                    xcb_grab_pointer(conn, 0, screen->root,
                                     XCB_EVENT_MASK_BUTTON_RELEASE
                                     | XCB_EVENT_MASK_BUTTON_MOTION
                                     | XCB_EVENT_MASK_POINTER_MOTION_HINT, 
                                     XCB_GRAB_MODE_ASYNC,
                                     XCB_GRAB_MODE_ASYNC,
                                     screen->root,
                                     XCB_NONE,
                                     XCB_CURRENT_TIME);

                    xcb_flush(conn);

                    PDEBUG("mode now : %d\n", mode);
                }
            } /* subwindow */
            else
            {
                /*
                 * Do something on the root when mouse buttons are
                 * pressed?
                 */
            }
        }
        break;

        case XCB_MOTION_NOTIFY:
        {
            xcb_query_pointer_reply_t *pointer;
            
            /*
             * Our pointer is moving and since we even get this event
             * we're resizing or moving a window.
             */

            /* Get current pointer position. */
            pointer = xcb_query_pointer_reply(
                conn, xcb_query_pointer(conn, screen->root), 0);

            if (NULL == pointer)
            {
                PDEBUG("Couldn't get pointer position.\n");
                break;
            }

            if (mode == MCWM_MOVE)
            {
                mousemove(win, pointer->root_x, pointer->root_y);
            
            }
            else if (mode == MCWM_RESIZE)
            {
                /* Resize. */
                mouseresize(win, pointer->root_x, pointer->root_y);
            }
            else
            {
                PDEBUG("Motion event when we're not moving our resizing!\n");
            }

            free(pointer);
        }
        break;

        case XCB_BUTTON_RELEASE:
            PDEBUG("Mouse button released! mode = %d\n", mode);

            if (0 != mode)
            {
                xcb_button_release_event_t *e =
                    (xcb_button_release_event_t *)ev;
    
                /* We're finished moving or resizing. */

                xcb_ungrab_pointer(conn, XCB_CURRENT_TIME);
                xcb_flush(conn); /* Important! */
                
                mode = 0;
                free(geom);

                setfocus(e->event);
                
                PDEBUG("mode now = %d\n", mode);
                
            }
        break;
                
        case XCB_KEY_PRESS:
        {
            xcb_key_press_event_t *e = (xcb_key_press_event_t *)ev;
            
            win = e->child;
            
            PDEBUG("Key %d pressed in window %ld\n",
                    e->detail,
                    win);

            handle_keypress(win, e);
        }
        break;

        case XCB_ENTER_NOTIFY:
            PDEBUG("event: Enter notify.\n");
            if (0 == mode)
            {
                xcb_enter_notify_event_t *e = (xcb_enter_notify_event_t *)ev;
                
                /*
                 * If we're entering the same window we focus now,
                 * then don't bother focusing.
                 */
                if (e->event != focuswin)
                {
                    setfocus(e->event);
                }
            }
            break;        
        
        case XCB_CONFIGURE_NOTIFY:
        {
            xcb_configure_notify_event_t *e = (xcb_configure_notify_event_t *)ev;
            
            if (e->window == screen->root)
            {
                /*
                 * When using RANDR, the root can suddenly change
                 * geometry when the user adds a new screen, tilts
                 * their screen 90 degrees or whatnot.
                 *
                 * Since mcwm cares about the root edges, we need to
                 * update our view if this happens.
                 */
                PDEBUG("Notify event for root!\n");

                screen->width_in_pixels = e->width;
                screen->height_in_pixels = e->height;

                PDEBUG("New root geometry: %dx%d\n", e->width, e->height);

                /*
                 * FIXME: If the root suddenly got a lot smaller and
                 * some windows are outside of the root window, we
                 * need to rearrange them to fit the new geometry.
                 */
            }
        }
        break;
        
        case XCB_CONFIGURE_REQUEST:
        {
            xcb_configure_request_event_t *e
                = (xcb_configure_request_event_t *)ev;
            uint32_t mask = 0;
            uint32_t values[7];
            int i = -1;
            
            PDEBUG("event: Configure request.\n");

            /* Check if it's anything we care about, like a resize or move. */

            if (e->value_mask & XCB_CONFIG_WINDOW_X)
            {
                PDEBUG("Changing X coordinate.\n");
                mask |= XCB_CONFIG_WINDOW_X;
                i ++;                
                values[i] = e->x;

            }

            if (e->value_mask & XCB_CONFIG_WINDOW_Y)
            {
                PDEBUG("Changing Y coordinate.\n");                
                mask |= XCB_CONFIG_WINDOW_Y;
                i ++;                
                values[i] = e->y;

            }
            
            if (e->value_mask & XCB_CONFIG_WINDOW_WIDTH)
            {
                PDEBUG("Changing width.\n");
                mask |= XCB_CONFIG_WINDOW_WIDTH;
                i ++;                
                values[i] = e->width;
            }
            
            if (e->value_mask & XCB_CONFIG_WINDOW_HEIGHT)
            {
                PDEBUG("Changing height.\n");                
                mask |= XCB_CONFIG_WINDOW_HEIGHT;
                i ++;                
                values[i] = e->width;
            }
            
#if 0
            /* Still left to decide about: */

            XCB_CONFIG_WINDOW_SIBLING
            XCB_CONFIG_WINDOW_BORDER_WIDTH
                
            if (e->value_mask & XCB_CONFIG_WINDOW_STACK_MODE)
            {
                break;
            }
#endif

            if (-1 != i)
            {
                xcb_configure_window(conn, e->window, mask, values);
                xcb_flush(conn);
            }
        }
        break;
            
        } /* switch */
        
        free(ev);
    }
}

int main(int argc, char **argv)
{
    uint32_t mask = 0;
    uint32_t values[2];
    char ch;                    /* Option character */
    xcb_void_cookie_t cookie;
    xcb_generic_error_t *error;
    xcb_drawable_t root;

    conf.borders = true;
    
    while (1)
    {
        ch = getopt(argc, argv, "bm");
        if (-1 == ch)
        {
                
            /* No more options, break out of while loop. */
            break;
        }
        
        switch (ch)
        {
        case 'b':
            /* No borders. */
            conf.borders = false;
            break;
        }
    }
    
    conn = xcb_connect(NULL, NULL);
    if (xcb_connection_has_error(conn))
    {
        perror("xcb_connect");
        exit(1);
    }
    
    /* Get the first screen */
    screen = xcb_setup_roots_iterator(xcb_get_setup(conn)).data;

    root = screen->root;
    /* Initial focus is root */
    /* FIXME: Ask the X server what window has focus. */
    focuswin = screen->root;
    
    PDEBUG("Screen size: %dx%d\nRoot window: %d\n", screen->width_in_pixels,
           screen->height_in_pixels, screen->root);

    /* FIXME: Get some colours. */
    
    /* Loop over all clients and set up stuff. */
    if (0 != setupscreen())
    {
        fprintf(stderr, "Failed to initialize windows. Exiting.\n");
        exit(1);
    }

    /* Set up key bindings. */
    if (0 != setupkeys())
    {
        fprintf(stderr, "mcwm: Couldn't set up keycodes. Exiting.");
        exit(1);
    }

    /* Grab some keys and mouse buttons. */

    xcb_grab_key(conn, 1, root, MODKEY, XCB_NO_SYMBOL,
                 XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);

    xcb_grab_button(conn, 0, root, XCB_EVENT_MASK_BUTTON_PRESS
                    | XCB_EVENT_MASK_BUTTON_RELEASE,
                    XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC, root, XCB_NONE,
                    1 /* left mouse button */,
                    MOUSEMODKEY);

    xcb_grab_button(conn, 0, root, XCB_EVENT_MASK_BUTTON_PRESS
                    | XCB_EVENT_MASK_BUTTON_RELEASE,
                    XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC, root, XCB_NONE,
                    2 /* middle mouse button */,
                    MOUSEMODKEY);

    xcb_grab_button(conn, 0, root, XCB_EVENT_MASK_BUTTON_PRESS
                    | XCB_EVENT_MASK_BUTTON_RELEASE,
                    XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC, root, XCB_NONE,
                    3 /* right mouse button */,
                    MOUSEMODKEY);

    /* Subscribe to events. */
    mask = XCB_CW_EVENT_MASK;

    values[0] = XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT
        | XCB_EVENT_MASK_STRUCTURE_NOTIFY
        | XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY;

    cookie =
        xcb_change_window_attributes_checked(conn, root, mask, values);
    error = xcb_request_check(conn, cookie);

    xcb_flush(conn);
    
    if (NULL != error)
    {
        fprintf(stderr, "mcwm: Can't get SUBSTRUCTURE REDIRECT. "
                "Error code: %d\n"
                "Another window manager running? Exiting.\n",
                error->error_code);

        xcb_disconnect(conn);
        
        exit(1);
    }

    /* Loop over events. */
    events();

    xcb_disconnect(conn);
        
    exit(0);
}
