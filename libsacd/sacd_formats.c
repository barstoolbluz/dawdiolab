/**
 * SACD Library - Output Format Writers
 * 
 * Functions for writing DSF and DSDIFF format files with proper headers.
 */

#include "sacd_lib.h"
#include "sacd_internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

/* DSF format structures */
typedef struct {
    char signature[4];        /* "DSD " */
    uint64_t chunk_size;      /* Size of DSD chunk */
    uint64_t file_size;       /* Total file size */
    uint64_t id3_offset;      /* Offset to ID3 metadata */
} __attribute__((packed)) dsf_header_t;

typedef struct {
    char signature[4];        /* "fmt " */
    uint64_t chunk_size;      /* Size of fmt chunk (52) */
    uint32_t format_version;  /* Format version (1) */
    uint32_t format_id;       /* Format ID (0) */
    uint32_t channel_type;    /* Channel type */
    uint32_t channel_num;     /* Number of channels */
    uint32_t sampling_freq;   /* Sampling frequency */
    uint32_t bits_per_sample; /* Bits per sample (1) */
    uint64_t sample_count;    /* Total sample count */
    uint32_t block_size;      /* Block size per channel */
    uint32_t reserved;        /* Reserved (0) */
} __attribute__((packed)) dsf_fmt_chunk_t;

typedef struct {
    char signature[4];        /* "data" */
    uint64_t chunk_size;      /* Size of data chunk */
} __attribute__((packed)) dsf_data_chunk_t;

/* DSDIFF format structures */
typedef struct {
    char signature[4];        /* "FRM8" */
    uint64_t chunk_size;      /* Size of form chunk */
    char form_type[4];        /* "DSD " */
} __attribute__((packed)) dsdiff_header_t;

typedef struct {
    char signature[4];        /* "FVER" */
    uint64_t chunk_size;      /* Size of version chunk (4) */
    uint32_t version;         /* Version number */
} __attribute__((packed)) dsdiff_fver_chunk_t;

typedef struct {
    char signature[4];        /* "PROP" */
    uint64_t chunk_size;      /* Size of property chunk */
    char prop_type[4];        /* "SND " */
} __attribute__((packed)) dsdiff_prop_chunk_t;

typedef struct {
    char signature[4];        /* "FS  " */
    uint64_t chunk_size;      /* Size of sample rate chunk (4) */
    uint32_t sample_rate;     /* Sample rate */
} __attribute__((packed)) dsdiff_fs_chunk_t;

typedef struct {
    char signature[4];        /* "CHNL" */
    uint64_t chunk_size;      /* Size of channels chunk */
} __attribute__((packed)) dsdiff_chnl_chunk_t;

typedef struct {
    char signature[4];        /* "DSD " */
    uint64_t chunk_size;      /* Size of DSD data chunk */
} __attribute__((packed)) dsdiff_dsd_chunk_t;

/* Helper functions for endian conversion */
static void write_le32(uint8_t *data, uint32_t value) {
    data[0] = value & 0xFF;
    data[1] = (value >> 8) & 0xFF;
    data[2] = (value >> 16) & 0xFF;
    data[3] = (value >> 24) & 0xFF;
}

static void write_le64(uint8_t *data, uint64_t value) {
    data[0] = value & 0xFF;
    data[1] = (value >> 8) & 0xFF;
    data[2] = (value >> 16) & 0xFF;
    data[3] = (value >> 24) & 0xFF;
    data[4] = (value >> 32) & 0xFF;
    data[5] = (value >> 40) & 0xFF;
    data[6] = (value >> 48) & 0xFF;
    data[7] = (value >> 56) & 0xFF;
}

static void write_be64(uint8_t *data, uint64_t value) {
    data[0] = (value >> 56) & 0xFF;
    data[1] = (value >> 48) & 0xFF;
    data[2] = (value >> 40) & 0xFF;
    data[3] = (value >> 32) & 0xFF;
    data[4] = (value >> 24) & 0xFF;
    data[5] = (value >> 16) & 0xFF;
    data[6] = (value >> 8) & 0xFF;
    data[7] = value & 0xFF;
}

