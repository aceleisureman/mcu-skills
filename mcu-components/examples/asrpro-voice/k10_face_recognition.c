#include "k10.h"
#include "oled.h"

static UART_HandleTypeDef huart1;
static uint8_t k10_rx_buf[8];
static volatile uint8_t k10_rx_flag = 0U;
static volatile uint8_t k10_rx_count = 0U;

void K10_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_USART1_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIO_InitStruct.Pin = K10_TX_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(K10_TX_PORT, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = K10_RX_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(K10_RX_PORT, &GPIO_InitStruct);

    huart1.Instance = USART1;
    huart1.Init.BaudRate = K10_BAUD_RATE;
    huart1.Init.WordLength = UART_WORDLENGTH_8B;
    huart1.Init.StopBits = UART_STOPBITS_1;
    huart1.Init.Parity = UART_PARITY_NONE;
    huart1.Init.Mode = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&huart1);

    /* K210 上电会先发一个 6 字节响应(0xAA 0 0 0 0 0x55), 略等后清空接收缓冲。
     * 每次发命令前还会再 FlushRx, 且 WaitResponse 丢弃包头前垃圾, 故无需死等。 */
    HAL_Delay(300U);
    K10_FlushRx();
}

void K10_FlushRx(void)
{
    uint8_t data;
    /* 读空 USART 接收缓冲区里的残留数据 */
    while (HAL_UART_Receive(&huart1, &data, 1U, 5U) == HAL_OK)
    {
        /* 丢弃 */
    }
    k10_rx_count = 0U;
    k10_rx_flag = 0U;
}

static void K10_SendByte(uint8_t data)
{
    HAL_UART_Transmit(&huart1, &data, 1U, 100U);
}

uint8_t K10_SendCommand(uint8_t cmd, uint8_t param)
{
    uint8_t checksum = cmd ^ param;

    /* 发送前清空接收缓冲, 丢弃 K210 上电响应等残留, 避免响应错位 */
    K10_FlushRx();

    K10_SendByte(K10_HEADER);
    K10_SendByte(cmd);
    K10_SendByte(param);
    K10_SendByte(checksum);
    K10_SendByte(K10_TAIL);

    return 0U;
}

uint8_t K10_Ping(void)
{
    K10_Result result;

    K10_SendCommand(0x00U, 0x00U);
    return (K10_WaitResponse(&result, 500U) == 0U) ? 0U : 1U;
}

uint8_t K10_WaitResponse(K10_Result *result, uint16_t timeout_ms)
{
    uint32_t start_tick = HAL_GetTick();
    uint8_t data;
    uint8_t count = 0U;

    /* 状态机: 等包头 0xAA, 收满 6 字节且包尾 0x55 才算完整。
     * 包头前的垃圾字节(如残留)一律丢弃。 */
    while ((HAL_GetTick() - start_tick) < timeout_ms)
    {
        if (HAL_UART_Receive(&huart1, &data, 1U, 5U) != HAL_OK)
            continue;

        if (data == K10_HEADER && count == 0U)
        {
            k10_rx_buf[count++] = data;
        }
        else if (count > 0U)
        {
            if (count < sizeof(k10_rx_buf))
                k10_rx_buf[count++] = data;

            if (count >= 6U && k10_rx_buf[count - 1U] == K10_TAIL)
            {
                k10_rx_flag = 1U;
                break;
            }
            /* 包尾不对就丢弃这帧, 重新等包头 */
            if (count >= 6U)
                count = 0U;
        }
    }

    if (!k10_rx_flag)
    {
        result->status = K10_STATUS_TIMEOUT;
        result->face_id = 0U;
        result->confidence = 0U;
        k10_rx_flag = 0U;
        return 1U;
    }

    result->status = k10_rx_buf[1];
    result->face_id = k10_rx_buf[2];
    result->confidence = k10_rx_buf[3];

    k10_rx_flag = 0U;
    k10_rx_count = 0U;
    return 0U;
}

uint8_t K10_EnrollFace(uint8_t face_id)
{
    K10_Result result;
    K10_SendCommand(K10_CMD_FACE_ENROLL, face_id);
    /* K210 capture_feature 最多 10 秒 + 保存, 留余量 14 秒 */
    K10_WaitResponse(&result, 14000U);
    return result.status;
}

uint8_t K10_RecognizeFace(K10_Result *result)
{
    K10_SendCommand(K10_CMD_FACE_RECOGNIZE, 0x00U);
    /* K210 capture_feature 最多 8 秒 + 比对, 留余量 11 秒 */
    K10_WaitResponse(result, 11000U);
    return result->status;
}

uint8_t K10_DeleteFace(uint8_t face_id)
{
    K10_Result result;
    K10_SendCommand(K10_CMD_FACE_DELETE, face_id);
    K10_WaitResponse(&result, 3000U);
    return result.status;
}

uint8_t K10_ClearAll(void)
{
    K10_Result result;
    K10_SendCommand(K10_CMD_FACE_CLEAR, 0x00U);
    K10_WaitResponse(&result, 3000U);
    return result.status;
}

static char K10_HexNibble(uint8_t v)
{
    return (v < 10U) ? (char)('0' + v) : (char)('A' + (v - 10U));
}

void K10_RxSelfTest(void)
{
    uint8_t last[6] = {0};
    uint8_t last_n = 0U;
    uint32_t total = 0U;
    uint32_t last_draw = 0U;
    uint8_t data;

    OLED_Clear();
    OLED_ShowString(0, 0, "K10 RX SELFTEST", 8);
    OLED_ShowString(0, 7, "reset to exit", 8);

    for (;;)
    {
        if (HAL_UART_Receive(&huart1, &data, 1U, 10U) == HAL_OK)
        {
            total++;
            if (last_n < 6U)
            {
                last[last_n++] = data;
            }
            else
            {
                uint8_t i;
                for (i = 0U; i < 5U; i++)
                    last[i] = last[i + 1U];
                last[5] = data;
            }
        }

        if ((HAL_GetTick() - last_draw) >= 150U)
        {
            char hex[3U * 6U + 1U];
            uint8_t i;
            uint8_t p = 0U;

            last_draw = HAL_GetTick();
            OLED_ShowString(0, 2, "RX:", 8);
            OLED_ShowNum(24, 2, total, 6, 8);

            for (i = 0U; i < last_n; i++)
            {
                hex[p++] = K10_HexNibble((uint8_t)(last[i] >> 4));
                hex[p++] = K10_HexNibble((uint8_t)(last[i] & 0x0FU));
                hex[p++] = ' ';
            }
            hex[p] = '\0';
            OLED_ShowString(0, 4, "                    ", 8);
            OLED_ShowString(0, 4, hex, 8);

            if (total > 0U)
                OLED_ShowString(0, 6, "GOT DATA PA10 OK", 8);
            else
                OLED_ShowString(0, 6, "NO RX..check PA10", 8);
        }
    }
}
