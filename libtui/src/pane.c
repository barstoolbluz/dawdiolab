#include "tui.h"
#include <stdlib.h>
#include <string.h>

tui_pane_t *tui_create_pane(tui_pane_type_t type) {
    tui_pane_t *pane = calloc(1, sizeof(tui_pane_t));
    if (!pane) return NULL;
    
    pane->type = type;
    pane->active = false;
    pane->title = NULL;
    pane->win = NULL;
    pane->border_win = NULL;
    
    return pane;
}

void tui_destroy_pane(tui_pane_t *pane) {
    if (!pane) return;
    
    if (pane->border_win) {
        delwin(pane->border_win);
    }
    if (pane->win) {
        delwin(pane->win);
    }
    
    free(pane);
}

void tui_pane_set_title(tui_pane_t *pane, const char *title) {
    if (pane) {
        pane->title = title;
    }
}

void tui_pane_draw(tui_pane_t *pane) {
    if (!pane || !pane->window || !pane->window->app) return;
    
    tui_theme_t *theme = pane->window->app->theme;
    
    /* Create or resize windows */
    if (!pane->border_win) {
        pane->border_win = newwin(pane->height, pane->width, pane->y, pane->x);
    } else {
        wresize(pane->border_win, pane->height, pane->width);
        mvwin(pane->border_win, pane->y, pane->x);
    }
    
    if (!pane->win) {
        /* Content window is inside border */
        pane->win = newwin(pane->height - 2, pane->width - 2, 
                          pane->y + 1, pane->x + 1);
    } else {
        wresize(pane->win, pane->height - 2, pane->width - 2);
        mvwin(pane->win, pane->y + 1, pane->x + 1);
    }
    
    /* Clear windows */
    werase(pane->border_win);
    werase(pane->win);
    
    /* Draw border */
    tui_draw_border(pane->border_win, pane->active, theme);
    
    /* Draw title if present */
    if (pane->title) {
        int title_x = 2;  /* Start 2 chars from left */
        mvwaddch(pane->border_win, 0, title_x - 1, ' ');
        mvwaddstr(pane->border_win, 0, title_x, pane->title);
        mvwaddch(pane->border_win, 0, title_x + strlen(pane->title), ' ');
    }
    
    /* Call custom draw function */
    if (pane->draw) {
        pane->draw(pane);
    }
    
    /* Refresh */
    wnoutrefresh(pane->border_win);
    wnoutrefresh(pane->win);
    doupdate();
}

void tui_pane_refresh(tui_pane_t *pane) {
    if (!pane) return;
    
    if (pane->border_win) {
        wnoutrefresh(pane->border_win);
    }
    if (pane->win) {
        wnoutrefresh(pane->win);
    }
    doupdate();
}