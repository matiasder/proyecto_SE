// ==========================
// SSD1306_MINI.h
// ==========================

#ifndef SSD1306_MINI_H
#define SSD1306_MINI_H

#include "driver/i2c.h"

#define OLED_ADDR 0x3C

void oled_init(void);
void oled_clear(void);
void oled_set_cursor(uint8_t page, uint8_t col);
void oled_write_char(char c);
void oled_write_string(const char *str);

void oled_print(uint8_t page,
                uint8_t col,
                const char *str);

void oled_print_float(uint8_t page,
                      uint8_t col,
                      float value,
                      uint8_t decimals);

#endif