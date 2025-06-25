#include "dvd_internal.h"
#include <stdlib.h>
#include <string.h>

/* DVD-Video Parsing Functions */

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
        
        /* Extract filename */
        char entry_filename[256];
        uint8_t filename_len = entry->filename_length;
        if (filename_len > 0 && filename_len < 255) {
            memcpy(entry_filename, dir_data + offset + sizeof(iso9660_directory_entry_t), filename_len);
            entry_filename[filename_len] = '\0';
            
            /* Remove version suffix (;1) if present */
            char *version = strchr(entry_filename, ';');
            if (version) {
                *version = '\0';
            }
            
            /* Check if this matches our target file */
            if (strcasecmp(entry_filename, filename) == 0 && !(entry->flags & 0x02)) { /* Not a directory */
                *file_lba = le32_to_cpu((uint8_t*)&entry->location_le);
                *file_size = le32_to_cpu((uint8_t*)&entry->data_length_le);
                return DVD_RESULT_OK;
            }
        }
        
        offset += entry->length;
    }
    
    return DVD_RESULT_INVALID_FILE; /* File not found */
}

/* DVD-Video IFO structures - Based on DVD-Video specification */

/* Video Manager Information (VMGI) or Video Title Set Information (VTSI) */
typedef struct {
    char identifier[12];        /* "DVDVIDEO-VMG" or "DVDVIDEO-VTS" */
    uint32_t last_sector;       /* Last sector of this IFO */
    uint8_t reserved1[12];      /* Reserved */
    uint32_t last_sector_ifo;   /* Last sector of IFO */
    uint8_t reserved2[4];       /* Reserved */
    uint32_t start_byte_vmgi;   /* Start byte of VMGI/VTSI */
    uint8_t reserved3[56];      /* Reserved */
    uint32_t start_sector_menu; /* Start sector of menu VOBs */
    uint32_t start_sector_title; /* Start sector of title VOBs */
    uint32_t start_byte_ptr_tab; /* Start byte of pointer table */
    uint32_t start_byte_attr_tab; /* Start byte of attribute table */
    uint32_t start_byte_pgci;   /* Start byte of Program Chain Information */
    uint32_t start_byte_ptl_mait; /* Start byte of PTL_MAIT */
    uint32_t start_byte_vts_atrt; /* Start byte of VTS_ATRT */
    uint32_t start_byte_txtdt_mgi; /* Start byte of TXTDT_MGI */
    uint32_t start_byte_c_adt;  /* Start byte of C_ADT */
    uint32_t start_byte_vobu_admap; /* Start byte of VOBU_ADMAP */
    uint8_t reserved4[32];      /* Reserved */
    /* Video/Audio/Subpicture attributes follow */
} dvd_ifo_header_t;

/* Audio Stream Attributes */
typedef struct {
    uint8_t coding_mode;        /* Bit 7-5: Audio coding mode */
    uint8_t multichannel_ext;   /* Bit 4: Multichannel extension */
    uint8_t lang_type;          /* Bit 3-2: Language type */
    uint8_t app_info;           /* Bit 1-0: Application info */
    uint8_t lang_code[2];       /* Language code (ISO 639) */
    uint8_t lang_ext;           /* Language extension */
    uint8_t code_ext;           /* Code extension */
    uint8_t unknown;            /* Unknown byte */
    uint8_t channels;           /* Channel assignment */
    uint16_t sample_freq;       /* Sample frequency */
    uint8_t quantization;       /* Quantization/DRC */
    uint8_t reserved[3];        /* Reserved */
} dvd_audio_attr_t;

/* Program Chain Information Table (PGCIT) */
typedef struct {
    uint16_t num_pgci;          /* Number of program chains */
    uint16_t reserved;          /* Reserved */
    uint32_t last_byte;         /* Last byte of PGCIT */
    /* PGC Information Search Pointers follow */
} dvd_pgcit_t;

