/**
 * SACD Library - Utility Functions
 * 
 * Various utility functions for time conversion, string handling, etc.
 */

#include "sacd_lib.h"
#include "sacd_internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <math.h>

/* Convert SACD time to seconds */
double sacd_time_to_seconds(const sacd_time_t *time) {
    if (!time) {
        return 0.0;
    }
    
    return (double)time->minutes * 60.0 + 
           (double)time->seconds + 
           (double)time->frames / (double)SACD_FRAME_RATE;
}

/* Convert seconds to SACD time */
void sacd_seconds_to_time(double seconds, sacd_time_t *time) {
    if (!time) {
        return;
    }
    
    if (seconds < 0.0) {
        seconds = 0.0;
    }
    
    time->minutes = (uint8_t)(seconds / 60.0);
    seconds -= time->minutes * 60.0;
    
    time->seconds = (uint8_t)seconds;
    seconds -= time->seconds;
    
    time->frames = (uint8_t)(seconds * SACD_FRAME_RATE);
}

/* Get result string */
const char *sacd_result_string(sacd_result_t result) {
    switch (result) {
        case SACD_RESULT_OK:
            return "Success";
        case SACD_RESULT_ERROR:
            return "General error";
        case SACD_RESULT_INVALID_FILE:
            return "Invalid or corrupted SACD file";
        case SACD_RESULT_INVALID_AREA:
            return "Invalid area specification";
        case SACD_RESULT_INVALID_TRACK:
            return "Invalid track specification";
        case SACD_RESULT_OUT_OF_MEMORY:
            return "Out of memory";
        case SACD_RESULT_IO_ERROR:
            return "Input/output error";
        case SACD_RESULT_CANCELLED:
            return "Operation cancelled";
        default:
            return "Unknown error";
    }
}

/* Get format extension */
const char *sacd_format_extension(sacd_output_format_t format) {
    switch (format) {
        case SACD_FORMAT_DSF:
            return "dsf";
        case SACD_FORMAT_DSDIFF:
        case SACD_FORMAT_DSDIFF_EM:
            return "dff";
        default:
            return "dsf";
    }
}

/* Get format description */
const char *sacd_format_description(sacd_output_format_t format) {
    switch (format) {
        case SACD_FORMAT_DSF:
            return "DSF (Sony DSD Stream File)";
        case SACD_FORMAT_DSDIFF:
            return "DSDIFF (DSD Interchange File Format)";
        case SACD_FORMAT_DSDIFF_EM:
            return "DSDIFF Edit Master";
        default:
            return "Unknown format";
    }
}

/* Initialize default extraction options */
void sacd_extraction_options_init(sacd_extraction_options_t *options) {
    if (!options) {
        return;
    }
    
    memset(options, 0, sizeof(sacd_extraction_options_t));
    
    /* Set default values */
    options->format = SACD_FORMAT_DSF;
    options->convert_dst = true;
    options->export_cue_sheet = false;
    options->include_pauses = true;
    options->trim_whitespace = true;
    options->dsf_nopad = false;
    options->add_id3_tags = false;
    options->id3_version = 3;
    options->add_artist_to_folder = false;
    options->add_performer_to_filename = false;
}

/* Create safe filename from text */
void sacd_create_safe_filename(const char *text, char *buffer, size_t buffer_size) {
    if (!text || !buffer || buffer_size == 0) {
        return;
    }
    
    size_t src_len = strlen(text);
    size_t dst_pos = 0;
    
    for (size_t i = 0; i < src_len && dst_pos < buffer_size - 1; i++) {
        char c = text[i];
        
        /* Replace unsafe characters */
        if (c == '/' || c == '\\' || c == ':' || c == '*' || 
            c == '?' || c == '"' || c == '<' || c == '>' || c == '|') {
            c = '_';
        }
        
        /* Skip control characters */
        if (c < 32 || c == 127) {
            continue;
        }
        
        buffer[dst_pos++] = c;
    }
    
    buffer[dst_pos] = '\0';
    
    /* Trim trailing whitespace */
    while (dst_pos > 0 && isspace((unsigned char)buffer[dst_pos - 1])) {
        buffer[--dst_pos] = '\0';
    }
    
    /* Ensure we have something */
    if (dst_pos == 0) {
        strncpy(buffer, "Track", buffer_size - 1);
        buffer[buffer_size - 1] = '\0';
    }
}

