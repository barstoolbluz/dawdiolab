#include "tui.h"
#include <ncurses.h>

void tui_enable_mouse(tui_app_t *app) {
    if (!app) return;
    
    /* Enable mouse events */
    mousemask(ALL_MOUSE_EVENTS | REPORT_MOUSE_POSITION, NULL);
    
    /* Enable mouse tracking in xterm-compatible terminals */
    printf("\033[?1003h\n");
    fflush(stdout);
    
    app->mouse_enabled = true;
}

void tui_disable_mouse(tui_app_t *app) {
    if (!app) return;
    
    /* Disable mouse tracking */
    printf("\033[?1003l\n");
    fflush(stdout);
    
    /* Disable mouse events */
    mousemask(0, NULL);
    
    app->mouse_enabled = false;
}