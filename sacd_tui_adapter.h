#ifndef SACD_TUI_ADAPTER_H
#define SACD_TUI_ADAPTER_H

#include "libtui/include/tui.h"
/* Use new libsacd API instead of old conflicting headers */
#include "libsacd/sacd_lib.h"
#include <dirent.h>
#include <sys/types.h>

/* Simple file entry for libtui browser */
typedef struct file_entry {
    char *name;
    char *path;
    bool is_directory;
    bool is_sacd;
    off_t size;
    struct file_entry *next;
    struct file_entry *prev;
} file_entry_t;

/* SACD ISO information structure */
typedef struct {
    char title[256];
    char artist[256];
    char year[16];
    int total_tracks;
    const sacd_area_t *stereo_area;   /* Direct reference to libsacd area */
    const sacd_area_t *mulch_area;    /* Direct reference to libsacd area */
    bool has_metadata;
    size_t file_size;
    
    /* Track selection state */
    bool *track_selected;             /* Array of selected track flags */
    int track_selection_cursor;       /* Current cursor position in track list */
    bool track_selection_mode;        /* True when in track selection mode */
} sacd_iso_info_t;

/* SACD-specific pane data */
typedef struct {
    char *current_dir;
    file_entry_t *files;
    file_entry_t *selected;
    int file_count;
    int scroll_offset;
    sacd_disc_t *current_disc;        /* Direct libsacd disc handle */
    sacd_iso_info_t *current_sacd;    /* Cached metadata */
} sacd_browser_data_t;

/* Forward declaration */
struct tui_pane;

/* Track selection state */
typedef struct {
    bool *selected_tracks; /* Array of selected track flags */
    int track_count;
    int cursor_pos;
    bool select_all;
    bool showing_selection; /* True when track selection dialog is active */
} track_selection_t;

/* Extraction progress data */
typedef struct {
    char status_message[256];
    int percent_complete;
    bool extraction_active;
    sacd_output_format_t selected_format; /* Use new libsacd format enum */
    char output_dir[512];
    struct tui_pane *pane; /* Reference to the extraction pane for redraws */
    track_selection_t track_selection; /* Track selection state */
    
    /* NEW LIBSACD API */
    sacd_extractor_t *libsacd_extractor;
    sacd_disc_t *libsacd_disc;
    
    /* Enhanced progress tracking */
    time_t start_time;
    time_t last_update_time;
    int last_percent;
    char current_track_name[256];
    double extraction_speed; /* MB/s or tracks/min */
} sacd_extract_data_t;

/* Initialize SACD browser pane */
tui_pane_t *create_sacd_browser_pane(void);

/* Initialize SACD info pane */
tui_pane_t *create_sacd_info_pane(void);

/* Track selection helper functions */
void init_track_selection(sacd_iso_info_t *sacd_info);
void cleanup_track_selection(sacd_iso_info_t *sacd_info);
void toggle_track_selection(sacd_iso_info_t *sacd_info, int track_index);
void select_all_tracks(sacd_iso_info_t *sacd_info);
void select_no_tracks(sacd_iso_info_t *sacd_info);
int count_selected_tracks(sacd_iso_info_t *sacd_info);
double calculate_selected_duration(sacd_iso_info_t *sacd_info);

/* Initialize extraction progress pane */
tui_pane_t *create_sacd_extract_pane(void);

#endif /* SACD_TUI_ADAPTER_H */