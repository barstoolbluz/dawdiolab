/**
 * SACD Library - Disc Reading and Parsing
 * 
 * Core functionality for reading SACD ISO files and parsing the disc structure.
 */

#include "sacd_lib.h"
#include "sacd_internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>

/* Internal disc structure */
typedef struct sacd_disc_internal {
    sacd_disc_t public;           /* Public interface */
    
    /* File handling */
    int fd;                       /* File descriptor for ISO file */
    char *iso_path;               /* Path to ISO file */
    size_t file_size;             /* File size in bytes */
    
    /* Raw sector data */
    uint8_t *sector_buffer;       /* Buffer for reading sectors */
    
    /* Master TOC data */
    uint8_t *master_toc_data;     /* Raw master TOC data */
    
    /* Area data */
    uint8_t *area_data[SACD_MAX_AREAS]; /* Raw area data */
    
    /* Text data */
    uint8_t *text_data;           /* Raw text data */
    
    /* State */
    bool is_open;                 /* True if disc is open */
    bool areas_parsed;            /* True if areas have been parsed */
} sacd_disc_internal_t;

/* SACD constants from the specification */
#define SACD_MASTER_TOC_START_LSN  510
#define SACD_MASTER_TOC_LENGTH     10

/* SACD signature markers */
#define SACD_SIGNATURE             "SACDMTOC"
#define SACD_TEXT_SIGNATURE        "SACDText"

/* Endian conversion helpers */
static uint16_t be16_to_cpu(const uint8_t *data) {
    return (data[0] << 8) | data[1];
}

