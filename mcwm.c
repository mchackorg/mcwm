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
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <getopt.h>
#include <string.h>

#include <sys/types.h>
#include <sys/wait.h>

#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>
#include <xcb/xcb_atom.h>
#include <xcb/xcb_icccm.h>

#include <X11/keysym.h>

#ifdef DEBUG
#include "events.h"
#endif

#include "list.h"

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
    KEY_F,
    KEY_H,
    KEY_J,
    KEY_K,
    KEY_L,
    KEY_M,
    KEY_R,
    KEY_RET,
    KEY_X,
    KEY_TAB,
    KEY_1,
    KEY_2,
    KEY_3,
    KEY_4,
    KEY_5,
    KEY_6,
    KEY_7,
    KEY_8,
    KEY_9,
    KEY_0,
    KEY_MAX
} key_enum_t;

struct client
{
    xcb_drawable_t id;
    int32_t min_width, min_height;
    int32_t max_width, max_height;
    int32_t width_inc, height_inc;
    int32_t base_width, base_height;
    bool fixed;           /* Visible on all workspaces? */
    struct item *winitem; /* Pointer to our place in list of all windows. */
};
    

/* Globals */
xcb_connection_t *conn;         /* Connection to X server. */
xcb_screen_t *screen;           /* Our current screen.  */
int curws = 1;                  /* Current workspace. */
struct client *focuswin;        /* Current focus window. */
struct item *winlist = NULL;
struct item *wslist[10] =
{
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
};

struct keys
{
    xcb_keysym_t keysym;
    xcb_keycode_t keycode;
} keys[KEY_MAX] =
{
    { USERKEY_FIX, 0 },    
    { USERKEY_MOVE_LEFT, 0 },
    { USERKEY_MOVE_DOWN, 0 },
    { USERKEY_MOVE_UP, 0 },
    { USERKEY_MOVE_RIGHT, 0 },
    { USERKEY_MAXVERT, 0 },
    { USERKEY_RAISE, 0 },
    { USERKEY_TERMINAL, 0 },
    { USERKEY_MAX, 0 },    
    { USERKEY_CHANGE, 0, },
    { USERKEY_WS1, 0 },
    { USERKEY_WS2, 0 },
    { USERKEY_WS3, 0 },
    { USERKEY_WS4, 0 },
    { USERKEY_WS5, 0 },
    { USERKEY_WS6, 0 },
    { USERKEY_WS7, 0 },
    { USERKEY_WS8, 0 },
    { USERKEY_WS9, 0 },
    { USERKEY_WS10, 0 }
};    

struct conf
{
    bool borders;
    char *terminal; /* Path to terminal to start. */
    uint32_t focuscol;
    uint32_t unfocuscol;
} conf;


/* Functions declerations. */
void addtoworkspace(struct client *client, int ws);
void delfromworkspace(struct client *client, int ws);
void changeworkspace(int ws);
void fixwindow(struct client *client);
uint32_t getcolor(const char *colstr);
void forgetwin(xcb_window_t win);
void newwin(xcb_window_t win);
struct client *setupwin(xcb_window_t win,
                        xcb_get_window_attributes_reply_t *attr);
xcb_keycode_t keysymtokeycode(xcb_keysym_t keysym, xcb_key_symbols_t *keysyms);
int setupkeys(void);
int setupscreen(void);
void raisewindow(xcb_drawable_t win);
void raiseorlower(xcb_drawable_t win);
void movewindow(xcb_drawable_t win, uint16_t x, uint16_t y);
struct client *findclient(xcb_drawable_t win);
void focusnext(void);
void setunfocus(xcb_drawable_t win);
void setfocus(struct client *client);
int start_terminal(void);
void resize(xcb_drawable_t win, uint32_t width, uint32_t height);
void resizestep(xcb_drawable_t win, char direction);
void mousemove(xcb_drawable_t win, int rel_x, int rel_y);
void mouseresize(xcb_drawable_t win, int rel_x, int rel_y);
void movestep(xcb_drawable_t win, char direction);
void maximize(xcb_drawable_t win);
void maxvert(xcb_drawable_t win);
void handle_keypress(xcb_drawable_t win, xcb_key_press_event_t *ev);
void printhelp(void);


