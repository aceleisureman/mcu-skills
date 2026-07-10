#include "oled.h"
#include "oledfont.h"
#include <string.h>

/* Frame buffer (128x64 = 1024 bytes, 8 pages) */
static uint8_t OLED_Buffer[OLED_WIDTH][OLED_PAGES];

/*******************************************************************************
 * Software I2C (bit-bang) functions
 *******************************************************************************/
static void I2C_Delay(void)
{
    volatile uint8_t i = 8;
    while (i--) { __NOP(); }
}

static void I2C_Start(void)
{
    OLED_SDA_HIGH();
    OLED_SCL_HIGH();
    I2C_Delay();
    OLED_SDA_LOW();
    I2C_Delay();
    OLED_SCL_LOW();
}

static void I2C_Stop(void)
{
    OLED_SDA_LOW();
    OLED_SCL_HIGH();
    I2C_Delay();
    OLED_SDA_HIGH();
    I2C_Delay();
}

static void I2C_WaitAck(void)
{
    OLED_SDA_HIGH();
    I2C_Delay();
    OLED_SCL_HIGH();
    I2C_Delay();
    OLED_SCL_LOW();
}

static void I2C_WriteByte(uint8_t data)
{
    for (uint8_t i = 0; i < 8; i++)
    {
        if (data & 0x80)
            OLED_SDA_HIGH();
        else
            OLED_SDA_LOW();
        I2C_Delay();
        OLED_SCL_HIGH();
        I2C_Delay();
        OLED_SCL_LOW();
        data <<= 1;
    }
    I2C_WaitAck();
}

static void OLED_WriteCmd(uint8_t cmd)
{
    I2C_Start();
    I2C_WriteByte(OLED_ADDR);      /* slave address + write */
    I2C_WriteByte(0x00);           /* control byte: command */
    I2C_WriteByte(cmd);
    I2C_Stop();
}

static void OLED_WriteData(uint8_t data)
{
    I2C_Start();
    I2C_WriteByte(OLED_ADDR);      /* slave address + write */
    I2C_WriteByte(0x40);           /* control byte: data */
    I2C_WriteByte(data);
    I2C_Stop();
}

/*******************************************************************************
 * Public API
 *******************************************************************************/
void OLED_Init(void)
{
    /* Configure PB4 (SCL) and PB5 (SDA) as open-drain output */
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    __HAL_RCC_GPIOB_CLK_ENABLE();

    GPIO_InitStruct.Pin = OLED_SCL_PIN | OLED_SDA_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /* SSD1306 initialisation sequence */
    OLED_WriteCmd(0xAE); /* display off */
    OLED_WriteCmd(0x00); /* set low column */
    OLED_WriteCmd(0x10); /* set high column */
    OLED_WriteCmd(0x40); /* set start line */
    OLED_WriteCmd(0xB0); /* set page address */
    OLED_WriteCmd(0x81); /* contrast control */
    OLED_WriteCmd(0xFF); /* contrast = 255 */
    OLED_WriteCmd(0xA1); /* segment remap (left-right mirror) */
    OLED_WriteCmd(0xA6); /* normal / not inverted */
    OLED_WriteCmd(0xA8); /* multiplex ratio */
    OLED_WriteCmd(0x3F); /* duty = 1/64 */
    OLED_WriteCmd(0xC8); /* COM scan direction (bottom-up) */
    OLED_WriteCmd(0xD3); /* display offset */
    OLED_WriteCmd(0x00);
    OLED_WriteCmd(0xD5); /* oscillator frequency */
    OLED_WriteCmd(0x80);
    OLED_WriteCmd(0xD9); /* pre-charge period */
    OLED_WriteCmd(0xF1);
    OLED_WriteCmd(0xDA); /* COM pins config */
    OLED_WriteCmd(0x12);
    OLED_WriteCmd(0xDB); /* VCOMH deselect level */
    OLED_WriteCmd(0x40);
    OLED_WriteCmd(0x8D); /* charge pump */
    OLED_WriteCmd(0x14); /* enable charge pump */
    OLED_WriteCmd(0xA4); /* entire display on: follow RAM */
    OLED_WriteCmd(0xA6); /* normal display */
    OLED_WriteCmd(0xAF); /* display on */

    OLED_Clear();
    OLED_Refresh();
}

void OLED_Display_On(void)
{
    OLED_WriteCmd(0x8D);
    OLED_WriteCmd(0x14); /* charge pump on */
    OLED_WriteCmd(0xAF); /* display on */
}

