#include "Transport/Modbus/modbus_uart_ingress.h"

#include <string.h>

#if MODBUS_UART_INGRESS_MAIN_REQUIRES_IRQ_MASK

static uint32_t ModbusUartIngress_Load(
    const ModbusUartIngressAtomicU32 *value)
{
    return *value;
}

static void ModbusUartIngress_Store(ModbusUartIngressAtomicU32 *value,
                                    uint32_t desired)
{
    *value = desired;
}

static void ModbusUartIngress_Increment(ModbusUartIngressAtomicU32 *value)
{
    ++(*value);
}

static void ModbusUartIngress_Or(ModbusUartIngressAtomicU32 *value,
                                uint32_t bits)
{
    *value |= bits;
}

static uint32_t ModbusUartIngress_Exchange(
    ModbusUartIngressAtomicU32 *value,
    uint32_t desired)
{
    uint32_t previous = *value;

    *value = desired;
    return previous;
}

#else

static uint32_t ModbusUartIngress_Load(
    const ModbusUartIngressAtomicU32 *value)
{
    return atomic_load_explicit(value, memory_order_acquire);
}

static void ModbusUartIngress_Store(ModbusUartIngressAtomicU32 *value,
                                    uint32_t desired)
{
    atomic_store_explicit(value, desired, memory_order_release);
}

static void ModbusUartIngress_Increment(ModbusUartIngressAtomicU32 *value)
{
    (void)atomic_fetch_add_explicit(value, 1U, memory_order_relaxed);
}

static void ModbusUartIngress_Or(ModbusUartIngressAtomicU32 *value,
                                uint32_t bits)
{
    (void)atomic_fetch_or_explicit(value, bits, memory_order_release);
}

static uint32_t ModbusUartIngress_Exchange(
    ModbusUartIngressAtomicU32 *value,
    uint32_t desired)
{
    return atomic_exchange_explicit(value, desired,
                                    memory_order_acq_rel);
}

#endif

void ModbusUartIngress_Initialize(ModbusUartIngress *ingress)
{
    if (ingress != NULL)
    {
        (void)memset(ingress->bytes, 0, sizeof(ingress->bytes));
#if MODBUS_UART_INGRESS_MAIN_REQUIRES_IRQ_MASK
        ingress->write_index = 0U;
        ingress->read_index = 0U;
        ingress->rx_overflow_count = 0U;
        ingress->rx_rearm_count = 0U;
        ingress->rx_rearm_error_count = 0U;
        ingress->event_flags = 0U;
#else
        atomic_init(&ingress->write_index, 0U);
        atomic_init(&ingress->read_index, 0U);
        atomic_init(&ingress->rx_overflow_count, 0U);
        atomic_init(&ingress->rx_rearm_count, 0U);
        atomic_init(&ingress->rx_rearm_error_count, 0U);
        atomic_init(&ingress->event_flags, 0U);
#endif
    }
}

bool ModbusUartIngress_PushFromIsr(ModbusUartIngress *ingress,
                                  uint8_t byte)
{
    uint32_t write_index;
    uint32_t next_index;

    if (ingress == NULL)
    {
        return false;
    }

    write_index = ModbusUartIngress_Load(&ingress->write_index);
    next_index = (write_index + 1U) % MODBUS_UART_INGRESS_CAPACITY;
    if (next_index == ModbusUartIngress_Load(&ingress->read_index))
    {
        ModbusUartIngress_Increment(&ingress->rx_overflow_count);
        return false;
    }

    ingress->bytes[write_index] = byte;
    ModbusUartIngress_Store(&ingress->write_index, next_index);
    return true;
}

size_t ModbusUartIngress_DrainFromMain(ModbusUartIngress *ingress,
                                      uint8_t *destination,
                                      size_t capacity)
{
    size_t count = 0U;
    uint32_t read_index;
    uint32_t write_index;

    if ((ingress == NULL) || (destination == NULL) || (capacity == 0U))
    {
        return 0U;
    }

    read_index = ModbusUartIngress_Load(&ingress->read_index);
    write_index = ModbusUartIngress_Load(&ingress->write_index);
    while ((read_index != write_index) && (count < capacity))
    {
        destination[count] = ingress->bytes[read_index];
        ++count;
        read_index = (read_index + 1U) % MODBUS_UART_INGRESS_CAPACITY;
    }
    ModbusUartIngress_Store(&ingress->read_index, read_index);
    return count;
}

void ModbusUartIngress_RecordRearmFromIsr(ModbusUartIngress *ingress,
                                          bool succeeded)
{
    if (ingress == NULL)
    {
        return;
    }

    ModbusUartIngress_Increment(&ingress->rx_rearm_count);
    if (!succeeded)
    {
        ModbusUartIngress_Increment(&ingress->rx_rearm_error_count);
    }
}

void ModbusUartIngress_SignalEventFromIsr(ModbusUartIngress *ingress,
                                          uint32_t event_flags)
{
    if (ingress != NULL)
    {
        ModbusUartIngress_Or(&ingress->event_flags, event_flags);
    }
}

uint32_t ModbusUartIngress_TakeEventsFromMain(
    ModbusUartIngress *ingress)
{
    if (ingress == NULL)
    {
        return 0U;
    }
    return ModbusUartIngress_Exchange(&ingress->event_flags, 0U);
}

void ModbusUartIngress_ReadSnapshotFromMain(
    const ModbusUartIngress *ingress,
    ModbusUartIngressSnapshot *snapshot)
{
    if ((ingress == NULL) || (snapshot == NULL))
    {
        return;
    }

    snapshot->rx_overflow_count =
        ModbusUartIngress_Load(&ingress->rx_overflow_count);
    snapshot->rx_rearm_count =
        ModbusUartIngress_Load(&ingress->rx_rearm_count);
    snapshot->rx_rearm_error_count =
        ModbusUartIngress_Load(&ingress->rx_rearm_error_count);
}
