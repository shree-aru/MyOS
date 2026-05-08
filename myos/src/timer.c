/* ============================================================================
 * MyOS - PIT Timer
 * Programmable Interval Timer at 100 Hz for scheduling and timekeeping
 * ============================================================================ */

#include "kernel.h"

#define PIT_CMD  0x43
#define PIT_CH0  0x40

static volatile uint32_t tick_count = 0;

static void timer_callback(registers_t* regs) {
    (void)regs;
    tick_count++;
}

void timer_init(void) {
    /* Register timer IRQ handler */
    isr_register_handler(IRQ0, timer_callback);

    /* Configure PIT channel 0: rate generator mode, lo/hi byte */
    uint16_t divisor = PIT_FREQ / TIMER_HZ;

    outb(PIT_CMD, 0x36);           /* Channel 0, lo/hi, rate generator */
    outb(PIT_CH0, divisor & 0xFF); /* Low byte */
    outb(PIT_CH0, (divisor >> 8) & 0xFF); /* High byte */
}

uint32_t timer_get_ticks(void) {
    return tick_count;
}

uint32_t timer_get_seconds(void) {
    return tick_count / TIMER_HZ;
}
