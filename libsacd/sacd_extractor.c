/**
 * SACD Library - Extraction Engine
 * 
 * Main extraction functionality for converting SACD tracks to DSF/DSDIFF files.
 */

#include "sacd_lib.h"
#include "sacd_internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>

/* Create an extractor */
sacd_result_t sacd_extractor_create(
    const sacd_disc_t *disc,
    const sacd_area_t *area,
    const char *output_dir,
    const sacd_extraction_options_t *options,
    sacd_extractor_t **extractor) {
    
    if (!disc || !area || !output_dir || !options || !extractor) {
        return SACD_RESULT_ERROR;
    }
    
    *extractor = NULL;
    
    /* Allocate internal structure */
    sacd_extractor_internal_t *internal = calloc(1, sizeof(sacd_extractor_internal_t));
    if (!internal) {
        return SACD_RESULT_OUT_OF_MEMORY;
    }
    
    /* Initialize fields */
    internal->disc = disc;
    internal->area = area;
    internal->disc_internal = (sacd_disc_internal_t*)disc->internal_data;
    
    /* Copy output directory */
    internal->output_dir = strdup(output_dir);
    if (!internal->output_dir) {
        free(internal);
        return SACD_RESULT_OUT_OF_MEMORY;
    }
    
    /* Copy options */
    internal->options = *options;
    
    /* Initialize track queue */
    internal->track_queue_capacity = 16;
    internal->track_queue = malloc(internal->track_queue_capacity * sizeof(int));
    if (!internal->track_queue) {
        free(internal->output_dir);
        free(internal);
        return SACD_RESULT_OUT_OF_MEMORY;
    }
    
    /* Initialize mutex */
    if (pthread_mutex_init(&internal->state_mutex, NULL) != 0) {
        free(internal->track_queue);
        free(internal->output_dir);
        free(internal);
        return SACD_RESULT_ERROR;
    }
    
    /* Initialize DST decoder */
    sacd_result_t result = sacd_internal_dst_decoder_init(&internal->dst_decoder);
    if (result != SACD_RESULT_OK) {
        pthread_mutex_destroy(&internal->state_mutex);
        free(internal->track_queue);
        free(internal->output_dir);
        free(internal);
        return result;
    }
    
    /* Create output directory if it doesn't exist */
    struct stat st;
    if (stat(output_dir, &st) != 0) {
        if (mkdir(output_dir, 0755) != 0) {
            sacd_internal_dst_decoder_cleanup(&internal->dst_decoder);
            pthread_mutex_destroy(&internal->state_mutex);
            free(internal->track_queue);
            free(internal->output_dir);
            free(internal);
            return SACD_RESULT_IO_ERROR;
        }
    }
    
    internal->public.internal_data = internal;
    *extractor = &internal->public;
    
    return SACD_RESULT_OK;
}

/* Destroy an extractor */
void sacd_extractor_destroy(sacd_extractor_t *extractor) {
    if (!extractor) {
        return;
    }
    
    sacd_extractor_internal_t *internal = (sacd_extractor_internal_t*)extractor->internal_data;
    if (!internal) {
        return;
    }
    
    /* Cancel and wait for any running extraction */
    if (internal->is_running) {
        sacd_extractor_cancel(extractor);
        sacd_extractor_wait(extractor);
    }
    
    /* Close any open output file */
    if (internal->current_output_file) {
        fclose(internal->current_output_file);
    }
    
    /* Cleanup DST decoder */
    sacd_internal_dst_decoder_cleanup(&internal->dst_decoder);
    
    /* Destroy mutex */
    pthread_mutex_destroy(&internal->state_mutex);
    
    /* Free allocated memory */
    free(internal->track_queue);
    free(internal->output_dir);
    free(internal->current_frame.data);
    free(internal);
}

