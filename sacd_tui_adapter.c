/* _GNU_SOURCE is already defined by Makefile */
/* Use new libsacd API instead of old conflicting headers */
#include "libtui/include/tui.h"
#include "libsacd/sacd_lib.h"
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>
#include <dirent.h>
#include <errno.h>
#include <strings.h>
#include <ctype.h>
#include <time.h>
#include <math.h>
#include <stdbool.h>

/* Include the header for all type definitions */
#include "sacd_tui_adapter.h"

/* Helper functions to replace old API calls */
static bool libsacd_is_valid_iso(const char *path) {
    sacd_disc_t *disc = NULL;
    sacd_result_t result = sacd_disc_open(path, &disc);
    if (result == SACD_RESULT_OK) {
        sacd_disc_close(disc);
        return true;
    }
    return false;
}

static void libsacd_format_duration(double seconds, char *buffer, size_t size) {
    int mins = (int)(seconds / 60);
    int secs = (int)(seconds) % 60;
    snprintf(buffer, size, "%d:%02d", mins, secs);
}

static void libsacd_format_filesize(size_t bytes, char *buffer, size_t size) {
    if (bytes >= 1024 * 1024 * 1024) {
        snprintf(buffer, size, "%.1f GB", (double)bytes / (1024.0 * 1024.0 * 1024.0));
    } else if (bytes >= 1024 * 1024) {
        snprintf(buffer, size, "%.1f MB", (double)bytes / (1024.0 * 1024.0));
    } else if (bytes >= 1024) {
        snprintf(buffer, size, "%.1f KB", (double)bytes / 1024.0);
    } else {
        snprintf(buffer, size, "%zu bytes", bytes);
    }
}

static void libsacd_read_iso_info(const char *iso_path, sacd_iso_info_t *info) {
    if (!info) return;
    
    /* Clear the structure */
    memset(info, 0, sizeof(sacd_iso_info_t));
    
    /* Try to open with libsacd */
    sacd_disc_t *disc = NULL;
    sacd_result_t result = sacd_disc_open(iso_path, &disc);
    if (result != SACD_RESULT_OK) {
        /* Set default values for invalid file */
        strcpy(info->title, "Invalid SACD");
        strcpy(info->artist, "Unknown");
        strcpy(info->year, "0000");
        return;
    }
    
    /* Extract metadata from disc */
    strncpy(info->title, disc->text.title ? disc->text.title : "SACD Album", sizeof(info->title) - 1);
    strncpy(info->artist, disc->text.artist ? disc->text.artist : "Unknown Artist", sizeof(info->artist) - 1);
    
    if (disc->year > 0) {
        snprintf(info->year, sizeof(info->year), "%d", disc->year);
    } else {
        strcpy(info->year, "0000");
    }
    
    /* Store area references */
    info->stereo_area = sacd_disc_get_area(disc, SACD_AREA_STEREO);
    info->mulch_area = sacd_disc_get_area(disc, SACD_AREA_MULTICHANNEL);
    
    info->total_tracks = 0;
    if (info->stereo_area) {
        info->total_tracks = info->stereo_area->track_count;
    } else if (info->mulch_area) {
        info->total_tracks = info->mulch_area->track_count;
    }
    
    info->has_metadata = true;
    
    /* Get file size */
    struct stat st;
    if (stat(iso_path, &st) == 0) {
        info->file_size = st.st_size;
    }
    
    /* Don't close disc here - store it for later use */
    /* sacd_disc_close(disc); */
}

/* Track selection helper functions */
void init_track_selection(sacd_iso_info_t *sacd_info) {
    if (!sacd_info) return;
    
    /* Clean up existing selection */
    cleanup_track_selection(sacd_info);
    
    /* Get track count from primary area */
    const sacd_area_t *primary_area = sacd_info->stereo_area ? 
                                     sacd_info->stereo_area : sacd_info->mulch_area;
    
    if (primary_area && primary_area->track_count > 0) {
        sacd_info->track_selected = calloc(primary_area->track_count, sizeof(bool));
        if (sacd_info->track_selected) {
            /* Select all tracks by default */
            for (int i = 0; i < primary_area->track_count; i++) {
                sacd_info->track_selected[i] = true;
            }
        }
        sacd_info->track_selection_cursor = 0;
        sacd_info->track_selection_mode = false;
    }
}

void cleanup_track_selection(sacd_iso_info_t *sacd_info) {
    if (!sacd_info) return;
    
    if (sacd_info->track_selected) {
        free(sacd_info->track_selected);
        sacd_info->track_selected = NULL;
    }
    sacd_info->track_selection_cursor = 0;
    sacd_info->track_selection_mode = false;
}

void toggle_track_selection(sacd_iso_info_t *sacd_info, int track_index) {
    if (!sacd_info || !sacd_info->track_selected) return;
    
    const sacd_area_t *primary_area = sacd_info->stereo_area ? 
                                     sacd_info->stereo_area : sacd_info->mulch_area;
    
    if (primary_area && track_index >= 0 && track_index < primary_area->track_count) {
        sacd_info->track_selected[track_index] = !sacd_info->track_selected[track_index];
    }
}

void select_all_tracks(sacd_iso_info_t *sacd_info) {
    if (!sacd_info || !sacd_info->track_selected) return;
    
    const sacd_area_t *primary_area = sacd_info->stereo_area ? 
                                     sacd_info->stereo_area : sacd_info->mulch_area;
    
    if (primary_area) {
        for (int i = 0; i < primary_area->track_count; i++) {
            sacd_info->track_selected[i] = true;
        }
    }
}

void select_no_tracks(sacd_iso_info_t *sacd_info) {
    if (!sacd_info || !sacd_info->track_selected) return;
    
    const sacd_area_t *primary_area = sacd_info->stereo_area ? 
                                     sacd_info->stereo_area : sacd_info->mulch_area;
    
    if (primary_area) {
        for (int i = 0; i < primary_area->track_count; i++) {
            sacd_info->track_selected[i] = false;
        }
    }
}