/* Write DSF file header */
sacd_result_t sacd_internal_write_dsf_header(
    FILE *file,
    const sacd_track_t *track,
    const sacd_area_t *area,
    size_t audio_data_size) {
    
    if (!file || !track || !area) {
        return SACD_RESULT_ERROR;
    }
    
    /* Calculate total file size */
    size_t header_size = sizeof(dsf_header_t) + sizeof(dsf_fmt_chunk_t) + sizeof(dsf_data_chunk_t);
    size_t total_file_size = header_size + audio_data_size;
    
    /* Calculate sample count */
    uint64_t sample_count = sacd_track_duration_samples(track);
    
    /* Write DSD header */
    dsf_header_t dsd_header;
    memcpy(dsd_header.signature, "DSD ", 4);
    dsd_header.chunk_size = 28; /* Fixed size for DSD chunk */
    dsd_header.file_size = total_file_size;
    dsd_header.id3_offset = 0; /* No ID3 for now */
    
    /* Convert to little endian */
    uint8_t header_buf[28];
    memcpy(header_buf, dsd_header.signature, 4);
    write_le64(header_buf + 4, dsd_header.chunk_size);
    write_le64(header_buf + 12, dsd_header.file_size);
    write_le64(header_buf + 20, dsd_header.id3_offset);
    
    if (fwrite(header_buf, 1, 28, file) != 28) {
        return SACD_RESULT_IO_ERROR;
    }
    
    /* Write fmt chunk */
    dsf_fmt_chunk_t fmt_chunk;
    memcpy(fmt_chunk.signature, "fmt ", 4);
    fmt_chunk.chunk_size = 52; /* Fixed size for fmt chunk */
    fmt_chunk.format_version = 1;
    fmt_chunk.format_id = 0; /* DSD raw */
    fmt_chunk.channel_type = (track->channel_count == 2) ? 2 : 7; /* 2=stereo, 7=multi */
    fmt_chunk.channel_num = track->channel_count;
    fmt_chunk.sampling_freq = area->sample_frequency;
    fmt_chunk.bits_per_sample = 1;
    fmt_chunk.sample_count = sample_count;
    fmt_chunk.block_size = 4096; /* Standard block size */
    fmt_chunk.reserved = 0;
    
    /* Convert to little endian */
    uint8_t fmt_buf[64];
    memcpy(fmt_buf, fmt_chunk.signature, 4);
    write_le64(fmt_buf + 4, fmt_chunk.chunk_size);
    write_le32(fmt_buf + 12, fmt_chunk.format_version);
    write_le32(fmt_buf + 16, fmt_chunk.format_id);
    write_le32(fmt_buf + 20, fmt_chunk.channel_type);
    write_le32(fmt_buf + 24, fmt_chunk.channel_num);
    write_le32(fmt_buf + 28, fmt_chunk.sampling_freq);
    write_le32(fmt_buf + 32, fmt_chunk.bits_per_sample);
    write_le64(fmt_buf + 36, fmt_chunk.sample_count);
    write_le32(fmt_buf + 44, fmt_chunk.block_size);
    write_le32(fmt_buf + 48, fmt_chunk.reserved);
    
    if (fwrite(fmt_buf, 1, 52, file) != 52) {
        return SACD_RESULT_IO_ERROR;
    }
    
    /* Write data chunk header */
    dsf_data_chunk_t data_chunk;
    memcpy(data_chunk.signature, "data", 4);
    data_chunk.chunk_size = 12 + audio_data_size; /* Header + data */
    
    uint8_t data_buf[12];
    memcpy(data_buf, data_chunk.signature, 4);
    write_le64(data_buf + 4, data_chunk.chunk_size);
    
    if (fwrite(data_buf, 1, 12, file) != 12) {
        return SACD_RESULT_IO_ERROR;
    }
    
    return SACD_RESULT_OK;
}

