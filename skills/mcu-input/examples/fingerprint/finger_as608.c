#include "finger.h"

/* AS608 通信协议说明
 * 帧格式: 包头(EF 01) + 地址(4) + 包标识PID(1) + 长度(2,大端) + 内容 + 校验和(2,大端)
 * 长度值 = 内容字节数 + 2(校验和占2字节, 但不含长度字段本身)
 * 校验和 = PID + 长度高 + 长度低 + 全部内容字节
 */
#define FINGER_PID_CMD     0x01U   /* 命令包 */
#define FINGER_PID_ACK     0x07U   /* 应答包 */

#define FINGER_CMD_GENIMG  0x01U   /* 采集指纹图像 */
#define FINGER_CMD_GENCHAR 0x02U   /* 生成特征 */
#define FINGER_CMD_SEARCH  0x04U   /* 搜索指纹 (标准 AS608 PS_Search) */
#define FINGER_CMD_REGMODEL 0x05U  /* 合成模板 (CharBuffer1+2 -> 模板) */
#define FINGER_CMD_STORE   0x06U   /* 存储模板 (标准 AS608 PS_StoreChar) */
#define FINGER_CMD_SETSYSPARA 0x0EU  /* 设置系统参数(改安全等级/波特率等) */
#define FINGER_CMD_DELETE  0x0CU   /* 删除模板 */
#define FINGER_CMD_EMPTY   0x0DU   /* 清空指纹库 */
#define FINGER_CMD_READSYSPARA 0x0FU   /* 读系统参数(握手用) */

static UART_HandleTypeDef huart3;
static uint32_t s_finger_baud = FINGER_BAUD_RATE;   /* 实际使用的波特率 */
static uint8_t  s_finger_found = 0U;                /* 初始化是否探测到模块 */

/* 发送一帧命令包 */
static void Finger_SendPacket(uint8_t pid, const uint8_t *content, uint8_t content_len)
{
    uint8_t buf[24];
    uint16_t length;
    uint16_t checksum;
    uint8_t i;

    length = (uint16_t)(content_len + 2U);
    checksum = (uint16_t)(pid + (uint8_t)(length >> 8) + (uint8_t)(length & 0xFFU));

    buf[0] = 0xEFU;
    buf[1] = 0x01U;
    buf[2] = (uint8_t)(FINGER_ADDR >> 24);
    buf[3] = (uint8_t)(FINGER_ADDR >> 16);
    buf[4] = (uint8_t)(FINGER_ADDR >> 8);
    buf[5] = (uint8_t)(FINGER_ADDR);
    buf[6] = pid;
    buf[7] = (uint8_t)(length >> 8);
    buf[8] = (uint8_t)(length & 0xFFU);
    for (i = 0U; i < content_len; i++)
    {
        buf[9U + i] = content[i];
        checksum = (uint16_t)(checksum + content[i]);
    }
    buf[9U + content_len] = (uint8_t)(checksum >> 8);
    buf[9U + content_len + 1U] = (uint8_t)(checksum & 0xFFU);

    HAL_UART_Transmit(&huart3, buf, (uint16_t)(9U + content_len + 2U), 200U);
}

/* 清空接收缓冲区中的残留字节 */
static void Finger_FlushRx(void)
{
    uint8_t b;
    uint8_t n = 32U;

    while (n-- > 0U)
    {
        if (__HAL_UART_GET_FLAG(&huart3, UART_FLAG_RXNE) == RESET)
            break;
        if (HAL_UART_Receive(&huart3, &b, 1U, 2U) != HAL_OK)
            break;
    }
}