int count_selected_tracks(sacd_iso_info_t *sacd_info) {
    if (!sacd_info || !sacd_info->track_selected) return 0;
    
    const sacd_area_t *primary_area = sacd_info->stereo_area ? 
                                     sacd_info->stereo_area : sacd_info->mulch_area;
    
    if (!primary_area) return 0;
    
    int count = 0;
    for (int i = 0; i < primary_area->track_count; i++) {
        if (sacd_info->track_selected[i]) count++;
    }
    return count;
}

double calculate_selected_duration(sacd_iso_info_t *sacd_info) {
    if (!sacd_info || !sacd_info->track_selected) return 0.0;
    
    const sacd_area_t *primary_area = sacd_info->stereo_area ? 
                                     sacd_info->stereo_area : sacd_info->mulch_area;
    
    if (!primary_area) return 0.0;
    
    double total_seconds = 0.0;
    for (int i = 0; i < primary_area->track_count; i++) {
        if (sacd_info->track_selected[i]) {
            total_seconds += sacd_time_to_seconds(&primary_area->tracks[i].duration);
        }
    }
    return total_seconds;
}

/* Forward declarations */
static void draw_sacd_browser(tui_pane_t *pane);
static bool handle_sacd_browser_event(tui_pane_t *pane, const tui_event_t *event);
static void draw_sacd_info(tui_pane_t *pane);
static bool handle_sacd_info_event(tui_pane_t *pane, const tui_event_t *event);
static void draw_sacd_extract(tui_pane_t *pane);
static int load_directory(sacd_browser_data_t *data, const char *path);
static void free_file_list(sacd_browser_data_t *data);
static int file_entry_compare(const void *a, const void *b);
static bool is_audio_video_file(const char *filename);
static void start_extraction(sacd_extract_data_t *extract_data, sacd_iso_info_t *iso_info, const char *iso_path);
static void tui_progress_callback(int track_number, int total_tracks, int track_progress_percent, int overall_progress_percent, const char *status_message, void *userdata);
/* Removed old callback function */

tui_pane_t *create_sacd_browser_pane(void) {
    tui_pane_t *pane = tui_create_pane(TUI_PANE_BROWSER);
    if (!pane) return NULL;
    
    tui_pane_set_title(pane, "SACD Browser");
    
    /* Initialize SACD browser data */
    sacd_browser_data_t *data = calloc(1, sizeof(sacd_browser_data_t));
    if (data) {
        /* Load test-isos directory for testing, fallback to current directory */
        char cwd[PATH_MAX];
        if (load_directory(data, "./test-isos") == 0) {
            /* Success - we're in test-isos */
        } else if (getcwd(cwd, sizeof(cwd))) {
            load_directory(data, cwd);
        } else {
            load_directory(data, ".");
        }
    }
    
    pane->user_data = data;
    pane->draw = draw_sacd_browser;
    pane->handle_event = handle_sacd_browser_event;
    
    return pane;
}

tui_pane_t *create_sacd_info_pane(void) {
    tui_pane_t *pane = tui_create_pane(TUI_PANE_EDITOR);
    if (!pane) return NULL;
    
    tui_pane_set_title(pane, "SACD Information");
    pane->draw = draw_sacd_info;
    pane->handle_event = handle_sacd_info_event;
    
    return pane;
}

tui_pane_t *create_sacd_extract_pane(void) {
    tui_pane_t *pane = tui_create_pane(TUI_PANE_RESULTS);
    if (!pane) return NULL;
    
    tui_pane_set_title(pane, "Extraction Progress");
    
    /* Initialize extraction data */
    sacd_extract_data_t *extract_data = calloc(1, sizeof(sacd_extract_data_t));
    if (extract_data) {
        extract_data->selected_format = SACD_FORMAT_DSF;
        strncpy(extract_data->output_dir, "./extracted", sizeof(extract_data->output_dir) - 1);
        strncpy(extract_data->status_message, "Ready", sizeof(extract_data->status_message) - 1);
        extract_data->extraction_active = false;
        extract_data->percent_complete = 0;
        extract_data->start_time = 0;
        extract_data->last_update_time = 0;
        extract_data->last_percent = 0;
        extract_data->extraction_speed = 0.0;
        strncpy(extract_data->current_track_name, "", sizeof(extract_data->current_track_name) - 1);
        
        /* Initialize track selection */
        extract_data->track_selection.selected_tracks = NULL;
        extract_data->track_selection.track_count = 0;
        extract_data->track_selection.cursor_pos = 0;
        extract_data->track_selection.select_all = true;
        extract_data->track_selection.showing_selection = false;
    }
    
    pane->user_data = extract_data;
    pane->draw = draw_sacd_extract;
    
    /* Store pane reference for progress updates */
    if (extract_data) {
        extract_data->pane = pane;
    }
    
    return pane;
}