/* Program Chain Information (PGCI) */
typedef struct {
    uint16_t reserved1;         /* Reserved */
    uint8_t num_programs;       /* Number of programs */
    uint8_t num_cells;          /* Number of cells */
    uint32_t playback_time;     /* Playback time */
    uint32_t prohibited_ops;    /* Prohibited user operations */
    uint16_t audio_control[8];  /* Audio stream control */
    uint32_t subpic_control[32]; /* Subpicture stream control */
    uint16_t next_pgcn;         /* Next PGCN */
    uint16_t prev_pgcn;         /* Previous PGCN */
    uint16_t goup_pgcn;         /* GoUp PGCN */
    uint8_t still_time;         /* Still time */
    uint8_t pg_playback_mode;   /* PG playback mode */
    uint32_t palette[16];       /* Palette */
    uint16_t command_tbl_offset; /* Command table offset */
    uint16_t program_map_offset; /* Program map offset */
    uint16_t cell_playback_offset; /* Cell playback offset */
    uint16_t cell_position_offset; /* Cell position offset */
    /* Command table, Program map, Cell playback/position tables follow */
} dvd_pgci_t;

/* Cell Playback Information */
typedef struct {
    uint8_t block_mode;         /* Block mode */
    uint8_t block_type;         /* Block type */
    uint8_t seamless_play;      /* Seamless play */
    uint8_t interleaved;        /* Interleaved */
    uint8_t stc_discontinuity;  /* STC discontinuity */
    uint8_t seamless_angle;     /* Seamless angle */
    uint8_t playback_mode;      /* Playback mode */
    uint8_t restricted;         /* Restricted */
    uint8_t unknown1;           /* Unknown */
    uint8_t still_time;         /* Still time */
    uint8_t cell_cmd_nr;        /* Cell command number */
    uint32_t playback_time;     /* Playback time */
    uint32_t first_sector;      /* First sector */
    uint32_t first_ilvu_end_sector; /* First ILVU end sector */
    uint32_t last_vobu_start_sector; /* Last VOBU start sector */
    uint32_t last_sector;       /* Last sector */
} dvd_cell_playback_t;

/* VOB Audio Stream Information - now defined in dvd_internal.h */

/* Helper functions for DVD-Video */
static dvd_audio_format_t get_video_audio_format_from_code(uint8_t code) {
    switch (code) {
        case 0: return DVD_AUDIO_FORMAT_AC3;
        case 1: 
        case 2: return DVD_AUDIO_FORMAT_MPEG;
        case 3: return DVD_AUDIO_FORMAT_LPCM;
        case 4: return DVD_AUDIO_FORMAT_DTS;
        default: return DVD_AUDIO_FORMAT_AC3;
    }
}

static uint32_t get_video_sample_rate_from_code(uint8_t code) {
    switch (code) {
        case 0: return 48000;
        case 1: return 96000;
        default: return 48000;
    }
}

static uint8_t get_video_bit_depth_from_code(uint8_t code) {
    switch (code) {
        case 0: return 16;
        case 1: return 20;
        case 2: return 24;
        default: return 16;
    }
}

static uint8_t get_channels_from_multichannel(uint8_t multichannel_ext) {
    /* Simplified channel mapping */
    switch (multichannel_ext & 0x07) {
        case 0: return 1; /* Mono */
        case 1: return 2; /* Stereo */
        case 2: return 3; /* 2.1 */
        case 3: return 4; /* 4.0 */
        case 4: return 5; /* 4.1 */
        case 5: return 6; /* 5.1 */
        case 6: return 7; /* 6.1 */
        case 7: return 8; /* 7.1 */
        default: return 2;
    }
}