/* Add tracks to extraction queue */
sacd_result_t sacd_extractor_add_tracks(
    sacd_extractor_t *extractor,
    const int *track_numbers,
    int track_count) {
    
    if (!extractor || !track_numbers || track_count <= 0) {
        return SACD_RESULT_ERROR;
    }
    
    sacd_extractor_internal_t *internal = (sacd_extractor_internal_t*)extractor->internal_data;
    if (!internal) {
        return SACD_RESULT_ERROR;
    }
    
    pthread_mutex_lock(&internal->state_mutex);
    
    /* Check if extraction is running */
    if (internal->is_running) {
        pthread_mutex_unlock(&internal->state_mutex);
        return SACD_RESULT_ERROR;
    }
    
    /* Expand queue if needed */
    int new_count = internal->track_queue_count + track_count;
    if (new_count > internal->track_queue_capacity) {
        int new_capacity = new_count * 2;
        int *new_queue = realloc(internal->track_queue, new_capacity * sizeof(int));
        if (!new_queue) {
            pthread_mutex_unlock(&internal->state_mutex);
            return SACD_RESULT_OUT_OF_MEMORY;
        }
        internal->track_queue = new_queue;
        internal->track_queue_capacity = new_capacity;
    }
    
    /* Validate and add track numbers */
    for (int i = 0; i < track_count; i++) {
        int track_num = track_numbers[i];
        if (track_num < 0 || track_num >= internal->area->track_count) {
            pthread_mutex_unlock(&internal->state_mutex);
            return SACD_RESULT_INVALID_TRACK;
        }
        internal->track_queue[internal->track_queue_count++] = track_num;
    }
    
    pthread_mutex_unlock(&internal->state_mutex);
    return SACD_RESULT_OK;
}

/* Add all tracks to extraction queue */
sacd_result_t sacd_extractor_add_all_tracks(sacd_extractor_t *extractor) {
    if (!extractor) {
        return SACD_RESULT_ERROR;
    }
    
    sacd_extractor_internal_t *internal = (sacd_extractor_internal_t*)extractor->internal_data;
    if (!internal) {
        return SACD_RESULT_ERROR;
    }
    
    /* Create array of all track numbers */
    int track_count = internal->area->track_count;
    int *track_numbers = malloc(track_count * sizeof(int));
    if (!track_numbers) {
        return SACD_RESULT_OUT_OF_MEMORY;
    }
    
    for (int i = 0; i < track_count; i++) {
        track_numbers[i] = i;
    }
    
    sacd_result_t result = sacd_extractor_add_tracks(extractor, track_numbers, track_count);
    free(track_numbers);
    
    return result;
}

