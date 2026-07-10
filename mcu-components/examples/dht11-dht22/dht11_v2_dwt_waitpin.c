#include "dht11.h"

static uint8_t DHT11_Buf[5];  /* 40 bits: 5 bytes */

/* Enable DWT cycle counter for microsecond delays */
static void DWT_Init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

static void delay_us(uint32_t us)
{
    uint32_t start = DWT->CYCCNT;
    uint32_t ticks = us * (SystemCoreClock / 1000000U);
    while ((DWT->CYCCNT - start) < ticks);
}

static uint8_t DHT11_WaitPin(GPIO_PinState state, uint32_t timeout_us)
{
    uint32_t start = DWT->CYCCNT;
    uint32_t ticks = timeout_us * (SystemCoreClock / 1000000U);

    while (HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN) == state)
    {
        if ((DWT->CYCCNT - start) >= ticks) return 1;
    }
    return 0;
}

/* Set DHT11 pin to output mode */
static void DHT11_SetOutput(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = DHT11_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(DHT11_PORT, &GPIO_InitStruct);
}

/* Set DHT11 pin to input mode with pull-up */
static void DHT11_SetInput(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = DHT11_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(DHT11_PORT, &GPIO_InitStruct);
}

/* Wait for DHT11 response: low 80us, high 80us */
static uint8_t DHT11_CheckResponse(void)
{
    if (DHT11_WaitPin(GPIO_PIN_SET, 100)) return 1;    /* sensor pulls low */
    if (DHT11_WaitPin(GPIO_PIN_RESET, 100)) return 1;  /* low ends */
    if (DHT11_WaitPin(GPIO_PIN_SET, 100)) return 1;    /* high ends */
    return 0;
}

/* Read one byte (8 bits, MSB first) from DHT11 */
static uint8_t DHT11_ReadByte(void)
{
    uint8_t byte = 0;

    for (uint8_t i = 0; i < 8; i++)
    {
        /* Wait for low (start of bit, ~50us) */
        if (DHT11_WaitPin(GPIO_PIN_RESET, 80)) return 0;

        /* Measure high duration */
        delay_us(40);
        if (HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN))
        {
            byte |= (1 << (7 - i));  /* high > 40us → bit = 1 */
            /* Wait for high to end */
            (void)DHT11_WaitPin(GPIO_PIN_SET, 100);
        }
        /* else: high < 40us → bit = 0, already done */
    }
    return byte;
}

/*******************************************************************************
 * Public API
 *******************************************************************************/
void DHT11_Init(void)
{
    __HAL_RCC_GPIOC_CLK_ENABLE();
    DWT_Init();
    DHT11_SetOutput();
    HAL_GPIO_WritePin(DHT11_PORT, DHT11_PIN, GPIO_PIN_SET);  /* idle high */
}

/*
 * Read temperature and humidity from DHT11.
 * Returns 0 on success, 1 on error (timeout or checksum failure).
 * temperature and humidity contain the integer values on success.
 */
uint8_t DHT11_Read(uint8_t *temperature, uint8_t *humidity)
{
    /* Host start signal: pull low 20ms, then high 30us */
    DHT11_SetOutput();
    HAL_GPIO_WritePin(DHT11_PORT, DHT11_PIN, GPIO_PIN_RESET);
    HAL_Delay(20);  /* 20ms low */
    HAL_GPIO_WritePin(DHT11_PORT, DHT11_PIN, GPIO_PIN_SET);
    delay_us(30);   /* 30us high */

    /* Switch to input and read response */
    DHT11_SetInput();
    if (DHT11_CheckResponse()) return 1;

    /* Read 40 bits (5 bytes): humidity_int, humidity_dec, temp_int, temp_dec, checksum */
    for (uint8_t i = 0; i < 5; i++)
    {
        DHT11_Buf[i] = DHT11_ReadByte();
    }

    /* Verify checksum */
    if (DHT11_Buf[4] != (DHT11_Buf[0] + DHT11_Buf[1] + DHT11_Buf[2] + DHT11_Buf[3]))
        return 1;

    *humidity    = DHT11_Buf[0];  /* integer RH% */
    *temperature = DHT11_Buf[2];  /* integer degrees C */

    /* Set output + idle high for next read */
    DHT11_SetOutput();
    HAL_GPIO_WritePin(DHT11_PORT, DHT11_PIN, GPIO_PIN_SET);

    return 0;
}