/* Parse audio stream attributes from IFO */
static dvd_result_t parse_audio_attributes(const dvd_audio_attr_t *attr, dvd_vob_audio_stream_t *stream) {
    if (!attr || !stream) {
        return DVD_RESULT_INVALID_PARAM;
    }
    
    /* Parse audio coding mode */
    uint8_t coding_mode = (attr->coding_mode >> 5) & 0x07;
    switch (coding_mode) {
        case 0: stream->format = DVD_AUDIO_FORMAT_AC3; break;
        case 1: stream->format = DVD_AUDIO_FORMAT_MPEG; break;
        case 2: stream->format = DVD_AUDIO_FORMAT_MPEG; break;
        case 3: stream->format = DVD_AUDIO_FORMAT_LPCM; break;
        case 4: stream->format = DVD_AUDIO_FORMAT_DTS; break;
        case 6: stream->format = DVD_AUDIO_FORMAT_DTS; break; /* DTS-HD */
        default: stream->format = DVD_AUDIO_FORMAT_AC3; break;
    }
    
    /* Parse channel configuration */
    uint8_t ch_config = attr->channels & 0x07;
    switch (ch_config) {
        case 0: stream->channels = 1; break; /* Mono */
        case 1: stream->channels = 2; break; /* Stereo */
        case 2: stream->channels = 3; break; /* 2.1 */
        case 3: stream->channels = 4; break; /* 4.0 */
        case 4: stream->channels = 5; break; /* 4.1 */
        case 5: stream->channels = 6; break; /* 5.1 */
        case 6: stream->channels = 7; break; /* 6.1 */
        case 7: stream->channels = 8; break; /* 7.1 */
        default: stream->channels = 2; break;
    }
    
    /* Parse sample rate (for LPCM) */
    if (stream->format == DVD_AUDIO_FORMAT_LPCM) {
        uint8_t sample_freq = (le16_to_cpu((uint8_t*)&attr->sample_freq) >> 6) & 0x03;
        switch (sample_freq) {
            case 0: stream->sample_rate = 48000; break;
            case 1: stream->sample_rate = 96000; break;
            case 2: stream->sample_rate = 192000; break;
            default: stream->sample_rate = 48000; break;
        }
        
        /* Parse quantization */
        uint8_t quant = (attr->quantization >> 6) & 0x03;
        switch (quant) {
            case 0: stream->bits_per_sample = 16; break;
            case 1: stream->bits_per_sample = 20; break;
            case 2: stream->bits_per_sample = 24; break;
            default: stream->bits_per_sample = 16; break;
        }
    } else {
        /* For compressed formats, use standard values */
        stream->sample_rate = 48000;
        stream->bits_per_sample = 16;
    }
    
    /* Parse language code */
    if (attr->lang_code[0] && attr->lang_code[1]) {
        stream->language[0] = attr->lang_code[0];
        stream->language[1] = attr->lang_code[1];
        stream->language[2] = '\0';
        stream->language[3] = '\0';
    } else {
        strcpy(stream->language, "un"); /* Unknown */
    }
    
    return DVD_RESULT_OK;
}

/* Parse Program Chain Information Table */
static dvd_result_t parse_pgci_table(const uint8_t *ifo_data, size_t ifo_size, uint32_t pgci_offset, 
                                   dvd_vob_audio_stream_t *streams, int max_streams, int *stream_count) {
    if (!ifo_data || pgci_offset >= ifo_size) {
        return DVD_RESULT_INVALID_PARAM;
    }
    
    const dvd_pgcit_t *pgcit = (const dvd_pgcit_t*)(ifo_data + pgci_offset);
    uint16_t num_pgc = le16_to_cpu((uint8_t*)&pgcit->num_pgci);
    
    if (num_pgc == 0) {
        *stream_count = 0;
        return DVD_RESULT_OK;
    }
    
    /* For simplicity, parse only the first program chain */
    /* In a full implementation, we'd iterate through all PGCs */
    uint32_t pgci_ptr_offset = pgci_offset + sizeof(dvd_pgcit_t);
    if (pgci_ptr_offset + 8 >= ifo_size) {
        return DVD_RESULT_INVALID_FILE;
    }
    
    /* Read PGC search pointer */
    uint32_t pgc_offset = le32_to_cpu(ifo_data + pgci_ptr_offset + 4) + pgci_offset;
    if (pgc_offset >= ifo_size || pgc_offset + sizeof(dvd_pgci_t) >= ifo_size) {
        return DVD_RESULT_INVALID_FILE;
    }
    
    const dvd_pgci_t *pgci = (const dvd_pgci_t*)(ifo_data + pgc_offset);
    
    /* Extract cell playback information for sector ranges */
    uint16_t cell_offset = le16_to_cpu((uint8_t*)&pgci->cell_playback_offset);
    if (cell_offset == 0 || pgc_offset + cell_offset >= ifo_size) {
        return DVD_RESULT_INVALID_FILE;
    }
    
    const dvd_cell_playback_t *cells = (const dvd_cell_playback_t*)(ifo_data + pgc_offset + cell_offset);
    uint8_t num_cells = pgci->num_cells;
    
    if (num_cells > 0 && pgc_offset + cell_offset + (num_cells * sizeof(dvd_cell_playback_t)) <= ifo_size) {
        /* Calculate total duration and sector range from first and last cells */
        uint32_t first_sector = le32_to_cpu((uint8_t*)&cells[0].first_sector);
        uint32_t last_sector = le32_to_cpu((uint8_t*)&cells[num_cells-1].last_sector);
        uint32_t total_time = le32_to_cpu((uint8_t*)&pgci->playback_time);
        
        /* Set sector ranges and duration for all streams */
        for (int i = 0; i < *stream_count && i < max_streams; i++) {
            streams[i].start_sector = first_sector;
            streams[i].end_sector = last_sector;
            
            /* Convert BCD time to seconds */
            uint8_t hours = ((total_time >> 20) & 0x0F) + (((total_time >> 24) & 0x0F) * 10);
            uint8_t minutes = ((total_time >> 12) & 0x0F) + (((total_time >> 16) & 0x0F) * 10);
            uint8_t seconds = ((total_time >> 4) & 0x0F) + (((total_time >> 8) & 0x0F) * 10);
            
            streams[i].duration = (hours * 3600.0) + (minutes * 60.0) + seconds;
        }
    }
    
    return DVD_RESULT_OK;
}

