#ifndef APP_FREERTOS_H
#define APP_FREERTOS_H
#include <stdint.h>

extern volatile uint32_t g_rx_dropped;
extern volatile uint32_t g_uart_errors;
extern volatile uint32_t g_uart_restarts;
extern volatile uint32_t g_rx_stack_free;
extern volatile uint32_t g_control_stack_free;
extern volatile uint32_t g_display_stack_free;
void App_Start(void);
#endif
