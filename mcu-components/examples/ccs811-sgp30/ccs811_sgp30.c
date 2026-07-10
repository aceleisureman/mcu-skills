/* SGP30 空气质量传感器驱动（独立软件 I2C：PB10/PB11） */
#include "ccs811.h"
#include "board_config.h"
#include "soft_i2c.h"

#define SGP30_ADDR_W      (CCS811_I2C_ADDR << 1)
#define SGP30_ADDR_R      ((CCS811_I2C_ADDR << 1) | 0x01u)
#define SGP30_CMD_INIT_AQ 0x2003u
#define SGP30_CMD_MEAS_AQ 0x2008u
#define SGP30_CMD_SET_HUM 0x2061u

static void sgp_delay(void)
{
  delay_us(4);
}

static void sgp_scl_high(void) { HAL_GPIO_WritePin(SGP30_SCL_PORT, SGP30_SCL_PIN, GPIO_PIN_SET); }
static void sgp_scl_low(void)  { HAL_GPIO_WritePin(SGP30_SCL_PORT, SGP30_SCL_PIN, GPIO_PIN_RESET); }
static void sgp_sda_high(void)  { HAL_GPIO_WritePin(SGP30_SDA_PORT, SGP30_SDA_PIN, GPIO_PIN_SET); }
static void sgp_sda_low(void)   { HAL_GPIO_WritePin(SGP30_SDA_PORT, SGP30_SDA_PIN, GPIO_PIN_RESET); }
static uint8_t sgp_sda_read(void)
{
  return (HAL_GPIO_ReadPin(SGP30_SDA_PORT, SGP30_SDA_PIN) == GPIO_PIN_SET) ? 1u : 0u;
}

static void sgp_sda_out(void)
{
  GPIO_InitTypeDef g = {0};
  g.Pin = SGP30_SDA_PIN;
  g.Mode = GPIO_MODE_OUTPUT_OD;
  g.Pull = GPIO_NOPULL;
  g.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(SGP30_SDA_PORT, &g);
}

static void sgp_sda_in(void)
{
  GPIO_InitTypeDef g = {0};
  g.Pin = SGP30_SDA_PIN;
  g.Mode = GPIO_MODE_INPUT;
  g.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(SGP30_SDA_PORT, &g);
}

static void sgp_start(void)
{
  sgp_sda_out();
  sgp_sda_high();
  sgp_scl_high();
  sgp_delay();
  sgp_sda_low();
  sgp_delay();
  sgp_scl_low();
}

static void sgp_stop(void)
{
  sgp_sda_out();
  sgp_scl_low();
  sgp_sda_low();
  sgp_delay();
  sgp_scl_high();
  sgp_delay();
  sgp_sda_high();
  sgp_delay();
}

static uint8_t sgp_write_byte(uint8_t byte)
{
  uint8_t i;
  sgp_sda_out();
  for (i = 0; i < 8u; i++)
  {
    if (byte & 0x80u) sgp_sda_high(); else sgp_sda_low();
    sgp_delay();
    sgp_scl_high();
    sgp_delay();
    sgp_scl_low();
    byte <<= 1;
  }
  sgp_sda_in();
  sgp_delay();
  sgp_scl_high();
  sgp_delay();
  i = sgp_sda_read();
  sgp_scl_low();
  return (uint8_t)(i == 0u);
}

static uint8_t sgp_read_byte(uint8_t ack)
{
  uint8_t i, byte = 0u;
  sgp_sda_in();
  for (i = 0; i < 8u; i++)
  {
    byte <<= 1;
    sgp_scl_high();
    sgp_delay();
    if (sgp_sda_read()) byte |= 1u;
    sgp_scl_low();
    sgp_delay();
  }
  sgp_sda_out();
  if (ack) sgp_sda_low(); else sgp_sda_high();
  sgp_delay();
  sgp_scl_high();
  sgp_delay();
  sgp_scl_low();
  sgp_sda_high();
  return byte;
}

static uint8_t crc8(const uint8_t *data, uint8_t len)
{
  uint8_t crc = 0xFFu;
  uint8_t i, bit;
  for (i = 0; i < len; i++)
  {
    crc ^= data[i];
    for (bit = 0; bit < 8u; bit++)
    {
      crc = (crc & 0x80u) ? (uint8_t)((crc << 1) ^ 0x31u) : (uint8_t)(crc << 1);
    }
  }
  return crc;
}

static uint8_t sgp_send_cmd(uint16_t cmd)
{
  sgp_start();
  if (!sgp_write_byte(SGP30_ADDR_W)) { sgp_stop(); return 1; }
  if (!sgp_write_byte((uint8_t)(cmd >> 8)) || !sgp_write_byte((uint8_t)cmd)) { sgp_stop(); return 1; }
  sgp_stop();
  return 0;
}

uint8_t CCS811_Init(void)
{
  GPIO_InitTypeDef g = {0};

  __HAL_RCC_GPIOB_CLK_ENABLE();

  g.Pin = SGP30_SDA_PIN | SGP30_SCL_PIN;
  g.Mode = GPIO_MODE_OUTPUT_OD;
  g.Pull = GPIO_PULLUP;
  g.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOB, &g);
  sgp_sda_high();
  sgp_scl_high();
  HAL_Delay(10);

  if (sgp_send_cmd(SGP30_CMD_INIT_AQ)) return 1;
  HAL_Delay(10);
  return 0;
}

uint8_t CCS811_Read(uint16_t *eco2, uint16_t *tvoc)
{
  uint8_t buf[6];

  if (sgp_send_cmd(SGP30_CMD_MEAS_AQ)) return 1;
  HAL_Delay(12);

  sgp_start();
  if (!sgp_write_byte(SGP30_ADDR_R)) { sgp_stop(); return 1; }
  buf[0] = sgp_read_byte(1);
  buf[1] = sgp_read_byte(1);
  buf[2] = sgp_read_byte(1);
  buf[3] = sgp_read_byte(1);
  buf[4] = sgp_read_byte(1);
  buf[5] = sgp_read_byte(0);
  sgp_stop();

  if (crc8(&buf[0], 2) != buf[2] || crc8(&buf[3], 2) != buf[5]) return 2;
  *eco2 = ((uint16_t)buf[0] << 8) | buf[1];
  *tvoc = ((uint16_t)buf[3] << 8) | buf[4];
  return 0;
}

void CCS811_SetEnvData(float temp, float humi)
{
  (void)temp;
  (void)humi;
  /* TODO: PCB 未提供额外气体补偿参数，这里保持安全无操作。 */
  (void)SGP30_CMD_SET_HUM;
}
