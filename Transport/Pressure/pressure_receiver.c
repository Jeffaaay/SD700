#include "Transport/Pressure/pressure_receiver.h"

#include <string.h>

#include "Protocol/PressureSensor/pressure_sensor_decode_frame.h"

static void PressureReceiver_PublishValidFrame(
    PressureReceiver *receiver,
    const PressureFrameData *frame_data,
    uint32_t received_at_ms)
{
    uint32_t frame_interval_ms;

    if (receiver->snapshot.valid_frame_count != 0U)
    {
        frame_interval_ms =
            (uint32_t)(received_at_ms -
                       receiver->snapshot.last_valid_frame_ms);
        if ((receiver->snapshot.frame_interval_min_ms == 0U) ||
            (frame_interval_ms <
             receiver->snapshot.frame_interval_min_ms))
        {
            receiver->snapshot.frame_interval_min_ms = frame_interval_ms;
        }
        if (frame_interval_ms > receiver->snapshot.frame_interval_max_ms)
        {
            receiver->snapshot.frame_interval_max_ms = frame_interval_ms;
        }
    }
    receiver->snapshot.raw_pressure_counts =
        frame_data->raw_pressure_counts;
    ++receiver->snapshot.sample_sequence;
    receiver->snapshot.received_at_ms = received_at_ms;
    ++receiver->snapshot.valid_frame_count;
    receiver->snapshot.last_valid_frame_ms = received_at_ms;
}

static void PressureReceiver_CountFrameError(
    PressureReceiver *receiver,
    PressureFrameResult result)
{
    if (result == PRESSURE_FRAME_INVALID_BCC)
    {
        ++receiver->snapshot.invalid_bcc_count;
    }
    else if (result == PRESSURE_FRAME_INVALID_MARKER)
    {
        ++receiver->snapshot.invalid_marker_count;
    }
}

static void PressureReceiver_Resynchronize(PressureReceiver *receiver)
{
    size_t header_index;

    for (header_index = 1U;
         header_index < receiver->frame_length;
         ++header_index)
    {
        if (receiver->frame[header_index] == PRESSURE_SENSOR_FRAME_HEADER)
        {
            break;
        }
    }

    receiver->snapshot.dropped_resync_byte_count +=
        (uint32_t)header_index;
    if (header_index < receiver->frame_length)
    {
        receiver->frame_length -= header_index;
        (void)memmove(receiver->frame,
                      &receiver->frame[header_index],
                      receiver->frame_length);
    }
    else
    {
        receiver->frame_length = 0U;
    }
}

void PressureReceiver_Initialize(PressureReceiver *receiver)
{
    if (receiver != NULL)
    {
        (void)memset(receiver, 0, sizeof(*receiver));
    }
}

void PressureReceiver_ProcessBytes(PressureReceiver *receiver,
                                   const uint8_t *bytes,
                                   size_t byte_count,
                                   uint32_t received_at_ms)
{
    size_t byte_index;

    if ((receiver == NULL) || ((bytes == NULL) && (byte_count != 0U)))
    {
        return;
    }

    for (byte_index = 0U; byte_index < byte_count; ++byte_index)
    {
        if (receiver->frame_length == 0U)
        {
            if (bytes[byte_index] != PRESSURE_SENSOR_FRAME_HEADER)
            {
                ++receiver->snapshot.invalid_header_count;
                ++receiver->snapshot.dropped_resync_byte_count;
                continue;
            }
        }

        receiver->frame[receiver->frame_length] = bytes[byte_index];
        ++receiver->frame_length;
        if (receiver->frame_length == PRESSURE_SENSOR_FRAME_LENGTH)
        {
            PressureFrameData frame_data = {0U};
            PressureFrameResult result = PressureSensor_DecodeFrame(
                receiver->frame,
                receiver->frame_length,
                &frame_data);

            if (result == PRESSURE_FRAME_OK)
            {
                (void)memcpy(receiver->snapshot.last_valid_frame,
                             receiver->frame,
                             PRESSURE_SENSOR_FRAME_LENGTH);
                PressureReceiver_PublishValidFrame(receiver,
                                                   &frame_data,
                                                   received_at_ms);
                receiver->frame_length = 0U;
            }
            else
            {
                PressureReceiver_CountFrameError(receiver, result);
                PressureReceiver_Resynchronize(receiver);
            }
        }
    }
}

const PressureReceiverSnapshot *PressureReceiver_GetSnapshot(
    const PressureReceiver *receiver)
{
    return (receiver == NULL) ? NULL : &receiver->snapshot;
}
