/**
 * SACD Library - Self-contained SACD extraction library
 * 
 * A clean, modern library for reading and extracting SACD (Super Audio CD) files.
 * Provides full control over the extraction process with rich progress reporting.
 *
 * Copyright (c) 2024
 * Licensed under MIT License
 */

#ifndef SACD_LIB_H
#define SACD_LIB_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Library version */
#define SACD_LIB_VERSION_MAJOR 1
#define SACD_LIB_VERSION_MINOR 0
#define SACD_LIB_VERSION_PATCH 0

/* Constants */
#define SACD_MAX_TRACKS        255
#define SACD_MAX_AREAS         2
#define SACD_MAX_LANGUAGES     8
#define SACD_LSN_SIZE          2048
#define SACD_SAMPLING_FREQ     2822400
#define SACD_FRAME_RATE        75

/* Forward declarations */
typedef struct sacd_disc sacd_disc_t;
typedef struct sacd_area sacd_area_t;
typedef struct sacd_track sacd_track_t;
typedef struct sacd_extractor sacd_extractor_t;

/* Enumerations */
typedef enum {
    SACD_AREA_STEREO = 0,    /* 2-channel area */
    SACD_AREA_MULTICHANNEL   /* Multi-channel area */
} sacd_area_type_t;

typedef enum {
    SACD_FORMAT_DSF = 0,     /* Sony DSD Stream File */
    SACD_FORMAT_DSDIFF,      /* DSD Interchange File Format */
    SACD_FORMAT_DSDIFF_EM    /* DSDIFF Edit Master */
} sacd_output_format_t;

typedef enum {
    SACD_FRAME_DST = 0,      /* DST compressed */
    SACD_FRAME_DSD_3_IN_14,  /* DSD 3-in-14 */
    SACD_FRAME_DSD_3_IN_16   /* DSD 3-in-16 */
} sacd_frame_format_t;

typedef enum {
    SACD_RESULT_OK = 0,
    SACD_RESULT_ERROR,
    SACD_RESULT_INVALID_FILE,
    SACD_RESULT_INVALID_AREA,
    SACD_RESULT_INVALID_TRACK,
    SACD_RESULT_OUT_OF_MEMORY,
    SACD_RESULT_IO_ERROR,
    SACD_RESULT_CANCELLED
} sacd_result_t;

/* Character sets for text fields */
typedef enum {
    SACD_CHARSET_UNKNOWN = 0,
    SACD_CHARSET_ISO646,
    SACD_CHARSET_ISO8859_1,
    SACD_CHARSET_MUSIC_SHIFT_JIS,
    SACD_CHARSET_KSC5601,
    SACD_CHARSET_GB2312,
    SACD_CHARSET_BIG5,
    SACD_CHARSET_ISO8859_1_ESC
} sacd_charset_t;

/* Genre information */
typedef struct {
    uint8_t category;
    uint8_t genre;
} sacd_genre_t;

/* Time representation */
typedef struct {
    uint8_t minutes;
    uint8_t seconds;
    uint8_t frames;
} sacd_time_t;

/* Text information structure */
typedef struct {
    char *title;
    char *title_phonetic;
    char *artist;
    char *artist_phonetic;
    char *publisher;
    char *publisher_phonetic;
    char *copyright;
    char *copyright_phonetic;
} sacd_text_t;

/* Track information */
struct sacd_track {
    int number;                    /* Track number (0-based) */
    sacd_time_t start_time;        /* Track start time */
    sacd_time_t duration;          /* Track duration */
    uint32_t start_lsn;            /* Starting logical sector */
    uint32_t length_lsn;           /* Length in logical sectors */
    
    sacd_text_t text;              /* Track text information */
    sacd_genre_t genre;            /* Track genre */
    char isrc[13];                 /* ISRC code */
    
    /* Audio properties */
    int channel_count;             /* Number of channels */
    sacd_frame_format_t frame_format; /* Frame format */
    bool dst_encoded;              /* True if DST compressed */
    
    /* Track flags */
    bool copyright_protected;
    bool pre_emphasis;
    bool track_flags[4];           /* Additional track flags */
};

