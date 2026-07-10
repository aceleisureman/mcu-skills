#include "esp8266.h"
#include "debug_uart.h"
#include <string.h>
#include <stdio.h>

UART_HandleTypeDef huart1;

/* Ring buffer */
uint8_t esp_rx_buf[ESP_RX_BUF_SIZE];
volatile uint16_t esp_rx_head = 0;
volatile uint16_t esp_rx_tail = 0;

/* Single-byte buffer for interrupt receive */
static uint8_t esp_rx_byte;

/* IP address storage */
static char esp_ip[ESP_IP_BUF_SIZE] = "0.0.0.0";
static char stringBuffer[256];

/*******************************************************************************
 * UART Init
 *******************************************************************************/
void ESP8266_Init(void)
{
    __HAL_RCC_USART1_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    /* PA9 = TX, PA10 = RX */
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* TX: PA9, alternate function push-pull */
    GPIO_InitStruct.Pin = GPIO_PIN_9;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* RX: PA10, input floating */
    GPIO_InitStruct.Pin = GPIO_PIN_10;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* USART1: 115200 baud, 8N1 */
    huart1.Instance = USART1;
    huart1.Init.BaudRate = 115200;
    huart1.Init.WordLength = UART_WORDLENGTH_8B;
    huart1.Init.StopBits = UART_STOPBITS_1;
    huart1.Init.Parity = UART_PARITY_NONE;
    huart1.Init.Mode = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&huart1);

    /* Enable USART1 interrupt for receive */
    HAL_NVIC_SetPriority(USART1_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(USART1_IRQn);
    HAL_UART_Receive_IT(&huart1, &esp_rx_byte, 1);
}

/*******************************************************************************
 * UART Receive Callback (called from ISR)
 *******************************************************************************/
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
    {
        uint16_t next = (esp_rx_head + 1) % ESP_RX_BUF_SIZE;
        /* drop the byte when full: overwriting would corrupt head/tail */
        if (next != esp_rx_tail)
        {
            esp_rx_buf[esp_rx_head] = esp_rx_byte;
            esp_rx_head = next;
        }
        HAL_UART_Receive_IT(&huart1, &esp_rx_byte, 1);
    }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
    {
        __HAL_UART_CLEAR_OREFLAG(huart);
        HAL_UART_Receive_IT(&huart1, &esp_rx_byte, 1);
    }
}

/*******************************************************************************
 * Send AT command and wait for expected response
 *******************************************************************************/
uint8_t ESP8266_SendCmd(const char *cmd, const char *expected, uint32_t timeout_ms)
{
    /* Clear receive buffer */
    esp_rx_head = 0;
    esp_rx_tail = 0;
    memset(esp_rx_buf, 0, ESP_RX_BUF_SIZE);

    /* Send command */
    if (cmd)
    {
        /* Debug: print outgoing command */
        DebugUART_Print("[TX] ");
        DebugUART_Print(cmd);
        DebugUART_Print("\r\n");

        HAL_UART_Transmit(&huart1, (uint8_t *)cmd, strlen(cmd), 1000);
        HAL_UART_Transmit(&huart1, (uint8_t *)"\r\n", 2, 100);
    }

    /* Wait for expected response */
    uint8_t found = 0;
    uint16_t last_len = 0;
    uint32_t start = HAL_GetTick();

    while ((HAL_GetTick() - start) < timeout_ms)
    {
        if (esp_rx_tail != esp_rx_head)
        {
            /* Copy available data to search buffer */
            uint16_t avail = (esp_rx_head >= esp_rx_tail)
                ? (esp_rx_head - esp_rx_tail)
                : (ESP_RX_BUF_SIZE - esp_rx_tail + esp_rx_head);
            if (avail > 0)
            {
                /* Build contiguous string */
                uint16_t len = avail;
                if (len > 255) len = 255;
                last_len = len;
                for (uint16_t i = 0; i < len; i++)
                {
                    stringBuffer[i] = esp_rx_buf[(esp_rx_tail + i) % ESP_RX_BUF_SIZE];
                }
                stringBuffer[len] = '\0';

                if (len >= strlen(expected) && strstr(stringBuffer, expected))
                {
                    found = 1;
                    break;
                }
            }
        }
    }

    if (found)
    {
        DebugUART_Print("[OK]\r\n");
    }
    else
    {
        DebugUART_Print("[FAIL]\r\n");
        if (last_len > 0)
        {
            DebugUART_Print("[RX] ");
            for (uint16_t i = 0; i < last_len; i++)
            {
                uint8_t ch = (uint8_t)stringBuffer[i];
                if (ch == '\r' || ch == '\n' || (ch >= 32 && ch <= 126))
                    DebugUART_SendByte(ch);
                else
                    DebugUART_SendByte('.');
            }
            DebugUART_Print("\r\n");
        }
    }
    return found ? 0 : 1;
}

