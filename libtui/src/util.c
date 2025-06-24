#include "tui.h"
#include <string.h>

void tui_draw_border(WINDOW *win, bool active, tui_theme_t *theme) {
    if (!win || !theme) return;
    
    int color_pair = active ? TUI_COLOR_BORDER_ACTIVE : TUI_COLOR_BORDER_INACTIVE;
    int attrs = active ? theme->border_active.attrs : theme->border_inactive.attrs;
    
    wattron(win, COLOR_PAIR(color_pair) | attrs);
    
    /* Draw rounded corners like Harlequin */
    int h, w;
    getmaxyx(win, h, w);
    
    /* Top border */
    mvwaddch(win, 0, 0, ACS_ULCORNER);  /* Top-left corner */
    for (int x = 1; x < w - 1; x++) {
        mvwaddch(win, 0, x, ACS_HLINE);
    }
    mvwaddch(win, 0, w - 1, ACS_URCORNER);  /* Top-right corner */
    
    /* Side borders */
    for (int y = 1; y < h - 1; y++) {
        mvwaddch(win, y, 0, ACS_VLINE);      /* Left border */
        mvwaddch(win, y, w - 1, ACS_VLINE);  /* Right border */
    }
    
    /* Bottom border */
    mvwaddch(win, h - 1, 0, ACS_LLCORNER);  /* Bottom-left corner */
    for (int x = 1; x < w - 1; x++) {
        mvwaddch(win, h - 1, x, ACS_HLINE);
    }
    mvwaddch(win, h - 1, w - 1, ACS_LRCORNER);  /* Bottom-right corner */
    
    wattroff(win, COLOR_PAIR(color_pair) | attrs);
}

void tui_draw_text(WINDOW *win, int y, int x, const char *text, int color_pair) {
    if (!win || !text) return;
    
    wattron(win, COLOR_PAIR(color_pair));
    mvwaddstr(win, y, x, text);
    wattroff(win, COLOR_PAIR(color_pair));
}