#include "tui.h"
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

/* Custom data for file browser pane */
typedef struct {
    char **files;
    int file_count;
    int selected;
    int scroll_offset;
} browser_data_t;

/* Function prototypes */
static void quit_handler(tui_app_t *app);
static void draw_browser(tui_pane_t *pane);
static void draw_editor(tui_pane_t *pane);
static void draw_results(tui_pane_t *pane);
static bool browser_event(tui_pane_t *pane, const tui_event_t *event);
static void load_directory(browser_data_t *data, const char *path);

int main(void) {
    /* Create application */
    tui_app_t *app = tui_create_app();
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
    
    /* Create panes */
    tui_pane_t *browser = tui_create_pane(TUI_PANE_BROWSER);
    tui_pane_set_title(browser, "File Browser");
    browser->draw = draw_browser;
    browser->handle_event = browser_event;
    
    /* Initialize browser data */
    browser_data_t *browser_data = calloc(1, sizeof(browser_data_t));
    load_directory(browser_data, ".");
    browser->user_data = browser_data;
    
    tui_pane_t *editor = tui_create_pane(TUI_PANE_EDITOR);
    tui_pane_set_title(editor, "Editor");
    editor->draw = draw_editor;
    
    tui_pane_t *results = tui_create_pane(TUI_PANE_RESULTS);
    tui_pane_set_title(results, "Output");
    results->draw = draw_results;
    
    /* Add panes to window */
    tui_window_add_pane(window, browser);
    tui_window_add_pane(window, editor);
    tui_window_add_pane(window, results);
    
    /* Set up key bindings */
    struct {
        int key;
        const char *label;
        void (*handler)(tui_app_t *app);
    } bindings[] = {
        { 'q', "^q Quit", quit_handler },
        { KEY_F(1), "f1 Help", NULL },
        { KEY_F(8), "f8 Theme", NULL },
        { '\t', "Tab Next", NULL }
    };
    
    app->key_bindings = malloc(sizeof(bindings));
    memcpy(app->key_bindings, bindings, sizeof(bindings));
    app->key_binding_count = sizeof(bindings) / sizeof(bindings[0]);
    
    /* Set status */
    tui_set_status(app, "Harlequin-style TUI Demo");
    
    /* Run the application */
    tui_run(app);
    
    /* Cleanup */
    free(browser_data->files);
    free(browser_data);
    tui_cleanup(app);
    tui_destroy_app(app);
    
    return 0;
}

static void quit_handler(tui_app_t *app) {
    tui_quit(app);
}

static void draw_browser(tui_pane_t *pane) {
    if (!pane || !pane->win) return;
    
    browser_data_t *data = (browser_data_t *)pane->user_data;
    if (!data) return;
    
    int h, w;
    getmaxyx(pane->win, h, w);
    
    /* Draw files */
    for (int i = 0; i < h && i + data->scroll_offset < data->file_count; i++) {
        int file_idx = i + data->scroll_offset;
        bool selected = (file_idx == data->selected);
        
        if (selected) {
            wattron(pane->win, COLOR_PAIR(TUI_COLOR_HIGHLIGHT) | A_BOLD);
        }
        
        mvwprintw(pane->win, i, 0, "%-*s", w, data->files[file_idx]);
        
        if (selected) {
            wattroff(pane->win, COLOR_PAIR(TUI_COLOR_HIGHLIGHT) | A_BOLD);
        }
    }
}

static void draw_editor(tui_pane_t *pane) {
    if (!pane || !pane->win) return;
    
    /* Sample editor content */
    mvwaddstr(pane->win, 0, 0, "// Sample code");
    mvwaddstr(pane->win, 1, 0, "#include <stdio.h>");
    mvwaddstr(pane->win, 3, 0, "int main() {");
    mvwaddstr(pane->win, 4, 4, "printf(\"Hello, TUI!\\n\");");
    mvwaddstr(pane->win, 5, 4, "return 0;");
    mvwaddstr(pane->win, 6, 0, "}");
}

static void draw_results(tui_pane_t *pane) {
    if (!pane || !pane->win) return;
    
    /* Sample output */
    wattron(pane->win, COLOR_PAIR(TUI_COLOR_NORMAL));
    mvwaddstr(pane->win, 0, 0, "Program output:");
    mvwaddstr(pane->win, 1, 0, "Hello, TUI!");
    wattroff(pane->win, COLOR_PAIR(TUI_COLOR_NORMAL));
}

static bool browser_event(tui_pane_t *pane, const tui_event_t *event) {
    if (!pane || !event) return false;
    
    browser_data_t *data = (browser_data_t *)pane->user_data;
    if (!data) return false;
    
    if (event->type == TUI_EVENT_KEY) {
        switch (event->data.key.key) {
            case KEY_UP:
            case 'k':
                if (data->selected > 0) {
                    data->selected--;
                    tui_pane_draw(pane);
                    return true;
                }
                break;
                
            case KEY_DOWN:
            case 'j':
                if (data->selected < data->file_count - 1) {
                    data->selected++;
                    tui_pane_draw(pane);
                    return true;
                }
                break;
        }
    }
    else if (event->type == TUI_EVENT_MOUSE) {
        /* Handle mouse click on file */
        if (event->data.mouse.pressed) {
            int clicked = event->data.mouse.y + data->scroll_offset;
            if (clicked >= 0 && clicked < data->file_count) {
                data->selected = clicked;
                tui_pane_draw(pane);
                return true;
            }
        }
    }
    
    return false;
}

static void load_directory(browser_data_t *data, const char *path) {
    DIR *dir = opendir(path);
    if (!dir) return;
    
    /* Count entries */
    struct dirent *entry;
    int count = 0;
    while ((entry = readdir(dir)) != NULL) {
        count++;
    }
    
    /* Allocate memory */
    data->files = malloc(count * sizeof(char*));
    data->file_count = 0;
    
    /* Read entries */
    rewinddir(dir);
    while ((entry = readdir(dir)) != NULL && data->file_count < count) {
        data->files[data->file_count] = strdup(entry->d_name);
        data->file_count++;
    }
    
    closedir(dir);
    data->selected = 0;
    data->scroll_offset = 0;
}