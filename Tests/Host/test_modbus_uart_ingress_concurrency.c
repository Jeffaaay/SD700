#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <pthread.h>
#include <sched.h>

#include "Transport/Modbus/modbus_uart_ingress.h"

#define STRESS_BYTE_COUNT 200000U

typedef struct
{
    ModbusUartIngress *ingress;
    uint32_t seen_events;
} StressContext;

static uint8_t StressByte(uint32_t index)
{
    return (uint8_t)((index * 37U) + 11U);
}

static void *ProduceFromIsrStyleContext(void *argument)
{
    StressContext *context = (StressContext *)argument;
    uint32_t index;

    for (index = 0U; index < STRESS_BYTE_COUNT; ++index)
    {
        while (!ModbusUartIngress_PushFromIsr(context->ingress,
                                               StressByte(index)))
        {
            (void)sched_yield();
        }
        ModbusUartIngress_RecordRearmFromIsr(context->ingress, true);
        if ((index % 97U) == 0U)
        {
            ModbusUartIngress_SignalEventFromIsr(
                context->ingress,
                MODBUS_UART_EVENT_TX_COMPLETE);
        }
    }
    ModbusUartIngress_SignalEventFromIsr(context->ingress,
                                        MODBUS_UART_EVENT_ERROR);
    return NULL;
}

static void *ConsumeFromMainStyleContext(void *argument)
{
    StressContext *context = (StressContext *)argument;
    uint8_t bytes[17U];
    uint32_t received = 0U;

    while (received < STRESS_BYTE_COUNT)
    {
        size_t count = ModbusUartIngress_DrainFromMain(
            context->ingress,
            bytes,
            sizeof(bytes));
        size_t index;

        for (index = 0U; index < count; ++index)
        {
            assert(bytes[index] == StressByte(received));
            ++received;
        }
        context->seen_events |= ModbusUartIngress_TakeEventsFromMain(
            context->ingress);
        if (count == 0U)
        {
            (void)sched_yield();
        }
    }
    context->seen_events |= ModbusUartIngress_TakeEventsFromMain(
        context->ingress);
    return NULL;
}

int main(void)
{
    ModbusUartIngress ingress;
    ModbusUartIngressSnapshot snapshot;
    StressContext context = {&ingress, 0U};
    pthread_t producer;
    pthread_t consumer;
    void *result;

    ModbusUartIngress_Initialize(&ingress);
    assert(pthread_create(&producer,
                          NULL,
                          ProduceFromIsrStyleContext,
                          &context) == 0);
    assert(pthread_create(&consumer,
                          NULL,
                          ConsumeFromMainStyleContext,
                          &context) == 0);
    assert(pthread_join(producer, &result) == 0);
    assert(result == NULL);
    assert(pthread_join(consumer, &result) == 0);
    assert(result == NULL);

    context.seen_events |= ModbusUartIngress_TakeEventsFromMain(
        &ingress);
    ModbusUartIngress_ReadSnapshotFromMain(&ingress, &snapshot);
    assert(snapshot.rx_rearm_count == STRESS_BYTE_COUNT);
    assert(snapshot.rx_rearm_error_count == 0U);
    assert((context.seen_events & MODBUS_UART_EVENT_TX_COMPLETE) != 0U);
    assert((context.seen_events & MODBUS_UART_EVENT_ERROR) != 0U);
    puts("Modbus UART SPSC concurrency stress test: PASS");
    return 0;
}