/* Internal utility: create output filename */
sacd_result_t sacd_internal_create_filename(
    const sacd_extraction_options_t *options,
    const sacd_track_t *track,
    const char *output_dir,
    char *filename,
    size_t filename_size) {
    
    if (!options || !track || !output_dir || !filename) {
        return SACD_RESULT_ERROR;
    }
    
    /* Get track title */
    const char *title = track->text.title;
    if (!title || strlen(title) == 0) {
        title = "Track";
    }
    
    /* Create safe title */
    char safe_title[256];
    sacd_create_safe_filename(title, safe_title, sizeof(safe_title));
    
    /* Get artist if requested */
    char artist_part[256] = "";
    if (options->add_performer_to_filename && track->text.artist) {
        char safe_artist[128];
        sacd_create_safe_filename(track->text.artist, safe_artist, sizeof(safe_artist));
        snprintf(artist_part, sizeof(artist_part), " - %s", safe_artist);
    }
    
    /* Get file extension */
    const char *ext = sacd_format_extension(options->format);
    
    /* Build filename */
    int result = snprintf(filename, filename_size, "%s/%02d - %s%s.%s",
                         output_dir, track->number + 1, safe_title, artist_part, ext);
    
    if (result < 0 || (size_t)result >= filename_size) {
        return SACD_RESULT_ERROR;
    }
    
    return SACD_RESULT_OK;
}

/* Internal utility: parse text with character set handling */
char *sacd_internal_parse_text(const uint8_t *text_data, size_t offset, sacd_charset_t charset) {
    if (!text_data) {
        return NULL;
    }
    
    /* For now, we do simple ASCII/UTF-8 parsing */
    /* TODO: Implement proper character set conversion */
    
    const char *start = (const char*)text_data + offset;
    size_t len = strlen(start);
    
    if (len == 0) {
        return NULL;
    }
    
    char *result = malloc(len + 1);
    if (!result) {
        return NULL;
    }
    
    strcpy(result, start);
    return result;
}

/* Format time for display */
void sacd_format_time(const sacd_time_t *time, char *buffer, size_t buffer_size) {
    if (!time || !buffer) {
        return;
    }
    
    snprintf(buffer, buffer_size, "%02d:%02d.%02d", 
             time->minutes, time->seconds, time->frames);
}

/* Calculate track duration in samples */
uint64_t sacd_track_duration_samples(const sacd_track_t *track) {
    if (!track) {
        return 0;
    }
    
    double seconds = sacd_time_to_seconds(&track->duration);
    return (uint64_t)(seconds * SACD_SAMPLING_FREQ);
}

/* Calculate estimated file size for a track */
size_t sacd_estimate_track_file_size(const sacd_track_t *track, sacd_output_format_t format) {
    if (!track) {
        return 0;
    }
    
    uint64_t samples = sacd_track_duration_samples(track);
    size_t channels = track->channel_count;
    
    /* DSD is 1 bit per sample per channel */
    size_t audio_data_size = (samples * channels) / 8;
    
    /* Add format-specific header overhead */
    size_t header_size = 0;
    switch (format) {
        case SACD_FORMAT_DSF:
            header_size = 96; /* DSF header size */
            break;
        case SACD_FORMAT_DSDIFF:
        case SACD_FORMAT_DSDIFF_EM:
            header_size = 512; /* Estimated DSDIFF header size */
            break;
    }
    
    return audio_data_size + header_size;
}