/* Parse DVD-Video IFO file */
dvd_result_t dvd_video_parse_ifo(dvd_disc_internal_t *internal, const uint8_t *ifo_data, size_t size) {
    if (!internal || !ifo_data || size < sizeof(dvd_ifo_header_t)) {
        return DVD_RESULT_INVALID_PARAM;
    }
    
    const dvd_ifo_header_t *header = (const dvd_ifo_header_t*)ifo_data;
    
    /* Verify signature */
    if (memcmp(header->identifier, DVD_VIDEO_IFO_SIGNATURE, 12) != 0 &&
        memcmp(header->identifier, DVD_VIDEO_VMG_SIGNATURE, 12) != 0) {
        return DVD_RESULT_INVALID_FILE;
    }
    
    /* Parse audio attributes from the attribute table */
    uint32_t attr_offset = le32_to_cpu((uint8_t*)&header->start_byte_attr_tab);
    if (attr_offset == 0 || attr_offset >= size) {
        /* No audio attributes - create default tracks */
        return dvd_video_create_default_tracks(internal);
    }
    
    /* Read audio attributes - typically 8 possible audio streams */
    const dvd_audio_attr_t *audio_attrs = (const dvd_audio_attr_t*)(ifo_data + attr_offset + 2); /* Skip video attr */
    dvd_vob_audio_stream_t temp_streams[DVD_MAX_AUDIO_TRACKS];
    int valid_streams = 0;
    
    /* Parse each audio attribute */
    for (int i = 0; i < DVD_MAX_AUDIO_TRACKS && (attr_offset + 2 + (i * sizeof(dvd_audio_attr_t))) < size; i++) {
        const dvd_audio_attr_t *attr = &audio_attrs[i];
        
        /* Check if this audio stream is valid (coding mode != 0xFF) */
        if (attr->coding_mode != 0xFF) {
            dvd_vob_audio_stream_t *stream = &temp_streams[valid_streams];
            memset(stream, 0, sizeof(dvd_vob_audio_stream_t));
            
            stream->stream_id = 0x80 + i; /* Audio stream IDs start at 0x80 */
            
            dvd_result_t result = parse_audio_attributes(attr, stream);
            if (result == DVD_RESULT_OK) {
                valid_streams++;
            }
        }
    }
    
    if (valid_streams == 0) {
        /* No valid audio streams found - create defaults */
        return dvd_video_create_default_tracks(internal);
    }
    
    /* Parse Program Chain Information for sector ranges and durations */
    uint32_t pgci_offset = le32_to_cpu((uint8_t*)&header->start_byte_pgci);
    if (pgci_offset > 0 && pgci_offset < size) {
        parse_pgci_table(ifo_data, size, pgci_offset, temp_streams, valid_streams, &valid_streams);
    }
    
    /* Create title structure */
    internal->public.title_count = 1;
    internal->public.titles = calloc(1, sizeof(dvd_title_t));
    if (!internal->public.titles) {
        return DVD_RESULT_OUT_OF_MEMORY;
    }
    
    dvd_title_t *title = &internal->public.titles[0];
    title->title_number = 1;
    title->audio_track_count = valid_streams;
    strcpy(title->title_name, "DVD-Video Main Title");
    
    /* Create audio tracks from parsed streams */
    title->audio_tracks = calloc(valid_streams, sizeof(dvd_audio_track_t));
    if (!title->audio_tracks) {
        free(internal->public.titles);
        return DVD_RESULT_OUT_OF_MEMORY;
    }
    
    /* Convert parsed streams to DVD audio tracks */
    for (int i = 0; i < valid_streams; i++) {
        dvd_audio_track_t *track = &title->audio_tracks[i];
        dvd_vob_audio_stream_t *stream = &temp_streams[i];
        
        track->track_number = i + 1;
        track->format = stream->format;
        track->channels = stream->channels;
        track->sample_rate = stream->sample_rate;
        track->bits_per_sample = stream->bits_per_sample;
        track->start_sector = stream->start_sector;
        track->end_sector = stream->end_sector;
        track->duration_seconds = stream->duration;
        track->duration_samples = (uint64_t)(stream->duration * stream->sample_rate);
        
        /* Generate track title */
        const char *format_name = "Unknown";
        switch (stream->format) {
            case DVD_AUDIO_FORMAT_LPCM: format_name = "LPCM"; break;
            case DVD_AUDIO_FORMAT_AC3: format_name = "AC3"; break;
            case DVD_AUDIO_FORMAT_DTS: format_name = "DTS"; break;
            case DVD_AUDIO_FORMAT_MPEG: format_name = "MPEG"; break;
            default: format_name = "Unknown"; break;
        }
        
        snprintf(track->title, sizeof(track->title), "%s %d.%d %dkHz",
                format_name, 
                stream->channels >= 6 ? stream->channels - 1 : stream->channels,
                stream->channels >= 6 ? 1 : 0,
                stream->sample_rate / 1000);
        
        strncpy(track->language, stream->language, sizeof(track->language) - 1);
        track->language[sizeof(track->language) - 1] = '\0';
    }
    
    /* Set title duration to the longest track */
    title->duration_seconds = 0;
    for (int i = 0; i < valid_streams; i++) {
        if (title->audio_tracks[i].duration_seconds > title->duration_seconds) {
            title->duration_seconds = title->audio_tracks[i].duration_seconds;
        }
    }
    
    return DVD_RESULT_OK;
}