/* Extract a single track */
static sacd_result_t extract_track(sacd_extractor_internal_t *internal, int track_index) {
    const sacd_track_t *track = &internal->area->tracks[track_index];
    sacd_result_t result;
    
    /* Create output filename */
    char filename[1024];
    result = sacd_internal_create_filename(&internal->options, track, 
                                         internal->output_dir, filename, sizeof(filename));
    if (result != SACD_RESULT_OK) {
        return result;
    }
    
    /* Call track start callback */
    if (internal->options.track_start_callback) {
        internal->options.track_start_callback(track->number + 1, track, filename,
                                             internal->options.callback_userdata);
    }
    
    /* Open output file */
    internal->current_output_file = fopen(filename, "wb");
    if (!internal->current_output_file) {
        return SACD_RESULT_IO_ERROR;
    }
    
    /* Estimate audio data size */
    size_t estimated_audio_size = sacd_estimate_track_file_size(track, internal->options.format);
    
    /* Write format-specific header */
    if (internal->options.format == SACD_FORMAT_DSF) {
        result = sacd_internal_write_dsf_header(internal->current_output_file, 
                                              track, internal->area, estimated_audio_size);
    } else {
        result = sacd_internal_write_dsdiff_header(internal->current_output_file,
                                                 track, internal->area, estimated_audio_size);
    }
    
    if (result != SACD_RESULT_OK) {
        fclose(internal->current_output_file);
        internal->current_output_file = NULL;
        return result;
    }
    
    /* Extract real DSD audio data from SACD sectors */
    internal->bytes_written = 0;
    
    SACD_DEBUG_LOG("Track %d: Extracting from LSN %d to %d (%d sectors)",
                   track->number, track->start_lsn, track->start_lsn + track->length_lsn - 1, track->length_lsn);
    
    /* REAL SACD AUDIO EXTRACTION */
    uint8_t *sector_buffer = malloc(SACD_LSN_SIZE);
    if (!sector_buffer) {
        fclose(internal->current_output_file);
        internal->current_output_file = NULL;
        return SACD_RESULT_OUT_OF_MEMORY;
    }
    
    size_t bytes_written = 0;
    uint32_t sectors_processed = 0;
    
    /* Read each sector in the track */
    for (uint32_t lsn = track->start_lsn; lsn < track->start_lsn + track->length_lsn && !internal->cancel_requested; lsn++) {
        /* Read sector from disc */
        sacd_result_t sector_result = sacd_internal_read_sector(internal->disc_internal, lsn, sector_buffer);
        if (sector_result != SACD_RESULT_OK) {
            SACD_DEBUG_LOG("Failed to read sector %d: %s", lsn, sacd_result_string(sector_result));
            free(sector_buffer);
            fclose(internal->current_output_file);
            internal->current_output_file = NULL;
            return sector_result;
        }
        
        /* Extract DSD audio data from sector */
        size_t audio_data_size;
        uint8_t *audio_data = sacd_internal_extract_dsd_from_sector(sector_buffer, &audio_data_size);
        if (!audio_data || audio_data_size == 0) {
            /* Skip sectors without audio data */
            continue;
        }
        
        /* Process DST decompression if needed */
        if (track->dst_encoded) {
            uint8_t *decompressed_data = NULL;
            size_t decompressed_size = 0;
            
            sacd_result_t dst_result = sacd_internal_dst_decode_frame(&internal->dst_decoder, 
                                                                   audio_data, audio_data_size,
                                                                   &decompressed_data, &decompressed_size);
            if (dst_result == SACD_RESULT_OK && decompressed_data) {
                /* Write decompressed DSD data */
                if (fwrite(decompressed_data, 1, decompressed_size, internal->current_output_file) != decompressed_size) {
                    free(decompressed_data);
                    free(sector_buffer);
                    fclose(internal->current_output_file);
                    internal->current_output_file = NULL;
                    return SACD_RESULT_IO_ERROR;
                }
                bytes_written += decompressed_size;
                free(decompressed_data);
            }
        } else {
            /* Write raw DSD data directly */
            if (fwrite(audio_data, 1, audio_data_size, internal->current_output_file) != audio_data_size) {
                free(sector_buffer);
                fclose(internal->current_output_file);
                internal->current_output_file = NULL;
                return SACD_RESULT_IO_ERROR;
            }
            bytes_written += audio_data_size;
        }
        
        sectors_processed++;
        internal->bytes_written = bytes_written;
        
        /* Update progress */
        int track_progress = (int)((sectors_processed * 100ULL) / track->length_lsn);
        internal->current_track_progress = track_progress;
        
        /* Only call progress callback every 1% to reduce overhead */
        static int last_reported_progress = -1;
        if (internal->options.progress_callback && track_progress != last_reported_progress) {
            last_reported_progress = track_progress;
            int overall_progress = ((internal->current_track_index * 100) + track_progress) / 
                                 internal->track_queue_count;
            
            char status[256];
            snprintf(status, sizeof(status), "Extracting track %d/%d: %s (%d%%) - %zu MB",
                    internal->current_track_index + 1, internal->track_queue_count,
                    track->text.title ? track->text.title : "Unknown", track_progress,
                    bytes_written / (1024 * 1024));
            
            internal->options.progress_callback(track->number + 1, internal->track_queue_count,
                                              track_progress, overall_progress, status,
                                              internal->options.callback_userdata);
        }
    }
    
    free(sector_buffer);
    
    SACD_DEBUG_LOG("Track %d: Extracted %zu bytes from %d sectors", 
                   track->number, bytes_written, sectors_processed);
    
    /* Finalize file headers */
    result = sacd_internal_finalize_file_headers(internal->current_output_file,
                                               internal->options.format, bytes_written);
    
    /* Close output file */
    fclose(internal->current_output_file);
    internal->current_output_file = NULL;
    
    if (result == SACD_RESULT_OK) {
        internal->total_bytes_written += bytes_written;
        
        /* Call track complete callback */
        if (internal->options.track_complete_callback) {
            internal->options.track_complete_callback(track->number + 1, track, filename,
                                                    bytes_written, internal->options.callback_userdata);
        }
    }
    
    return result;
}