static void draw_sacd_browser(tui_pane_t *pane) {
    if (!pane || !pane->win) return;
    
    sacd_browser_data_t *data = (sacd_browser_data_t *)pane->user_data;
    if (!data) return;
    
    int h, w;
    getmaxyx(pane->win, h, w);
    
    /* Draw current directory path */
    wattron(pane->win, COLOR_PAIR(TUI_COLOR_STATUS));
    mvwprintw(pane->win, 0, 1, " %s [%d files] ", 
              data->current_dir ? data->current_dir : "(no dir)", 
              data->file_count);
    wattroff(pane->win, COLOR_PAIR(TUI_COLOR_STATUS));
    
    /* Draw files */
    file_entry_t *entry = data->files;
    int line = 1;
    int skip = data->scroll_offset;
    
    /* If no files, show a message */
    if (!entry) {
        mvwprintw(pane->win, 2, 1, "Empty directory");
    }
    
    /* Skip to visible start */
    while (entry && skip > 0) {
        entry = entry->next;
        skip--;
    }
    
    /* Draw visible entries */
    while (entry && line < h - 1) {
        bool selected = (entry == data->selected);
        
        if (selected) {
            wattron(pane->win, COLOR_PAIR(TUI_COLOR_HIGHLIGHT) | A_BOLD);
        }
        
        /* Choose icon and color based on type */
        const char *icon;
        int color = TUI_COLOR_NORMAL;
        
        if (entry->is_directory) {
            icon = "";
            color = TUI_COLOR_ACTIVE;
        } else if (entry->is_sacd) {
            icon = "[S]";
            color = TUI_COLOR_BUTTON;
        } else {
            icon = "[ ]";
            color = TUI_COLOR_INACTIVE;
        }
        
        if (!selected) {
            wattron(pane->win, COLOR_PAIR(color));
        }
        
        /* Format the line */
        char display_name[256];
        if (entry->is_directory) {
            snprintf(display_name, sizeof(display_name), "%s/", entry->name);
        } else {
            snprintf(display_name, sizeof(display_name), "%s %s", icon, entry->name);
        }
        
        mvwprintw(pane->win, line, 1, "%-*s", w - 2, display_name);
        
        if (selected) {
            wattroff(pane->win, COLOR_PAIR(TUI_COLOR_HIGHLIGHT) | A_BOLD);
        } else {
            wattroff(pane->win, COLOR_PAIR(color));
        }
        
        entry = entry->next;
        line++;
    }
}

static bool handle_sacd_browser_event(tui_pane_t *pane, const tui_event_t *event) {
    if (!pane || !event) return false;
    
    sacd_browser_data_t *data = (sacd_browser_data_t *)pane->user_data;
    if (!data) return false;
    
    if (event->type == TUI_EVENT_KEY) {
        switch (event->data.key.key) {
            case KEY_UP:
            case 'k':
                if (data->selected && data->selected->prev) {
                    data->selected = data->selected->prev;
                    
                    /* Adjust scroll if cursor moved above visible area */
                    int selected_index = 0;
                    file_entry_t *entry = data->files;
                    while (entry && entry != data->selected) {
                        entry = entry->next;
                        selected_index++;
                    }
                    
                    if (selected_index < data->scroll_offset) {
                        data->scroll_offset = selected_index;
                    }
                    
                    tui_pane_draw(pane);
                    return true;
                }
                break;
                
            case KEY_DOWN:
            case 'j':
                if (data->selected && data->selected->next) {
                    data->selected = data->selected->next;
                    
                    /* Adjust scroll if cursor moved below visible area */
                    int h, w __attribute__((unused));
                    getmaxyx(pane->win, h, w);
                    int visible_lines = h - 2; /* Account for header */
                    
                    int selected_index = 0;
                    file_entry_t *entry = data->files;
                    while (entry && entry != data->selected) {
                        entry = entry->next;
                        selected_index++;
                    }
                    
                    if (selected_index >= data->scroll_offset + visible_lines) {
                        data->scroll_offset = selected_index - visible_lines + 1;
                    }
                    
                    tui_pane_draw(pane);
                    return true;
                }
                break;
                
            case KEY_ENTER:
            case '\r':
            case '\n':
                if (data->selected) {
                    if (data->selected->is_directory) {
                        /* Change directory */
                        if (strcmp(data->selected->name, "..") == 0) {
                            /* Go to parent directory */
                            char parent_path[PATH_MAX];
                            char *slash = strrchr(data->current_dir, '/');
                            if (slash && slash != data->current_dir) {
                                /* Copy everything before the last slash */
                                size_t parent_len = slash - data->current_dir;
                                strncpy(parent_path, data->current_dir, parent_len);
                                parent_path[parent_len] = '\0';
                                load_directory(data, parent_path);
                            } else if (strcmp(data->current_dir, "/") != 0) {
                                /* Go to root */
                                load_directory(data, "/");
                            }
                        } else {
                            /* Enter subdirectory */
                            FILE *debug = fopen("/tmp/sacd_debug.log", "a");
                            if (debug) {
                                fprintf(debug, "=== ENTERING SUBDIRECTORY ===\n");
                                fprintf(debug, "Selected path: '%s'\n", data->selected->path);
                                fprintf(debug, "Selected name: '%s'\n", data->selected->name);
                                fprintf(debug, "Is directory: %s\n", data->selected->is_directory ? "yes" : "no");
                                fclose(debug);
                            }
                            /* Make a copy of the path to prevent memory corruption */
                            char *path_copy = malloc(strlen(data->selected->path) + 1);
                            if (path_copy) {
                                strcpy(path_copy, data->selected->path);
                                load_directory(data, path_copy);
                                free(path_copy);
                            }
                        }
                        tui_pane_draw(pane);
                        return true;
                    } else if (data->selected->is_sacd) {
                        /* Load SACD info */
                        if (data->current_sacd) {
                            cleanup_track_selection(data->current_sacd);
                            free(data->current_sacd);
                        }
                        data->current_sacd = calloc(1, sizeof(sacd_iso_info_t));
                        if (data->current_sacd) {
                            libsacd_read_iso_info(data->selected->path, data->current_sacd);
                            
                            /* Initialize track selection */
                            init_track_selection(data->current_sacd);
                            
                            /* Trigger redraw of all panes in the window to update info panel */
                            if (pane->window) {
                                for (int i = 0; i < pane->window->pane_count; i++) {
                                    if (pane->window->panes[i]) {
                                        tui_pane_draw(pane->window->panes[i]);
                                    }
                                }
                            }
                        }
                        return true;
                    }
                }
                break;
                
            case KEY_F(5):
                /* Start extraction of current SACD */
                {
                    FILE *debug = fopen("/tmp/f5_debug.log", "w");
                    if (debug) {
                        fprintf(debug, "F5 pressed!\n");
                        fprintf(debug, "current_sacd: %p\n", (void*)data->current_sacd);
                        if (data->current_sacd) {
                            fprintf(debug, "current_sacd->has_metadata: %s\n", data->current_sacd->has_metadata ? "true" : "false");
                            fprintf(debug, "current_sacd->title: %s\n", data->current_sacd->title);
                        }
                        fprintf(debug, "selected: %p\n", (void*)data->selected);
                        if (data->selected) {
                            fprintf(debug, "selected->path: %s\n", data->selected->path);
                            fprintf(debug, "selected->is_sacd: %s\n", data->selected->is_sacd ? "true" : "false");
                        }
                        fclose(debug);
                    }
                }
                
                if (data->current_sacd && data->current_sacd->has_metadata) {
                    /* Find the extract pane in the window and start extraction */
                    if (pane->window) {
                        for (int i = 0; i < pane->window->pane_count; i++) {
                            tui_pane_t *check_pane = pane->window->panes[i];
                            if (check_pane && check_pane->user_data) {
                                sacd_extract_data_t *extract_data = (sacd_extract_data_t*)check_pane->user_data;
                                /* Check if this is the extract pane by checking if it has extraction data */
                                if (extract_data && !extract_data->extraction_active) {
                                    start_extraction(extract_data, data->current_sacd, data->selected->path);
                                    break;
                                }
                            }
                        }
                    }
                    return true;
                }
                break;
        }
    }
    else if (event->type == TUI_EVENT_MOUSE) {
        /* Handle mouse click on file */
        if (event->data.mouse.pressed && event->data.mouse.y > 0) {
            int clicked_line = event->data.mouse.y - 1 + data->scroll_offset;
            
            /* Find the clicked entry */
            file_entry_t *entry = data->files;
            int count = 0;
            while (entry && count < clicked_line) {
                entry = entry->next;
                count++;
            }
            
            if (entry) {
                data->selected = entry;
                tui_pane_draw(pane);
                return true;
            }
        }
    }
    
    return false;
}