/* 接收一帧应答, 返回PID; 0xFF=通信失败. *conf=确认码, content=内容(含确认码) */
static uint8_t Finger_ReceivePacket(uint8_t *conf, uint8_t *content, uint8_t max_content, uint16_t timeout_ms)
{
    uint32_t start = HAL_GetTick();
    uint8_t b = 0U;
    uint8_t pid = 0U;
    uint8_t len_hi = 0U;
    uint8_t len_lo = 0U;
    uint8_t chk_hi = 0U;
    uint8_t chk_lo = 0U;
    uint16_t length;
    uint8_t content_len;
    uint16_t calc;
    uint8_t i;
    uint8_t got_header = 0U;

    /* 1. 搜索包头 EF 01 */
    while ((HAL_GetTick() - start) < timeout_ms)
    {
        if (HAL_UART_Receive(&huart3, &b, 1U, 5U) != HAL_OK)
            continue;
        if (b != 0xEFU)
            continue;
        if (HAL_UART_Receive(&huart3, &b, 1U, 50U) != HAL_OK)
            return FINGER_COMM_ERR;
        if (b == 0x01U)
        {
            got_header = 1U;
            break;
        }
        /* EF 后非 01, 继续搜索 */
    }
    if (got_header == 0U)
        return FINGER_COMM_ERR;

    /* 2. 地址 4 字节(忽略) */
    for (i = 0U; i < 4U; i++)
    {
        if (HAL_UART_Receive(&huart3, &b, 1U, 50U) != HAL_OK)
            return FINGER_COMM_ERR;
    }
    /* 3. PID */
    if (HAL_UART_Receive(&huart3, &pid, 1U, 50U) != HAL_OK)
        return FINGER_COMM_ERR;
    /* 4. 长度(大端) */
    if (HAL_UART_Receive(&huart3, &len_hi, 1U, 50U) != HAL_OK)
        return FINGER_COMM_ERR;
    if (HAL_UART_Receive(&huart3, &len_lo, 1U, 50U) != HAL_OK)
        return FINGER_COMM_ERR;
    length = (uint16_t)(((uint16_t)len_hi << 8) | len_lo);
    if (length < 2U)
        return FINGER_COMM_ERR;
    content_len = (uint8_t)(length - 2U);

    /* 5. 内容 + 校验和 */
    calc = (uint16_t)(pid + len_hi + len_lo);
    for (i = 0U; i < content_len; i++)
    {
        if (HAL_UART_Receive(&huart3, &b, 1U, 50U) != HAL_OK)
            return FINGER_COMM_ERR;
        if (i < max_content)
            content[i] = b;
        calc = (uint16_t)(calc + b);
    }
    if (HAL_UART_Receive(&huart3, &chk_hi, 1U, 50U) != HAL_OK)
        return FINGER_COMM_ERR;
    if (HAL_UART_Receive(&huart3, &chk_lo, 1U, 50U) != HAL_OK)
        return FINGER_COMM_ERR;

    if (((uint16_t)(((uint16_t)chk_hi << 8) | chk_lo)) != (uint16_t)(calc & 0xFFFFU))
        return FINGER_COMM_ERR;

    *conf = (content_len >= 1U) ? content[0] : FINGER_COMM_ERR;
    return pid;
}

/* 发送命令并等待应答, 返回确认码(0x00=成功), 0xFF=通信失败 */
static uint8_t Finger_Cmd(uint8_t cmd, const uint8_t *params, uint8_t param_len,
                          uint8_t *resp, uint8_t resp_max, uint16_t timeout_ms)
{
    uint8_t content[8];
    uint8_t conf = FINGER_COMM_ERR;
    uint8_t i;

    content[0] = cmd;
    for (i = 0U; i < param_len && i < 7U; i++)
        content[1U + i] = params[i];

    Finger_FlushRx();
    Finger_SendPacket(FINGER_PID_CMD, content, (uint8_t)(1U + param_len));
    if (Finger_ReceivePacket(&conf, resp, resp_max, timeout_ms) == FINGER_COMM_ERR)
        return FINGER_COMM_ERR;
    return conf;
}

uint8_t Finger_Ping(void)
{
    uint8_t resp[8] = {0};
    return Finger_Cmd(FINGER_CMD_READSYSPARA, NULL, 0U, resp, sizeof(resp), 500U);
}

/* 以指定波特率初始化并尝试握手, 1=该波特率下模块在线 */
static uint8_t Finger_TryBaud(uint32_t baud)
{
    huart3.Init.BaudRate = baud;
    if (HAL_UART_Init(&huart3) != HAL_OK)
        return 0U;
    return (Finger_Ping() == FINGER_OK) ? 1U : 0U;
}

void Finger_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    static const uint32_t bauds[] = {57600UL, 9600UL, 115200UL, 38400UL, 19200UL, 4800UL};
    uint8_t i;
    uint8_t found = 0U;

    __HAL_RCC_USART3_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    GPIO_InitStruct.Pin = FINGER_TX_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(FINGER_PORT, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = FINGER_RX_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(FINGER_PORT, &GPIO_InitStruct);

    huart3.Instance = USART3;
    huart3.Init.WordLength = UART_WORDLENGTH_8B;
    huart3.Init.StopBits = UART_STOPBITS_1;
    huart3.Init.Parity = UART_PARITY_NONE;
    huart3.Init.Mode = UART_MODE_TX_RX;
    huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart3.Init.OverSampling = UART_OVERSAMPLING_16;

    /* 等待模块上电就绪 */
    HAL_Delay(300U);

    /* 自动探测模块波特率: 逐个尝试, 握手成功即采用 */
    for (i = 0U; i < (uint8_t)(sizeof(bauds) / sizeof(bauds[0])); i++)
    {
        if (Finger_TryBaud(bauds[i]) == 1U)
        {
            found = 1U;
            s_finger_baud = bauds[i];
            break;
        }
    }

    if (found == 0U)
    {
        /* 未检测到模块, 回退到默认波特率保证配置有效 */
        huart3.Init.BaudRate = FINGER_BAUD_RATE;
        HAL_UART_Init(&huart3);
        s_finger_baud = FINGER_BAUD_RATE;
    }
    else
    {
        /* 探测到模块: 设置安全等级=3(平衡), 改善识别率 */
        (void)Finger_SetSecLevel(3U);
    }

    s_finger_found = found;
}

