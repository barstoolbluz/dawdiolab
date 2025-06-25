#ifndef DVD_INTERNAL_H
#define DVD_INTERNAL_H

#include "dvd_lib.h"
#include <stdio.h>

/* Internal DVD library definitions */

/* DVD constants */
#define DVD_SECTOR_SIZE         2048
#define DVD_MAX_TITLES          99
#define DVD_MAX_AUDIO_TRACKS    8
#define DVD_MAX_PATH_LENGTH     256

/* ISO 9660 constants */
#define ISO9660_SECTOR_SIZE     2048
#define ISO9660_PRIMARY_VOLUME_DESCRIPTOR_SECTOR    16

/* DVD/Blu-ray filesystem signatures */
#define DVD_AUDIO_DIR           "AUDIO_TS"
#define DVD_VIDEO_DIR           "VIDEO_TS"
#define BLURAY_DIR              "BDMV"
#define BLURAY_PLAYLIST_DIR     "PLAYLIST"
#define BLURAY_STREAM_DIR       "STREAM"

/* IFO file signatures */
#define DVD_AUDIO_IFO_SIGNATURE "DVDAUDIO-ATS"
#define DVD_VIDEO_IFO_SIGNATURE "DVDVIDEO-VTS"
#define DVD_AUDIO_AMG_SIGNATURE "DVDAUDIO-AMG"
#define DVD_VIDEO_VMG_SIGNATURE "DVDVIDEO-VMG"
#define DVD_AUDIO_APP_SIGNATURE "DVDAUDIOSAPP"

/* Internal disc structure */
typedef struct dvd_disc_internal {
    dvd_disc_t public;          /* Public interface */
    
    /* File handling */
    int fd;                     /* File descriptor for ISO file */
    char *iso_path;             /* Path to ISO file */
    size_t file_size;           /* File size in bytes */
    
    /* ISO 9660 filesystem data */
    uint8_t *primary_volume_descriptor; /* PVD sector data */
    uint32_t root_directory_lba;        /* Root directory location */
    uint32_t root_directory_size;       /* Root directory size */
    
    /* DVD structure */
    bool has_audio_ts;          /* True if AUDIO_TS directory found */
    bool has_video_ts;          /* True if VIDEO_TS directory found */
    uint32_t audio_ts_lba;      /* AUDIO_TS directory location */
    uint32_t video_ts_lba;      /* VIDEO_TS directory location */
    
    /* Parsed data */
    bool titles_parsed;         /* True if titles have been parsed */
    
    /* State */
    bool is_open;               /* True if disc is open */
} dvd_disc_internal_t;

/* ISO 9660 directory entry */
typedef struct {
    uint8_t length;             /* Length of directory record */
    uint8_t extended_length;    /* Extended attribute record length */
    uint32_t location_le;       /* Location of extent (little-endian) */
    uint32_t location_be;       /* Location of extent (big-endian) */
    uint32_t data_length_le;    /* Data length (little-endian) */
    uint32_t data_length_be;    /* Data length (big-endian) */
    uint8_t date[7];           /* Recording date and time */
    uint8_t flags;             /* File flags */
    uint8_t unit_size;         /* File unit size */
    uint8_t gap_size;          /* Interleave gap size */
    uint16_t volume_seq_le;    /* Volume sequence number (little-endian) */
    uint16_t volume_seq_be;    /* Volume sequence number (big-endian) */
    uint8_t filename_length;   /* Length of filename */
    /* Filename follows */
} iso9660_directory_entry_t;

/* VOB Audio Stream Information - defined here for header */
typedef struct dvd_vob_audio_stream {
    uint8_t stream_id;          /* Audio stream ID (0x80-0x87, 0x89-0x8F, 0xBD) */
    dvd_audio_format_t format;  /* Audio format */
    uint8_t channels;           /* Number of channels */
    uint32_t sample_rate;       /* Sample rate */
    uint8_t bits_per_sample;    /* Bits per sample */
    char language[4];           /* Language code */
    uint32_t start_sector;      /* Start sector in VOB */
    uint32_t end_sector;        /* End sector in VOB */
    double duration;            /* Duration in seconds */
} dvd_vob_audio_stream_t;

/* Internal functions */

/* ISO 9660 filesystem functions */
dvd_result_t dvd_iso_read_sector(dvd_disc_internal_t *internal, uint32_t lba, uint8_t *buffer);
dvd_result_t dvd_iso_parse_primary_volume_descriptor(dvd_disc_internal_t *internal);
dvd_result_t dvd_iso_find_directory(dvd_disc_internal_t *internal, const char *dirname, uint32_t *lba, uint32_t *size);
dvd_result_t dvd_iso_read_directory(dvd_disc_internal_t *internal, uint32_t lba, uint32_t size, uint8_t **data);

/* DVD-Audio parsing functions */
dvd_result_t dvd_audio_parse_audio_ts(dvd_disc_internal_t *internal);
dvd_result_t dvd_audio_parse_ifo(dvd_disc_internal_t *internal, const uint8_t *ifo_data, size_t size);

/* DVD-Video parsing functions */
dvd_result_t dvd_video_parse_video_ts(dvd_disc_internal_t *internal);
dvd_result_t dvd_video_parse_ifo(dvd_disc_internal_t *internal, const uint8_t *ifo_data, size_t size);
dvd_result_t dvd_video_create_default_tracks(dvd_disc_internal_t *internal);
dvd_result_t dvd_video_scan_vob_audio_streams(dvd_disc_internal_t *internal, uint32_t vob_start_sector, 
                                            uint32_t vob_end_sector, dvd_vob_audio_stream_t *streams, 
                                            int max_streams, int *stream_count);

/* Blu-ray MPLS parsing functions */
dvd_result_t bluray_detect_disc(dvd_disc_internal_t *disc);
dvd_result_t bluray_scan_playlists(dvd_disc_internal_t *disc);
dvd_result_t bluray_parse_mpls(dvd_disc_internal_t *disc, const uint8_t *mpls_data, size_t size);

/* Audio extraction functions */
dvd_result_t dvd_extract_lpcm_track(dvd_disc_internal_t *internal, dvd_audio_track_t *track, const char *output_path, dvd_progress_callback_t callback, void *userdata);
dvd_result_t dvd_extract_compressed_track(dvd_disc_internal_t *internal, dvd_audio_track_t *track, const char *output_path, dvd_progress_callback_t callback, void *userdata);

/* Utility functions */
uint16_t le16_to_cpu(const uint8_t *data);
uint32_t le32_to_cpu(const uint8_t *data);
uint16_t be16_to_cpu(const uint8_t *data);
uint32_t be32_to_cpu(const uint8_t *data);
void cpu_to_le16(uint8_t *data, uint16_t value);
void cpu_to_le32(uint8_t *data, uint32_t value);
void cpu_to_be16(uint8_t *data, uint16_t value);
void cpu_to_be32(uint8_t *data, uint32_t value);

#endif /* DVD_INTERNAL_H */