#include "dvd_internal.h"
#include <stdlib.h>
#include <string.h>

/* DVD-Audio Parsing Functions */

/* DVD-Audio IFO structures */
typedef struct {
    char signature[12];         /* "DVDAUDIO-ATS" or "DVDAUDIO-AMG" */
    uint32_t last_sector;       /* Last sector of ATS */
    uint8_t version_info[4];    /* Version information */
    uint32_t ats_category;      /* ATS category */
    uint8_t reserved[8];        /* Reserved */
    uint32_t audio_attrib_start; /* Start of audio attribute table */
    uint32_t audio_title_start; /* Start of audio title table */
    /* More fields... */
} dvd_audio_ifo_header_t;

typedef struct {
    uint8_t audio_format;       /* 0=LPCM, 1=MLP, 2=DTS, 3=SDDS */
    uint8_t quantization;       /* 0=16bit, 1=20bit, 2=24bit */
    uint8_t sample_rate;        /* 0=48kHz, 1=96kHz, 2=192kHz, etc. */
    uint8_t channels;           /* Number of channels - 1 */
    uint32_t start_lsn;         /* Starting LSN */
    uint32_t end_lsn;           /* Ending LSN */
} dvd_audio_track_header_t;

/* Helper functions */
static uint32_t get_sample_rate_from_code(uint8_t code) {
    switch (code) {
        case 0: return 48000;
        case 1: return 96000;
        case 2: return 192000;
        case 8: return 44100;
        case 9: return 88200;
        case 10: return 176400;
        default: return 48000;
    }
}

static uint8_t get_bit_depth_from_code(uint8_t code) {
    switch (code) {
        case 0: return 16;
        case 1: return 20;
        case 2: return 24;
        default: return 16;
    }
}

static dvd_audio_format_t get_audio_format_from_code(uint8_t code) {
    switch (code) {
        case 0: return DVD_AUDIO_FORMAT_LPCM;
        case 1: return DVD_AUDIO_FORMAT_MLP;
        case 2: return DVD_AUDIO_FORMAT_DTS;
        default: return DVD_AUDIO_FORMAT_LPCM;
    }
}

/* Read directory contents from ISO */
dvd_result_t dvd_iso_read_directory(dvd_disc_internal_t *internal, uint32_t lba, uint32_t size, uint8_t **data) {
    if (!internal || !data) {
        return DVD_RESULT_INVALID_PARAM;
    }
    
    uint32_t sectors_needed = (size + DVD_SECTOR_SIZE - 1) / DVD_SECTOR_SIZE;
    *data = malloc(sectors_needed * DVD_SECTOR_SIZE);
    if (!*data) {
        return DVD_RESULT_OUT_OF_MEMORY;
    }
    
    for (uint32_t i = 0; i < sectors_needed; i++) {
        dvd_result_t result = dvd_iso_read_sector(internal, lba + i, *data + (i * DVD_SECTOR_SIZE));
        if (result != DVD_RESULT_OK) {
            free(*data);
            *data = NULL;
            return result;
        }
    }
    
    return DVD_RESULT_OK;
}

/* Find a file within a directory */
static dvd_result_t find_file_in_directory(uint8_t *dir_data, uint32_t dir_size, const char *filename, uint32_t *file_lba, uint32_t *file_size) {
    uint32_t offset = 0;
    
    while (offset < dir_size) {
        iso9660_directory_entry_t *entry = (iso9660_directory_entry_t*)(dir_data + offset);
        
        if (entry->length == 0) {
            /* Skip to next sector */
            offset = ((offset / DVD_SECTOR_SIZE) + 1) * DVD_SECTOR_SIZE;
            continue;
        }
        
        /* Extract filename - use correct ISO 9660 offsets */
        char entry_filename[256];
        uint8_t filename_len = dir_data[offset + 32]; /* Filename length at offset 32 */
        if (filename_len > 0 && filename_len < 255) {
            memcpy(entry_filename, dir_data + offset + 33, filename_len); /* Filename at offset 33 */
            entry_filename[filename_len] = '\0';
            
            /* Remove version suffix (;1) if present */
            char *version = strchr(entry_filename, ';');
            if (version) {
                *version = '\0';
            }
            
            /* Check if this matches our target file */
            uint8_t flags = dir_data[offset + 25]; /* Flags at offset 25 */
            if (strcasecmp(entry_filename, filename) == 0 && !(flags & 0x02)) { /* Not a directory */
                *file_lba = (dir_data[offset + 2]) | (dir_data[offset + 3] << 8) | (dir_data[offset + 4] << 16) | (dir_data[offset + 5] << 24);
                *file_size = (dir_data[offset + 10]) | (dir_data[offset + 11] << 8) | (dir_data[offset + 12] << 16) | (dir_data[offset + 13] << 24);
                return DVD_RESULT_OK;
            }
        }
        
        offset += entry->length;
    }
    
    return DVD_RESULT_INVALID_FILE; /* File not found */
}