/* Function bodies. */

void addtoworkspace(struct client *client, int ws)
{
    struct item *item;
    
    item = additem(&wslist[ws]);
    if (NULL == item)
    {
        PDEBUG("addtoworkspace: Out of memory.\n");
        return;
    }

    item->data = client;
}

void delfromworkspace(struct client *client, int ws)
{
    struct item *item;

    /* Find client in list. */
    for (item = wslist[ws]; item != NULL; item = item->next)
    {
        if (client == item->data)
        {
            delitem(&wslist[ws], item);
        }
    }
}

void changeworkspace(int ws)
{
    struct item *item;
    struct client *client;

    if (ws == curws)
    {
        PDEBUG("Changing to same workspace!\n");
        return;
    }

    PDEBUG("Changing from workspace #%d to #%d\n", curws, ws);
    
    /* Go through list of current ws. Unmap everything that isn't fixed. */
    for (item = wslist[curws]; item != NULL; item = item->next)
    {
        client = item->data;
        if (!client->fixed)
        {
            xcb_unmap_window(conn, client->id);
        }
    }
    
    /* Go through list of new ws. Map everything that isn't fixed. */
    for (item = wslist[ws]; item != NULL; item = item->next)
    {
        client = item->data;
        if (!client->fixed)
        {
            xcb_map_window(conn, client->id);
        }
    }

    xcb_flush(conn);

    curws = ws;
}

void fixwindow(struct client *client)
{
    if (client->fixed)
    {
        client->fixed = false;

        addtoworkspace(client, curws);
    }
    else
    {
        client->fixed = true;
        delfromworkspace(client, curws);
    }
}

    
uint32_t getcolor(const char *colstr)
{
    xcb_alloc_named_color_reply_t *col_reply;    
    xcb_colormap_t colormap; 
    xcb_generic_error_t *error;
    xcb_alloc_named_color_cookie_t colcookie;

    colormap = screen->default_colormap;

    colcookie = xcb_alloc_named_color(conn, colormap, strlen(colstr), colstr);
    
    col_reply = xcb_alloc_named_color_reply(conn, colcookie, &error);
    if (NULL != error)
    {
        fprintf(stderr, "mcwm: Couldn't get pixel value for colour %s. "
                "Exiting.\n", colstr);

        xcb_disconnect(conn);
        exit(1);
    }

    return col_reply->pixel;
}

void forgetwin(xcb_window_t win)
{
    struct item *item;
    struct client *client;

    for (item = winlist; item != NULL; item = item->next)
    {
        client = item->data;

        PDEBUG("Win %d == client ID %d\n", win, client->id);
        if (win == client->id)
        {
            /* Found it. */
            PDEBUG("Found it. Forgetting...\n");
          
            free(item->data);

            delitem(&winlist, item);
            return;
        }
    }
}

/*
 * Set position, geometry and attributes of a new window and show it
 * on the screen.
 */
void newwin(xcb_window_t win)
{
    xcb_query_pointer_reply_t *pointer;
    int x;
    int y;
    int32_t width;
    int32_t height;
    xcb_get_window_attributes_reply_t *attr;
    xcb_get_geometry_reply_t *geom;
    struct client *client;
    
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
     * Set up stuff, like borders, add the window to the client list,
     * et cetera.
     */
    attr = xcb_get_window_attributes_reply(
        conn, xcb_get_window_attributes(conn, win), NULL);

    if (!attr)
    {
        fprintf(stderr, "Couldn't get attributes for window %d.", win);
        return;
    }
    client = setupwin(win, attr);
    
    if (NULL == client)
    {
        fprintf(stderr, "mcwm: Couldn't set up window. Out of memory.\n");
        return;
    }

    geom = xcb_get_geometry_reply(conn,
                                  xcb_get_geometry(conn, win),
                                  NULL);
    if (NULL == geom)
    {
        fprintf(stderr, "mcwm: Couldn't get geometry for win %d.\n", win);
        return;
    }

    width = geom->width;
    height = geom->height;
    
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
    
    /* Show window on screen. */
    xcb_map_window(conn, win);

    /*
     * Move cursor into the middle of the window so we don't lose the
     * pointer to another window.
     */
    xcb_warp_pointer(conn, XCB_NONE, win, 0, 0, 0, 0,
                     geom->width / 2, geom->height / 2);
    
    xcb_flush(conn);

    free(geom);
}

