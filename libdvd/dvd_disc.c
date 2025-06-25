#include "dvd_internal.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>

/* DVD Disc Operations - Core functionality for opening and parsing DVD ISOs */

/* Read a sector from the ISO file */
dvd_result_t dvd_iso_read_sector(dvd_disc_internal_t *internal, uint32_t lba, uint8_t *buffer) {
    if (!internal || !buffer) {
        return DVD_RESULT_ERROR;
    }
    
    off_t offset = (off_t)lba * DVD_SECTOR_SIZE;
    if (lseek(internal->fd, offset, SEEK_SET) != offset) {
        return DVD_RESULT_IO_ERROR;
    }
    
    ssize_t bytes_read = read(internal->fd, buffer, DVD_SECTOR_SIZE);
    if (bytes_read != DVD_SECTOR_SIZE) {
        return DVD_RESULT_IO_ERROR;
    }
    
    return DVD_RESULT_OK;
}

/* Parse ISO 9660 Primary Volume Descriptor */
dvd_result_t dvd_iso_parse_primary_volume_descriptor(dvd_disc_internal_t *internal) {
    if (!internal) {
        return DVD_RESULT_ERROR;
    }
    
    /* Allocate buffer for PVD */
    internal->primary_volume_descriptor = malloc(DVD_SECTOR_SIZE);
    if (!internal->primary_volume_descriptor) {
        return DVD_RESULT_OUT_OF_MEMORY;
    }
    
    /* Read Primary Volume Descriptor (sector 16) */
    dvd_result_t result = dvd_iso_read_sector(internal, ISO9660_PRIMARY_VOLUME_DESCRIPTOR_SECTOR, 
                                             internal->primary_volume_descriptor);
    if (result != DVD_RESULT_OK) {
        free(internal->primary_volume_descriptor);
        internal->primary_volume_descriptor = NULL;
        return result;
    }
    
    uint8_t *pvd = internal->primary_volume_descriptor;
    
    /* Verify ISO 9660 signature */
    if (memcmp(pvd + 1, "CD001", 5) != 0) {
        free(internal->primary_volume_descriptor);
        internal->primary_volume_descriptor = NULL;
        return DVD_RESULT_INVALID_FILE;
    }
    
    /* Extract volume identifier */
    memset(internal->public.volume_id, 0, sizeof(internal->public.volume_id));
    strncpy(internal->public.volume_id, (char*)pvd + 40, 31);
    
    /* Extract root directory information */
    uint8_t *root_dir_entry = pvd + 156; /* Root directory entry in PVD */
    internal->root_directory_lba = le32_to_cpu(root_dir_entry + 2);
    internal->root_directory_size = le32_to_cpu(root_dir_entry + 10);
    
    return DVD_RESULT_OK;
}

/* Find a directory within the ISO filesystem */
dvd_result_t dvd_iso_find_directory(dvd_disc_internal_t *internal, const char *dirname, uint32_t *lba, uint32_t *size) {
    if (!internal || !dirname || !lba || !size) {
        return DVD_RESULT_ERROR;
    }
    
    /* Read root directory */
    uint32_t sectors_needed = (internal->root_directory_size + DVD_SECTOR_SIZE - 1) / DVD_SECTOR_SIZE;
    uint8_t *root_data = malloc(sectors_needed * DVD_SECTOR_SIZE);
    if (!root_data) {
        return DVD_RESULT_OUT_OF_MEMORY;
    }
    
    /* Read all sectors of root directory */
    for (uint32_t i = 0; i < sectors_needed; i++) {
        dvd_result_t result = dvd_iso_read_sector(internal, internal->root_directory_lba + i,
                                                 root_data + (i * DVD_SECTOR_SIZE));
        if (result != DVD_RESULT_OK) {
            free(root_data);
            return result;
        }
    }
    
    /* Parse directory entries */
    uint32_t offset = 0;
    while (offset < internal->root_directory_size) {
        iso9660_directory_entry_t *entry = (iso9660_directory_entry_t*)(root_data + offset);
        
        if (entry->length == 0) {
            /* Skip to next sector */
            offset = ((offset / DVD_SECTOR_SIZE) + 1) * DVD_SECTOR_SIZE;
            continue;
        }
        
        /* Extract filename - use correct ISO 9660 offsets */
        char filename[256];
        uint8_t filename_len = root_data[offset + 32]; /* Filename length at offset 32 */
        if (filename_len > 0 && filename_len < 255) {
            memcpy(filename, root_data + offset + 33, filename_len); /* Filename at offset 33 */
            filename[filename_len] = '\0';
            
            /* Remove version suffix (;1) if present */
            char *version = strchr(filename, ';');
            if (version) {
                *version = '\0';
            }
            
            /* Check if this matches our target directory */
            uint8_t flags = root_data[offset + 25]; /* Flags at offset 25 */
            if (strcmp(filename, dirname) == 0 && (flags & 0x02)) { /* Directory flag */
                *lba = le32_to_cpu(root_data + offset + 2); /* LBA at offset 2 */
                *size = le32_to_cpu(root_data + offset + 10); /* Size at offset 10 */
                free(root_data);
                return DVD_RESULT_OK;
            }
        }
        
        offset += entry->length;
    }
    
    free(root_data);
    return DVD_RESULT_INVALID_FILE; /* Directory not found */
}

