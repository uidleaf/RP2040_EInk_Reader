#ifndef _DEV_CONFIG_H_
#define _DEV_CONFIG_H_

#include "pico/stdlib.h"
#include "hardware/spi.h"
#include <stdint.h>

// ============================================================
// RP2040 Pin Mapping for 2.9" E-Ink Display
// ============================================================
//  RP2040          E-Ink Panel
//  GP2  (SPI0 SCK) ──── PA5  SCK
//  GP3  (SPI0 TX)  ──── PA7  MOSI
//  GP4             ──── PA4  CS
//  GP5             ──── PA3  DC
//  GP6             ──── PA2  RST
//  GP7             ──── PA1  BUSY
// ============================================================

#define EPD_SPI_PORT    spi0

#define EPD_SCK_PIN     2
#define EPD_MOSI_PIN    3
#define EPD_CS_PIN      4
#define EPD_DC_PIN      5
#define EPD_RST_PIN     6
#define EPD_BUSY_PIN    7

// Button pin
#define BUTTON_PIN      0

// Convenience macros matching original API style
#define DEV_Digital_Write(_pin, _value) \
    gpio_put(_pin, (_value) ? 1 : 0)

#define DEV_Digital_Read(_pin) \
    gpio_get(_pin)

#define DEV_Delay_ms(__xms) sleep_ms(__xms)

uint8_t DEV_Module_Init(void);
void    DEV_SPI_WriteByte(uint8_t value);
void    DEV_Module_Exit(void);

#endif
