#include "dvd_internal.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <inttypes.h>

/* DVD Audio Extraction Functions */

/* Extract a single audio track from DVD disc */
dvd_result_t dvd_extract_audio_track(
    dvd_disc_t *disc,
    uint8_t title_number,
    uint8_t track_number,
    const char *output_path,
    dvd_progress_callback_t progress_callback,
    void *userdata
) {
    if (!disc || !output_path) {
        return DVD_RESULT_INVALID_PARAM;
    }
    
    dvd_disc_internal_t *internal = (dvd_disc_internal_t*)disc->internal_data;
    if (!internal) {
        return DVD_RESULT_ERROR;
    }
    
    /* Validate title and track numbers */
    if (title_number < 1 || title_number > disc->title_count) {
        return DVD_RESULT_INVALID_PARAM;
    }
    
    dvd_title_t *title = &disc->titles[title_number - 1];
    if (track_number < 1 || track_number > title->audio_track_count) {
        return DVD_RESULT_INVALID_PARAM;
    }
    
    dvd_audio_track_t *track = &title->audio_tracks[track_number - 1];
    
    /* Open output file */
    int out_fd = open(output_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (out_fd < 0) {
        return DVD_RESULT_IO_ERROR;
    }
    
    /* Calculate extraction parameters */
    uint32_t start_sector = track->start_sector;
    uint32_t end_sector = track->end_sector;
    uint32_t total_sectors = end_sector - start_sector + 1;
    uint64_t total_bytes = (uint64_t)total_sectors * DVD_SECTOR_SIZE;
    uint64_t bytes_processed = 0;
    
    /* Allocate sector buffer */
    uint8_t *sector_buffer = malloc(DVD_SECTOR_SIZE);
    if (!sector_buffer) {
        close(out_fd);
        return DVD_RESULT_OUT_OF_MEMORY;
    }
    
    /* Extract sectors from disc */
    dvd_result_t result = DVD_RESULT_OK;
    
    for (uint32_t sector = start_sector; sector <= end_sector; sector++) {
        /* Read sector from disc */
        result = dvd_iso_read_sector(internal, sector, sector_buffer);
        if (result != DVD_RESULT_OK) {
            break;
        }
        
        /* Write sector to output file */
        ssize_t bytes_written = write(out_fd, sector_buffer, DVD_SECTOR_SIZE);
        if (bytes_written != DVD_SECTOR_SIZE) {
            result = DVD_RESULT_IO_ERROR;
            break;
        }
        
        bytes_processed += DVD_SECTOR_SIZE;
        
        /* Report progress */
        if (progress_callback) {
            double percent = (double)bytes_processed / (double)total_bytes * 100.0;
            progress_callback(percent, bytes_processed, total_bytes, userdata);
        }
    }
    
    /* Cleanup */
    free(sector_buffer);
    close(out_fd);
    
    /* Remove output file if extraction failed */
    if (result != DVD_RESULT_OK) {
        unlink(output_path);
    }
    
    return result;
}

/* Extract all audio tracks from a title */
dvd_result_t dvd_extract_title_audio(
    dvd_disc_t *disc,
    uint8_t title_number,
    const char *output_dir,
    dvd_progress_callback_t progress_callback,
    void *userdata
) {
    if (!disc || !output_dir) {
        return DVD_RESULT_INVALID_PARAM;
    }
    
    /* Validate title number */
    if (title_number < 1 || title_number > disc->title_count) {
        return DVD_RESULT_INVALID_PARAM;
    }
    
    dvd_title_t *title = &disc->titles[title_number - 1];
    
    /* Extract each audio track */
    for (uint8_t track = 1; track <= title->audio_track_count; track++) {
        dvd_audio_track_t *audio_track = &title->audio_tracks[track - 1];
        
        /* Generate output filename */
        char output_path[512];
        const char *format_ext = "raw";
        
        switch (audio_track->format) {
            case DVD_AUDIO_FORMAT_LPCM:
                format_ext = "wav";
                break;
            case DVD_AUDIO_FORMAT_MLP:
                format_ext = "mlp";
                break;
            case DVD_AUDIO_FORMAT_AC3:
                format_ext = "ac3";
                break;
            case DVD_AUDIO_FORMAT_DTS:
                format_ext = "dts";
                break;
            case DVD_AUDIO_FORMAT_MPEG:
                format_ext = "mp2";
                break;
            default:
                format_ext = "raw";
                break;
        }
        
        snprintf(output_path, sizeof(output_path), "%s/Title_%02d_Track_%02d_%s_%dkHz_%dch.%s",
                 output_dir, title_number, track,
                 dvd_get_format_name(audio_track->format),
                 audio_track->sample_rate / 1000,
                 audio_track->channels,
                 format_ext);
        
        /* Extract this track */
        dvd_result_t result = dvd_extract_audio_track(disc, title_number, track, output_path, 
                                                     progress_callback, userdata);
        if (result != DVD_RESULT_OK) {
            return result;
        }
    }
    
    return DVD_RESULT_OK;
}

/* Simple progress callback for testing */
static void simple_progress_callback(double percent, uint64_t bytes_processed, uint64_t total_bytes, void *userdata) {
    static int last_percent = -1;
    int current_percent = (int)percent;
    
    /* Only update every 5% to avoid flooding output */
    if (current_percent >= last_percent + 5 || current_percent >= 100) {
        printf("Extraction progress: %.1f%% (%" PRIu64 "/%" PRIu64 " bytes)\n", 
               percent, bytes_processed, total_bytes);
        last_percent = current_percent;
    }
}

/* Create default output directory and extract with simple progress */
dvd_result_t dvd_extract_title_audio_simple(dvd_disc_t *disc, uint8_t title_number, const char *base_output_dir) {
    if (!disc || !base_output_dir) {
        return DVD_RESULT_INVALID_PARAM;
    }
    
    /* Create title-specific output directory */
    char output_dir[512];
    snprintf(output_dir, sizeof(output_dir), "%s/DVD_Title_%02d", base_output_dir, title_number);
    
    /* Create directory (ignore errors if it already exists) */
    char mkdir_cmd[600];
    snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p \"%s\"", output_dir);
    system(mkdir_cmd);
    
    printf("Extracting DVD Title %d audio tracks to: %s\n", title_number, output_dir);
    
    return dvd_extract_title_audio(disc, title_number, output_dir, simple_progress_callback, NULL);
}