static bool handle_sacd_info_event(tui_pane_t *pane, const tui_event_t *event) {
    if (!pane || !event) return false;
    
    if (event->type == TUI_EVENT_KEY) {
        /* Find the current SACD from the browser pane */
        sacd_iso_info_t *current_sacd = NULL;
        if (pane->window) {
            for (int i = 0; i < pane->window->pane_count; i++) {
                tui_pane_t *other_pane = pane->window->panes[i];
                if (other_pane && other_pane->type == TUI_PANE_BROWSER && other_pane->user_data) {
                    sacd_browser_data_t *browser_data = (sacd_browser_data_t *)other_pane->user_data;
                    if (browser_data && browser_data->current_sacd) {
                        current_sacd = browser_data->current_sacd;
                        break;
                    }
                }
            }
        }
        
        if (!current_sacd || !current_sacd->has_metadata || !current_sacd->track_selected) {
            return false;
        }
        
        const sacd_area_t *primary_area = current_sacd->stereo_area ? 
                                         current_sacd->stereo_area : current_sacd->mulch_area;
        
        if (!primary_area) return false;
        
        switch (event->data.key.key) {
            case KEY_UP:
            case 'k':
                if (current_sacd->track_selection_cursor > 0) {
                    current_sacd->track_selection_cursor--;
                    tui_pane_draw(pane);
                    return true;
                }
                break;
                
            case KEY_DOWN:
            case 'j':
                if (current_sacd->track_selection_cursor < primary_area->track_count - 1) {
                    current_sacd->track_selection_cursor++;
                    tui_pane_draw(pane);
                    return true;
                }
                break;
                
            case ' ':  /* Space - toggle current track */
                current_sacd->track_selection_mode = true;
                toggle_track_selection(current_sacd, current_sacd->track_selection_cursor);
                tui_pane_draw(pane);
                return true;
                
            case 'a':  /* A - select all tracks */
            case 'A':
                current_sacd->track_selection_mode = true;
                select_all_tracks(current_sacd);
                tui_pane_draw(pane);
                return true;
                
            case 'n':  /* N - select no tracks */
            case 'N':
                current_sacd->track_selection_mode = true;
                select_no_tracks(current_sacd);
                tui_pane_draw(pane);
                return true;
                
            case KEY_ENTER:
            case '\r':
            case '\n':
                /* Enter track selection mode */
                current_sacd->track_selection_mode = !current_sacd->track_selection_mode;
                tui_pane_draw(pane);
                return true;
        }
    }
    
    return false;
}

