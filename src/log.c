#include "os.h"

#include "board.h"

#if LIPPUS_DEBUG_UART
#include "hardware/gpio.h"
#include "hardware/uart.h"
#endif

void log_init(void) {
#if LIPPUS_DEBUG_UART
    uart_init(uart0, 115200);
    gpio_set_function(16, GPIO_FUNC_UART);
    uart_puts(uart0, "\nLippus Boy\n");
#endif
}

void lippus_log(const char *msg) {
#if LIPPUS_DEBUG_UART
    uart_puts(uart0, msg);
    uart_putc(uart0, '\n');
#else
    (void)msg;
#endif
}
