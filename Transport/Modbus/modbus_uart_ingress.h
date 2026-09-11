#ifndef TRANSPORT_MODBUS_MODBUS_UART_INGRESS_H
#define TRANSPORT_MODBUS_MODBUS_UART_INGRESS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MODBUS_UART_INGRESS_CAPACITY 64U

#define MODBUS_UART_EVENT_TX_COMPLETE (1UL << 0U)
#define MODBUS_UART_EVENT_ERROR       (1UL << 1U)

/*
 * ARMCC 5 does not provide C11 atomics. Its users must serialize every main
 * side access with the producing UART IRQ, as modbus_uart2.c does. Host and
 * modern compiler builds use C11 atomics so the SPSC implementation can be
 * exercised directly by ThreadSanitizer.
 */
#if defined(__CC_ARM) && !defined(__clang__)
#define MODBUS_UART_INGRESS_MAIN_REQUIRES_IRQ_MASK 1
typedef volatile uint32_t ModbusUartIngressAtomicU32;
#else
#define MODBUS_UART_INGRESS_MAIN_REQUIRES_IRQ_MASK 0
#include <stdatomic.h>
typedef atomic_uint_least32_t ModbusUartIngressAtomicU32;
#endif

typedef struct
{
    uint32_t rx_overflow_count;
    uint32_t rx_rearm_count;
    uint32_t rx_rearm_error_count;
} ModbusUartIngressSnapshot;

typedef struct
{
    uint8_t bytes[MODBUS_UART_INGRESS_CAPACITY];
    ModbusUartIngressAtomicU32 write_index;
    ModbusUartIngressAtomicU32 read_index;
    ModbusUartIngressAtomicU32 rx_overflow_count;
    ModbusUartIngressAtomicU32 rx_rearm_count;
    ModbusUartIngressAtomicU32 rx_rearm_error_count;
    ModbusUartIngressAtomicU32 event_flags;
} ModbusUartIngress;

void ModbusUartIngress_Initialize(ModbusUartIngress *ingress);
bool ModbusUartIngress_PushFromIsr(ModbusUartIngress *ingress,
                                  uint8_t byte);
size_t ModbusUartIngress_DrainFromMain(ModbusUartIngress *ingress,
                                      uint8_t *destination,
                                      size_t capacity);
void ModbusUartIngress_RecordRearmFromIsr(ModbusUartIngress *ingress,
                                          bool succeeded);
void ModbusUartIngress_SignalEventFromIsr(ModbusUartIngress *ingress,
                                          uint32_t event_flags);
uint32_t ModbusUartIngress_TakeEventsFromMain(
    ModbusUartIngress *ingress);
void ModbusUartIngress_ReadSnapshotFromMain(
    const ModbusUartIngress *ingress,
    ModbusUartIngressSnapshot *snapshot);

#endif