static void draw_sacd_info(tui_pane_t *pane) {
    if (!pane || !pane->win) return;
    
    mvwaddstr(pane->win, 0, 1, "SACD Information:");
    
    /* Try to find the browser pane to get current SACD info */
    sacd_iso_info_t *current_sacd = NULL;
    if (pane->window) {
        /* Look for browser pane in the same window */
        for (int i = 0; i < pane->window->pane_count; i++) {
            tui_pane_t *other_pane = pane->window->panes[i];
            if (other_pane && other_pane->type == TUI_PANE_BROWSER && other_pane->user_data) {
                sacd_browser_data_t *browser_data = (sacd_browser_data_t *)other_pane->user_data;
                if (browser_data && browser_data->current_sacd) {
                    current_sacd = browser_data->current_sacd;
                    break;
                }
            }
        }
    }
    
    if (current_sacd && current_sacd->has_metadata) {
        /* Display real SACD information */
        mvwprintw(pane->win, 2, 1, "Album: %s", current_sacd->title);
        mvwprintw(pane->win, 3, 1, "Artist: %s", current_sacd->artist);
        mvwprintw(pane->win, 4, 1, "Year: %s", current_sacd->year);
        
        int y = 5;
        
        /* Show available areas */
        mvwprintw(pane->win, y++, 1, "Available Areas:");
        
        /* Two-channel area information */
        if (current_sacd->stereo_area) {
            const sacd_area_t *area = current_sacd->stereo_area;
            const char* config_name = (area->channel_count == 2) ? "Stereo" : "Multi-Channel";
            
            mvwprintw(pane->win, y++, 3, "2-Channel: %d tracks, %s (%d ch)", 
                     area->track_count, config_name, area->channel_count);
            
            /* Calculate total duration from tracks */
            double total_seconds = 0;
            for (int i = 0; i < area->track_count; i++) {
                total_seconds += sacd_time_to_seconds(&area->tracks[i].duration);
            }
            int total_minutes = (int)(total_seconds / 60);
            int remaining_seconds = (int)total_seconds % 60;
            mvwprintw(pane->win, y++, 3, "Duration: %d:%02d", total_minutes, remaining_seconds);
        }
        
        /* Multi-channel area information */
        if (current_sacd->mulch_area) {
            const sacd_area_t *area = current_sacd->mulch_area;
            const char* config_name;
            switch (area->channel_count) {
                case 5: config_name = "5.0 Surround"; break;
                case 6: config_name = "5.1 Surround"; break;
                default: config_name = "Multi-Channel"; break;
            }
            
            mvwprintw(pane->win, y++, 3, "Multi-Channel: %d tracks, %s (%d ch)", 
                     area->track_count, config_name, area->channel_count);
            
            /* Calculate total duration from tracks */
            double total_seconds = 0;
            for (int i = 0; i < area->track_count; i++) {
                total_seconds += sacd_time_to_seconds(&area->tracks[i].duration);
            }
            int total_minutes = (int)(total_seconds / 60);
            int remaining_seconds = (int)total_seconds % 60;
            mvwprintw(pane->win, y++, 3, "Duration: %d:%02d", total_minutes, remaining_seconds);
        }
        
        y++; /* Add spacing */
        
        /* Show track selection interface */
        const sacd_area_t *primary_area = current_sacd->stereo_area ? 
                                         current_sacd->stereo_area : current_sacd->mulch_area;
        
        if (primary_area && current_sacd->track_selected) {
            /* Track selection header */
            mvwprintw(pane->win, y++, 1, "Track Selection (Space=toggle, A=all, N=none):");
            y++; /* Add spacing */
            
            /* Calculate display parameters */
            int h, w __attribute__((unused));
            getmaxyx(pane->win, h, w);
            int max_tracks_display = h - y - 6; /* Leave room for summary and controls */
            int tracks_to_show = (primary_area->track_count < max_tracks_display) ? 
                                primary_area->track_count : max_tracks_display;
            
            /* Draw track list with checkmarks */
            for (int i = 0; i < tracks_to_show; i++) {
                const sacd_track_t *track = &primary_area->tracks[i];
                char duration_str[16];
                double track_seconds = sacd_time_to_seconds(&track->duration);
                libsacd_format_duration(track_seconds, duration_str, sizeof(duration_str));
                
                const char *track_title = track->text.title ? track->text.title : "Unknown Track";
                
                /* Highlight current cursor position */
                if (i == current_sacd->track_selection_cursor) {
                    wattron(pane->win, A_REVERSE);
                }
                
                /* Draw checkmark or space */
                if (current_sacd->track_selected[i]) {
                    wattron(pane->win, COLOR_PAIR(2)); /* Green for selected */
                    mvwprintw(pane->win, y, 1, "✓ %02d - %s", track->number + 1, track_title);
                    wattroff(pane->win, COLOR_PAIR(2));
                } else {
                    mvwprintw(pane->win, y, 1, "  %02d - %s", track->number + 1, track_title);
                }
                
                /* Show duration aligned to the right */
                mvwprintw(pane->win, y, 50, "%s", duration_str);
                
                if (i == current_sacd->track_selection_cursor) {
                    wattroff(pane->win, A_REVERSE);
                }
                
                y++;
            }
            
            if (primary_area->track_count > tracks_to_show) {
                mvwprintw(pane->win, y++, 1, "... and %d more tracks", 
                         primary_area->track_count - tracks_to_show);
            }
            
            y++; /* Add spacing */
            
            /* Show selection summary */
            int selected_count = count_selected_tracks(current_sacd);
            double selected_duration = calculate_selected_duration(current_sacd);
            int selected_minutes = (int)(selected_duration / 60);
            int selected_seconds = (int)selected_duration % 60;
            
            mvwprintw(pane->win, y++, 1, "Selected: %d/%d tracks (~%d:%02d)", 
                     selected_count, primary_area->track_count, 
                     selected_minutes, selected_seconds);
        }
        
        /* Show format and file size in remaining space */
        mvwprintw(pane->win, y++, 1, "Format: DSD64");
        
        char size_str[32];
        libsacd_format_filesize(current_sacd->file_size, size_str, sizeof(size_str));
        mvwprintw(pane->win, y++, 1, "File Size: %s", size_str);
        
    } else {
        /* No SACD selected - show placeholder */
        mvwaddstr(pane->win, 2, 1, "No SACD selected");
        mvwaddstr(pane->win, 4, 1, "Select an ISO file in the browser");
        mvwaddstr(pane->win, 5, 1, "to view its information");
    }
}

