/**
 * SACD Library - Internal definitions and structures
 * 
 * Private header file for internal library use only.
 */

#ifndef SACD_INTERNAL_H
#define SACD_INTERNAL_H

#include "sacd_lib.h"
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <pthread.h>

/* Forward declarations for internal structures */
typedef struct sacd_disc_internal sacd_disc_internal_t;
typedef struct sacd_extractor_internal sacd_extractor_internal_t;

/* DST decompression structure (simplified) */
typedef struct {
    uint8_t *input_buffer;
    uint8_t *output_buffer;
    size_t input_size;
    size_t output_size;
    bool initialized;
} sacd_dst_decoder_t;

/* Audio frame structure for DSD data */
typedef struct {
    uint8_t *data;                    /* Frame data */
    size_t size;                      /* Frame size in bytes */
    int sector_count;                 /* Number of sectors in frame */
    int channel_count;                /* Number of channels */
    bool dst_encoded;                 /* True if DST compressed */
    sacd_time_t timecode;             /* Frame timecode */
} sacd_audio_frame_t;

/* Internal extraction context */
struct sacd_extractor_internal {
    sacd_extractor_t public;          /* Public interface */
    
    /* Source data */
    const sacd_disc_t *disc;          /* Source disc */
    const sacd_area_t *area;          /* Source area */
    sacd_disc_internal_t *disc_internal; /* Internal disc data */
    
    /* Output configuration */
    char *output_dir;                 /* Output directory */
    sacd_extraction_options_t options; /* Extraction options */
    
    /* Track queue */
    int *track_queue;                 /* Array of track numbers to extract */
    int track_queue_count;            /* Number of tracks in queue */
    int track_queue_capacity;         /* Capacity of track queue */
    
    /* Extraction state */
    bool is_running;                  /* True if extraction is active */
    bool cancel_requested;            /* True if cancellation requested */
    int current_track_index;          /* Current track being extracted */
    int current_track_progress;       /* Progress within current track */
    
    /* Threading */
    pthread_t extraction_thread;      /* Extraction thread */
    pthread_mutex_t state_mutex;      /* State protection mutex */
    
    /* Audio processing */
    sacd_audio_frame_t current_frame; /* Current audio frame */
    sacd_dst_decoder_t dst_decoder;   /* DST decoder state */
    
    /* Output file */
    FILE *current_output_file;        /* Current output file */
    size_t bytes_written;             /* Bytes written to current file */
    
    /* Statistics */
    size_t total_bytes_written;       /* Total bytes written */
    double extraction_start_time;     /* Extraction start time */
};

/* Internal utility functions */

/**
 * Read raw sectors from the disc
 */
sacd_result_t sacd_internal_read_sectors(
    sacd_disc_internal_t *disc,
    uint32_t start_lsn,
    uint32_t sector_count,
    uint8_t *buffer
);

/**
 * Parse SACD text data
 */
char *sacd_internal_parse_text(
    const uint8_t *text_data,
    size_t offset,
    sacd_charset_t charset
);

/**
 * Initialize DST decoder
 */
sacd_result_t sacd_internal_dst_decoder_init(sacd_dst_decoder_t *decoder);

/**
 * Decode DST frame to DSD
 */
sacd_result_t sacd_internal_dst_decode_frame(
    sacd_dst_decoder_t *decoder,
    const uint8_t *input,
    size_t input_size,
    uint8_t **output,
    size_t *output_size
);

/**
 * Cleanup DST decoder
 */
void sacd_internal_dst_decoder_cleanup(sacd_dst_decoder_t *decoder);

/**
 * Read a sector from SACD disc
 */
sacd_result_t sacd_internal_read_sector(
    sacd_disc_internal_t *internal,
    uint32_t lsn,
    uint8_t *buffer
);

/**
 * Extract DSD audio data from a sector
 */
uint8_t *sacd_internal_extract_dsd_from_sector(
    const uint8_t *sector_data,
    size_t *audio_size
);

/**
 * Create output filename for a track
 */
sacd_result_t sacd_internal_create_filename(
    const sacd_extraction_options_t *options,
    const sacd_track_t *track,
    const char *output_dir,
    char *filename,
    size_t filename_size
);

/**
 * Write DSF file header
 */
sacd_result_t sacd_internal_write_dsf_header(
    FILE *file,
    const sacd_track_t *track,
    const sacd_area_t *area,
    size_t audio_data_size
);

/**
 * Write DSDIFF file header
 */
sacd_result_t sacd_internal_write_dsdiff_header(
    FILE *file,
    const sacd_track_t *track,
    const sacd_area_t *area,
    size_t audio_data_size
);

/**
 * Update file headers with final sizes
 */
sacd_result_t sacd_internal_finalize_file_headers(
    FILE *file,
    sacd_output_format_t format,
    size_t audio_data_size
);

/**
 * Calculate track duration in DSD samples
 */
uint64_t sacd_track_duration_samples(const sacd_track_t *track);

/**
 * Estimate track file size
 */
size_t sacd_estimate_track_file_size(const sacd_track_t *track, sacd_output_format_t format);

/* Debugging and logging (when enabled) */
#ifdef SACD_DEBUG
#define SACD_DEBUG_LOG(fmt, ...) fprintf(stderr, "[SACD] " fmt "\n", ##__VA_ARGS__)
#else
#define SACD_DEBUG_LOG(fmt, ...)
#endif

/* Error handling macros */
#define SACD_CHECK_RESULT(expr) do { \
    sacd_result_t _result = (expr); \
    if (_result != SACD_RESULT_OK) { \
        return _result; \
    } \
} while (0)

#define SACD_CHECK_NULL(ptr, result) do { \
    if (!(ptr)) { \
        return (result); \
    } \
} while (0)

#endif /* SACD_INTERNAL_H */