void OLED_Display_Off(void)
{
    OLED_WriteCmd(0x8D);
    OLED_WriteCmd(0x10); /* charge pump off */
    OLED_WriteCmd(0xAE); /* display off */
}

void OLED_Clear(void)
{
    memset(OLED_Buffer, 0, sizeof(OLED_Buffer));
}

void OLED_ClearBuffer(void)
{
    memset(OLED_Buffer, 0, sizeof(OLED_Buffer));
}

void OLED_Refresh(void)
{
    uint8_t i;

    for (uint8_t page = 0; page < OLED_PAGES; page++)
    {
        /* Set page and column */
        OLED_WriteCmd(0xB0 + page);
        OLED_WriteCmd(0x00);
        OLED_WriteCmd(0x10);

        /* Send entire page in one I2C burst */
        I2C_Start();
        I2C_WriteByte(OLED_ADDR);
        I2C_WriteByte(0x40);  /* control byte: data stream */
        for (uint8_t col = 0; col < OLED_WIDTH; col++)
        {
            uint8_t data = OLED_Buffer[col][page];
            for (i = 0; i < 8; i++)
            {
                if (data & 0x80)
                    OLED_SDA_HIGH();
                else
                    OLED_SDA_LOW();
                I2C_Delay();
                OLED_SCL_HIGH();
                I2C_Delay();
                OLED_SCL_LOW();
                data <<= 1;
            }
            /* Skip ACK check for burst speed */
            OLED_SDA_HIGH();
            I2C_Delay();
            OLED_SCL_HIGH();
            I2C_Delay();
            OLED_SCL_LOW();
        }
        I2C_Stop();
    }
}

void OLED_SetCursor(uint8_t page, uint8_t col)
{
    /* Not used in buffered mode; kept for API compatibility */
    (void)page;
    (void)col;
}

/*
 * Draw a 6x8 ASCII character at pixel position (x, page_y)
 * size=8 uses 6x8 font, size=16 uses 8x16 font
 */
void OLED_ShowChar(uint8_t x, uint8_t y, char ch, uint8_t size)
{
    if (x > OLED_WIDTH - 6 || y > OLED_PAGES - 1) return;

    uint8_t idx = (uint8_t)ch - 0x20;
    if (idx > 94) return;

    if (size == 8)
    {
        for (uint8_t i = 0; i < 6; i++)
        {
            if (x + i < OLED_WIDTH)
                OLED_Buffer[x + i][y] = oled_asc2_0806[idx][i];
        }
    }
    else if (size == 16)
    {
        /* 8x16 font uses 2 pages */
        if (y >= OLED_PAGES - 1) return;

        for (uint8_t i = 0; i < 8; i++)
        {
            if (x + i < OLED_WIDTH)
            {
                OLED_Buffer[x + i][y]     = oled_asc2_1608[idx][i];
                OLED_Buffer[x + i][y + 1] = oled_asc2_1608[idx][i + 8];
            }
        }
    }
}

void OLED_ShowString(uint8_t x, uint8_t y, const char *str, uint8_t size)
{
    while (*str)
    {
        OLED_ShowChar(x, y, *str, size);
        x += (size == 16) ? 8 : 6;
        if (x + ((size == 16) ? 8 : 6) > OLED_WIDTH) break;
        str++;
    }
}

void OLED_ShowNum(uint8_t x, uint8_t y, uint32_t num, uint8_t len, uint8_t size)
{
    char buf[12];
    buf[len] = '\0';
    for (uint8_t i = len; i > 0; i--)
    {
        buf[i - 1] = (num % 10) + '0';
        num /= 10;
    }
    OLED_ShowString(x, y, buf, size);
}

void OLED_ShowChinese(uint8_t x, uint8_t y, uint8_t index)
{
    if (index >= CHINESE_MAX) return;
    if (x > OLED_WIDTH - 16 || y > OLED_PAGES - 2) return;

    for (uint8_t i = 0; i < 16; i++)
    {
        if (x + i < OLED_WIDTH)
        {
            OLED_Buffer[x + i][y]     = oled_hz16[index][i];
            OLED_Buffer[x + i][y + 1] = oled_hz16[index][i + 16];
        }
    }
}

void OLED_DrawBMP(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, const uint8_t *bmp)
{
    uint8_t width  = x1 - x0 + 1;
    uint8_t height = y1 - y0 + 1;
    (void)height;
    for (uint8_t x = 0; x < width; x++)
    {
        if (x0 + x >= OLED_WIDTH) break;
        OLED_Buffer[x0 + x][y0] = bmp[x];
    }
}