static void draw_sacd_extract(tui_pane_t *pane) {
    if (!pane || !pane->win) return;
    
    sacd_extract_data_t *extract_data = (sacd_extract_data_t*)pane->user_data;
    if (!extract_data) return;
    
    int y = 0;
    mvwaddstr(pane->win, y++, 1, "=== EXTRACTION STATUS ===");
    y++; /* blank line */
    
    /* Status */
    if (extract_data->extraction_active && extract_data->libsacd_extractor) {
        /* Main status message */
        mvwaddstr(pane->win, y++, 1, extract_data->status_message);
        y++; /* blank line */
        
        /* Enhanced progress bar with better visual */
        int bar_width = 60;
        int filled = (extract_data->percent_complete * bar_width) / 100;
        
        mvwaddstr(pane->win, y++, 1, "Progress:");
        mvwaddch(pane->win, y, 1, '[');
        
        /* Draw filled portion in green */
        wattron(pane->win, COLOR_PAIR(2)); /* Assuming green is color pair 2 */
        for (int i = 0; i < filled; i++) {
            mvwaddch(pane->win, y, 2 + i, '#');
        }
        wattroff(pane->win, COLOR_PAIR(2));
        
        /* Draw empty portion */
        for (int i = filled; i < bar_width; i++) {
            mvwaddch(pane->win, y, 2 + i, '.');
        }
        
        mvwaddch(pane->win, y, 2 + bar_width, ']');
        mvwprintw(pane->win, y, 4 + bar_width, " %3d%%", extract_data->percent_complete);
        y += 2; /* move past progress bar */
        
        /* Track information */
        if (sacd_extractor_is_running(extract_data->libsacd_extractor)) {
            mvwprintw(pane->win, y++, 1, "Extracting SACD tracks...");
            
            if (strlen(extract_data->current_track_name) > 0) {
                mvwprintw(pane->win, y++, 1, "Track Name: %s", extract_data->current_track_name);
            }
        }
        
        /* Time and speed statistics */
        if (extract_data->start_time > 0) {
            time_t current_time = time(NULL);
            int elapsed = (int)(current_time - extract_data->start_time);
            int eta = 0;
            
            if (extract_data->percent_complete > 5) {
                eta = (elapsed * (100 - extract_data->percent_complete)) / extract_data->percent_complete;
            }
            
            mvwprintw(pane->win, y++, 1, "Elapsed: %02d:%02d  ETA: %02d:%02d", 
                    elapsed / 60, elapsed % 60, eta / 60, eta % 60);
        }
        
        /* Extraction statistics */
        mvwprintw(pane->win, y++, 1, "Format: %s", 
                sacd_format_description(extract_data->selected_format));
        mvwprintw(pane->win, y++, 1, "Output: %s", extract_data->output_dir);
        
        y++; /* blank line */
        
        /* Real-time status indicators */
        if (sacd_extractor_is_running(extract_data->libsacd_extractor)) {
            mvwaddstr(pane->win, y++, 1, "🎵 Extracting audio data...");
        } else if (extract_data->percent_complete >= 100) {
            wattron(pane->win, COLOR_PAIR(2)); /* Green */
            mvwaddstr(pane->win, y++, 1, "✓ Extraction completed successfully!");
            wattroff(pane->win, COLOR_PAIR(2));
        } else {
            mvwaddstr(pane->win, y++, 1, "⚙️  Preparing extraction...");
        }
        
    } else {
        mvwaddstr(pane->win, y++, 1, "Status: Ready for extraction");
        y++;
        mvwaddstr(pane->win, y++, 1, "[WAIT] Waiting for SACD selection...");
    }
    
    y++; /* blank line */
    
    /* Controls and instructions */
    if (!extract_data->extraction_active) {
        mvwaddstr(pane->win, y++, 1, "====== CONTROLS ======");
        mvwaddstr(pane->win, y++, 1, "F5 - Start extraction");
        mvwaddstr(pane->win, y++, 1, "F6 - Select tracks (coming soon)");
        mvwaddstr(pane->win, y++, 1, "F7 - Change format (coming soon)");
        mvwaddstr(pane->win, y++, 1, "F8 - Output settings (coming soon)");
    } else {
        mvwaddstr(pane->win, y++, 1, "====== CONTROLS ======");
        wattron(pane->win, COLOR_PAIR(1)); /* Red for cancel */
        mvwaddstr(pane->win, y++, 1, "ESC - Cancel extraction");
        wattroff(pane->win, COLOR_PAIR(1));
    }
}

