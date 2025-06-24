/*
 * SACD Lab TUI - Using libtui
 * Beautiful Harlequin-inspired interface with mouse support
 */

#include "libtui/include/tui.h"
#include "sacd_tui_adapter.h"
#include <stdlib.h>
#include <string.h>

/* Global app instance */
static tui_app_t *app = NULL;

/* Key handlers */
static void quit_handler(tui_app_t *app) {
    tui_quit(app);
}

static void next_pane_handler(tui_app_t *app) {
    if (!app || !app->main_window) return;
    
    int next = (app->main_window->active_pane + 1) % app->main_window->pane_count;
    tui_window_set_active_pane(app->main_window, next);
}

static void prev_pane_handler(tui_app_t *app) {
    if (!app || !app->main_window) return;
    
    int prev = app->main_window->active_pane - 1;
    if (prev < 0) prev = app->main_window->pane_count - 1;
    tui_window_set_active_pane(app->main_window, prev);
}

static void extract_handler(tui_app_t *app) {
    if (!app || !app->main_window) return;
    
    /* Send F5 key event to the first pane (browser pane) */
    if (app->main_window->pane_count > 0) {
        tui_pane_t *browser_pane = app->main_window->panes[0];
        if (browser_pane && browser_pane->handle_event) {
            tui_event_t event = {
                .type = TUI_EVENT_KEY,
                .data = { .key = KEY_F(5) }
            };
            browser_pane->handle_event(browser_pane, &event);
        }
    }
}

int main(void) {
    /* Create application */
    app = tui_create_app();
    if (!app) {
        fprintf(stderr, "Failed to create application\n");
        return 1;
    }
    
    /* Initialize TUI */
    if (!tui_init(app)) {
        fprintf(stderr, "Failed to initialize TUI\n");
        tui_destroy_app(app);
        return 1;
    }
    
    /* Enable mouse support */
    tui_enable_mouse(app);
    
    /* Create main window */
    tui_window_t *window = tui_create_window(app);
    app->main_window = window;
    
    /* Create SACD-specific panes */
    tui_pane_t *browser = create_sacd_browser_pane();
    tui_pane_t *info = create_sacd_info_pane();
    tui_pane_t *extract = create_sacd_extract_pane();
    
    /* Add panes to window */
    tui_window_add_pane(window, browser);
    tui_window_add_pane(window, info);
    tui_window_add_pane(window, extract);
    
    /* Set up key bindings */
    struct {
        int key;
        const char *label;
        void (*handler)(tui_app_t *app);
    } bindings[] = {
        { 'q', "^q Quit", quit_handler },
        { KEY_F(1), "f1 Help", NULL },
        { '\t', "Tab Next", next_pane_handler },
        { KEY_BTAB, "S-Tab Prev", prev_pane_handler },
        { KEY_F(5), "f5 Extract", extract_handler },
        { KEY_F(8), "f8 Settings", NULL }
    };
    
    app->key_bindings = malloc(sizeof(bindings));
    memcpy(app->key_bindings, bindings, sizeof(bindings));
    app->key_binding_count = sizeof(bindings) / sizeof(bindings[0]);
    
    /* Set status */
    tui_set_status(app, "SACD Lab - Harlequin Edition");
    
    /* Run the application */
    tui_run(app);
    
    /* Cleanup */
    tui_cleanup(app);
    tui_destroy_app(app);
    
    return 0;
}