/* set border colour, width and event mask for window. */
struct client *setupwin(xcb_window_t win,
                        xcb_get_window_attributes_reply_t *attr)
{
    uint32_t mask = 0;    
    uint32_t values[2];

    struct item *item;
    struct client *client;
    
    if (conf.borders)
    {
        /* Set border color. */
        values[0] = conf.unfocuscol;
        xcb_change_window_attributes(conn, win, XCB_CW_BORDER_PIXEL, values);

        /* Set border width. */
        values[0] = BORDERWIDTH;
        mask = XCB_CONFIG_WINDOW_BORDER_WIDTH;
        xcb_configure_window(conn, win, mask, values);
    }
    
    mask = XCB_CW_EVENT_MASK;
    values[0] = XCB_EVENT_MASK_ENTER_WINDOW;
    xcb_change_window_attributes_checked(conn, win, mask, values);

    xcb_flush(conn);
    
    /* Remember window and store a few things about it. */
    
    item = additem(&winlist);
    
    if (NULL == item)
    {
        PDEBUG("newwin: Out of memory.\n");
        return NULL;
    }

    client = malloc(sizeof (struct client));
    if (NULL == client)
    {
        PDEBUG("newwin: Out of memory.\n");
        return NULL;
    }

    item->data = client;

    client->id = win;
    PDEBUG("Adding window %d\n", client->id);
    client->winitem = item;

    /* Add this window to the current workspace. */
    addtoworkspace(client, curws);
    
    return client;
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

    for (i = KEY_F; i < KEY_MAX; i ++)
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
    xcb_get_window_attributes_reply_t *attr;
     
    /* Get all children. */
    reply = xcb_query_tree_reply(conn,
                                 xcb_query_tree(conn, screen->root), 0);
    if (NULL == reply)
    {
        return -1;
    }

    len = xcb_query_tree_children_length(reply);    
    children = xcb_query_tree_children(reply);
    
    /* Set up all windows on this root. */
    for (i = 0; i < len; i ++)
    {
        attr = xcb_get_window_attributes_reply(
            conn, xcb_get_window_attributes(conn, children[i]), NULL);

        if (!attr)
        {
            fprintf(stderr, "Couldn't get attributes for window %d.",
                    children[i]);
            continue;
        }
        
        /*
         * Don't set up or even bother windows in override redirect
         * mode.
         *
         * This mode means they wouldn't have been reported to us
         * with a MapRequest if we had been running, so in the
         * normal case we wouldn't have seen them.
         *
         * Usually, this mode is used for menu windows and the
         * like.
         *
         * We don't care for any unmapped windows either. If they get
         * unmapped later, we handle them when we get a MapRequest.
         */    
        if (!attr->override_redirect &&
            XCB_MAP_STATE_UNMAPPED != attr->map_state)
        {
            setupwin(children[i], attr);
        }
        
        free(attr);
    } /* for */

    /*
     * Get pointer position so we can set focus on any window which
     * might be under it.
     */
    pointer = xcb_query_pointer_reply(
        conn, xcb_query_pointer(conn, screen->root), 0);

    if (NULL == pointer)
    {
        focuswin = NULL;
    }
    else
    {
        setfocus(findclient(pointer->child));
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

void focusnext(void)
{
    struct client *client;
    bool found = false;
    struct item *item;
    
#if DEBUG
    if (NULL != focuswin)
    {
        PDEBUG("Focus now in win %d\n", focuswin->id);
    }
#endif
    
    /* If we currently focus the root, focus first in list. */
    if (NULL == focuswin)
    {
        if (NULL == wslist[curws])
        {
            PDEBUG("No windows to focus on.\n");
            return;
        }
        
        client = wslist[curws]->data;
        found = true;
    }
    else
    {
        /* Find client in list. */
        /* FIXME: We need special treatment for fixed windows. */
        for (item = wslist[curws]; item != NULL; item = item->next)
        {
            if (focuswin == item->data)
            {
                if (NULL != item->next)
                {
                    client = item->next->data;
                    found = true;            
                }
                else
                {
                    /*
                     * We were at the end of list. Focusing on first window in
                     * list instead.
                     */
                    client = wslist[curws]->data;
                    found = true;
                }
            }
        }
    }

    if (found)
    {
        raisewindow(client->id);
        xcb_warp_pointer(conn, XCB_NONE, client->id, 0, 0, 0, 0,
                         0, 0);
        setfocus(client);
    }
    else
    {
        PDEBUG("Couldn't find any new window to focus on.\n");
    }
}

void setunfocus(xcb_drawable_t win)
{
    uint32_t values[1];

    if (NULL == focuswin)
    {
        return;
    }
    
    if (focuswin->id == screen->root || !conf.borders)
    {
        return;
    }

    /* Set new border colour. */
    values[0] = conf.unfocuscol;
    xcb_change_window_attributes(conn, win, XCB_CW_BORDER_PIXEL, values);

    xcb_flush(conn);
}

struct client *findclient(xcb_drawable_t win)
{
    struct item *item;
    struct client *client;

    for (item = winlist; item != NULL; item = item->next)
    {
        client = item->data;
        if (win == client->id)
        {
            PDEBUG("findclient: Found it. Win: %d\n", client->id);
            return client;
        }
    }

    return NULL;
}

void setfocus(struct client *client)
{
    uint32_t values[1];

    /* if client is NULL, we focus on the root. */
    if (NULL == client)
    {
        focuswin = NULL;

        xcb_set_input_focus(conn, XCB_INPUT_FOCUS_POINTER_ROOT, screen->root,
                            XCB_CURRENT_TIME);
        xcb_flush(conn);
        
        return;
    }
    
    /*
     * Don't bother focusing on the root window or on the same window
     * that already has focus.
     */
    if (client->id == screen->root || client == focuswin)
    {
        return;
    }

    if (conf.borders)
    {
        /* Set new border colour. */
        values[0] = conf.focuscol;
        xcb_change_window_attributes(conn, client->id, XCB_CW_BORDER_PIXEL,
                                     values);

        /* Unset last focus. */
        if (NULL != focuswin)
        {
            setunfocus(focuswin->id);
        }
    }

    /* Set new input focus. */

    xcb_set_input_focus(conn, XCB_INPUT_FOCUS_POINTER_ROOT, client->id,
                        XCB_CURRENT_TIME);

    xcb_flush(conn);

    focuswin = client;
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
        pid_t termpid;

        /* In our first child. */

        /*
         * Make this process a new process leader, otherwise the
         * terminal will die when the wm dies. Also, this makes any
         * SIGCHLD go to this process when we fork again.
         */
        if (-1 == setsid())
        {
            perror("setsid");
            exit(1);
        }
        
        /*
         * Fork again for the terminal process. This way, the wm won't
         * know anything about it.
         */
        termpid = fork();
        if (-1 == termpid)
        {
            perror("fork");
            exit(1);
        }
        else if (0 == termpid)
        {
            char *argv[2];
        
            /* In the second child, now starting terminal. */
        
            argv[0] = conf.terminal;
            argv[1] = NULL;

            if (-1 == execvp(conf.terminal, argv))
            {
                perror("execve");            
                exit(1);
            }
        } /* second child */

        /* Exit our first child so the wm can pick up and continue. */
        exit(0);
    } /* first child */
    else
    {
        /* Wait for the first forked process to exit. */
        waitpid(pid, NULL, 0);
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
     *
     * FIXME: Do this right. ICCCM v2.0, 4.1.2.3. WM_NORMAL_HINTS
     * Property says:
     *
     *   The base_width and base_height elements in conjunction with
     *   width_inc and height_inc define an arithmetic progression of
     *   preferred window widths and heights for nonnegative integers
     *   i and j:
     *
     *   width = base_width + (i × width_inc)
     *   height = base_height + (j × height_inc)
     *
     *   Window managers are encouraged to use i and j instead of
     *   width and height in reporting window sizes to users. If a
     *   base size is not provided, the minimum size is to be used in
     *   its place and vice versa.
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
    
    for (key = KEY_MAX, i = KEY_F; i < KEY_MAX; i ++)
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
            resizestep(focuswin->id, 'h');
            break;

        case KEY_J: /* j */
            resizestep(focuswin->id, 'j');
            break;

        case KEY_K: /* k */
            resizestep(focuswin->id, 'k');
            break;

        case KEY_L: /* l */
            resizestep(focuswin->id, 'l');
            break;

        default:
            /* Ignore other shifted keys. */
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

        case KEY_F: /* f */
            fixwindow(focuswin);
            break;
            
        case KEY_H: /* h */
            movestep(focuswin->id, 'h');
            break;

        case KEY_J: /* j */
            movestep(focuswin->id, 'j');
            break;

        case KEY_K: /* k */
            movestep(focuswin->id, 'k');
            break;

        case KEY_L: /* l */
            movestep(focuswin->id, 'l');
            break;

        case KEY_TAB: /* tab */
            focusnext();
            break;

        case KEY_M: /* m */
            maxvert(focuswin->id);
            break;

        case KEY_R: /* r*/
            raiseorlower(focuswin->id);
            break;
                    
        case KEY_X: /* x */
            maximize(focuswin->id);
            break;

        case KEY_1:
            changeworkspace(1);
            break;
            
        case KEY_2:
            changeworkspace(2);            
            break;

        case KEY_3:
            changeworkspace(3);            
            break;

        case KEY_4:
            changeworkspace(4);            
            break;

        case KEY_5:
            changeworkspace(5);            
            break;

        case KEY_6:
            changeworkspace(6);            
            break;

        case KEY_7:
            changeworkspace(7);            
            break;

        case KEY_8:
            changeworkspace(8);            
            break;

        case KEY_9:
            changeworkspace(9);            
            break;

        case KEY_0:
            changeworkspace(0);            
            break;
            
        default:
            /* Ignore other keys. */
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

#ifdef DEBUG
        if (ev->response_type <= MAXEVENTS)
        {
            PDEBUG("Event: %s\n", evnames[ev->response_type]);
        }
        else
        {
            PDEBUG("Event: #%d. Not known.\n", ev->response_type);
        }
#endif

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

            e = (xcb_destroy_notify_event_t *) ev;

            /*
             * If we had focus in this window, forget about the focus.
             * We will get an EnterNotify if there's another window
             * under the pointer so we can set the focus proper later.
             */
            if (NULL != focuswin)
            {
                if (focuswin->id == e->window)
                {
                    focuswin = NULL;
                }
            }
            
            /*
             * Find this window in list of clients and forget about
             * it.
             */
            forgetwin(e->window);
#if DEBUG
            listitems(winlist);
#endif
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

                setfocus(findclient(e->event));
                
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
        {
            xcb_enter_notify_event_t *e = (xcb_enter_notify_event_t *)ev;
            struct client *client;
            
            PDEBUG("event: Enter notify eventwin %d child %d.\n",
                   e->event,
                   e->child);

            /*
             * If this isn't a normal enter notify, don't bother.
             *
             * The other cases means the pointer is grabbed and that
             * either means someone is using it for menu selections or
             * that we're moving or resizing. We don't want to change
             * focus in these cases.
             *
             */
            if (e->mode == XCB_NOTIFY_MODE_NORMAL
                || e->mode == XCB_NOTIFY_MODE_UNGRAB)
            {
                /*
                 * If we're entering the same window we focus now,
                 * then don't bother focusing.
                 */
                if (NULL == focuswin || e->event != focuswin->id)
                {
                    /*
                     * Otherwise, set focus to the window we just entered.
                     */
                    client = findclient(e->event);
                    setfocus(client);
                }
            }
        }
        break;        
        
        case XCB_CONFIGURE_NOTIFY:
        {
            xcb_configure_notify_event_t *e
                = (xcb_configure_notify_event_t *)ev;
            
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
            
            PDEBUG("event: Configure request. mask = %d\n", e->value_mask);

            /* Check if it's anything we care about, like a resize or move. */

            if (e->value_mask & XCB_CONFIG_WINDOW_X)
            {
                PDEBUG("Changing X coordinate to %d\n", e->x);
                mask |= XCB_CONFIG_WINDOW_X;
                i ++;                
                values[i] = e->x;

            }

            if (e->value_mask & XCB_CONFIG_WINDOW_Y)
            {
                PDEBUG("Changing Y coordinate to %d.\n", e->y);
                mask |= XCB_CONFIG_WINDOW_Y;
                i ++;                
                values[i] = e->y;

            }
            
            if (e->value_mask & XCB_CONFIG_WINDOW_WIDTH)
            {
                PDEBUG("Changing width to %d.\n", e->width);
                mask |= XCB_CONFIG_WINDOW_WIDTH;
                i ++;                
                values[i] = e->width;
            }
            
            if (e->value_mask & XCB_CONFIG_WINDOW_HEIGHT)
            {
                PDEBUG("Changing height to %d.\n", e->height);
                mask |= XCB_CONFIG_WINDOW_HEIGHT;
                i ++;                
                values[i] = e->height;
            }

            /* We handle a request to change the border width, but
             * only change it to what we think is right.
             */
            if (e->value_mask & XCB_CONFIG_WINDOW_BORDER_WIDTH)
            {
                PDEBUG("Changing width to %d, but not really.\n",
                       e->border_width);
                mask |= XCB_CONFIG_WINDOW_BORDER_WIDTH;
                i ++;                
                values[i] = BORDERWIDTH;
            }

            if (e->value_mask & XCB_CONFIG_WINDOW_SIBLING)
            {
                mask |= XCB_CONFIG_WINDOW_SIBLING;
                i ++;                
                values[i] = e->sibling;
            }

            if (e->value_mask & XCB_CONFIG_WINDOW_STACK_MODE)
            {
                PDEBUG("Changing stack order.\n");
                mask |= XCB_CONFIG_WINDOW_STACK_MODE;
                i ++;                
                values[i] = e->stack_mode;
            }

            if (-1 != i)
            {
                xcb_configure_window(conn, e->window, mask, values);
                xcb_flush(conn);
            }
        }
        break;

        case XCB_CIRCULATE_REQUEST:
        {
            xcb_circulate_request_event_t *e
                = (xcb_circulate_request_event_t *)ev;

            /*
             * Subwindow e->window to parent e->event is about to be
             * restacked.
             *
             * Just do what was requested, e->place is either
             * XCB_PLACE_ON_TOP or _ON_BOTTOM. We don't care.
             */
            xcb_circulate_window(conn, e->window, e->place);
            
        }
        break;
        
        } /* switch */
        
        free(ev);
    }
}

