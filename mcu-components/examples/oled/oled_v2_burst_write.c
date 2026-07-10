#include "oled.h"
#include "oledfont.h"

/* 软件I2C延时 */
static void IIC_Delay(void)
{
    uint8_t i = 5;
    while (i--);
}

/* I2C起始信号 */
void OLED_IIC_Start(void)
{
    HAL_GPIO_WritePin(OLED_SDA_GPIO, OLED_SDA_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(OLED_SCL_GPIO, OLED_SCL_PIN, GPIO_PIN_SET);
    IIC_Delay();
    HAL_GPIO_WritePin(OLED_SDA_GPIO, OLED_SDA_PIN, GPIO_PIN_RESET);
    IIC_Delay();
    HAL_GPIO_WritePin(OLED_SCL_GPIO, OLED_SCL_PIN, GPIO_PIN_RESET);
}

/* I2C停止信号 */
void OLED_IIC_Stop(void)
{
    HAL_GPIO_WritePin(OLED_SDA_GPIO, OLED_SDA_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(OLED_SCL_GPIO, OLED_SCL_PIN, GPIO_PIN_SET);
    IIC_Delay();
    HAL_GPIO_WritePin(OLED_SDA_GPIO, OLED_SDA_PIN, GPIO_PIN_SET);
}

/* I2C写一个字节 */
void OLED_IIC_WriteByte(uint8_t data)
{
    uint8_t i;
    for (i = 0; i < 8; i++)
    {
        HAL_GPIO_WritePin(OLED_SCL_GPIO, OLED_SCL_PIN, GPIO_PIN_RESET);
        if (data & 0x80)
            HAL_GPIO_WritePin(OLED_SDA_GPIO, OLED_SDA_PIN, GPIO_PIN_SET);
        else
            HAL_GPIO_WritePin(OLED_SDA_GPIO, OLED_SDA_PIN, GPIO_PIN_RESET);
        IIC_Delay();
        HAL_GPIO_WritePin(OLED_SCL_GPIO, OLED_SCL_PIN, GPIO_PIN_SET);
        IIC_Delay();
        data <<= 1;
    }
    HAL_GPIO_WritePin(OLED_SCL_GPIO, OLED_SCL_PIN, GPIO_PIN_RESET);
    /* ACK */
    HAL_GPIO_WritePin(OLED_SDA_GPIO, OLED_SDA_PIN, GPIO_PIN_SET);
    IIC_Delay();
    HAL_GPIO_WritePin(OLED_SCL_GPIO, OLED_SCL_PIN, GPIO_PIN_SET);
    IIC_Delay();
    HAL_GPIO_WritePin(OLED_SCL_GPIO, OLED_SCL_PIN, GPIO_PIN_RESET);
}

/* 写命令 */
void OLED_IIC_WriteCmd(uint8_t cmd)
{
    OLED_IIC_Start();
    OLED_IIC_WriteByte(OLED_ADDRESS);
    OLED_IIC_WriteByte(0x00);  // Co=0, D/C=0 -> 命令
    OLED_IIC_WriteByte(cmd);
    OLED_IIC_Stop();
}

/* 批量写数据 (一次 START, 连续写 N 字节, 一次 STOP) */
void OLED_IIC_WriteDataBurst(const uint8_t *data, uint8_t len)
{
    uint8_t i;
    OLED_IIC_Start();
    OLED_IIC_WriteByte(OLED_ADDRESS);
    OLED_IIC_WriteByte(0x40);  // Co=0, D/C=1 -> 数据
    for (i = 0U; i < len; i++)
        OLED_IIC_WriteByte(data[i]);
    OLED_IIC_Stop();
}

/* 写数据 */
void OLED_IIC_WriteData(uint8_t data)
{
    OLED_IIC_Start();
    OLED_IIC_WriteByte(OLED_ADDRESS);
    OLED_IIC_WriteByte(0x40);  // Co=0, D/C=1 -> 数据
    OLED_IIC_WriteByte(data);
    OLED_IIC_Stop();
}

/* 设置显示位置 */
void OLED_SetPos(uint8_t x, uint8_t y)
{
    OLED_IIC_WriteCmd(0xB0 + y);
    OLED_IIC_WriteCmd(x & 0x0F);
    OLED_IIC_WriteCmd(((x & 0xF0) >> 4) | 0x10);
}

/* 填充整个屏幕 */
void OLED_Fill(uint8_t data)
{
    uint8_t m, n;
    uint8_t page_buf[128];
    for (n = 0; n < 128; n++)
        page_buf[n] = data;
    for (m = 0; m < 8; m++)
    {
        OLED_IIC_WriteCmd(0xB0 + m);
        OLED_IIC_WriteCmd(0x00);
        OLED_IIC_WriteCmd(0x10);
        OLED_IIC_WriteDataBurst(page_buf, 128);
    }
}

/* 清屏 */
void OLED_Clear(void)
{
    OLED_Fill(0x00);
}

/* 清除指定区域 */
void OLED_ClearArea(uint8_t x, uint8_t y, uint8_t w, uint8_t h)
{
    uint8_t page_start = y / 8;
    uint8_t page_end = (y + h - 1) / 8;
    uint8_t p, col;
    uint8_t row_buf[128];

    for (p = page_start; p <= page_end; p++)
    {
        OLED_SetPos(x, p);
        for (col = 0; col < w; col++)
            row_buf[col] = 0x00;
        OLED_IIC_WriteDataBurst(row_buf, w);
    }
}

/* 开显示 */
void OLED_Display_On(void)
{
    OLED_IIC_WriteCmd(0x8D);
    OLED_IIC_WriteCmd(0x14);
    OLED_IIC_WriteCmd(0xAF);
}

/* 关显示 */
void OLED_Display_Off(void)
{
    OLED_IIC_WriteCmd(0x8D);
    OLED_IIC_WriteCmd(0x10);
    OLED_IIC_WriteCmd(0xAE);
}

/* 显示一个字符 (6x8 / 8x16) */
static const unsigned char *OLED_FindAscii16Glyph(char chr)
{
    uint8_t i;

    for (i = 0U; i < ASCII16X16_COUNT; i++)
    {
        if (Ascii16x16[i].code == chr)
            return Ascii16x16[i].data;
    }

    return 0;
}

static void OLED_Show16x16GlyphMasked(uint8_t x, uint8_t y, const unsigned char *glyph, uint8_t inv)
{
    uint8_t page;
    uint8_t col;
    uint8_t v;

    /* 字模格式: 16行 x 2字节/行 (行主序, MSB=顶部像素)
     * byte_index = col*2 + page:
     *   page=0 → glyph[col*2]   = 上半页 8 像素 (bit7=顶)
     *   page=1 → glyph[col*2+1] = 下半页 8 像素 (bit7=顶)
     * inv=0xFF 时整字反白(白底黑字), inv=0x00 时正常.
     */
    for (page = 0U; page < 2U; page++)
    {
        OLED_SetPos(x, y + page);
        for (col = 0U; col < 16U; col++)
        {
            uint8_t data = 0U;
            uint8_t byte_index = (uint8_t)(col * 2U + page);
            for (v = 0U; v < 8U; v++)
            {
                uint8_t bit_mask = (uint8_t)(0x80U >> v);
                if ((glyph[byte_index] & bit_mask) != 0U)
                    data |= (uint8_t)(1U << v);
            }
            OLED_IIC_WriteData((uint8_t)(data ^ inv));
        }
    }
}

static void OLED_Show16x16Glyph(uint8_t x, uint8_t y, const unsigned char *glyph)
{
    OLED_Show16x16GlyphMasked(x, y, glyph, 0x00U);
}

void OLED_ShowChar(uint8_t x, uint8_t y, char chr, uint8_t size)
{
    uint8_t c = chr - ' ';
    const unsigned char *glyph;

    if (x > 127) { x = 0; y += 2; }
    if (size == 16)
    {
        glyph = OLED_FindAscii16Glyph(chr);
        if (glyph != 0)
        {
            OLED_Show16x16Glyph(x, y, glyph);
        }
    }
    else
    {
        OLED_SetPos(x, y);
        uint8_t i;
        for (i = 0; i < 6; i++)
            OLED_IIC_WriteData(F6x8[c][i]);
    }
}

/* 显示字符串 */
void OLED_ShowString(uint8_t x, uint8_t y, const char *str, uint8_t size)
{
    uint8_t j = 0;
    while (str[j] != '\0')
    {
        OLED_ShowChar(x, y, str[j], size);
        if (size == 16)
            x += 16;
        else
            x += 6;
        if (x > 120) { x = 0; y += 2; }
        j++;
    }
}

/* 显示BMP图片 */
void OLED_DrawBMP(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, const uint8_t *bmp)
{
    uint8_t x, y;
    uint32_t j = 0;
    for (y = y0; y < y1; y++)
    {
        OLED_SetPos(x0, y);
        for (x = x0; x < x1; x++)
            OLED_IIC_WriteData(bmp[j++]);
    }
}

/* OLED GPIO 初始化 */
void OLED_ShowChinese(uint8_t x, uint8_t y, uint8_t index)
{
    if (index >= CN_FONT_COUNT)
        return;

    OLED_Show16x16Glyph(x, y, Chinese16x16[index]);
}

/* 反白显示中文(白底黑字), 用于标题栏 */
void OLED_ShowChineseInv(uint8_t x, uint8_t y, uint8_t index)
{
    if (index >= CN_FONT_COUNT)
        return;

    OLED_Show16x16GlyphMasked(x, y, Chinese16x16[index], 0xFFU);
}

/* 反白显示 ASCII(白底黑字). size=16 用 16x16 字库, 其它走 6x8 */
void OLED_ShowCharInv(uint8_t x, uint8_t y, char chr, uint8_t size)
{
    if (size == 16U)
    {
        const unsigned char *glyph = OLED_FindAscii16Glyph(chr);
        if (glyph != 0)
            OLED_Show16x16GlyphMasked(x, y, glyph, 0xFFU);
    }
    else
    {
        uint8_t i;
        uint8_t c = (uint8_t)(chr - ' ');
        OLED_SetPos(x, y);
        for (i = 0U; i < 6U; i++)
            OLED_IIC_WriteData((uint8_t)(F6x8[c][i] ^ 0xFFU));
    }
}

/* 用指定字节填充矩形区域(按页). pages=页数(每页8像素高) */
void OLED_FillArea(uint8_t x, uint8_t page, uint8_t w, uint8_t pages, uint8_t data)
{
    uint8_t p, c;
    uint8_t buf[128];

    if (w > 128U)
        w = 128U;
    for (c = 0U; c < w; c++)
        buf[c] = data;
    for (p = 0U; p < pages; p++)
    {
        OLED_SetPos(x, (uint8_t)(page + p));
        OLED_IIC_WriteDataBurst(buf, w);
    }
}

static void OLED_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();

    GPIO_InitStruct.Pin = OLED_SCL_PIN | OLED_SDA_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    HAL_GPIO_WritePin(OLED_SCL_GPIO, OLED_SCL_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(OLED_SDA_GPIO, OLED_SDA_PIN, GPIO_PIN_SET);
}

/* OLED 初始化 */
void OLED_Init(void)
{
    OLED_GPIO_Init();

    HAL_Delay(100);

    /* SSD1306 page-addressing init sequence. */
    OLED_IIC_WriteCmd(0xAE);
    OLED_IIC_WriteCmd(0x20);
    OLED_IIC_WriteCmd(0x02);
    OLED_IIC_WriteCmd(0xB0);
    OLED_IIC_WriteCmd(0xC8);
    OLED_IIC_WriteCmd(0x00);
    OLED_IIC_WriteCmd(0x10);
    OLED_IIC_WriteCmd(0x40);
    OLED_IIC_WriteCmd(0x81);
    OLED_IIC_WriteCmd(0xCF);
    OLED_IIC_WriteCmd(0xA1);
    OLED_IIC_WriteCmd(0xA6);
    OLED_IIC_WriteCmd(0xA8);
    OLED_IIC_WriteCmd(0x3F);
    OLED_IIC_WriteCmd(0xA4);
    OLED_IIC_WriteCmd(0xD3);
    OLED_IIC_WriteCmd(0x00);
    OLED_IIC_WriteCmd(0xD5);
    OLED_IIC_WriteCmd(0x80);
    OLED_IIC_WriteCmd(0xD9);
    OLED_IIC_WriteCmd(0xF1);
    OLED_IIC_WriteCmd(0xDA);
    OLED_IIC_WriteCmd(0x12);
    OLED_IIC_WriteCmd(0xDB);
    OLED_IIC_WriteCmd(0x40);
    OLED_IIC_WriteCmd(0x8D);
    OLED_IIC_WriteCmd(0x14);
    OLED_IIC_WriteCmd(0xAF);

    OLED_Clear();
}

