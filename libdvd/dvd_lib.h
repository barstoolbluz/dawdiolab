#ifndef DVD_LIB_H
#define DVD_LIB_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* DVD Library - Comprehensive DVD-Audio and DVD-Video Parsing */

/* Result codes */
typedef enum {
    DVD_RESULT_OK = 0,
    DVD_RESULT_ERROR,
    DVD_RESULT_OUT_OF_MEMORY,
    DVD_RESULT_INVALID_FILE,
    DVD_RESULT_IO_ERROR,
    DVD_RESULT_NOT_IMPLEMENTED,
    DVD_RESULT_INVALID_PARAM
} dvd_result_t;

/* DVD disc types */
typedef enum {
    DVD_TYPE_UNKNOWN = 0,
    DVD_TYPE_AUDIO,         /* DVD-Audio disc (AUDIO_TS) */
    DVD_TYPE_VIDEO,         /* DVD-Video disc (VIDEO_TS) */
    DVD_TYPE_HYBRID,        /* Both AUDIO_TS and VIDEO_TS */
    DVD_TYPE_BLURAY         /* Blu-ray disc (BDMV) */
} dvd_type_t;

/* Audio format types within DVD/Blu-ray */
typedef enum {
    DVD_AUDIO_FORMAT_UNKNOWN = 0,
    DVD_AUDIO_FORMAT_LPCM,      /* Linear PCM */
    DVD_AUDIO_FORMAT_MLP,       /* Meridian Lossless Packing (DVD-Audio) */
    DVD_AUDIO_FORMAT_AC3,       /* Dolby Digital (DVD-Video) */
    DVD_AUDIO_FORMAT_DTS,       /* DTS (DVD-Video) */
    DVD_AUDIO_FORMAT_MPEG,      /* MPEG Audio (DVD-Video) */
    /* Blu-ray specific formats */
    DVD_AUDIO_FORMAT_TRUEHD,    /* Dolby TrueHD (Blu-ray) */
    DVD_AUDIO_FORMAT_DTS_HD,    /* DTS-HD Master Audio (Blu-ray) */
    DVD_AUDIO_FORMAT_DTS_HD_HR  /* DTS-HD High Resolution (Blu-ray) */
} dvd_audio_format_t;

/* Audio track information */
typedef struct {
    uint8_t track_number;       /* Track number (1-based) */
    dvd_audio_format_t format;  /* Audio format */
    uint8_t channels;           /* Number of channels */
    uint32_t sample_rate;       /* Sample rate in Hz */
    uint8_t bits_per_sample;    /* Bits per sample (16/20/24) */
    uint64_t duration_samples;  /* Duration in samples */
    double duration_seconds;    /* Duration in seconds */
    uint32_t start_sector;      /* Starting sector */
    uint32_t end_sector;        /* Ending sector */
    char title[256];            /* Track title (if available) */
    char language[4];           /* Language code (if available) */
} dvd_audio_track_t;

/* DVD title information */
typedef struct {
    uint8_t title_number;       /* Title number (1-based) */
    uint8_t audio_track_count;  /* Number of audio tracks */
    dvd_audio_track_t *audio_tracks; /* Array of audio tracks */
    double duration_seconds;    /* Total title duration */
    char title_name[256];       /* Title name (if available) */
} dvd_title_t;

/* DVD disc information */
typedef struct {
    dvd_type_t disc_type;       /* Type of DVD disc */
    char volume_id[32];         /* Volume identifier */
    uint8_t title_count;        /* Number of titles */
    dvd_title_t *titles;        /* Array of titles */
    
    /* Disc-level metadata */
    char album_title[256];      /* Album/disc title */
    char artist[256];           /* Artist/performer */
    uint16_t year;              /* Release year */
    
    /* Internal data - do not access directly */
    void *internal_data;
} dvd_disc_t;

/* Progress callback for extraction operations */
typedef void (*dvd_progress_callback_t)(
    double percent_complete,
    uint64_t bytes_processed,
    uint64_t total_bytes,
    void *userdata
);

/* Core DVD disc operations */
dvd_result_t dvd_disc_open(const char *iso_path, dvd_disc_t **disc);
void dvd_disc_close(dvd_disc_t *disc);
dvd_result_t dvd_disc_get_info(dvd_disc_t *disc);

/* Title and track access */
dvd_result_t dvd_get_title_count(dvd_disc_t *disc, uint8_t *count);
dvd_result_t dvd_get_title(dvd_disc_t *disc, uint8_t title_number, dvd_title_t **title);
dvd_result_t dvd_get_audio_track_count(dvd_disc_t *disc, uint8_t title_number, uint8_t *count);
dvd_result_t dvd_get_audio_track(dvd_disc_t *disc, uint8_t title_number, uint8_t track_number, dvd_audio_track_t **track);

/* Audio extraction */
dvd_result_t dvd_extract_audio_track(
    dvd_disc_t *disc,
    uint8_t title_number,
    uint8_t track_number,
    const char *output_path,
    dvd_progress_callback_t progress_callback,
    void *userdata
);

dvd_result_t dvd_extract_title_audio(
    dvd_disc_t *disc,
    uint8_t title_number,
    const char *output_dir,
    dvd_progress_callback_t progress_callback,
    void *userdata
);

/* Utility functions */
const char *dvd_get_format_name(dvd_audio_format_t format);
const char *dvd_result_to_string(dvd_result_t result);
bool dvd_is_lossless_format(dvd_audio_format_t format);
uint32_t dvd_get_bitrate(dvd_audio_format_t format, uint8_t channels, uint32_t sample_rate, uint8_t bits_per_sample);

#ifdef __cplusplus
}
#endif

#endif /* DVD_LIB_H */