/*******************************************************************************
 * Get received data from ring buffer into user buffer
 *******************************************************************************/
uint16_t ESP8266_GetReceivedData(char *buf, uint16_t max_len)
{
    uint16_t count = 0;
    while (esp_rx_tail != esp_rx_head && count < max_len - 1)
    {
        buf[count++] = esp_rx_buf[esp_rx_tail];
        esp_rx_tail = (esp_rx_tail + 1) % ESP_RX_BUF_SIZE;
    }
    buf[count] = '\0';
    return count;
}

/*******************************************************************************
 * Connect to WiFi network
 *******************************************************************************/
uint8_t ESP8266_ConnectWiFi(const char *ssid, const char *password)
{
    /* Test AT */
    if (ESP8266_SendCmd("AT", "OK", 2000)) return 1;

    /* Set mode: Station + SoftAP */
    if (ESP8266_SendCmd("AT+CWMODE=3", "OK", 2000)) return 2;

    /* Connect to WiFi */
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "AT+CWJAP=\"%s\",\"%s\"", ssid, password);
    if (ESP8266_SendCmd(cmd, "OK", 30000))
    {
        DebugUART_Print("[WiFi SCAN]\r\n");
        snprintf(cmd, sizeof(cmd), "AT+CWLAP=\"%s\"", ssid);
        (void)ESP8266_SendCmd(cmd, "+CWLAP:", 10000);
        return 3;
    }

    /* MQTT AT commands are more stable in single-connection mode. */
    if (ESP8266_SendCmd("AT+CIPMUX=0", "OK", 2000)) return 4;

    /* Get IP address */
    ESP8266_SendCmd("AT+CIFSR", "OK", 2000);

    return 0;
}

/*******************************************************************************
 * Connect to MQTT broker without username/password
 *******************************************************************************/
uint8_t ESP8266_ConnectMQTT(const char *host, uint16_t port, const char *client_id)
{
    char cmd[160];

    (void)ESP8266_SendCmd("AT+MQTTCLEAN=0", "OK", 3000);
    HAL_Delay(300);

    snprintf(cmd, sizeof(cmd),
             "AT+MQTTUSERCFG=0,1,\"%s\",\"\",\"\",0,0,\"\"",
             client_id);
    if (ESP8266_SendCmd(cmd, "OK", 3000))
    {
        snprintf(cmd, sizeof(cmd),
                 "AT+MQTTUSERCFG=0,1,\"%s\",\"\",\"\",0,0",
                 client_id);
        if (ESP8266_SendCmd(cmd, "OK", 3000)) return 1;
    }

    snprintf(cmd, sizeof(cmd),
             "AT+MQTTCONN=0,\"%s\",%u,1",
             host, port);
    if (ESP8266_SendCmd(cmd, "OK", 60000)) return 2;

    return 0;
}

uint8_t ESP8266_MQTTSubscribe(const char *topic)
{
    char cmd[160];

    snprintf(cmd, sizeof(cmd), "AT+MQTTSUB=0,\"%s\",0", topic);
    return ESP8266_SendCmd(cmd, "OK", 5000);
}

static void ESP8266_EscapeMQTTText(const char *in, char *out, uint16_t out_len)
{
    uint16_t pos = 0;

    while (*in && pos + 1 < out_len)
    {
        if ((*in == '"' || *in == '\\' || *in == ',') && pos + 2 < out_len)
        {
            out[pos++] = '\\';
            out[pos++] = *in++;
        }
        else
        {
            out[pos++] = *in++;
        }
    }
    out[pos] = '\0';
}

uint8_t ESP8266_MQTTPublish(const char *topic, const char *payload, uint8_t retain)
{
    char escaped[160];
    char cmd[256];

    ESP8266_EscapeMQTTText(payload, escaped, sizeof(escaped));
    snprintf(cmd, sizeof(cmd), "AT+MQTTPUB=0,\"%s\",\"%s\",0,%u",
             topic, escaped, retain ? 1U : 0U);
    return ESP8266_SendCmd(cmd, "OK", 5000);
}

/*******************************************************************************
 * Parse IP from CIFSR response
 *******************************************************************************/