/* Create default tracks when IFO parsing fails */
dvd_result_t dvd_video_create_default_tracks(dvd_disc_internal_t *internal) {
    internal->public.title_count = 1;
    internal->public.titles = calloc(1, sizeof(dvd_title_t));
    if (!internal->public.titles) {
        return DVD_RESULT_OUT_OF_MEMORY;
    }
    
    dvd_title_t *title = &internal->public.titles[0];
    title->title_number = 1;
    title->audio_track_count = 3; /* Common: LPCM, AC3, DTS */
    strcpy(title->title_name, "DVD-Video Title (Default)");
    
    /* Create audio tracks */
    title->audio_tracks = calloc(3, sizeof(dvd_audio_track_t));
    if (!title->audio_tracks) {
        free(internal->public.titles);
        return DVD_RESULT_OUT_OF_MEMORY;
    }
    
    /* Track 1: LPCM */
    dvd_audio_track_t *lpcm_track = &title->audio_tracks[0];
    lpcm_track->track_number = 1;
    lpcm_track->format = DVD_AUDIO_FORMAT_LPCM;
    lpcm_track->channels = 2;
    lpcm_track->sample_rate = 48000;
    lpcm_track->bits_per_sample = 16;
    lpcm_track->start_sector = 0;
    lpcm_track->end_sector = 100000; /* Estimate */
    lpcm_track->duration_seconds = 7200.0; /* 2 hours estimate */
    lpcm_track->duration_samples = lpcm_track->sample_rate * (uint64_t)lpcm_track->duration_seconds;
    strcpy(lpcm_track->title, "LPCM 2.0 48kHz");
    strcpy(lpcm_track->language, "en");
    
    /* Track 2: AC3 */
    dvd_audio_track_t *ac3_track = &title->audio_tracks[1];
    ac3_track->track_number = 2;
    ac3_track->format = DVD_AUDIO_FORMAT_AC3;
    ac3_track->channels = 6; /* 5.1 */
    ac3_track->sample_rate = 48000;
    ac3_track->bits_per_sample = 16;
    ac3_track->start_sector = 0;
    ac3_track->end_sector = 100000;
    ac3_track->duration_seconds = 7200.0;
    ac3_track->duration_samples = ac3_track->sample_rate * (uint64_t)ac3_track->duration_seconds;
    strcpy(ac3_track->title, "AC3 5.1 48kHz");
    strcpy(ac3_track->language, "en");
    
    /* Track 3: DTS */
    dvd_audio_track_t *dts_track = &title->audio_tracks[2];
    dts_track->track_number = 3;
    dts_track->format = DVD_AUDIO_FORMAT_DTS;
    dts_track->channels = 6; /* 5.1 */
    dts_track->sample_rate = 48000;
    dts_track->bits_per_sample = 16;
    dts_track->start_sector = 0;
    dts_track->end_sector = 100000;
    dts_track->duration_seconds = 7200.0;
    dts_track->duration_samples = dts_track->sample_rate * (uint64_t)dts_track->duration_seconds;
    strcpy(dts_track->title, "DTS 5.1 48kHz");
    strcpy(dts_track->language, "en");
    
    title->duration_seconds = 7200.0;
    
    return DVD_RESULT_OK;
}

