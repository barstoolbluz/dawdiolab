#include "tui.h"
#include <stdlib.h>
#include <string.h>
#include <signal.h>

static volatile sig_atomic_t resize_flag = 0;

static void handle_resize(int sig) {
    resize_flag = 1;
}

tui_app_t *tui_create_app(void) {
    tui_app_t *app = calloc(1, sizeof(tui_app_t));
    if (!app) return NULL;
    
    app->running = false;
    app->mouse_enabled = false;
    app->theme = tui_theme_harlequin();  // Default to Harlequin theme
    
    return app;
}

void tui_destroy_app(tui_app_t *app) {
    if (!app) return;
    
    if (app->main_window) {
        tui_destroy_window(app->main_window);
    }
    
    free(app->key_bindings);
    free(app);
}

bool tui_init(tui_app_t *app) {
    if (!app) return false;
    
    /* Initialize ncurses */
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
    
    /* Initialize colors */
    if (has_colors()) {
        start_color();
        use_default_colors();
        tui_init_colors(app->theme);
    }
    
    /* Set up signal handlers */
    signal(SIGWINCH, handle_resize);
    
    /* Clear and refresh */
    clear();
    refresh();
    
    return true;
}

void tui_cleanup(tui_app_t *app) {
    if (app->mouse_enabled) {
        tui_disable_mouse(app);
    }
    
    endwin();
}

void tui_run(tui_app_t *app) {
    if (!app || !app->main_window) return;
    
    app->running = true;
    
    /* Initial layout and draw */
    tui_window_layout(app->main_window);
    for (int i = 0; i < app->main_window->pane_count; i++) {
        tui_pane_draw(app->main_window->panes[i]);
    }
    tui_draw_status(app);
    
    /* Main event loop */
    while (app->running) {
        /* Handle resize */
        if (resize_flag) {
            resize_flag = 0;
            endwin();
            refresh();
            clear();
            
            tui_window_layout(app->main_window);
            for (int i = 0; i < app->main_window->pane_count; i++) {
                tui_pane_draw(app->main_window->panes[i]);
            }
            tui_draw_status(app);
        }
        
        /* Get input */
        int ch = getch();
        
        /* Handle mouse events */
        if (app->mouse_enabled && ch == KEY_MOUSE) {
            MEVENT mevent;
            if (getmouse(&mevent) == OK) {
                /* Find which pane was clicked */
                tui_pane_t *clicked_pane = tui_get_pane_at(app->main_window, 
                                                           mevent.x, mevent.y);
                if (clicked_pane) {
                    /* Activate the clicked pane */
                    for (int i = 0; i < app->main_window->pane_count; i++) {
                        if (app->main_window->panes[i] == clicked_pane) {
                            tui_window_set_active_pane(app->main_window, i);
                            break;
                        }
                    }
                    
                    /* Send mouse event to the pane */
                    if (clicked_pane->handle_event) {
                        tui_event_t event = {
                            .type = TUI_EVENT_MOUSE,
                            .data.mouse = {
                                .x = mevent.x - clicked_pane->x,
                                .y = mevent.y - clicked_pane->y,
                                .button = mevent.bstate,
                                .pressed = (mevent.bstate & BUTTON1_PRESSED) != 0
                            }
                        };
                        clicked_pane->handle_event(clicked_pane, &event);
                    }
                }
                continue;
            }
        }
        
        /* Handle global key bindings */
        bool handled = false;
        for (int i = 0; i < app->key_binding_count; i++) {
            if (app->key_bindings[i].key == ch) {
                if (app->key_bindings[i].handler) {
                    app->key_bindings[i].handler(app);
                }
                handled = true;
                break;
            }
        }
        
        /* Send to active pane if not handled globally */
        if (!handled && app->main_window->active_pane >= 0) {
            tui_pane_t *active = app->main_window->panes[app->main_window->active_pane];
            if (active->handle_event) {
                tui_event_t event = {
                    .type = TUI_EVENT_KEY,
                    .data.key = {
                        .key = ch,
                        .alt = false,
                        .ctrl = false
                    }
                };
                active->handle_event(active, &event);
            }
        }
    }
}

void tui_quit(tui_app_t *app) {
    if (app) {
        app->running = false;
    }
}

void tui_set_status(tui_app_t *app, const char *text) {
    if (app) {
        app->status_text = text;
        tui_draw_status(app);
    }
}

void tui_draw_status(tui_app_t *app) {
    if (!app) return;
    
    int y = LINES - 1;
    
    /* Clear status line */
    move(y, 0);
    clrtoeol();
    
    /* Draw key bindings */
    attron(COLOR_PAIR(TUI_COLOR_STATUS));
    
    int x = 0;
    for (int i = 0; i < app->key_binding_count && i < 6; i++) {  // Limit to 6 for space
        if (i > 0) {
            mvaddstr(y, x, "  ");
            x += 2;
        }
        
        /* Key in reverse video */
        attron(A_REVERSE);
        mvaddstr(y, x, app->key_bindings[i].label);
        attroff(A_REVERSE);
        x += strlen(app->key_bindings[i].label);
    }
    
    /* Draw status text on the right */
    if (app->status_text) {
        int len = strlen(app->status_text);
        mvaddstr(y, COLS - len - 1, app->status_text);
    }
    
    attroff(COLOR_PAIR(TUI_COLOR_STATUS));
    refresh();
}