/* Parse DVD-Audio IFO file */
dvd_result_t dvd_audio_parse_ifo(dvd_disc_internal_t *internal, const uint8_t *ifo_data, size_t size) {
    if (!internal || !ifo_data || size < 64) {
        return DVD_RESULT_INVALID_PARAM;
    }
    
    /* Verify signature */
    printf("🔍 IFO signature check: '%.12s'\n", ifo_data);
    
    if (memcmp(ifo_data, DVD_AUDIO_IFO_SIGNATURE, 12) == 0 ||
        memcmp(ifo_data, DVD_AUDIO_AMG_SIGNATURE, 12) == 0 ||
        memcmp(ifo_data, DVD_AUDIO_APP_SIGNATURE, 12) == 0) {
        printf("✅ DVD-Audio IFO signature verified: %.12s\n", ifo_data);
    } else {
        printf("❌ Unknown IFO signature - may not be DVD-Audio\n");
        return DVD_RESULT_INVALID_FILE;
    }
    
    /* Parse IFO header - use endian conversion from dvd_utils.c */
    uint32_t last_sector = (ifo_data[12]) | (ifo_data[13] << 8) | (ifo_data[14] << 16) | (ifo_data[15] << 24);
    uint32_t ats_category = (ifo_data[20]) | (ifo_data[21] << 8) | (ifo_data[22] << 16) | (ifo_data[23] << 24);
    
    printf("📀 IFO Analysis: last_sector=%u, ats_category=0x%08X\n", last_sector, ats_category);
    
    /* Look for title information in the IFO */
    /* DVD-Audio typically has title info starting around offset 0xC0+ */
    uint8_t title_count = 1;
    uint8_t track_count = 1;
    
    /* Try to extract track count from IFO data */
    if (size >= 0xC8) {
        /* Check for track count indicator at offset 0xC7 */
        uint8_t possible_track_count = ifo_data[0xC7];
        if (possible_track_count > 0 && possible_track_count <= 20) {
            track_count = possible_track_count;
            printf("🎵 Found track count: %d tracks\n", track_count);
        }
        
        /* Check for title count at offset 0xC6 */
        uint8_t possible_title_count = ifo_data[0xC6];
        if (possible_title_count > 0 && possible_title_count <= 10) {
            title_count = possible_title_count;
        }
    }
    
    internal->public.title_count = title_count;
    internal->public.titles = calloc(title_count, sizeof(dvd_title_t));
    if (!internal->public.titles) {
        return DVD_RESULT_OUT_OF_MEMORY;
    }
    
    for (int title_idx = 0; title_idx < title_count; title_idx++) {
        dvd_title_t *title = &internal->public.titles[title_idx];
        title->title_number = title_idx + 1;
        title->audio_track_count = track_count;
        snprintf(title->title_name, sizeof(title->title_name), "DVD-Audio Title %d", title_idx + 1);
        
        /* Create audio tracks */
        title->audio_tracks = calloc(track_count, sizeof(dvd_audio_track_t));
        if (!title->audio_tracks) {
            return DVD_RESULT_OUT_OF_MEMORY;
        }
        
        double total_duration = 0.0;
        
        for (int track_idx = 0; track_idx < track_count; track_idx++) {
            dvd_audio_track_t *track = &title->audio_tracks[track_idx];
            track->track_number = track_idx + 1;
            
            /* Try to extract real track information */
            /* DVD-Audio track info often starts around offset 0x100+ */
            uint32_t track_offset = 0x100 + (track_idx * 32); /* Estimated track record size */
            
            if (size >= track_offset + 16) {
                /* Try to read track attributes */
                uint8_t audio_format = ifo_data[track_offset];
                uint8_t sample_rate_code = ifo_data[track_offset + 1];
                uint8_t channels_code = ifo_data[track_offset + 2];
                uint8_t bits_code = ifo_data[track_offset + 3];
                
                /* Parse format */
                switch (audio_format & 0x0F) {
                    case 0: track->format = DVD_AUDIO_FORMAT_LPCM; break;
                    case 1: track->format = DVD_AUDIO_FORMAT_MLP; break;
                    case 2: track->format = DVD_AUDIO_FORMAT_DTS; break;
                    default: track->format = DVD_AUDIO_FORMAT_LPCM; break;
                }
                
                /* Parse sample rate */
                switch (sample_rate_code & 0x0F) {
                    case 0: track->sample_rate = 48000; break;
                    case 1: track->sample_rate = 96000; break;
                    case 2: track->sample_rate = 192000; break;
                    case 8: track->sample_rate = 44100; break;
                    case 9: track->sample_rate = 88200; break;
                    case 10: track->sample_rate = 176400; break;
                    default: track->sample_rate = 96000; break;
                }
                
                /* Parse channels */
                track->channels = (channels_code & 0x07) + 1;
                if (track->channels > 8) track->channels = 2; /* Sanity check */
                
                /* Parse bits per sample */
                switch (bits_code & 0x03) {
                    case 0: track->bits_per_sample = 16; break;
                    case 1: track->bits_per_sample = 20; break;
                    case 2: track->bits_per_sample = 24; break;
                    default: track->bits_per_sample = 24; break;
                }
                
                printf("🎵 Track %d: %s %d.%d @ %d Hz, %d-bit\n", 
                       track_idx + 1,
                       (track->format == DVD_AUDIO_FORMAT_LPCM) ? "LPCM" : 
                       (track->format == DVD_AUDIO_FORMAT_MLP) ? "MLP" : "DTS",
                       track->channels > 1 ? track->channels - 1 : track->channels,
                       track->channels > 2 ? 1 : 0,
                       track->sample_rate,
                       track->bits_per_sample);
            } else {
                /* Fallback to default values */
                track->format = DVD_AUDIO_FORMAT_LPCM;
                track->channels = 2;
                track->sample_rate = 96000;
                track->bits_per_sample = 24;
            }
            
            /* Set track metadata */
            snprintf(track->title, sizeof(track->title), "Track %d", track_idx + 1);
            strcpy(track->language, "en");
            
            /* Estimate sector range and duration */
            track->start_sector = track_idx * 10000;  /* Rough estimate */
            track->end_sector = (track_idx + 1) * 10000 - 1;
            
            /* Calculate duration estimate based on format */
            double track_duration = 180.0 + (track_idx * 30.0); /* Rough estimate */
            track->duration_seconds = track_duration;
            track->duration_samples = (uint64_t)(track_duration * track->sample_rate);
            
            total_duration += track_duration;
        }
        
        title->duration_seconds = total_duration;
    }
    
    printf("✅ DVD-Audio parsing complete: %d title(s), %d track(s) per title\n", 
           title_count, track_count);
    
    return DVD_RESULT_OK;
}