/* Parse disc structure to identify DVD type */
static dvd_result_t parse_disc_structure(dvd_disc_internal_t *internal) {
    dvd_result_t result;
    
    /* Parse ISO 9660 filesystem */
    result = dvd_iso_parse_primary_volume_descriptor(internal);
    if (result != DVD_RESULT_OK) {
        return result;
    }
    
    /* Look for AUDIO_TS directory */
    uint32_t audio_ts_lba, audio_ts_size;
    if (dvd_iso_find_directory(internal, DVD_AUDIO_DIR, &audio_ts_lba, &audio_ts_size) == DVD_RESULT_OK) {
        internal->has_audio_ts = true;
        internal->audio_ts_lba = audio_ts_lba;
    }
    
    /* Look for VIDEO_TS directory */
    uint32_t video_ts_lba, video_ts_size;
    if (dvd_iso_find_directory(internal, DVD_VIDEO_DIR, &video_ts_lba, &video_ts_size) == DVD_RESULT_OK) {
        internal->has_video_ts = true;
        internal->video_ts_lba = video_ts_lba;
    }
    
    /* Determine disc type based on directories found */
    if (internal->has_audio_ts && internal->has_video_ts) {
        internal->public.disc_type = DVD_TYPE_HYBRID;
    } else if (internal->has_audio_ts) {
        internal->public.disc_type = DVD_TYPE_AUDIO;
    } else if (internal->has_video_ts) {
        internal->public.disc_type = DVD_TYPE_VIDEO;
    } else {
        /* No DVD directories found - check for Blu-ray structure */
        if (bluray_detect_disc(internal) == DVD_RESULT_OK) {
            internal->public.disc_type = DVD_TYPE_BLURAY;
        } else {
            /* Default to DVD-Video for unknown disc types - be more lenient */
            printf("⚠️  No recognizable disc structure found, defaulting to DVD-Video\n");
            internal->public.disc_type = DVD_TYPE_VIDEO;
        }
    }
    
    return DVD_RESULT_OK;
}

/* Public API Implementation */

dvd_result_t dvd_disc_open(const char *iso_path, dvd_disc_t **disc) {
    if (!iso_path || !disc) {
        return DVD_RESULT_INVALID_PARAM;
    }
    
    *disc = NULL;
    
    /* Allocate internal structure */
    dvd_disc_internal_t *internal = calloc(1, sizeof(dvd_disc_internal_t));
    if (!internal) {
        return DVD_RESULT_OUT_OF_MEMORY;
    }
    
    /* Store ISO path */
    internal->iso_path = strdup(iso_path);
    if (!internal->iso_path) {
        free(internal);
        return DVD_RESULT_OUT_OF_MEMORY;
    }
    
    /* Open ISO file */
    internal->fd = open(iso_path, O_RDONLY);
    if (internal->fd < 0) {
        free(internal->iso_path);
        free(internal);
        return (errno == ENOENT) ? DVD_RESULT_INVALID_FILE : DVD_RESULT_IO_ERROR;
    }
    
    /* Get file size */
    struct stat st;
    if (fstat(internal->fd, &st) != 0) {
        close(internal->fd);
        free(internal->iso_path);
        free(internal);
        return DVD_RESULT_IO_ERROR;
    }
    internal->file_size = st.st_size;
    
    /* Parse disc structure */
    dvd_result_t result = parse_disc_structure(internal);
    if (result != DVD_RESULT_OK) {
        dvd_disc_close((dvd_disc_t*)internal);
        return result;
    }
    
    internal->is_open = true;
    internal->public.internal_data = internal;
    *disc = &internal->public;
    
    return DVD_RESULT_OK;
}

void dvd_disc_close(dvd_disc_t *disc) {
    if (!disc) return;
    
    dvd_disc_internal_t *internal = (dvd_disc_internal_t*)disc->internal_data;
    if (!internal) return;
    
    /* Close file descriptor */
    if (internal->fd >= 0) {
        close(internal->fd);
    }
    
    /* Free allocated memory */
    free(internal->primary_volume_descriptor);
    free(internal->iso_path);
    
    /* Free titles and tracks */
    if (disc->titles) {
        for (int i = 0; i < disc->title_count; i++) {
            free(disc->titles[i].audio_tracks);
        }
        free(disc->titles);
    }
    
    free(internal);
}

