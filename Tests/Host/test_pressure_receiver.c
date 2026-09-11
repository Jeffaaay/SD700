#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "Protocol/PressureSensor/pressure_sensor_bcc.h"
#include "Transport/Pressure/pressure_receiver.h"

static void MakeValidFrame(
    uint8_t frame[PRESSURE_SENSOR_FRAME_LENGTH],
    uint16_t raw_pressure_counts)
{
    frame[0] = PRESSURE_SENSOR_FRAME_HEADER;
    frame[1] = (uint8_t)(raw_pressure_counts & 0xFFU);
    frame[2] = (uint8_t)(raw_pressure_counts >> 8U);
    frame[3] = 0x12U;
    frame[4] = 0x34U;
    frame[5] = PressureSensor_CalculateBcc(frame, 5U);
    frame[6] = PRESSURE_SENSOR_FRAME_MARKER;
}

static const PressureReceiverSnapshot *Snapshot(
    const PressureReceiver *receiver)
{
    const PressureReceiverSnapshot *snapshot =
        PressureReceiver_GetSnapshot(receiver);
    assert(snapshot != NULL);
    return snapshot;
}

static void TestOneCompleteValidFrame(void)
{
    PressureReceiver receiver;
    uint8_t frame[PRESSURE_SENSOR_FRAME_LENGTH];
    const PressureReceiverSnapshot *snapshot;

    PressureReceiver_Initialize(&receiver);
    MakeValidFrame(frame, 1234U);
    PressureReceiver_ProcessBytes(&receiver, frame, sizeof(frame), 10U);

    snapshot = Snapshot(&receiver);
    assert(snapshot->raw_pressure_counts == 1234U);
    assert(snapshot->sample_sequence == 1U);
    assert(snapshot->received_at_ms == 10U);
    assert(snapshot->last_valid_frame_ms == 10U);
    assert(snapshot->valid_frame_count == 1U);
    for (uint8_t index = 0U; index < PRESSURE_SENSOR_FRAME_LENGTH; ++index)
    {
        uint8_t value = 0U;

        value = snapshot->last_valid_frame[index];
        assert(value == frame[index]);
    }
}

static void TestFrameSplitAcrossChunks(void)
{
    PressureReceiver receiver;
    uint8_t frame[PRESSURE_SENSOR_FRAME_LENGTH];
    const PressureReceiverSnapshot *snapshot;

    PressureReceiver_Initialize(&receiver);
    MakeValidFrame(frame, 321U);
    PressureReceiver_ProcessBytes(&receiver, frame, 3U, 20U);
    assert(Snapshot(&receiver)->valid_frame_count == 0U);
    PressureReceiver_ProcessBytes(&receiver,
                                  &frame[3],
                                  sizeof(frame) - 3U,
                                  25U);

    snapshot = Snapshot(&receiver);
    assert(snapshot->raw_pressure_counts == 321U);
    assert(snapshot->sample_sequence == 1U);
    assert(snapshot->received_at_ms == 25U);
}

static void TestMultipleFramesInOneChunk(void)
{
    PressureReceiver receiver;
    uint8_t bytes[PRESSURE_SENSOR_FRAME_LENGTH * 2U];
    const PressureReceiverSnapshot *snapshot;

    PressureReceiver_Initialize(&receiver);
    MakeValidFrame(bytes, 100U);
    MakeValidFrame(&bytes[PRESSURE_SENSOR_FRAME_LENGTH], 200U);
    PressureReceiver_ProcessBytes(&receiver, bytes, sizeof(bytes), 30U);

    snapshot = Snapshot(&receiver);
    assert(snapshot->valid_frame_count == 2U);
    assert(snapshot->sample_sequence == 2U);
    assert(snapshot->raw_pressure_counts == 200U);
    assert(snapshot->received_at_ms == 30U);
}

static void TestGarbageBeforeValidFrame(void)
{
    PressureReceiver receiver;
    uint8_t bytes[3U + PRESSURE_SENSOR_FRAME_LENGTH];
    const PressureReceiverSnapshot *snapshot;

    bytes[0] = 0x00U;
    bytes[1] = 0xAAU;
    bytes[2] = 0xFEU;
    MakeValidFrame(&bytes[3], 444U);
    PressureReceiver_Initialize(&receiver);
    PressureReceiver_ProcessBytes(&receiver, bytes, sizeof(bytes), 40U);

    snapshot = Snapshot(&receiver);
    assert(snapshot->raw_pressure_counts == 444U);
    assert(snapshot->valid_frame_count == 1U);
    assert(snapshot->invalid_header_count == 3U);
    assert(snapshot->dropped_resync_byte_count == 3U);
}

static void TestInvalidBccRecovery(void)
{
    PressureReceiver receiver;
    uint8_t bytes[PRESSURE_SENSOR_FRAME_LENGTH * 2U];
    const PressureReceiverSnapshot *snapshot;

    MakeValidFrame(bytes, 500U);
    bytes[PRESSURE_SENSOR_FRAME_BCC_INDEX] ^= 1U;
    MakeValidFrame(&bytes[PRESSURE_SENSOR_FRAME_LENGTH], 501U);
    PressureReceiver_Initialize(&receiver);
    PressureReceiver_ProcessBytes(&receiver, bytes, sizeof(bytes), 50U);

    snapshot = Snapshot(&receiver);
    assert(snapshot->invalid_bcc_count == 1U);
    assert(snapshot->valid_frame_count == 1U);
    assert(snapshot->raw_pressure_counts == 501U);
    assert(snapshot->sample_sequence == 1U);
}