/* Parse AUDIO_TS directory */
dvd_result_t dvd_audio_parse_audio_ts(dvd_disc_internal_t *internal) {
    if (!internal || !internal->has_audio_ts) {
        return DVD_RESULT_ERROR;
    }
    
    /* Read AUDIO_TS directory */
    uint8_t *audio_ts_data;
    uint32_t audio_ts_size = 2048; /* Assume one sector for now */
    dvd_result_t result = dvd_iso_read_directory(internal, internal->audio_ts_lba, audio_ts_size, &audio_ts_data);
    if (result != DVD_RESULT_OK) {
        return result;
    }
    
    /* Look for IFO files */
    uint32_t ifo_lba, ifo_size;
    
    printf("🔍 Searching AUDIO_TS directory for IFO files...\n");
    
    /* Try to find AUDIO_PP.IFO, ATS_01_0.IFO, or .BUP files */
    if (find_file_in_directory(audio_ts_data, audio_ts_size, "AUDIO_PP.IFO", &ifo_lba, &ifo_size) == DVD_RESULT_OK) {
        printf("✅ Found AUDIO_PP.IFO (LBA=%u, Size=%u)\n", ifo_lba, ifo_size);
    } else if (find_file_in_directory(audio_ts_data, audio_ts_size, "ATS_01_0.IFO", &ifo_lba, &ifo_size) == DVD_RESULT_OK) {
        printf("✅ Found ATS_01_0.IFO (LBA=%u, Size=%u)\n", ifo_lba, ifo_size);
    } else if (find_file_in_directory(audio_ts_data, audio_ts_size, "ATS_01_0.BUP", &ifo_lba, &ifo_size) == DVD_RESULT_OK) {
        printf("✅ Found ATS_01_0.BUP (LBA=%u, Size=%u)\n", ifo_lba, ifo_size);
    } else {
        printf("❌ No IFO files found in AUDIO_TS\n");
        free(audio_ts_data);
        return DVD_RESULT_INVALID_FILE; /* No valid IFO found */
    }
    
    if (1) { /* Always enter this block if any file was found */
        
        /* Read IFO file */
        uint32_t ifo_sectors = (ifo_size + DVD_SECTOR_SIZE - 1) / DVD_SECTOR_SIZE;
        uint8_t *ifo_data = malloc(ifo_sectors * DVD_SECTOR_SIZE);
        if (!ifo_data) {
            free(audio_ts_data);
            return DVD_RESULT_OUT_OF_MEMORY;
        }
        
        for (uint32_t i = 0; i < ifo_sectors; i++) {
            result = dvd_iso_read_sector(internal, ifo_lba + i, ifo_data + (i * DVD_SECTOR_SIZE));
            if (result != DVD_RESULT_OK) {
                free(ifo_data);
                free(audio_ts_data);
                return result;
            }
        }
        
        /* Parse IFO file */
        printf("📀 Reading IFO file (%u sectors)...\n", ifo_sectors);
        result = dvd_audio_parse_ifo(internal, ifo_data, ifo_size);
        printf("📀 IFO parsing result: %s\n", 
               (result == DVD_RESULT_OK) ? "SUCCESS" : 
               (result == DVD_RESULT_INVALID_FILE) ? "INVALID_FILE" : "ERROR");
        
        free(ifo_data);
        free(audio_ts_data);
        return result;
    }
    
    free(audio_ts_data);
    return DVD_RESULT_INVALID_FILE; /* No valid IFO found */
}