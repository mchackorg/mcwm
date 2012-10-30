/*
 * mcmenu - small menu program using only XCB.
 *
 * MC, mc at the domain hack.org
 * http://hack.org/mc/
 *
 * Copyright (c) 2010, 2011, 2012 Michael Cardell Widerkrantz, mc at
 * the domain hack.org.
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

#include <sys/select.h>

#include <X11/keysym.h>

#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/*
 * FIXME calculate height and width.
 * How? We get font size *after* opening a window! Resize afterwards?
 *
 * Can I ask a font about information before using setfont()? I need
 * pixel sizes.
 * 
 */
#define WIDTH 100
#define HEIGHT 200

xcb_connection_t *conn;
xcb_screen_t *screen;

int sigcode;                    /* Signal code. Non-zero if we've been
                                 * interruped by a signal. */
xcb_keycode_t keycode_j;
xcb_keycode_t keycode_k;
xcb_keycode_t keycode_ret;

struct font
{
    char *name;
    /* width, height. */
};
    
struct window
{
    xcb_window_t id;
    xcb_gc_t curfontc;
};

static uint32_t getcolor(const char *);
static int setfont(struct window *, const char *, const char *, const char *);
static void printat(struct window *, int16_t, int16_t, const char *);
static struct window *window(int16_t, int16_t, uint16_t, uint16_t);
static void init(void);
static void cleanup(void);
static xcb_keycode_t keysymtokeycode(xcb_keysym_t, xcb_key_symbols_t *);
static void grabkeys(struct window *);
static void printrows(struct window *, char *[], int, int);
static void invertrow(struct window *, char *[], int, int);
static void normalrow(struct window *, char *[], int, int);
static int keypress(xcb_key_press_event_t *, struct window *, char *[], int,
                    int, int);
static void redraw(struct window *, char *[], int, int, int);

/*
 * Get the pixel values of a named colour colstr.
 *
 * Returns pixel values.
 */
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

int setfont(struct window *win, const char *fontname, const char *fg,
            const char *bg)
{
    uint32_t fgcol;          /* Focused border colour. */
    uint32_t bgcol;          /* Focused border colour. */
    xcb_font_t font;
    xcb_gcontext_t gc;
    uint32_t mask;
    uint32_t values[3];
    
    fgcol = getcolor(fg);
    bgcol = getcolor(bg);

    font = xcb_generate_id(conn);
    xcb_open_font(conn, font, strlen(fontname), fontname);

    gc = xcb_generate_id(conn);
    mask = XCB_GC_FOREGROUND | XCB_GC_BACKGROUND | XCB_GC_FONT;
    values[0] = fgcol;
    values[1] = bgcol;
    values[2] = font;
    xcb_void_cookie_t cookie = xcb_create_gc_checked(conn, gc, win->id, mask,
                                                     values);

    xcb_generic_error_t *error = xcb_request_check(conn, cookie);
    if (error)
    {
        fprintf(stderr, "ERROR: can't print text : %d\n", error->error_code);
        xcb_disconnect(conn);
        exit(-1);
    }
    
    xcb_close_font(conn, font);
    
    win->curfontc = gc;

    return 0;
}

void printat(struct window *win, int16_t x, int16_t y, const char *text)
{
    xcb_void_cookie_t cookie;
    xcb_generic_error_t *error;

    cookie = xcb_image_text_8_checked(conn, strlen(text),
                                      win->id, win->curfontc, x, y, text);

    error = xcb_request_check(conn, cookie);
    
    if (error)
    {
        fprintf(stderr, "ERROR: can't print text : %d\n", error->error_code);
        xcb_disconnect(conn);
        exit(-1);
    }
    
    xcb_flush(conn);
}

struct window *window(int16_t x, int16_t y, uint16_t width, uint16_t height)
{
    struct window *win;
    xcb_void_cookie_t cookie;
    uint32_t mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
    uint32_t values[2];

    win = malloc(sizeof (struct window));
    if (NULL == win)
    {
        perror("malloc");
        return NULL;
    }