/* Extraction thread function */
static void *extraction_thread(void *arg) {
    sacd_extractor_internal_t *internal = (sacd_extractor_internal_t*)arg;
    
    /* Record start time */
    struct timeval tv;
    gettimeofday(&tv, NULL);
    internal->extraction_start_time = tv.tv_sec + tv.tv_usec / 1000000.0;
    
    /* Extract each track in the queue */
    for (int i = 0; i < internal->track_queue_count && !internal->cancel_requested; i++) {
        internal->current_track_index = i;
        int track_num = internal->track_queue[i];
        
        sacd_result_t result = extract_track(internal, track_num);
        if (result != SACD_RESULT_OK) {
            /* TODO: Handle extraction errors */
            SACD_DEBUG_LOG("Track %d extraction failed: %s", track_num, sacd_result_string(result));
        }
    }
    
    /* Update final status */
    pthread_mutex_lock(&internal->state_mutex);
    internal->is_running = false;
    pthread_mutex_unlock(&internal->state_mutex);
    
    /* Final progress callback */
    if (internal->options.progress_callback) {
        const char *status = internal->cancel_requested ? 
                           "Extraction cancelled" : "Extraction completed";
        int final_progress = internal->cancel_requested ? 
                           internal->current_track_progress : 100;
        
        internal->options.progress_callback(0, internal->track_queue_count,
                                          100, final_progress, status,
                                          internal->options.callback_userdata);
    }
    
    return NULL;
}

/* Start extraction */
sacd_result_t sacd_extractor_start(sacd_extractor_t *extractor) {
    if (!extractor) {
        return SACD_RESULT_ERROR;
    }
    
    sacd_extractor_internal_t *internal = (sacd_extractor_internal_t*)extractor->internal_data;
    if (!internal) {
        return SACD_RESULT_ERROR;
    }
    
    pthread_mutex_lock(&internal->state_mutex);
    
    /* Check if already running */
    if (internal->is_running) {
        pthread_mutex_unlock(&internal->state_mutex);
        return SACD_RESULT_ERROR;
    }
    
    /* Check if we have tracks to extract */
    if (internal->track_queue_count == 0) {
        pthread_mutex_unlock(&internal->state_mutex);
        return SACD_RESULT_ERROR;
    }
    
    /* Reset state */
    internal->cancel_requested = false;
    internal->current_track_index = 0;
    internal->current_track_progress = 0;
    internal->total_bytes_written = 0;
    
    /* Start extraction thread */
    internal->is_running = true;
    int result = pthread_create(&internal->extraction_thread, NULL, extraction_thread, internal);
    if (result != 0) {
        internal->is_running = false;
        pthread_mutex_unlock(&internal->state_mutex);
        return SACD_RESULT_ERROR;
    }
    
    pthread_mutex_unlock(&internal->state_mutex);
    return SACD_RESULT_OK;
}

/* Cancel extraction */
void sacd_extractor_cancel(sacd_extractor_t *extractor) {
    if (!extractor) {
        return;
    }
    
    sacd_extractor_internal_t *internal = (sacd_extractor_internal_t*)extractor->internal_data;
    if (!internal) {
        return;
    }
    
    pthread_mutex_lock(&internal->state_mutex);
    internal->cancel_requested = true;
    pthread_mutex_unlock(&internal->state_mutex);
}

/* Check if extraction is running */
bool sacd_extractor_is_running(const sacd_extractor_t *extractor) {
    if (!extractor) {
        return false;
    }
    
    sacd_extractor_internal_t *internal = (sacd_extractor_internal_t*)extractor->internal_data;
    if (!internal) {
        return false;
    }
    
    /* Cast away const for mutex operations - this is safe because we're only reading state */
    pthread_mutex_t *mutex = &((sacd_extractor_internal_t*)internal)->state_mutex;
    pthread_mutex_lock(mutex);
    bool running = internal->is_running;
    pthread_mutex_unlock(mutex);
    
    return running;
}

/* Wait for extraction to complete */
sacd_result_t sacd_extractor_wait(sacd_extractor_t *extractor) {
    if (!extractor) {
        return SACD_RESULT_ERROR;
    }
    
    sacd_extractor_internal_t *internal = (sacd_extractor_internal_t*)extractor->internal_data;
    if (!internal) {
        return SACD_RESULT_ERROR;
    }
    
    if (!internal->is_running) {
        return SACD_RESULT_OK;
    }
    
    int result = pthread_join(internal->extraction_thread, NULL);
    return (result == 0) ? SACD_RESULT_OK : SACD_RESULT_ERROR;
}