static void TestInvalidMarkerRecovery(void)
{
    PressureReceiver receiver;
    uint8_t bytes[PRESSURE_SENSOR_FRAME_LENGTH * 2U];
    const PressureReceiverSnapshot *snapshot;

    MakeValidFrame(bytes, 600U);
    bytes[6] = 0x00U;
    MakeValidFrame(&bytes[PRESSURE_SENSOR_FRAME_LENGTH], 601U);
    PressureReceiver_Initialize(&receiver);
    PressureReceiver_ProcessBytes(&receiver, bytes, sizeof(bytes), 60U);

    snapshot = Snapshot(&receiver);
    assert(snapshot->invalid_marker_count == 1U);
    assert(snapshot->valid_frame_count == 1U);
    assert(snapshot->raw_pressure_counts == 601U);
}

static void TestPartialFrameResynchronization(void)
{
    PressureReceiver receiver;
    uint8_t partial[PRESSURE_SENSOR_FRAME_LENGTH];
    uint8_t valid[PRESSURE_SENSOR_FRAME_LENGTH];
    const PressureReceiverSnapshot *snapshot;

    MakeValidFrame(partial, 700U);
    MakeValidFrame(valid, 701U);
    PressureReceiver_Initialize(&receiver);
    PressureReceiver_ProcessBytes(&receiver, partial, 4U, 70U);
    PressureReceiver_ProcessBytes(&receiver, valid, sizeof(valid), 71U);

    snapshot = Snapshot(&receiver);
    assert(snapshot->valid_frame_count == 1U);
    assert(snapshot->raw_pressure_counts == 701U);
    assert(snapshot->sample_sequence == 1U);
    assert(snapshot->received_at_ms == 71U);
    assert(snapshot->dropped_resync_byte_count == 4U);
}

static void TestSequenceAndTimestampOnlyOnValidFrames(void)
{
    PressureReceiver receiver;
    uint8_t frame[PRESSURE_SENSOR_FRAME_LENGTH];
    const PressureReceiverSnapshot *snapshot;

    PressureReceiver_Initialize(&receiver);
    MakeValidFrame(frame, 800U);
    frame[PRESSURE_SENSOR_FRAME_BCC_INDEX] ^= 1U;
    PressureReceiver_ProcessBytes(&receiver, frame, sizeof(frame), 80U);
    snapshot = Snapshot(&receiver);
    assert(snapshot->sample_sequence == 0U);
    assert(snapshot->received_at_ms == 0U);
    assert(snapshot->last_valid_frame_ms == 0U);

    MakeValidFrame(frame, 801U);
    PressureReceiver_ProcessBytes(&receiver, frame, sizeof(frame), 81U);
    snapshot = Snapshot(&receiver);
    assert(snapshot->sample_sequence == 1U);
    assert(snapshot->received_at_ms == 81U);
    assert(snapshot->last_valid_frame_ms == 81U);

    frame[6] = 0x00U;
    PressureReceiver_ProcessBytes(&receiver, frame, sizeof(frame), 82U);
    snapshot = Snapshot(&receiver);
    assert(snapshot->sample_sequence == 1U);
    assert(snapshot->received_at_ms == 81U);
    assert(snapshot->last_valid_frame_ms == 81U);

    MakeValidFrame(frame, 802U);
    PressureReceiver_ProcessBytes(&receiver, frame, sizeof(frame), 83U);
    snapshot = Snapshot(&receiver);
    assert(snapshot->sample_sequence == 2U);
    assert(snapshot->received_at_ms == 83U);
    assert(snapshot->last_valid_frame_ms == 83U);
}

static void TestObservedContinuousSevenByteFrames(void)
{
    static const uint8_t stream[] = {
        0xFDU, 0x02U, 0x01U, 0x02U, 0x01U, 0xFDU, 0xFEU,
        0xFDU, 0x02U, 0x01U, 0x02U, 0x01U, 0xFDU, 0xFEU,
        0xFDU, 0x02U, 0x01U, 0x02U, 0x01U, 0xFDU, 0xFEU
    };
    PressureReceiver receiver;
    const PressureReceiverSnapshot *snapshot;

    PressureReceiver_Initialize(&receiver);
    PressureReceiver_ProcessBytes(&receiver,
                                  stream,
                                  sizeof(stream),
                                  100U);

    snapshot = Snapshot(&receiver);
    assert(snapshot->raw_pressure_counts == 0x0102U);
    assert(snapshot->valid_frame_count == 3U);
    assert(snapshot->sample_sequence == 3U);
    assert(snapshot->invalid_header_count == 0U);
    assert(snapshot->invalid_bcc_count == 0U);
    assert(snapshot->invalid_marker_count == 0U);
    assert(snapshot->dropped_resync_byte_count == 0U);
}

int main(void)
{
    TestOneCompleteValidFrame();
    TestFrameSplitAcrossChunks();
    TestMultipleFramesInOneChunk();
    TestGarbageBeforeValidFrame();
    TestInvalidBccRecovery();
    TestInvalidMarkerRecovery();
    TestPartialFrameResynchronization();
    TestSequenceAndTimestampOnlyOnValidFrames();
    TestObservedContinuousSevenByteFrames();
    puts("Pressure receiver host tests: PASS");
    return 0;
}