void printhelp(void)
{
    printf("mcwm: Usage: mcwm [-b] [-t terminal-program] [-f color] "
           "[- u color]\n");
    printf("  -b means draw no borders\n");
    printf("  -t urxvt will start urxvt when MODKEY + Return is pressed\n");
    printf("  -f color sets colour for focused window borders of focused "
           "to a named color.\n");
    printf("  -u color sets colour for unfocused window borders.");
}

int main(int argc, char **argv)
{
    uint32_t mask = 0;
    uint32_t values[2];
    char ch;                    /* Option character */
    xcb_void_cookie_t cookie;
    xcb_generic_error_t *error;
    xcb_drawable_t root;
    char *focuscol;
    char *unfocuscol;

    /* Set up defaults. */
    
    conf.borders = true;
    conf.terminal = TERMINAL;
    focuscol = FOCUSCOL;
    unfocuscol = UNFOCUSCOL;
    
    while (1)
    {
        ch = getopt(argc, argv, "bt:f:u:");
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

        case 't':
            conf.terminal = optarg;
            break;

        case 'f':
            focuscol = optarg;
            break;

        case 'u':
            unfocuscol = optarg;
            break;
            
        default:
            printhelp();
            exit(0);
        } /* switch */
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
    
    PDEBUG("Screen size: %dx%d\nRoot window: %d\n", screen->width_in_pixels,
           screen->height_in_pixels, screen->root);
    
    /* Get some colours. */
    conf.focuscol = getcolor(focuscol);
    conf.unfocuscol = getcolor(unfocuscol);
    
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
