#include "tui.h"
#include <stdlib.h>
#include <string.h>

tui_window_t *tui_create_window(tui_app_t *app) {
    tui_window_t *window = calloc(1, sizeof(tui_window_t));
    if (!window) return NULL;
    
    window->app = app;
    window->panes = NULL;
    window->pane_count = 0;
    window->active_pane = -1;
    window->horizontal_split = true;
    window->split_ratio = 0.5;
    
    return window;
}

void tui_destroy_window(tui_window_t *window) {
    if (!window) return;
    
    for (int i = 0; i < window->pane_count; i++) {
        tui_destroy_pane(window->panes[i]);
    }
    
    free(window->panes);
    free(window);
}

void tui_window_add_pane(tui_window_t *window, tui_pane_t *pane) {
    if (!window || !pane) return;
    
    window->panes = realloc(window->panes, 
                           (window->pane_count + 1) * sizeof(tui_pane_t*));
    if (!window->panes) return;
    
    window->panes[window->pane_count] = pane;
    pane->window = window;
    window->pane_count++;
    
    /* Set first pane as active */
    if (window->active_pane < 0) {
        window->active_pane = 0;
        pane->active = true;
    }
}

void tui_window_set_active_pane(tui_window_t *window, int index) {
    if (!window || index < 0 || index >= window->pane_count) return;
    
    /* Deactivate current */
    if (window->active_pane >= 0) {
        window->panes[window->active_pane]->active = false;
        tui_pane_draw(window->panes[window->active_pane]);
    }
    
    /* Activate new */
    window->active_pane = index;
    window->panes[index]->active = true;
    tui_pane_draw(window->panes[index]);
}

void tui_window_layout(tui_window_t *window) {
    if (!window || window->pane_count == 0) return;
    
    int height = LINES - 1;  // Leave room for status line
    int width = COLS;
    
    if (window->pane_count == 1) {
        /* Single pane fills the screen */
        tui_pane_t *pane = window->panes[0];
        pane->x = 0;
        pane->y = 0;
        pane->width = width;
        pane->height = height;
        
        if (pane->resize) {
            pane->resize(pane, width, height);
        }
    }
    else if (window->pane_count == 2) {
        /* Two panes split horizontally or vertically */
        if (window->horizontal_split) {
            int split_x = (int)(width * window->split_ratio);
            
            /* Left pane */
            window->panes[0]->x = 0;
            window->panes[0]->y = 0;
            window->panes[0]->width = split_x;
            window->panes[0]->height = height;
            
            /* Right pane */
            window->panes[1]->x = split_x;
            window->panes[1]->y = 0;
            window->panes[1]->width = width - split_x;
            window->panes[1]->height = height;
        } else {
            int split_y = (int)(height * window->split_ratio);
            
            /* Top pane */
            window->panes[0]->x = 0;
            window->panes[0]->y = 0;
            window->panes[0]->width = width;
            window->panes[0]->height = split_y;
            
            /* Bottom pane */
            window->panes[1]->x = 0;
            window->panes[1]->y = split_y;
            window->panes[1]->width = width;
            window->panes[1]->height = height - split_y;
        }
    }
    else if (window->pane_count == 3) {
        /* Three panes: left sidebar, top right, bottom right */
        int sidebar_width = width / 4;  // 25% for sidebar
        int right_width = width - sidebar_width;
        int split_y = height * 2 / 3;  // 66% for top right pane
        
        /* Left sidebar */
        window->panes[0]->x = 0;
        window->panes[0]->y = 0;
        window->panes[0]->width = sidebar_width;
        window->panes[0]->height = height;
        
        /* Top right */
        window->panes[1]->x = sidebar_width;
        window->panes[1]->y = 0;
        window->panes[1]->width = right_width;
        window->panes[1]->height = split_y;
        
        /* Bottom right */
        window->panes[2]->x = sidebar_width;
        window->panes[2]->y = split_y;
        window->panes[2]->width = right_width;
        window->panes[2]->height = height - split_y;
    }
    
    /* Notify panes of resize */
    for (int i = 0; i < window->pane_count; i++) {
        if (window->panes[i]->resize) {
            window->panes[i]->resize(window->panes[i], 
                                   window->panes[i]->width,
                                   window->panes[i]->height);
        }
    }
}

tui_pane_t *tui_get_pane_at(tui_window_t *window, int x, int y) {
    if (!window) return NULL;
    
    for (int i = 0; i < window->pane_count; i++) {
        tui_pane_t *pane = window->panes[i];
        if (x >= pane->x && x < pane->x + pane->width &&
            y >= pane->y && y < pane->y + pane->height) {
            return pane;
        }
    }
    
    return NULL;
}