static int load_directory(sacd_browser_data_t *data, const char *path) {
    FILE *debug_f = fopen("/tmp/sacd_debug.log", "a");
    if (debug_f) {
        fprintf(debug_f, "=== LOAD_DIRECTORY START ===\n");
        fprintf(debug_f, "Trying to load path: '%s'\n", path ? path : "(null)");
        fprintf(debug_f, "Data pointer: %p\n", (void*)data);
        fclose(debug_f);
    }
    
    if (!data || !path) {
        FILE *debug_f2 = fopen("/tmp/sacd_debug.log", "a");
        if (debug_f2) {
            fprintf(debug_f2, "ERROR: data=%p, path=%p - returning -1\n", (void*)data, (void*)path);
            fclose(debug_f2);
        }
        return -1;
    }
    
    /* Free existing file list */
    free_file_list(data);
    
    /* Update current directory - manual copy instead of strdup */
    if (data->current_dir) {
        free(data->current_dir);
    }
    
    /* Manual string copy to avoid strdup issues */
    size_t len = strlen(path);
    data->current_dir = malloc(len + 1);
    if (data->current_dir) {
        strcpy(data->current_dir, path);
    } else {
        return -1;
    }
    
    DIR *dir = opendir(path);
    if (!dir) {
        /* If opendir fails, restore the directory state */
        FILE *debug_f3 = fopen("/tmp/sacd_debug.log", "a");
        if (debug_f3) {
            fprintf(debug_f3, "ERROR: opendir('%s') failed: %s\n", path, strerror(errno));
            fclose(debug_f3);
        }
        free(data->current_dir);
        data->current_dir = malloc(2);
        strcpy(data->current_dir, ".");
        return -1;
    }
    
    FILE *debug_f4 = fopen("/tmp/sacd_debug.log", "a");
    if (debug_f4) {
        fprintf(debug_f4, "SUCCESS: opendir('%s') succeeded\n", path);
        fclose(debug_f4);
    }
    
    file_entry_t *head = NULL;
    file_entry_t *tail = NULL;
    int count = 0;
    
    /* Add parent directory entry if not root */
    if (strcmp(path, "/") != 0) {
        file_entry_t *parent = calloc(1, sizeof(file_entry_t));
        if (parent) {
            parent->name = malloc(3);
            strcpy(parent->name, "..");
            parent->path = malloc(1);
            strcpy(parent->path, ""); /* Will be handled specially */
            parent->is_directory = true;
            parent->is_sacd = false;
            
            head = tail = parent;
            count++;
        }
    }
    
    /* Read directory entries */
    struct dirent *de;
    while ((de = readdir(dir)) != NULL) {
        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) {
            continue;
        }
        
        char full_path[4096];
        snprintf(full_path, sizeof(full_path), "%s/%s", 
                 strcmp(path, "/") == 0 ? "" : path, de->d_name);
        
        struct stat st;
        if (stat(full_path, &st) != 0) continue;
        
        file_entry_t *entry = calloc(1, sizeof(file_entry_t));
        if (!entry) continue;
        
        /* Manual string copy */
        size_t name_len = strlen(de->d_name);
        entry->name = malloc(name_len + 1);
        if (entry->name) strcpy(entry->name, de->d_name);
        
        size_t path_len = strlen(full_path);
        entry->path = malloc(path_len + 1);
        if (entry->path) strcpy(entry->path, full_path);
        entry->is_directory = S_ISDIR(st.st_mode);
        entry->is_sacd = !entry->is_directory && libsacd_is_valid_iso(full_path);
        entry->size = st.st_size;
        
        
        /* Only add directories and audio/video files */
        if (entry->is_directory || is_audio_video_file(de->d_name)) {
            /* Add to list */
            if (tail) {
                tail->next = entry;
                entry->prev = tail;
                tail = entry;
            } else {
                head = tail = entry;
            }
            count++;
        } else {
            /* Skip this file - free the allocated memory */
            free(entry->name);
            free(entry->path);
            free(entry);
        }
    }
    
    closedir(dir);
    
    /* Sort the file list alphabetically (directories first, then files) */
    if (head && count > 1) {
        /* Convert linked list to array for sorting */
        file_entry_t **entries = malloc(count * sizeof(file_entry_t*));
        if (entries) {
            file_entry_t *current = head;
            for (int i = 0; i < count; i++) {
                entries[i] = current;
                current = current->next;
            }
            
            /* Sort using qsort with custom comparator */
            qsort(entries, count, sizeof(file_entry_t*), file_entry_compare);
            
            /* Rebuild linked list in sorted order */
            head = entries[0];
            head->prev = NULL;
            for (int i = 0; i < count; i++) {
                entries[i]->prev = (i > 0) ? entries[i-1] : NULL;
                entries[i]->next = (i < count-1) ? entries[i+1] : NULL;
            }
            tail = entries[count-1];
            
            free(entries);
        }
    }
    
    /* Debug: write directory contents to a file */
    FILE *debug = fopen("/tmp/sacd_debug.log", "a");
    if (debug) {
        fprintf(debug, "=== Loading directory: %s ===\n", path);
        fprintf(debug, "Found %d entries:\n", count);
        file_entry_t *dbg_entry = head;
        while (dbg_entry) {
            fprintf(debug, "  %s %s (%s)\n", 
                    dbg_entry->is_directory ? " " : 
                    (dbg_entry->is_sacd ? "[S]" : "[ ]"),
                    dbg_entry->name, dbg_entry->path);
            dbg_entry = dbg_entry->next;
        }
        fprintf(debug, "=============================\n");
        fclose(debug);
    }
    
    data->files = head;
    data->selected = head;
    data->file_count = count;
    data->scroll_offset = 0;
    
    return 0;
}

static void free_file_list(sacd_browser_data_t *data) {
    if (!data) return;
    
    file_entry_t *entry = data->files;
    while (entry) {
        file_entry_t *next = entry->next;
        free(entry->name);
        free(entry->path);
        free(entry);
        entry = next;
    }
    
    data->files = NULL;
    data->selected = NULL;
    data->file_count = 0;
    data->scroll_offset = 0;
}

static int file_entry_compare(const void *a, const void *b) {
    file_entry_t *ea = *(file_entry_t**)a;
    file_entry_t *eb = *(file_entry_t**)b;
    
    /* Parent directory (..) always first */
    if (strcmp(ea->name, "..") == 0) return -1;
    if (strcmp(eb->name, "..") == 0) return 1;
    
    /* Directories before files */
    if (ea->is_directory != eb->is_directory) {
        return eb->is_directory - ea->is_directory;
    }
    
    /* Alphabetical within same type */
    return strcasecmp(ea->name, eb->name);
}

static bool is_audio_video_file(const char *filename) {
    if (!filename) return false;
    
    const char *dot = strrchr(filename, '.');
    if (!dot) return false;
    
    /* Convert extension to lowercase for comparison */
    char ext[16];
    strncpy(ext, dot + 1, sizeof(ext) - 1);
    ext[sizeof(ext) - 1] = '\0';
    
    for (char *p = ext; *p; p++) {
        *p = tolower(*p);
    }
    
    /* Audio formats */
    if (strcmp(ext, "iso") == 0) return true;    /* SACD ISO */
    if (strcmp(ext, "flac") == 0) return true;   /* FLAC */
    if (strcmp(ext, "dsf") == 0) return true;    /* DSD Stream File */
    if (strcmp(ext, "dff") == 0) return true;    /* DSD Interchange File */
    if (strcmp(ext, "wav") == 0) return true;    /* WAV */
    if (strcmp(ext, "aiff") == 0) return true;   /* AIFF */
    if (strcmp(ext, "mp3") == 0) return true;    /* MP3 */
    if (strcmp(ext, "m4a") == 0) return true;    /* M4A */
    if (strcmp(ext, "aac") == 0) return true;    /* AAC */
    if (strcmp(ext, "ogg") == 0) return true;    /* OGG */
    if (strcmp(ext, "opus") == 0) return true;   /* Opus */
    
    /* Video formats (often contain audio) */
    if (strcmp(ext, "mkv") == 0) return true;    /* Matroska */
    if (strcmp(ext, "mp4") == 0) return true;    /* MP4 */
    if (strcmp(ext, "m4v") == 0) return true;    /* M4V */
    if (strcmp(ext, "avi") == 0) return true;    /* AVI */
    if (strcmp(ext, "mov") == 0) return true;    /* QuickTime */
    if (strcmp(ext, "webm") == 0) return true;   /* WebM */
    
    /* Metadata files that might be useful */
    if (strcmp(ext, "xml") == 0) return true;    /* SACD metadata */
    if (strcmp(ext, "cue") == 0) return true;    /* Cue sheet */
    if (strcmp(ext, "log") == 0) return true;    /* Rip log */
    
    return false;
}