static uint32_t be32_to_cpu(const uint8_t *data) {
    return (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
}

/* Read a sector from the ISO file */
static sacd_result_t read_sector(sacd_disc_internal_t *internal, uint32_t lsn, uint8_t *buffer) {
    if (!internal || !buffer) {
        return SACD_RESULT_ERROR;
    }
    
    off_t offset = (off_t)lsn * SACD_LSN_SIZE;
    if (lseek(internal->fd, offset, SEEK_SET) != offset) {
        return SACD_RESULT_IO_ERROR;
    }
    
    ssize_t bytes_read = read(internal->fd, buffer, SACD_LSN_SIZE);
    if (bytes_read != SACD_LSN_SIZE) {
        return SACD_RESULT_IO_ERROR;
    }
    
    return SACD_RESULT_OK;
}

/* Parse text data with proper character set handling */
static char *parse_text_field(const uint8_t *data, size_t offset, size_t max_size, sacd_charset_t charset) {
    if (!data || offset >= max_size) {
        return NULL;
    }
    
    /* Find the end of the string (null-terminated or end of field) */
    size_t len = 0;
    while (len < max_size - offset && data[offset + len] != 0) {
        len++;
    }
    
    if (len == 0) {
        return NULL;
    }
    
    /* Allocate and copy string */
    char *result = malloc(len + 1);
    if (!result) {
        return NULL;
    }
    
    memcpy(result, data + offset, len);
    result[len] = '\0';
    
    /* TODO: Handle character set conversion if needed */
    /* For now, we assume UTF-8 compatible text */
    
    return result;
}

/* Parse master TOC (Table of Contents) */
static sacd_result_t parse_master_toc(sacd_disc_internal_t *internal) {
    if (!internal->master_toc_data) {
        return SACD_RESULT_ERROR;
    }
    
    uint8_t *data = internal->master_toc_data;
    sacd_disc_t *disc = &internal->public;
    
    /* Verify signature */
    if (memcmp(data, SACD_SIGNATURE, 8) != 0) {
        return SACD_RESULT_INVALID_FILE;
    }
    
    /* Parse version */
    disc->version_major = data[8];
    disc->version_minor = data[9];
    
    /* Parse album set info */
    uint16_t album_set_size = be16_to_cpu(data + 16);
    uint16_t album_sequence = be16_to_cpu(data + 18);
    
    /* Parse catalog numbers */
    memcpy(disc->catalog_number, data + 24, 16);
    disc->catalog_number[16] = '\0';
    memcpy(disc->disc_catalog_number, data + 88, 16);  /* Fixed offset */
    disc->disc_catalog_number[16] = '\0';
    
    /* Parse genres */
    for (int i = 0; i < 4; i++) {
        disc->genres[i].category = data[40 + i * 4];     /* Fixed offset and stride */
        disc->genres[i].genre = data[40 + i * 4 + 3];    /* Genre is at offset +3 in genre_table_t */
    }
    
    /* Parse area TOC locations */
    uint32_t area_1_toc_start = be32_to_cpu(data + 64);  /* 2-channel area */
    uint32_t area_2_toc_start = be32_to_cpu(data + 72);  /* Multi-channel area */
    
    /* Parse disc type */
    disc->is_hybrid = (data[80] & 0x01) != 0;
    
    /* Parse area TOC sizes */
    uint16_t area_1_toc_size = be16_to_cpu(data + 84);
    uint16_t area_2_toc_size = be16_to_cpu(data + 86);
    
    /* Parse date */
    disc->year = be16_to_cpu(data + 104);    /* Fixed offset */
    disc->month = data[106];
    disc->day = data[107];
    
    /* Set placeholder text information */
    disc->text.title = strdup("SACD Album");
    disc->text.artist = strdup("Unknown Artist");
    
    /* Initialize area count to 0 - will be set during area parsing */
    disc->area_count = 0;
    
    return SACD_RESULT_OK;
}

/* Parse an area TOC */
static sacd_result_t parse_area_toc(sacd_disc_internal_t *internal, int area_index, uint32_t toc_start, uint16_t toc_size) {
    if (area_index >= SACD_MAX_AREAS) {
        return SACD_RESULT_INVALID_AREA;
    }
    
    sacd_area_t *area = &internal->public.areas[area_index];
    
    /* Allocate buffer for area data */
    size_t area_data_size = toc_size * SACD_LSN_SIZE;
    internal->area_data[area_index] = malloc(area_data_size);
    if (!internal->area_data[area_index]) {
        return SACD_RESULT_OUT_OF_MEMORY;
    }
    
    /* Read area TOC sectors */
    for (int i = 0; i < toc_size; i++) {
        sacd_result_t result = read_sector(internal, toc_start + i, 
                                         internal->area_data[area_index] + i * SACD_LSN_SIZE);
        if (result != SACD_RESULT_OK) {
            return result;
        }
    }
    
    uint8_t *data = internal->area_data[area_index];
    
    /* Parse area TOC header - check for TWOCHTOC or MULCHTOC signature */
    if (memcmp(data, "TWOCHTOC", 8) != 0 && memcmp(data, "MULCHTOC", 8) != 0) {
        return SACD_RESULT_INVALID_FILE;
    }
    
    /* Parse version */
    uint8_t version_major = data[8];
    uint8_t version_minor = data[9];
    
    /* Parse TOC size */
    uint16_t toc_data_size = be16_to_cpu(data + 10);
    
    /* Parse channel count */
    area->channel_count = data[32];
    
    /* Parse sample frequency (should be 2822400 for DSD) */
    area->sample_frequency = SACD_SAMPLING_FREQ;
    
    /* Parse track count */
    area->track_count = data[69];
    if (area->track_count > SACD_MAX_TRACKS) {
        area->track_count = SACD_MAX_TRACKS;
    }
    
    /* Parse area bounds */
    area->start_lsn = be32_to_cpu(data + 72);
    area->end_lsn = be32_to_cpu(data + 76);
    
    /* Parse individual tracks by looking for SACDTRL1 and SACDTRL2 sections */
    uint8_t *p = data + SACD_LSN_SIZE;  /* Skip first sector which is the TOC header */
    
    while (p < (data + toc_data_size * SACD_LSN_SIZE)) {
        if (memcmp(p, "SACDTRL1", 8) == 0) {
            /* Track list with LSN offsets - parse track start/length information */
            for (int i = 0; i < area->track_count && i < SACD_MAX_TRACKS; i++) {
                area->tracks[i].number = i;
                area->tracks[i].start_lsn = be32_to_cpu(p + 8 + i * 4);
                area->tracks[i].length_lsn = be32_to_cpu(p + 8 + (255 + i) * 4);
                area->tracks[i].channel_count = area->channel_count;
            }
            p += SACD_LSN_SIZE;
        }
        else if (memcmp(p, "SACDTRL2", 8) == 0) {
            /* Track list with time information - parse track start times and durations */
            for (int i = 0; i < area->track_count && i < SACD_MAX_TRACKS; i++) {
                /* Start time at offset 8 + i*4 */
                uint8_t *time_data = p + 8 + i * 4;
                area->tracks[i].start_time.minutes = time_data[0];
                area->tracks[i].start_time.seconds = time_data[1];
                area->tracks[i].start_time.frames = time_data[2];
                
                /* Duration at offset 8 + 255*4 + i*4 */
                time_data = p + 8 + 255 * 4 + i * 4;
                area->tracks[i].duration.minutes = time_data[0];
                area->tracks[i].duration.seconds = time_data[1];
                area->tracks[i].duration.frames = time_data[2];
            }
            p += SACD_LSN_SIZE;
        }
        else if (memcmp(p, "SACDTTxt", 8) == 0) {
            /* Track text information - parse track titles and metadata */
            /* For now, set basic placeholder text - real parsing would be more complex */
            for (int i = 0; i < area->track_count && i < SACD_MAX_TRACKS; i++) {
                char track_title[64];
                snprintf(track_title, sizeof(track_title), "Track %02d", i + 1);
                area->tracks[i].text.title = strdup(track_title);
            }
            p += SACD_LSN_SIZE;
        }
        else {
            /* Skip unknown sections */
            p += SACD_LSN_SIZE;
        }
    }
    
    return SACD_RESULT_OK;
}

/* Main disc parsing function */
static sacd_result_t parse_disc_structure(sacd_disc_internal_t *internal) {
    sacd_result_t result;
    
    /* Allocate sector buffer */
    internal->sector_buffer = malloc(SACD_LSN_SIZE);
    if (!internal->sector_buffer) {
        return SACD_RESULT_OUT_OF_MEMORY;
    }
    
    /* Read master TOC */
    size_t master_toc_size = SACD_MASTER_TOC_LENGTH * SACD_LSN_SIZE;
    internal->master_toc_data = malloc(master_toc_size);
    if (!internal->master_toc_data) {
        return SACD_RESULT_OUT_OF_MEMORY;
    }
    
    for (int i = 0; i < SACD_MASTER_TOC_LENGTH; i++) {
        result = read_sector(internal, SACD_MASTER_TOC_START_LSN + i,
                           internal->master_toc_data + i * SACD_LSN_SIZE);
        if (result != SACD_RESULT_OK) {
            return result;
        }
    }
    
    /* Parse master TOC */
    result = parse_master_toc(internal);
    if (result != SACD_RESULT_OK) {
        return result;
    }
    
    /* Parse area TOCs for each available area */
    uint8_t *master_data = internal->master_toc_data;
    uint32_t area_1_toc_start = be32_to_cpu(master_data + 64);  /* 2-channel area */
    uint32_t area_2_toc_start = be32_to_cpu(master_data + 72);  /* Multi-channel area */
    uint16_t area_1_toc_size = be16_to_cpu(master_data + 84);
    uint16_t area_2_toc_size = be16_to_cpu(master_data + 86);
    
    int area_count = 0;
    
    /* Parse stereo area if present */
    if (area_1_toc_start > 0 && area_1_toc_size > 0) {
        result = parse_area_toc(internal, area_count, area_1_toc_start, area_1_toc_size);
        if (result == SACD_RESULT_OK) {
            internal->public.areas[area_count].type = SACD_AREA_STEREO;
            area_count++;
        }
    }
    
    /* Parse multichannel area if present */
    if (area_2_toc_start > 0 && area_2_toc_size > 0) {
        result = parse_area_toc(internal, area_count, area_2_toc_start, area_2_toc_size);
        if (result == SACD_RESULT_OK) {
            internal->public.areas[area_count].type = SACD_AREA_MULTICHANNEL;
            area_count++;
        }
    }
    
    /* Update actual area count */
    internal->public.area_count = area_count;
    
    internal->areas_parsed = true;
    return SACD_RESULT_OK;
}

/* Public API implementation */

sacd_result_t sacd_disc_open(const char *iso_path, sacd_disc_t **disc) {
    if (!iso_path || !disc) {
        return SACD_RESULT_ERROR;
    }
    
    *disc = NULL;
    
    /* Allocate internal structure */
    sacd_disc_internal_t *internal = calloc(1, sizeof(sacd_disc_internal_t));
    if (!internal) {
        return SACD_RESULT_OUT_OF_MEMORY;
    }
    
    /* Store ISO path */
    internal->iso_path = strdup(iso_path);
    if (!internal->iso_path) {
        free(internal);
        return SACD_RESULT_OUT_OF_MEMORY;
    }
    
    /* Open ISO file */
    internal->fd = open(iso_path, O_RDONLY);
    if (internal->fd < 0) {
        free(internal->iso_path);
        free(internal);
        return (errno == ENOENT) ? SACD_RESULT_INVALID_FILE : SACD_RESULT_IO_ERROR;
    }
    
    /* Get file size */
    struct stat st;
    if (fstat(internal->fd, &st) != 0) {
        close(internal->fd);
        free(internal->iso_path);
        free(internal);
        return SACD_RESULT_IO_ERROR;
    }
    internal->file_size = st.st_size;
    
    /* Parse disc structure */
    sacd_result_t result = parse_disc_structure(internal);
    if (result != SACD_RESULT_OK) {
        sacd_disc_close((sacd_disc_t*)internal);
        return result;
    }
    
    internal->is_open = true;
    internal->public.internal_data = internal;
    *disc = &internal->public;
    
    return SACD_RESULT_OK;
}

void sacd_disc_close(sacd_disc_t *disc) {
    if (!disc) {
        return;
    }
    
    sacd_disc_internal_t *internal = (sacd_disc_internal_t*)disc->internal_data;
    if (!internal) {
        return;
    }
    
    /* Close file */
    if (internal->fd >= 0) {
        close(internal->fd);
    }
    
    /* Free allocated memory */
    free(internal->iso_path);
    free(internal->sector_buffer);
    free(internal->master_toc_data);
    free(internal->text_data);
    
    for (int i = 0; i < SACD_MAX_AREAS; i++) {
        free(internal->area_data[i]);
    }
    
    /* Free text fields */
    free(disc->text.title);
    free(disc->text.title_phonetic);
    free(disc->text.artist);
    free(disc->text.artist_phonetic);
    free(disc->text.publisher);
    free(disc->text.publisher_phonetic);
    free(disc->text.copyright);
    free(disc->text.copyright_phonetic);
    
    /* Free area text fields */
    for (int i = 0; i < disc->area_count; i++) {
        sacd_area_t *area = &disc->areas[i];
        free(area->text.title);
        free(area->text.title_phonetic);
        free(area->text.artist);
        free(area->text.artist_phonetic);
        free(area->text.publisher);
        free(area->text.publisher_phonetic);
        free(area->text.copyright);
        free(area->text.copyright_phonetic);
        
        /* Free track text fields */
        for (int j = 0; j < area->track_count; j++) {
            sacd_track_t *track = &area->tracks[j];
            free(track->text.title);
            free(track->text.title_phonetic);
            free(track->text.artist);
            free(track->text.artist_phonetic);
            free(track->text.publisher);
            free(track->text.publisher_phonetic);
            free(track->text.copyright);
            free(track->text.copyright_phonetic);
        }
    }
    
    free(internal);
}

const sacd_area_t *sacd_disc_get_area(const sacd_disc_t *disc, sacd_area_type_t area_type) {
    if (!disc) {
        return NULL;
    }
    
    for (int i = 0; i < disc->area_count; i++) {
        if (disc->areas[i].type == area_type) {
            return &disc->areas[i];
        }
    }
    
    return NULL;
}

const sacd_area_t *sacd_disc_get_best_area(const sacd_disc_t *disc) {
    if (!disc || disc->area_count == 0) {
        return NULL;
    }
    
    /* Prefer stereo area if available */
    const sacd_area_t *stereo = sacd_disc_get_area(disc, SACD_AREA_STEREO);
    if (stereo) {
        return stereo;
    }
    
    /* Fall back to multichannel */
    return sacd_disc_get_area(disc, SACD_AREA_MULTICHANNEL);
}

/* Read a sector from SACD disc (wrapper function) */
sacd_result_t sacd_internal_read_sector(sacd_disc_internal_t *internal, uint32_t lsn, uint8_t *buffer) {
    return read_sector(internal, lsn, buffer);
}

/* Extract DSD audio data from a sector */
uint8_t *sacd_internal_extract_dsd_from_sector(const uint8_t *sector_data, size_t *audio_size) {
    if (!sector_data || !audio_size) {
        return NULL;
    }
    
    /* 
     * SACD sector structure:
     * - First 16 bytes: Sector header
     * - Next 2048 bytes: Audio data 
     * - Last 288 bytes: Error correction/subcode
     * 
     * For now, extract the main audio payload from sector
     */
    
    /* Skip 16-byte sector header */
    const uint8_t *audio_start = sector_data + 16;
    
    /* Audio data is 2048 bytes after header */
    size_t raw_audio_size = 2048;
    
    /* Allocate buffer for extracted audio */
    uint8_t *audio_data = malloc(raw_audio_size);
    if (!audio_data) {
        *audio_size = 0;
        return NULL;
    }
    
    /* Copy the audio data */
    memcpy(audio_data, audio_start, raw_audio_size);
    *audio_size = raw_audio_size;
    
    return audio_data;
}