/* Area information */
struct sacd_area {
    sacd_area_type_t type;         /* Area type (stereo/multichannel) */
    int track_count;               /* Number of tracks in this area */
    sacd_track_t tracks[SACD_MAX_TRACKS]; /* Track information */
    
    sacd_text_t text;              /* Area text information */
    
    /* Audio properties */
    int channel_count;             /* Number of channels */
    uint32_t max_user_data_block_size; /* Maximum block size */
    
    /* Area bounds */
    uint32_t start_lsn;            /* Area start LSN */
    uint32_t end_lsn;              /* Area end LSN */
    
    /* Sample rate and bit information */
    uint32_t sample_frequency;     /* Sample frequency */
    uint8_t channel_assignment;    /* Channel assignment */
};

/* Disc information */
struct sacd_disc {
    /* Disc metadata */
    sacd_text_t text;              /* Album/disc text information */
    sacd_genre_t genres[4];        /* Disc genres */
    char catalog_number[17];       /* Catalog number */
    char disc_catalog_number[17];  /* Disc catalog number */
    
    /* Version and technical info */
    uint8_t version_major;         /* Version major */
    uint8_t version_minor;         /* Version minor */
    bool is_hybrid;                /* Hybrid SACD (CD layer present) */
    
    /* Date information */
    uint16_t year;
    uint8_t month;
    uint8_t day;
    
    /* Areas */
    int area_count;                /* Number of areas (1 or 2) */
    sacd_area_t areas[SACD_MAX_AREAS]; /* Area information */
    
    /* Internal data */
    void *internal_data;           /* Private library data */
};

/* Progress callback function types */
typedef void (*sacd_progress_callback_t)(
    int track_number,              /* Current track (1-based) */
    int total_tracks,              /* Total tracks */
    int track_progress_percent,    /* Progress within current track (0-100) */
    int overall_progress_percent,  /* Overall progress (0-100) */
    const char *status_message,    /* Human-readable status */
    void *userdata                 /* User-provided data */
);

typedef void (*sacd_track_start_callback_t)(
    int track_number,              /* Track being started (1-based) */
    const sacd_track_t *track,     /* Track information */
    const char *output_filename,   /* Output filename */
    void *userdata                 /* User-provided data */
);

typedef void (*sacd_track_complete_callback_t)(
    int track_number,              /* Track completed (1-based) */
    const sacd_track_t *track,     /* Track information */
    const char *output_filename,   /* Output filename */
    size_t bytes_written,          /* Bytes written to file */
    void *userdata                 /* User-provided data */
);

/* Extraction options */
typedef struct {
    sacd_output_format_t format;   /* Output format */
    bool convert_dst;              /* Convert DST to DSD */
    bool export_cue_sheet;         /* Export cue sheet */
    bool include_pauses;           /* Include track pauses */
    bool trim_whitespace;          /* Trim whitespace from filenames */
    
    /* DSF-specific options */
    bool dsf_nopad;                /* Don't pad DSF files */
    
    /* ID3 tag options */
    bool add_id3_tags;             /* Add ID3 tags to files */
    int id3_version;               /* ID3 version (3 or 4) */
    
    /* Filename options */
    bool add_artist_to_folder;     /* Add artist to folder name */
    bool add_performer_to_filename; /* Add performer to filename */
    
    /* Progress callbacks */
    sacd_progress_callback_t progress_callback;
    sacd_track_start_callback_t track_start_callback;
    sacd_track_complete_callback_t track_complete_callback;
    void *callback_userdata;       /* User data for callbacks */
} sacd_extraction_options_t;

/* Extractor opaque structure */
struct sacd_extractor {
    void *internal_data;               /* Internal implementation data */
};

/* Main library functions */

/**
 * Open an SACD ISO file and parse its structure
 * 
 * @param iso_path Path to the SACD ISO file
 * @param disc Pointer to receive disc information
 * @return SACD_RESULT_OK on success, error code on failure
 */
sacd_result_t sacd_disc_open(const char *iso_path, sacd_disc_t **disc);

/**
 * Close an SACD disc and free all associated memory
 * 
 * @param disc Disc to close
 */
void sacd_disc_close(sacd_disc_t *disc);

