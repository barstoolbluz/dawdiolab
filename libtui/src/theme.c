#include "tui.h"
#include <ncurses.h>

/* Harlequin-inspired theme with true colors */
static tui_theme_t harlequin_theme = {
    .name = "Harlequin",
    .normal = { COLOR_WHITE, COLOR_BLACK, 0 },
    .active = { COLOR_GREEN, COLOR_BLACK, A_BOLD },      /* Bright cyan/green for active elements */
    .inactive = { 8, COLOR_BLACK, A_DIM },               /* Dark gray for inactive */
    .highlight = { COLOR_BLACK, COLOR_CYAN, A_BOLD },    /* Black on cyan background for selection */
    .button = { COLOR_BLACK, COLOR_YELLOW, A_BOLD },     /* Black on yellow for buttons */
    .status = { COLOR_CYAN, COLOR_BLACK, 0 },            /* Cyan text for status */
    .error = { COLOR_RED, COLOR_BLACK, A_BOLD },
    .border_active = { COLOR_CYAN, COLOR_BLACK, A_BOLD }, /* Cyan borders for active panes */
    .border_inactive = { 8, COLOR_BLACK, 0 }             /* Dark gray borders for inactive */
};

/* Default theme */
static tui_theme_t default_theme = {
    .name = "Default",
    .normal = { COLOR_WHITE, COLOR_BLACK, 0 },
    .active = { COLOR_WHITE, COLOR_BLUE, 0 },
    .inactive = { COLOR_BLACK, COLOR_WHITE, 0 },
    .highlight = { COLOR_YELLOW, COLOR_BLACK, A_BOLD },
    .button = { COLOR_BLACK, COLOR_WHITE, A_REVERSE },
    .status = { COLOR_WHITE, COLOR_BLUE, 0 },
    .error = { COLOR_RED, COLOR_BLACK, A_BOLD },
    .border_active = { COLOR_WHITE, COLOR_BLUE, 0 },
    .border_inactive = { COLOR_BLACK, COLOR_WHITE, 0 }
};

tui_theme_t *tui_theme_harlequin(void) {
    return &harlequin_theme;
}

tui_theme_t *tui_theme_default(void) {
    return &default_theme;
}

void tui_set_theme(tui_app_t *app, tui_theme_t *theme) {
    if (app && theme) {
        app->theme = theme;
        tui_init_colors(theme);
    }
}

void tui_init_colors(tui_theme_t *theme) {
    if (!theme || !has_colors()) return;
    
    init_pair(TUI_COLOR_NORMAL, theme->normal.fg, theme->normal.bg);
    init_pair(TUI_COLOR_ACTIVE, theme->active.fg, theme->active.bg);
    init_pair(TUI_COLOR_INACTIVE, theme->inactive.fg, theme->inactive.bg);
    init_pair(TUI_COLOR_HIGHLIGHT, theme->highlight.fg, theme->highlight.bg);
    init_pair(TUI_COLOR_BUTTON, theme->button.fg, theme->button.bg);
    init_pair(TUI_COLOR_STATUS, theme->status.fg, theme->status.bg);
    init_pair(TUI_COLOR_ERROR, theme->error.fg, theme->error.bg);
    init_pair(TUI_COLOR_BORDER_ACTIVE, theme->border_active.fg, theme->border_active.bg);
    init_pair(TUI_COLOR_BORDER_INACTIVE, theme->border_inactive.fg, theme->border_inactive.bg);
}