void ESP8266_GetIP(char *ip_buf)
{
    /* Parse from stringBuffer (filled by SendCmd) instead of consuming the
     * ring buffer, so pending async MQTT data is not thrown away. */
    if (ESP8266_SendCmd("AT+CIFSR", "OK", 2000) == 0)
    {
        /* Parse STAIP line: +CIFSR:STAIP,"192.168.1.100" */
        char *staip = strstr(stringBuffer, "STAIP");
        if (staip)
        {
            char *q1 = strchr(staip, '"');
            if (q1)
            {
                char *q2 = strchr(q1 + 1, '"');
                if (q2)
                {
                    uint8_t ip_len = (uint8_t)(q2 - q1 - 1);
                    if (ip_len < ESP_IP_BUF_SIZE - 1)
                    {
                        memcpy(ip_buf, q1 + 1, ip_len);
                        ip_buf[ip_len] = '\0';
                        memcpy(esp_ip, ip_buf, ESP_IP_BUF_SIZE);
                        return;
                    }
                }
            }
        }
    }
    strcpy(ip_buf, esp_ip);  /* fallback to cached IP */
}

/*******************************************************************************
 * Check WiFi connection status
 *******************************************************************************/
uint8_t ESP8266_SendCmdSilent(const char *cmd, const char *expected, uint32_t timeout_ms)
{
    esp_rx_head = 0;
    esp_rx_tail = 0;
    memset(esp_rx_buf, 0, ESP_RX_BUF_SIZE);

    if (cmd)
    {
        HAL_UART_Transmit(&huart1, (uint8_t *)cmd, strlen(cmd), 1000);
        HAL_UART_Transmit(&huart1, (uint8_t *)"\r\n", 2, 100);
    }

    uint8_t found = 0;
    uint32_t start = HAL_GetTick();

    while ((HAL_GetTick() - start) < timeout_ms)
    {
        if (esp_rx_tail != esp_rx_head)
        {
            uint16_t avail = (esp_rx_head >= esp_rx_tail)
                ? (esp_rx_head - esp_rx_tail)
                : (ESP_RX_BUF_SIZE - esp_rx_tail + esp_rx_head);
            if (avail > 0)
            {
                uint16_t len = avail;
                if (len > 255) len = 255;
                for (uint16_t i = 0; i < len; i++)
                    stringBuffer[i] = esp_rx_buf[(esp_rx_tail + i) % ESP_RX_BUF_SIZE];
                stringBuffer[len] = '\0';

                if (len >= strlen(expected) && strstr(stringBuffer, expected))
                {
                    found = 1;
                    break;
                }
            }
        }
    }
    return found ? 0 : 1;
}

/*******************************************************************************
 * Check WiFi connection status
 *******************************************************************************/
uint8_t ESP8266_CheckConnection(void)
{
    /* Check stringBuffer instead of consuming the ring buffer, so pending
     * async MQTT data is not thrown away. */
    if (ESP8266_SendCmdSilent("AT+CWJAP?", "OK", 3000) == 0)
    {
        if (strstr(stringBuffer, "+CWJAP:")) return 1;
    }
    return 0;
}

/*******************************************************************************
 * Get time via ESP8266 SNTP
 * Returns HH:MM format in time_str
 *******************************************************************************/
uint8_t ESP8266_GetTime(char *time_str)
{
    /* Configure SNTP: timezone = UTC+8 (China), server = ntp.aliyun.com */
    static uint8_t sntp_configured = 0;
    if (!sntp_configured)
    {
        ESP8266_SendCmd("AT+CIPSNTPCFG=1,8,\"ntp.aliyun.com\"", "OK", 2000);
        sntp_configured = 1;
    }

    /* Wait for OK so the whole time line is in stringBuffer */
    if (ESP8266_SendCmd("AT+CIPSNTPTIME?", "OK", 5000) == 0)
    {
        /* +CIPSNTPTIME:Mon Jun  9 14:30:00 2026 — day is space-padded,
         * so skip fields with sscanf instead of counting space chars */
        char *line = strstr(stringBuffer, "+CIPSNTPTIME:");
        if (line)
        {
            unsigned int hour, min, year;
            if (sscanf(line + 13, "%*s %*s %*u %u:%u:%*u %u",
                       &hour, &min, &year) == 3 &&
                hour < 24U && min < 60U && year >= 2020U)
            {
                snprintf(time_str, 6, "%02u:%02u", hour, min);
                return 0;
            }
        }
    }
    return 1;
}