    values[0] = screen->black_pixel;
    values[1] = XCB_EVENT_MASK_KEY_RELEASE |
        XCB_EVENT_MASK_BUTTON_PRESS |
        XCB_EVENT_MASK_EXPOSURE |
        XCB_EVENT_MASK_POINTER_MOTION;

    win->id = xcb_generate_id(conn);

    cookie = xcb_create_window(conn, screen->root_depth, win->id, screen->root,
                               x, y, 
                               width, height,
                               0, XCB_WINDOW_CLASS_INPUT_OUTPUT,
                               screen->root_visual,
                               mask, values);

    xcb_map_window(conn, win->id);

    xcb_flush(conn);

    return win;
}

void init(void)
{
    int scrno;
    xcb_screen_iterator_t iter;
    
    conn = xcb_connect(NULL, &scrno);
    if (!conn)
    {
        fprintf(stderr, "can't connect to an X server\n");
        exit(1);
    }

    iter = xcb_setup_roots_iterator(xcb_get_setup(conn));

    for (int i = 0; i < scrno; ++i)
    {
        xcb_screen_next(&iter);
    }

    screen = iter.data;

    if (!screen)
    {
        fprintf(stderr, "can't get the current screen\n");
        xcb_disconnect(conn);
        exit(1);
    }
}

void cleanup(void)
{
    xcb_disconnect(conn);
}

/*
 * Get a keycode from a keysym.
 *
 * Returns keycode value. 
 */
xcb_keycode_t keysymtokeycode(xcb_keysym_t keysym, xcb_key_symbols_t *keysyms)
{
    xcb_keycode_t *keyp;
    xcb_keycode_t key;

    /* We only use the first keysymbol, even if there are more. */
    keyp = xcb_key_symbols_get_keycode(keysyms, keysym);
    if (NULL == keyp)
    {
        fprintf(stderr, "mcmenu: Couldn't look up key. Exiting.\n");
        exit(1);
        return 0;
    }

    key = *keyp;
    free(keyp);
    
    return key;
}

void grabkeys(struct window *win)
{
    xcb_key_symbols_t *keysyms;    

    /* Get all the keysymbols. */
    keysyms = xcb_key_symbols_alloc(conn);

    keycode_j = keysymtokeycode(XK_J, keysyms);
    keycode_k = keysymtokeycode(XK_K, keysyms);
    keycode_ret = keysymtokeycode(XK_Return, keysyms);

    xcb_grab_key(conn, 1, win->id, XCB_MOD_MASK_ANY,
                 keycode_j,
                 XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);

    xcb_grab_key(conn, 1, win->id, XCB_MOD_MASK_ANY,
                 keycode_k,
                 XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);

    xcb_grab_key(conn, 1, win->id, XCB_MOD_MASK_ANY,
                 keycode_ret,
                 XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);
    
    xcb_flush(conn);

    /* Get rid of the key symbols table. */
    xcb_key_symbols_free(keysyms);
}

/*
 * Print some rows of text in window win, stopping at max
 * automatically moving fontheight pixels down at a time.
 */
void printrows(struct window *win, char *rows[], int max, int fontheight)
{
    int i;
    int y;

    if (0 != setfont(win, "6x13", "white", "black"))
    {
        fprintf(stderr, "Couldn't set new font.\n");
        exit(1);
    }
    
    for (i = 0, y = fontheight; i < max; i ++, y += fontheight)
    {
        printat(win, 1, y, rows[i]);
    }
}

/*
 * Invert row row (counting from 1).
 */
void invertrow(struct window *win, char *rows[], int row, int fontheight)
{
    int y = fontheight * row;

    if (0 != setfont(win, "6x13", "black", "white"))
    {
        fprintf(stderr, "Couldn't set new font.\n");
        exit(1);
    }

    /* TODO Pad to window width. */
    printat(win, 1, y, rows[row - 1]);
}