/* Parse VIDEO_TS directory */
dvd_result_t dvd_video_parse_video_ts(dvd_disc_internal_t *internal) {
    if (!internal || !internal->has_video_ts) {
        return DVD_RESULT_ERROR;
    }
    
    /* Read VIDEO_TS directory */
    uint8_t *video_ts_data;
    uint32_t video_ts_size = 2048; /* Assume one sector for now */
    dvd_result_t result = dvd_iso_read_directory(internal, internal->video_ts_lba, video_ts_size, &video_ts_data);
    if (result != DVD_RESULT_OK) {
        return result;
    }
    
    /* Look for IFO files */
    uint32_t ifo_lba, ifo_size;
    
    /* Try to find VIDEO_TS.IFO first, then VTS_01_0.IFO */
    if (find_file_in_directory(video_ts_data, video_ts_size, "VIDEO_TS.IFO", &ifo_lba, &ifo_size) == DVD_RESULT_OK ||
        find_file_in_directory(video_ts_data, video_ts_size, "VTS_01_0.IFO", &ifo_lba, &ifo_size) == DVD_RESULT_OK) {
        
        /* Read IFO file */
        uint32_t ifo_sectors = (ifo_size + DVD_SECTOR_SIZE - 1) / DVD_SECTOR_SIZE;
        uint8_t *ifo_data = malloc(ifo_sectors * DVD_SECTOR_SIZE);
        if (!ifo_data) {
            free(video_ts_data);
            return DVD_RESULT_OUT_OF_MEMORY;
        }
        
        for (uint32_t i = 0; i < ifo_sectors; i++) {
            result = dvd_iso_read_sector(internal, ifo_lba + i, ifo_data + (i * DVD_SECTOR_SIZE));
            if (result != DVD_RESULT_OK) {
                free(ifo_data);
                free(video_ts_data);
                return result;
            }
        }
        
        /* Parse IFO file */
        result = dvd_video_parse_ifo(internal, ifo_data, ifo_size);
        
        free(ifo_data);
        free(video_ts_data);
        return result;
    }
    
    free(video_ts_data);
    return DVD_RESULT_INVALID_FILE; /* No valid IFO found */
}

