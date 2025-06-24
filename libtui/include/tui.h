#ifndef TUI_H
#define TUI_H

#include <ncurses.h>
#include <stdbool.h>

/* Core types */
typedef struct tui_theme tui_theme_t;
typedef struct tui_pane tui_pane_t;
typedef struct tui_window tui_window_t;
typedef struct tui_app tui_app_t;

/* Pane types */
typedef enum {
    TUI_PANE_BROWSER,
    TUI_PANE_EDITOR,
    TUI_PANE_RESULTS,
    TUI_PANE_CUSTOM
} tui_pane_type_t;

/* Mouse events */
typedef struct {
    int x, y;
    int button;
    bool pressed;
} tui_mouse_event_t;

/* Key events */
typedef struct {
    int key;
    bool alt;
    bool ctrl;
} tui_key_event_t;

/* Event types */
typedef enum {
    TUI_EVENT_KEY,
    TUI_EVENT_MOUSE,
    TUI_EVENT_RESIZE,
    TUI_EVENT_CUSTOM
} tui_event_type_t;

/* Generic event */
typedef struct {
    tui_event_type_t type;
    union {
        tui_key_event_t key;
        tui_mouse_event_t mouse;
        void *custom;
    } data;
} tui_event_t;

/* Callbacks */
typedef void (*tui_draw_cb)(tui_pane_t *pane);
typedef bool (*tui_event_cb)(tui_pane_t *pane, const tui_event_t *event);
typedef void (*tui_resize_cb)(tui_pane_t *pane, int w, int h);

/* Theme colors */
typedef struct {
    short fg;
    short bg;
    int attrs;
} tui_color_pair_t;

/* Theme definition */
struct tui_theme {
    const char *name;
    tui_color_pair_t normal;
    tui_color_pair_t active;
    tui_color_pair_t inactive;
    tui_color_pair_t highlight;
    tui_color_pair_t button;
    tui_color_pair_t status;
    tui_color_pair_t error;
    tui_color_pair_t border_active;
    tui_color_pair_t border_inactive;
};

/* Pane structure */
struct tui_pane {
    tui_window_t *window;
    tui_pane_type_t type;
    const char *title;
    bool active;
    
    /* Position and size */
    int x, y;
    int width, height;
    
    /* Callbacks */
    tui_draw_cb draw;
    tui_event_cb handle_event;
    tui_resize_cb resize;
    
    /* User data */
    void *user_data;
    
    /* Internal */
    WINDOW *win;
    WINDOW *border_win;
};

/* Window (container for panes) */
struct tui_window {
    tui_app_t *app;
    tui_pane_t **panes;
    int pane_count;
    int active_pane;
    
    /* Layout */
    bool horizontal_split;
    float split_ratio;
};

/* Application */
struct tui_app {
    tui_window_t *main_window;
    tui_theme_t *theme;
    bool running;
    bool mouse_enabled;
    
    /* Status line */
    const char *status_text;
    
    /* Global key bindings */
    struct {
        int key;
        const char *label;
        void (*handler)(tui_app_t *app);
    } *key_bindings;
    int key_binding_count;
};

/* Core API */
tui_app_t *tui_create_app(void);
void tui_destroy_app(tui_app_t *app);
bool tui_init(tui_app_t *app);
void tui_cleanup(tui_app_t *app);
void tui_run(tui_app_t *app);
void tui_quit(tui_app_t *app);

/* Theme API */
tui_theme_t *tui_theme_harlequin(void);
tui_theme_t *tui_theme_default(void);
void tui_set_theme(tui_app_t *app, tui_theme_t *theme);
void tui_init_colors(tui_theme_t *theme);

/* Window API */
tui_window_t *tui_create_window(tui_app_t *app);
void tui_destroy_window(tui_window_t *window);
void tui_window_add_pane(tui_window_t *window, tui_pane_t *pane);
void tui_window_set_active_pane(tui_window_t *window, int index);
void tui_window_layout(tui_window_t *window);

/* Pane API */
tui_pane_t *tui_create_pane(tui_pane_type_t type);
void tui_destroy_pane(tui_pane_t *pane);
void tui_pane_set_title(tui_pane_t *pane, const char *title);
void tui_pane_draw(tui_pane_t *pane);
void tui_pane_refresh(tui_pane_t *pane);

/* Mouse API */
void tui_enable_mouse(tui_app_t *app);
void tui_disable_mouse(tui_app_t *app);
tui_pane_t *tui_get_pane_at(tui_window_t *window, int x, int y);

/* Status line API */
void tui_set_status(tui_app_t *app, const char *text);
void tui_draw_status(tui_app_t *app);

/* Utility API */
void tui_draw_border(WINDOW *win, bool active, tui_theme_t *theme);
void tui_draw_text(WINDOW *win, int y, int x, const char *text, int color_pair);

/* Color pair constants */
enum {
    TUI_COLOR_NORMAL = 1,
    TUI_COLOR_ACTIVE,
    TUI_COLOR_INACTIVE,
    TUI_COLOR_HIGHLIGHT,
    TUI_COLOR_BUTTON,
    TUI_COLOR_STATUS,
    TUI_COLOR_ERROR,
    TUI_COLOR_BORDER_ACTIVE,
    TUI_COLOR_BORDER_INACTIVE,
    TUI_COLOR_MAX
};

#endif /* TUI_H */