uint8_t Finger_SetSecLevel(uint8_t level)
{
    uint8_t resp[8] = {0};
    uint8_t params[2];

    params[0] = 5U;        /* 参数号 5 = 安全等级 */
    params[1] = level;     /* 1~5, 越小越易识别 */
    return Finger_Cmd(FINGER_CMD_SETSYSPARA, params, 2U, resp, sizeof(resp), 1000U);
}

uint32_t Finger_GetBaud(void)
{
    return s_finger_baud;
}

uint8_t Finger_IsFound(void)
{
    return s_finger_found;
}

uint8_t Finger_GenImg(void)
{
    uint8_t resp[8] = {0};
    return Finger_Cmd(FINGER_CMD_GENIMG, NULL, 0U, resp, sizeof(resp), 1000U);
}

uint8_t Finger_GenChar(uint8_t buf_id)
{
    uint8_t resp[8] = {0};
    return Finger_Cmd(FINGER_CMD_GENCHAR, &buf_id, 1U, resp, sizeof(resp), 1000U);
}

uint8_t Finger_RegModel(void)
{
    uint8_t resp[8] = {0};
    return Finger_Cmd(FINGER_CMD_REGMODEL, NULL, 0U, resp, sizeof(resp), 1500U);
}

uint8_t Finger_Store(uint8_t buf_id, uint16_t page_id)
{
    uint8_t resp[8] = {0};
    uint8_t params[3];

    params[0] = buf_id;
    params[1] = (uint8_t)(page_id >> 8);
    params[2] = (uint8_t)(page_id & 0xFFU);
    return Finger_Cmd(FINGER_CMD_STORE, params, 3U, resp, sizeof(resp), 1500U);
}

uint8_t Finger_Search(uint8_t buf_id, uint16_t start_page, uint16_t count, uint16_t *page_id)
{
    uint8_t resp[8] = {0};
    uint8_t params[5];
    uint8_t conf;

    params[0] = buf_id;
    params[1] = (uint8_t)(start_page >> 8);
    params[2] = (uint8_t)(start_page & 0xFFU);
    params[3] = (uint8_t)(count >> 8);
    params[4] = (uint8_t)(count & 0xFFU);

    conf = Finger_Cmd(FINGER_CMD_SEARCH, params, 5U, resp, sizeof(resp), 2000U);
    if (conf == FINGER_OK && page_id != NULL)
        *page_id = (uint16_t)(((uint16_t)resp[1] << 8) | resp[2]);
    return conf;
}

uint8_t Finger_Delete(uint16_t page_id, uint16_t count)
{
    uint8_t resp[8] = {0};
    uint8_t params[4];

    params[0] = (uint8_t)(page_id >> 8);
    params[1] = (uint8_t)(page_id & 0xFFU);
    params[2] = (uint8_t)(count >> 8);
    params[3] = (uint8_t)(count & 0xFFU);
    return Finger_Cmd(FINGER_CMD_DELETE, params, 4U, resp, sizeof(resp), 1500U);
}

uint8_t Finger_EmptyLib(void)
{
    uint8_t resp[8] = {0};
    return Finger_Cmd(FINGER_CMD_EMPTY, NULL, 0U, resp, sizeof(resp), 1500U);
}

uint8_t Fingerprint_Enroll(uint8_t page_id)
{
    uint32_t start = HAL_GetTick();

    /* 等待手指按下(单次采集), 超时8秒.
     * AS608 允许直接将 CharBuffer 中的特征存入指纹库, 无需两次采集合并模板.
     * 如需更高识别率, 可改为: 采集2 -> GenChar1 -> 采集2 -> GenChar2 -> RegModel(0x04) -> Store.
     */
    while ((HAL_GetTick() - start) < 8000U)
    {
        if (Finger_GenImg() == FINGER_OK)
        {
            HAL_Delay(10);
            if (Finger_GenChar(1U) != FINGER_OK)
                return 1U;
            if (Finger_Store(1U, (uint16_t)page_id) != FINGER_OK)
                return 1U;
            return 0U;
        }
        HAL_Delay(20);
    }
    return 1U;
}