/**
 * Get a specific area from the disc
 * 
 * @param disc The disc
 * @param area_type Area type to retrieve
 * @return Pointer to area, or NULL if not present
 */
const sacd_area_t *sacd_disc_get_area(const sacd_disc_t *disc, sacd_area_type_t area_type);

/**
 * Get the best available area (prefers stereo, falls back to multichannel)
 * 
 * @param disc The disc
 * @return Pointer to best area, or NULL if no areas
 */
const sacd_area_t *sacd_disc_get_best_area(const sacd_disc_t *disc);

/**
 * Create an extractor for the specified area
 * 
 * @param disc The disc
 * @param area The area to extract from
 * @param output_dir Output directory for files
 * @param options Extraction options
 * @param extractor Pointer to receive extractor
 * @return SACD_RESULT_OK on success, error code on failure
 */
sacd_result_t sacd_extractor_create(
    const sacd_disc_t *disc,
    const sacd_area_t *area,
    const char *output_dir,
    const sacd_extraction_options_t *options,
    sacd_extractor_t **extractor
);

/**
 * Destroy an extractor and free all associated memory
 * 
 * @param extractor Extractor to destroy
 */
void sacd_extractor_destroy(sacd_extractor_t *extractor);

/**
 * Add tracks to the extraction queue
 * 
 * @param extractor The extractor
 * @param track_numbers Array of track numbers (0-based)
 * @param track_count Number of tracks to add
 * @return SACD_RESULT_OK on success, error code on failure
 */
sacd_result_t sacd_extractor_add_tracks(
    sacd_extractor_t *extractor,
    const int *track_numbers,
    int track_count
);

/**
 * Add all tracks in the area to the extraction queue
 * 
 * @param extractor The extractor
 * @return SACD_RESULT_OK on success, error code on failure
 */
sacd_result_t sacd_extractor_add_all_tracks(sacd_extractor_t *extractor);

/**
 * Start the extraction process
 * 
 * @param extractor The extractor
 * @return SACD_RESULT_OK on success, error code on failure
 */
sacd_result_t sacd_extractor_start(sacd_extractor_t *extractor);

/**
 * Cancel a running extraction
 * 
 * @param extractor The extractor
 */
void sacd_extractor_cancel(sacd_extractor_t *extractor);

/**
 * Check if extraction is currently running
 * 
 * @param extractor The extractor
 * @return True if extraction is running
 */
bool sacd_extractor_is_running(const sacd_extractor_t *extractor);

/**
 * Wait for extraction to complete
 * 
 * @param extractor The extractor
 * @return SACD_RESULT_OK on success, error code on failure
 */
sacd_result_t sacd_extractor_wait(sacd_extractor_t *extractor);

/* Utility functions */

/**
 * Convert SACD time to seconds
 * 
 * @param time SACD time structure
 * @return Time in seconds
 */
double sacd_time_to_seconds(const sacd_time_t *time);

/**
 * Convert seconds to SACD time
 * 
 * @param seconds Time in seconds
 * @param time Pointer to receive SACD time
 */
void sacd_seconds_to_time(double seconds, sacd_time_t *time);

/**
 * Get a human-readable string for the result code
 * 
 * @param result Result code
 * @return Human-readable string
 */
const char *sacd_result_string(sacd_result_t result);

/**
 * Get file extension for output format
 * 
 * @param format Output format
 * @return File extension (without dot)
 */
const char *sacd_format_extension(sacd_output_format_t format);

/**
 * Get description for output format
 * 
 * @param format Output format
 * @return Human-readable format description
 */
const char *sacd_format_description(sacd_output_format_t format);

/**
 * Initialize default extraction options
 * 
 * @param options Pointer to options structure to initialize
 */
void sacd_extraction_options_init(sacd_extraction_options_t *options);

/**
 * Create a safe filename from track text
 * 
 * @param text Source text
 * @param buffer Buffer to receive safe filename
 * @param buffer_size Size of buffer
 */
void sacd_create_safe_filename(const char *text, char *buffer, size_t buffer_size);

#ifdef __cplusplus
}
#endif

#endif /* SACD_LIB_H */