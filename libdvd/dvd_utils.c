#include "dvd_internal.h"
#include <string.h>

/* DVD Utility Functions - Endian conversion and helper functions */

/* Little-endian conversion functions */
uint16_t le16_to_cpu(const uint8_t *data) {
    return (uint16_t)data[0] | ((uint16_t)data[1] << 8);
}

uint32_t le32_to_cpu(const uint8_t *data) {
    return (uint32_t)data[0] | 
           ((uint32_t)data[1] << 8) | 
           ((uint32_t)data[2] << 16) | 
           ((uint32_t)data[3] << 24);
}

/* Big-endian conversion functions */
uint16_t be16_to_cpu(const uint8_t *data) {
    return ((uint16_t)data[0] << 8) | (uint16_t)data[1];
}

uint32_t be32_to_cpu(const uint8_t *data) {
    return ((uint32_t)data[0] << 24) | 
           ((uint32_t)data[1] << 16) | 
           ((uint32_t)data[2] << 8) | 
           (uint32_t)data[3];
}

/* CPU to little-endian conversion functions */
void cpu_to_le16(uint8_t *data, uint16_t value) {
    data[0] = value & 0xFF;
    data[1] = (value >> 8) & 0xFF;
}

void cpu_to_le32(uint8_t *data, uint32_t value) {
    data[0] = value & 0xFF;
    data[1] = (value >> 8) & 0xFF;
    data[2] = (value >> 16) & 0xFF;
    data[3] = (value >> 24) & 0xFF;
}

/* CPU to big-endian conversion functions */
void cpu_to_be16(uint8_t *data, uint16_t value) {
    data[0] = (value >> 8) & 0xFF;
    data[1] = value & 0xFF;
}

void cpu_to_be32(uint8_t *data, uint32_t value) {
    data[0] = (value >> 24) & 0xFF;
    data[1] = (value >> 16) & 0xFF;
    data[2] = (value >> 8) & 0xFF;
    data[3] = value & 0xFF;
}