dvd_result_t dvd_disc_get_info(dvd_disc_t *disc) {
    if (!disc) {
        return DVD_RESULT_INVALID_PARAM;
    }
    
    dvd_disc_internal_t *internal = (dvd_disc_internal_t*)disc->internal_data;
    if (!internal || !internal->is_open) {
        return DVD_RESULT_ERROR;
    }
    
    /* If already parsed, return success */
    if (internal->titles_parsed) {
        return DVD_RESULT_OK;
    }
    
    dvd_result_t result = DVD_RESULT_OK;
    
    /* Parse based on disc type */
    switch (disc->disc_type) {
        case DVD_TYPE_AUDIO:
            result = dvd_audio_parse_audio_ts(internal);
            break;
        case DVD_TYPE_VIDEO:
            result = dvd_video_parse_video_ts(internal);
            break;
        case DVD_TYPE_HYBRID:
            /* Parse both - prefer audio content */
            result = dvd_audio_parse_audio_ts(internal);
            if (result != DVD_RESULT_OK) {
                result = dvd_video_parse_video_ts(internal);
            }
            break;
        case DVD_TYPE_BLURAY:
            result = bluray_scan_playlists(internal);
            break;
        default:
            result = DVD_RESULT_ERROR;
            break;
    }
    
    if (result == DVD_RESULT_OK) {
        internal->titles_parsed = true;
    }
    
    return result;
}

dvd_result_t dvd_get_title_count(dvd_disc_t *disc, uint8_t *count) {
    if (!disc || !count) {
        return DVD_RESULT_INVALID_PARAM;
    }
    
    /* Ensure disc info is parsed */
    dvd_result_t result = dvd_disc_get_info(disc);
    if (result != DVD_RESULT_OK) {
        return result;
    }
    
    *count = disc->title_count;
    return DVD_RESULT_OK;
}

dvd_result_t dvd_get_title(dvd_disc_t *disc, uint8_t title_number, dvd_title_t **title) {
    if (!disc || !title || title_number == 0) {
        return DVD_RESULT_INVALID_PARAM;
    }
    
    /* Ensure disc info is parsed */
    dvd_result_t result = dvd_disc_get_info(disc);
    if (result != DVD_RESULT_OK) {
        return result;
    }
    
    if (title_number > disc->title_count) {
        return DVD_RESULT_INVALID_PARAM;
    }
    
    *title = &disc->titles[title_number - 1];
    return DVD_RESULT_OK;
}

/* Utility functions */
const char *dvd_get_format_name(dvd_audio_format_t format) {
    switch (format) {
        case DVD_AUDIO_FORMAT_LPCM: return "LPCM";
        case DVD_AUDIO_FORMAT_MLP: return "MLP";
        case DVD_AUDIO_FORMAT_AC3: return "AC3";
        case DVD_AUDIO_FORMAT_DTS: return "DTS";
        case DVD_AUDIO_FORMAT_MPEG: return "MPEG";
        case DVD_AUDIO_FORMAT_TRUEHD: return "TrueHD";
        case DVD_AUDIO_FORMAT_DTS_HD: return "DTS-HD MA";
        case DVD_AUDIO_FORMAT_DTS_HD_HR: return "DTS-HD HR";
        default: return "Unknown";
    }
}

const char *dvd_result_to_string(dvd_result_t result) {
    switch (result) {
        case DVD_RESULT_OK: return "OK";
        case DVD_RESULT_ERROR: return "Error";
        case DVD_RESULT_OUT_OF_MEMORY: return "Out of memory";
        case DVD_RESULT_INVALID_FILE: return "Invalid file";
        case DVD_RESULT_IO_ERROR: return "I/O error";
        case DVD_RESULT_NOT_IMPLEMENTED: return "Not implemented";
        case DVD_RESULT_INVALID_PARAM: return "Invalid parameter";
        default: return "Unknown error";
    }
}

bool dvd_is_lossless_format(dvd_audio_format_t format) {
    return (format == DVD_AUDIO_FORMAT_LPCM || 
            format == DVD_AUDIO_FORMAT_MLP ||
            format == DVD_AUDIO_FORMAT_TRUEHD ||
            format == DVD_AUDIO_FORMAT_DTS_HD);
}

uint32_t dvd_get_bitrate(dvd_audio_format_t format, uint8_t channels, uint32_t sample_rate, uint8_t bits_per_sample) {
    switch (format) {
        case DVD_AUDIO_FORMAT_LPCM:
        case DVD_AUDIO_FORMAT_MLP:
        case DVD_AUDIO_FORMAT_TRUEHD:
        case DVD_AUDIO_FORMAT_DTS_HD:
            return channels * sample_rate * bits_per_sample;
        case DVD_AUDIO_FORMAT_AC3:
            return 448000; /* Typical AC3 bitrate */
        case DVD_AUDIO_FORMAT_DTS:
            return 1536000; /* Typical DTS bitrate */
        case DVD_AUDIO_FORMAT_DTS_HD_HR:
            return 6144000; /* DTS-HD High Resolution max bitrate */
        case DVD_AUDIO_FORMAT_MPEG:
            return 384000; /* Typical MPEG audio bitrate */
        default:
            return 0;
    }
}