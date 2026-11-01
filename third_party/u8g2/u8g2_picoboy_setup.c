/* Nur der Aufbau fuer das SH1106 des PicoBoy, entnommen aus u8g2_d_setup.c und
   u8g2_d_memory.c (U8g2, BSD-Lizenz) - statt der beiden Riesendateien. */
#include "u8g2.h"

static uint8_t *u8g2_m_16_8_f_pb(uint8_t *page_cnt)
{
  static uint8_t buf[1024];
  *page_cnt = 8;
  return buf;
}

void u8g2_Setup_sh1106_128x64_noname_f(u8g2_t *u8g2, const u8g2_cb_t *rotation, u8x8_msg_cb byte_cb, u8x8_msg_cb gpio_and_delay_cb)
{
  uint8_t tile_buf_height;
  uint8_t *buf;
  u8g2_SetupDisplay(u8g2, u8x8_d_sh1106_128x64_noname, u8x8_cad_001, byte_cb, gpio_and_delay_cb);
  buf = u8g2_m_16_8_f_pb(&tile_buf_height);
  u8g2_SetupBuffer(u8g2, buf, tile_buf_height, u8g2_ll_hvline_vertical_top_lsb, rotation);
}