/* Parse VOB file to detect actual audio streams */
dvd_result_t dvd_video_scan_vob_audio_streams(dvd_disc_internal_t *internal, uint32_t vob_start_sector, 
                                            uint32_t vob_end_sector, dvd_vob_audio_stream_t *streams, 
                                            int max_streams, int *stream_count) {
    if (!internal || !streams || !stream_count) {
        return DVD_RESULT_INVALID_PARAM;
    }
    
    *stream_count = 0;
    
    /* Read several sectors to analyze the VOB structure */
    uint8_t sector_buffer[DVD_SECTOR_SIZE];
    uint32_t sectors_to_scan = (vob_end_sector - vob_start_sector > 100) ? 100 : (vob_end_sector - vob_start_sector);
    
    bool found_streams[8] = {false}; /* Track which audio stream IDs we've found */
    
    for (uint32_t sector = vob_start_sector; sector < vob_start_sector + sectors_to_scan && sector <= vob_end_sector; sector++) {
        dvd_result_t result = dvd_iso_read_sector(internal, sector, sector_buffer);
        if (result != DVD_RESULT_OK) {
            continue;
        }
        
        /* Look for MPEG Program Stream headers */
        for (int offset = 0; offset < DVD_SECTOR_SIZE - 14; offset++) {
            /* Check for packet start code prefix (0x000001) */
            if (sector_buffer[offset] == 0x00 && sector_buffer[offset+1] == 0x00 && 
                sector_buffer[offset+2] == 0x01) {
                
                uint8_t stream_id = sector_buffer[offset+3];
                
                /* Audio stream IDs: 0x80-0x87 (AC3), 0x88-0x8F (DTS), 0xA0-0xA7 (LPCM), 0xBD (private) */
                if ((stream_id >= 0x80 && stream_id <= 0x8F) || 
                    (stream_id >= 0xA0 && stream_id <= 0xA7) || 
                    stream_id == 0xBD) {
                    
                    uint8_t audio_index;
                    dvd_audio_format_t format;
                    
                    if (stream_id >= 0x80 && stream_id <= 0x87) {
                        /* AC3 audio */
                        audio_index = stream_id - 0x80;
                        format = DVD_AUDIO_FORMAT_AC3;
                    } else if (stream_id >= 0x88 && stream_id <= 0x8F) {
                        /* DTS audio */
                        audio_index = stream_id - 0x88;
                        format = DVD_AUDIO_FORMAT_DTS;
                    } else if (stream_id >= 0xA0 && stream_id <= 0xA7) {
                        /* LPCM audio */
                        audio_index = stream_id - 0xA0;
                        format = DVD_AUDIO_FORMAT_LPCM;
                    } else if (stream_id == 0xBD) {
                        /* Private stream - could be AC3, DTS, or other */
                        /* Check substream ID */
                        if (offset + 8 < DVD_SECTOR_SIZE) {
                            uint8_t substream_id = sector_buffer[offset+7];
                            if (substream_id >= 0x80 && substream_id <= 0x87) {
                                audio_index = substream_id - 0x80;
                                format = DVD_AUDIO_FORMAT_AC3;
                            } else if (substream_id >= 0x88 && substream_id <= 0x8F) {
                                audio_index = substream_id - 0x88;
                                format = DVD_AUDIO_FORMAT_DTS;
                            } else {
                                continue; /* Skip unknown substreams */
                            }
                        } else {
                            continue;
                        }
                    } else {
                        continue;
                    }
                    
                    /* Check if we haven't already found this stream */
                    if (audio_index < 8 && !found_streams[audio_index] && *stream_count < max_streams) {
                        found_streams[audio_index] = true;
                        
                        dvd_vob_audio_stream_t *stream = &streams[*stream_count];
                        memset(stream, 0, sizeof(dvd_vob_audio_stream_t));
                        
                        stream->stream_id = stream_id;
                        stream->format = format;
                        stream->start_sector = vob_start_sector;
                        stream->end_sector = vob_end_sector;
                        
                        /* Set default parameters based on format */
                        switch (format) {
                            case DVD_AUDIO_FORMAT_LPCM:
                                stream->channels = 2;
                                stream->sample_rate = 48000;
                                stream->bits_per_sample = 16;
                                break;
                            case DVD_AUDIO_FORMAT_AC3:
                                stream->channels = 6; /* Assume 5.1 */
                                stream->sample_rate = 48000;
                                stream->bits_per_sample = 16;
                                break;
                            case DVD_AUDIO_FORMAT_DTS:
                                stream->channels = 6; /* Assume 5.1 */
                                stream->sample_rate = 48000;
                                stream->bits_per_sample = 16;
                                break;
                            default:
                                stream->channels = 2;
                                stream->sample_rate = 48000;
                                stream->bits_per_sample = 16;
                                break;
                        }
                        
                        /* Estimate duration based on sector count */
                        uint32_t total_sectors = vob_end_sector - vob_start_sector;
                        stream->duration = (double)total_sectors * 2048.0 / (stream->sample_rate * stream->channels * (stream->bits_per_sample / 8));
                        if (stream->duration > 10800.0) stream->duration = 7200.0; /* Cap at 2 hours */
                        
                        strcpy(stream->language, "en"); /* Default to English */
                        
                        (*stream_count)++;
                    }
                }
            }
        }
    }
    
    return DVD_RESULT_OK;
}