/* Write DSDIFF file header */
sacd_result_t sacd_internal_write_dsdiff_header(
    FILE *file,
    const sacd_track_t *track,
    const sacd_area_t *area,
    size_t audio_data_size) {
    
    if (!file || !track || !area) {
        return SACD_RESULT_ERROR;
    }
    
    /* Calculate chunk sizes */
    size_t chnl_chunk_size = 8 + (track->channel_count * 4); /* 4 bytes per channel ID */
    size_t prop_chunk_size = 4 + 16 + chnl_chunk_size; /* "SND " + FS + CHNL */
    size_t form_chunk_size = 4 + 12 + prop_chunk_size + 16 + audio_data_size; /* DSD + FVER + PROP + DSD data */
    
    /* Write FORM header */
    dsdiff_header_t form_header;
    memcpy(form_header.signature, "FRM8", 4);
    form_header.chunk_size = form_chunk_size;
    memcpy(form_header.form_type, "DSD ", 4);
    
    uint8_t form_buf[16];
    memcpy(form_buf, form_header.signature, 4);
    write_be64(form_buf + 4, form_header.chunk_size);
    memcpy(form_buf + 12, form_header.form_type, 4);
    
    if (fwrite(form_buf, 1, 16, file) != 16) {
        return SACD_RESULT_IO_ERROR;
    }
    
    /* Write FVER chunk */
    dsdiff_fver_chunk_t fver_chunk;
    memcpy(fver_chunk.signature, "FVER", 4);
    fver_chunk.chunk_size = 4;
    fver_chunk.version = 0x01050000; /* Version 1.5.0.0 */
    
    uint8_t fver_buf[12];
    memcpy(fver_buf, fver_chunk.signature, 4);
    write_be64(fver_buf + 4, fver_chunk.chunk_size);
    write_le32(fver_buf + 8, fver_chunk.version); /* Version is little endian */
    
    /* Pad to even size */
    if (fwrite(fver_buf, 1, 12, file) != 12) {
        return SACD_RESULT_IO_ERROR;
    }
    
    /* Write PROP chunk header */
    dsdiff_prop_chunk_t prop_chunk;
    memcpy(prop_chunk.signature, "PROP", 4);
    prop_chunk.chunk_size = prop_chunk_size;
    memcpy(prop_chunk.prop_type, "SND ", 4);
    
    uint8_t prop_buf[16];
    memcpy(prop_buf, prop_chunk.signature, 4);
    write_be64(prop_buf + 4, prop_chunk.chunk_size);
    memcpy(prop_buf + 12, prop_chunk.prop_type, 4);
    
    if (fwrite(prop_buf, 1, 16, file) != 16) {
        return SACD_RESULT_IO_ERROR;
    }
    
    /* Write FS chunk */
    dsdiff_fs_chunk_t fs_chunk;
    memcpy(fs_chunk.signature, "FS  ", 4);
    fs_chunk.chunk_size = 4;
    fs_chunk.sample_rate = area->sample_frequency;
    
    uint8_t fs_buf[12];
    memcpy(fs_buf, fs_chunk.signature, 4);
    write_be64(fs_buf + 4, fs_chunk.chunk_size);
    write_le32(fs_buf + 8, fs_chunk.sample_rate); /* Sample rate is little endian */
    
    if (fwrite(fs_buf, 1, 12, file) != 12) {
        return SACD_RESULT_IO_ERROR;
    }
    
    /* Write CHNL chunk */
    dsdiff_chnl_chunk_t chnl_chunk;
    memcpy(chnl_chunk.signature, "CHNL", 4);
    chnl_chunk.chunk_size = track->channel_count * 4;
    
    uint8_t chnl_buf[8];
    memcpy(chnl_buf, chnl_chunk.signature, 4);
    write_be64(chnl_buf + 4, chnl_chunk.chunk_size);
    
    if (fwrite(chnl_buf, 1, 8, file) != 8) {
        return SACD_RESULT_IO_ERROR;
    }
    
    /* Write channel IDs */
    const char *channel_ids[] = { "SLFT", "SRGT", "C   ", "LFE ", "LS  ", "RS  " };
    for (int i = 0; i < track->channel_count && i < 6; i++) {
        if (fwrite(channel_ids[i], 1, 4, file) != 4) {
            return SACD_RESULT_IO_ERROR;
        }
    }
    
    /* Write DSD data chunk header */
    dsdiff_dsd_chunk_t dsd_chunk;
    memcpy(dsd_chunk.signature, "DSD ", 4);
    dsd_chunk.chunk_size = audio_data_size;
    
    uint8_t dsd_buf[12];
    memcpy(dsd_buf, dsd_chunk.signature, 4);
    write_be64(dsd_buf + 4, dsd_chunk.chunk_size);
    
    if (fwrite(dsd_buf, 1, 12, file) != 12) {
        return SACD_RESULT_IO_ERROR;
    }
    
    return SACD_RESULT_OK;
}

/* Finalize file headers with actual sizes */
sacd_result_t sacd_internal_finalize_file_headers(
    FILE *file,
    sacd_output_format_t format,
    size_t audio_data_size) {
    
    if (!file) {
        return SACD_RESULT_ERROR;
    }
    
    /* Get current position */
    long current_pos = ftell(file);
    if (current_pos < 0) {
        return SACD_RESULT_IO_ERROR;
    }
    
    if (format == SACD_FORMAT_DSF) {
        /* Update DSF file size in header */
        if (fseek(file, 12, SEEK_SET) != 0) {
            return SACD_RESULT_IO_ERROR;
        }
        
        uint64_t file_size = current_pos;
        uint8_t size_buf[8];
        write_le64(size_buf, file_size);
        
        if (fwrite(size_buf, 1, 8, file) != 8) {
            return SACD_RESULT_IO_ERROR;
        }
        
        /* Update data chunk size */
        if (fseek(file, 80 + 4, SEEK_SET) != 0) {
            return SACD_RESULT_IO_ERROR;
        }
        
        uint64_t data_chunk_size = 12 + audio_data_size;
        write_le64(size_buf, data_chunk_size);
        
        if (fwrite(size_buf, 1, 8, file) != 8) {
            return SACD_RESULT_IO_ERROR;
        }
    } else if (format == SACD_FORMAT_DSDIFF || format == SACD_FORMAT_DSDIFF_EM) {
        /* Update DSDIFF form chunk size */
        if (fseek(file, 4, SEEK_SET) != 0) {
            return SACD_RESULT_IO_ERROR;
        }
        
        uint64_t form_size = current_pos - 12; /* Exclude FORM header itself */
        uint8_t size_buf[8];
        write_be64(size_buf, form_size);
        
        if (fwrite(size_buf, 1, 8, file) != 8) {
            return SACD_RESULT_IO_ERROR;
        }
    }
    
    /* Restore file position */
    if (fseek(file, current_pos, SEEK_SET) != 0) {
        return SACD_RESULT_IO_ERROR;
    }
    
    return SACD_RESULT_OK;
}