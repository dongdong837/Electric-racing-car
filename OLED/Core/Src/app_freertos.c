#include "app_freertos.h"
#include "main.h"
#include "aim_control.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

typedef struct {
    uint32_t tick, epoch;
    uint16_t size;
    uint8_t bytes[AIM_FRAME_MAX];
} RxChunk;

typedef struct {
    AimInput input;
    AimControl control;
} DisplayState;

static QueueHandle_t rx_queue, input_queue, display_queue;
static uint8_t dma_rx[AIM_FRAME_MAX];
static volatile uint8_t restart_rx;
static volatile uint32_t rx_epoch;
volatile uint32_t g_rx_dropped;
volatile uint32_t g_uart_errors;
volatile uint32_t g_uart_restarts;
volatile uint32_t g_rx_stack_free;
volatile uint32_t g_control_stack_free;
volatile uint32_t g_display_stack_free;
const char * volatile g_assert_file;
volatile int g_assert_line;

static void Check(BaseType_t result)
{
    configASSERT(result == pdPASS);
}

static void CheckHAL(HAL_StatusTypeDef result)
{
    configASSERT(result == HAL_OK);
}

static HAL_StatusTypeDef StartReceive(void)
{
    HAL_StatusTypeDef status = HAL_UARTEx_ReceiveToIdle_DMA(&huart1, dma_rx, sizeof(dma_rx));
    if (status == HAL_OK) {
        __HAL_DMA_DISABLE_IT(&hdma_usart1_rx, DMA_IT_HT);
    }
    return status;
}

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t size)
{
    BaseType_t wake = pdFALSE;
    RxChunk chunk = {0};
    if (huart != &huart1) {
        return;
    }
    if (size > 0U && size <= sizeof(dma_rx)) {
        chunk.tick = xTaskGetTickCountFromISR();
        chunk.epoch = rx_epoch;
        chunk.size = size;
        memcpy(chunk.bytes, dma_rx, size);
        if (xQueueSendFromISR(rx_queue, &chunk, &wake) != pdPASS) {
            ++g_rx_dropped;
            ++rx_epoch;
        }
    }
    if (StartReceive() != HAL_OK) {
        restart_rx = 1;
    }
    portYIELD_FROM_ISR(wake);
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart == &huart1) {
        ++g_uart_errors;
        ++rx_epoch;
        restart_rx = 1;
    }
}

static void ReceiveTask(void *argument)
{
    RxChunk chunk;
    AimParser parser = {0};
    AimInput input = {0};
    uint32_t last_epoch = 0;
    (void)argument;
    restart_rx = 1;
    for (;;) {
        if (restart_rx) {
            taskENTER_CRITICAL();
            (void)HAL_UART_AbortReceive(&huart1);
            ++rx_epoch;
            restart_rx = (StartReceive() != HAL_OK);
            taskEXIT_CRITICAL();
            ++g_uart_restarts;
            AimParser_Reset(&parser);
        }
        if (xQueueReceive(rx_queue, &chunk, pdMS_TO_TICKS(20)) == pdPASS) {
            if (chunk.epoch != last_epoch) {
                AimParser_Reset(&parser);
                last_epoch = chunk.epoch;
            }
            for (uint16_t i = 0; i < chunk.size; ++i) {
                uint8_t kind = AimParser_Push(&parser, chunk.bytes[i], &input);
                if (kind != 0U) {
                    if (kind == 11U) {
                        input.position_tick = chunk.tick;
                    }
                    Check(xQueueOverwrite(input_queue, &input));
                }
            }
        }
        g_rx_stack_free = uxTaskGetStackHighWaterMark(NULL);
    }
}

static void ControlTask(void *argument)
{
    DisplayState state = {0};
    TickType_t last_wake = xTaskGetTickCount();
    (void)argument;
    AimControl_Init(&state.control);
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, state.control.pwm_x);
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_2, state.control.pwm_y);
    CheckHAL(HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1));
    CheckHAL(HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_2));
    for (;;) {
        (void)xQueuePeek(input_queue, &state.input, 0);
        AimControl_Step(&state.control, &state.input, xTaskGetTickCount(), pdMS_TO_TICKS(500));
        __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, state.control.pwm_x);
        __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_2, state.control.pwm_y);
        Check(xQueueOverwrite(display_queue, &state));
        g_control_stack_free = uxTaskGetStackHighWaterMark(NULL);
        (void)xTaskDelayUntil(&last_wake, pdMS_TO_TICKS(10));
    }
}

static void DisplayTask(void *argument)
{
    DisplayState state = {0};
    char text[24];
    (void)argument;
    OLED_Init();
    OLED_Clear();
    for (;;) {
        (void)xQueuePeek(display_queue, &state, 0);
        snprintf(text, sizeof(text), "T:%5u,%5u", (unsigned)state.input.x[0], (unsigned)state.input.y[0]);
        OLED_ShowString(0, 0, (unsigned char *)text, 16);
        snprintf(text, sizeof(text), "P:%5u,%5u", (unsigned)state.input.current_x, (unsigned)state.input.current_y);
        OLED_ShowString(0, 16, (unsigned char *)text, 16);
        snprintf(text, sizeof(text), "PWM:%4d,%4d", state.control.pwm_x, state.control.pwm_y);
        OLED_ShowString(0, 32, (unsigned char *)text, 16);
        snprintf(text, sizeof(text), "S:%3u Done:%u", (unsigned)state.input.shape, (unsigned)state.control.reached);
        OLED_ShowString(0, 48, (unsigned char *)text, 16);
        OLED_Refresh_Gram();
        g_display_stack_free = uxTaskGetStackHighWaterMark(NULL);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void App_Start(void)
{
    rx_queue = xQueueCreate(8, sizeof(RxChunk));
    input_queue = xQueueCreate(1, sizeof(AimInput));
    display_queue = xQueueCreate(1, sizeof(DisplayState));
    configASSERT(rx_queue != NULL && input_queue != NULL && display_queue != NULL);
    Check(xTaskCreate(ReceiveTask, "receive", 384, NULL, 3, NULL));
    Check(xTaskCreate(ControlTask, "control", 256, NULL, 4, NULL));
    Check(xTaskCreate(DisplayTask, "display", 512, NULL, 1, NULL));
    vTaskStartScheduler();
    App_AssertFailed(__FILE__, __LINE__);
}

void App_AssertFailed(const char *file, int line)
{
    g_assert_file = file;
    g_assert_line = line;
    __disable_irq();
    for (;;) {
    }
}

void vApplicationMallocFailedHook(void)
{
    App_AssertFailed("FreeRTOS heap exhausted", 0);
}

void vApplicationStackOverflowHook(TaskHandle_t task, char *name)
{
    (void)task;
    App_AssertFailed(name, 0);
}