/* TUI progress callback for new libsacd API */
static void tui_progress_callback(
    int track_number,              /* Current track (1-based) */
    int total_tracks,              /* Total tracks */
    int track_progress_percent __attribute__((unused)),    /* Progress within current track (0-100) */
    int overall_progress_percent,  /* Overall progress (0-100) */
    const char *status_message,    /* Human-readable status */
    void *userdata                 /* User-provided data */
) {
    sacd_extract_data_t *extract_data = (sacd_extract_data_t*)userdata;
    if (!extract_data) return;
    
    /* Update progress data with timing */
    time_t current_time = time(NULL);
    
    if (extract_data->start_time == 0) {
        extract_data->start_time = current_time;
    }
    
    /* Throttle UI updates - only update every 1 second OR when progress changes by 1% */
    static time_t last_ui_update = 0;
    static int last_ui_percent = -1;
    
    bool should_update = false;
    
    /* Update if enough time has passed (1 second) */
    if (current_time - last_ui_update >= 1) {
        should_update = true;
    }
    
    /* Update if progress changed by at least 1% */
    if (overall_progress_percent != last_ui_percent) {
        should_update = true;
    }
    
    /* Always update progress data, but conditionally update UI */
    extract_data->last_update_time = current_time;
    extract_data->last_percent = extract_data->percent_complete;
    extract_data->percent_complete = overall_progress_percent;
    
    strncpy(extract_data->status_message, status_message, sizeof(extract_data->status_message) - 1);
    extract_data->status_message[sizeof(extract_data->status_message) - 1] = '\0';
    
    /* Update current track name */
    snprintf(extract_data->current_track_name, sizeof(extract_data->current_track_name), 
             "Track %d of %d", track_number, total_tracks);
    
    /* Only update UI when throttle conditions are met */
    if (should_update && extract_data->pane && extract_data->pane->draw) {
        /* Clear the pane first for clean redraw */
        werase(extract_data->pane->win);
        box(extract_data->pane->win, 0, 0);
        
        /* Redraw with updated information */
        extract_data->pane->draw(extract_data->pane);
        
        /* Force immediate screen refresh */
        wrefresh(extract_data->pane->win);
        doupdate(); /* Ensure all updates are displayed immediately */
        
        /* Update throttle state */
        last_ui_update = current_time;
        last_ui_percent = overall_progress_percent;
    }
}

/* Old callback function removed - now using libsacd API directly */

/* Start extraction of current SACD */
static void start_extraction(sacd_extract_data_t *extract_data, sacd_iso_info_t *iso_info, const char *iso_path) {
    if (!extract_data || !iso_info || !iso_path) return;
    
    /* Don't start if extraction is already active */
    if (extract_data->extraction_active) return;
    
    /* USE NEW LIBSACD API FOR REAL EXTRACTION */
    printf("Starting REAL SACD extraction using libsacd...\n");
    
    /* Open disc with new libsacd API */
    sacd_disc_t *disc = NULL;
    sacd_result_t result = sacd_disc_open(iso_path, &disc);
    if (result != SACD_RESULT_OK) {
        snprintf(extract_data->status_message, sizeof(extract_data->status_message), 
                "Failed to open SACD: %s", sacd_result_string(result));
        return;
    }
    
    /* Get stereo area (prefer stereo over multichannel for TUI) */
    const sacd_area_t *area = sacd_disc_get_area(disc, SACD_AREA_STEREO);
    if (!area) {
        area = sacd_disc_get_area(disc, SACD_AREA_MULTICHANNEL);
    }
    
    if (!area) {
        strncpy(extract_data->status_message, "No playable areas found", 
               sizeof(extract_data->status_message) - 1);
        sacd_disc_close(disc);
        return;
    }
    
    printf("Found area with %d tracks\n", area->track_count);
    
    /* Create output directory */
    system("mkdir -p ./extracted");
    
    /* Set up real libsacd extraction options */
    sacd_extraction_options_t libsacd_options;
    sacd_extraction_options_init(&libsacd_options);
    libsacd_options.format = SACD_FORMAT_DSF;
    
    /* Set up progress callback to update TUI */
    libsacd_options.progress_callback = tui_progress_callback;
    libsacd_options.callback_userdata = extract_data;
    
    /* Create real extractor */
    sacd_extractor_t *extractor = NULL;
    result = sacd_extractor_create(disc, area, "./extracted", &libsacd_options, &extractor);
    if (result != SACD_RESULT_OK) {
        snprintf(extract_data->status_message, sizeof(extract_data->status_message), 
                "Failed to create extractor: %s", sacd_result_string(result));
        sacd_disc_close(disc);
        return;
    }
    
    /* Add all tracks for extraction */
    result = sacd_extractor_add_all_tracks(extractor);
    if (result != SACD_RESULT_OK) {
        snprintf(extract_data->status_message, sizeof(extract_data->status_message), 
                "Failed to add tracks: %s", sacd_result_string(result));
        sacd_extractor_destroy(extractor);
        sacd_disc_close(disc);
        return;
    }
    
    /* Start real extraction */
    result = sacd_extractor_start(extractor);
    if (result != SACD_RESULT_OK) {
        snprintf(extract_data->status_message, sizeof(extract_data->status_message), 
                "Failed to start extraction: %s", sacd_result_string(result));
        sacd_extractor_destroy(extractor);
        sacd_disc_close(disc);
        return;
    }
    
    /* Store extractor for cleanup later */
    extract_data->libsacd_extractor = extractor;
    extract_data->libsacd_disc = disc;
    extract_data->extraction_active = true;
    
    snprintf(extract_data->status_message, sizeof(extract_data->status_message), 
            "Extracting %d tracks with real SACD library...", area->track_count);
    
    extract_data->percent_complete = 0;
}

