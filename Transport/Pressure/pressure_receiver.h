#ifndef TRANSPORT_PRESSURE_PRESSURE_RECEIVER_H
#define TRANSPORT_PRESSURE_PRESSURE_RECEIVER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "Protocol/PressureSensor/pressure_sensor_protocol.h"

typedef struct
{
    uint16_t raw_pressure_counts;
    uint8_t last_valid_frame[PRESSURE_SENSOR_FRAME_LENGTH];
    uint64_t sample_sequence;
    uint32_t received_at_ms;
    uint32_t valid_frame_count;
    uint32_t invalid_header_count;
    uint32_t invalid_bcc_count;
    uint32_t invalid_marker_count;
    uint32_t dropped_resync_byte_count;
    uint32_t last_valid_frame_ms;
    uint32_t frame_interval_min_ms;
    uint32_t frame_interval_max_ms;
} PressureReceiverSnapshot;

typedef struct
{
    uint8_t frame[PRESSURE_SENSOR_FRAME_LENGTH];
    size_t frame_length;
    PressureReceiverSnapshot snapshot;
} PressureReceiver;

void PressureReceiver_Initialize(PressureReceiver *receiver);
void PressureReceiver_ProcessBytes(PressureReceiver *receiver,
                                   const uint8_t *bytes,
                                   size_t byte_count,
                                   uint32_t received_at_ms);
const PressureReceiverSnapshot *PressureReceiver_GetSnapshot(
    const PressureReceiver *receiver);

#endif
