#ifndef _EW_DATA_H
#define _EW_DATA_H

#include <types.h>

struct ew_data_t {
    uint16_t offset;
    const uint8_t *data;
    const uint32_t length;
};

extern struct ew_data_t ew_data_8k_10;
extern struct ew_data_t ew_data_32k_13;
extern struct ew_data_t ew_data_32k_12;
extern struct ew_data_t ew_data_32k_10;
extern struct ew_data_t ew_data_256k_10;
extern struct ew_data_t ew_data_128k_22;
extern struct ew_data_t ew_data_128k_21;
extern struct ew_data_t ew_data_128k_20;

#endif