void normalrow(struct window *win, char *rows[], int row, int fontheight)
{
    int y = fontheight * row;

    if (0 != setfont(win, "6x13", "white", "black"))
    {
        fprintf(stderr, "Couldn't set new font.\n");
        exit(1);
    }

    /* TODO Pad to window width. */
    printat(win, 1, y, rows[row - 1]);    
}

int keypress(xcb_key_press_event_t *ev, struct window *win, char *menu[],
             int currow, int max, int height)
{ 
    int oldrow;

    oldrow = currow;

    if (ev->detail == keycode_j)
    {
        printf("DOWN!\n");
        currow ++;
    }
    else if (ev->detail == keycode_k)
    {
        printf("UP!\n");
        currow --;
    }
    else if (ev->detail == keycode_ret)
    {
        printf("Choice: %s\n", menu[currow - 1]);
        cleanup();
        exit(0);        
    }
    else
    {
        printf("Unknown key pressed.\n");
    }

    if (currow < 1)
    {
        currow = max;
    }
    else if (currow > max)
    {
        currow = 1;
    }

    invertrow(win, menu, currow, height);
    
    if (currow != oldrow)
    {
        normalrow(win, menu, oldrow, height);
    }

    return currow;
}

void redraw(struct window *win, char *menu[], int max, int cur, int height)
{
    printrows(win, menu, max, height);
    invertrow(win, menu, cur, height);
}

int main(void)
{
    struct window *win;
    char *menu[] =
    {
        "foo",
        "gurka",
        "sallad"
    };
    int currow = 1;
    int maxrow = 3;
    char *font = "6x13";
    int fontheight = 13;
    xcb_generic_event_t *ev;
    int fd;                         /* Our X file descriptor */
    fd_set in;                      /* For select */
    int found;                      /* Ditto. */
    
    init();

    /*
     * This creates a new window. I might want to have a window
     * created already after init(). The default window, where the
     * program was started. Of course, this won't work in X but might
     * work in another window systems.
     */
    win = window(1, 1, WIDTH, HEIGHT);
    if (NULL == win)
    {
        fprintf(stderr, "Couldn't create window.\n");
        exit(1);
    }
    
    if (0 != setfont(win, font, "white", "black"))
    {
        fprintf(stderr, "Couldn't set new font.\n");
        exit(1);
    }

    grabkeys(win);

    redraw(win, menu, maxrow, currow, fontheight);

    /* Get the file descriptor so we can do select() on it. */
    fd = xcb_get_file_descriptor(conn);

    for (sigcode = 0; 0 == sigcode;)
    {
        /* Prepare for select(). */
        FD_ZERO(&in);
        FD_SET(fd, &in);

        /*
         * Check for events, again and again. When poll returns NULL,
         * we block on select() until the event file descriptor gets
         * readable again.
         */
        ev = xcb_poll_for_event(conn);
        if (NULL == ev)
        {
            printf("xcb_poll_for_event() returned NULL.\n");

            /* Check if we have an unrecoverably error. */
            if (xcb_connection_has_error(conn))
            {
                cleanup();
                exit(1);
            }
        
            found = select(fd + 1, &in, NULL, NULL, NULL);
            if (-1 == found)
            {
                if (EINTR == errno)
                {
                    /* We received a signal. Break out of loop. */
                    break;
                }
                else
                {
                    /* Something was seriously wrong with select(). */
                    fprintf(stderr, "mcwm: select failed.");
                    cleanup();
                    exit(1);
                }
            }
            else
            {
                /* We found more events. Goto start of loop. */
                continue;
            }
        }
            
        switch (ev->response_type & ~0x80)
        {
            /* TODO Add mouse events and mouse button click. */
            
        case XCB_KEY_PRESS:
        {
            xcb_key_press_event_t *e = (xcb_key_press_event_t *)ev;
            
            printf("Key %d pressed\n", e->detail);

            currow = keypress(e, win, menu, currow, maxrow, fontheight);
        }
        break;
        
        case XCB_EXPOSE:
            redraw(win, menu, maxrow, currow, fontheight);
            break;

        default:
            printf("Unknown event.\n");
        
        } /* switch */
    } /* for */
    
    cleanup();
    exit(0);
}
