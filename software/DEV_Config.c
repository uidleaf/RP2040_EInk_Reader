#include "DEV_Config.h"

// ============================================================
// Send one byte over SPI to the EPD.
// ============================================================
void DEV_SPI_WriteByte(uint8_t value)
{
    spi_write_blocking(EPD_SPI_PORT, &value, 1);
}

// ============================================================
// 初始化ialise GPIOs and SPI for the 2.9" E-Ink panel.
// RP2040 SPI0 runs at ~2 MHz (125 MHz / 64) — safe for the
// panel and close to the original 1.5 MHz reference speed.
// ============================================================
uint8_t DEV_Module_Init(void)
{
    // --- SPI0: SCK=GP2, MOSI=GP3, Mode 0, MSB first ---
    spi_init(EPD_SPI_PORT, 2000000);               // 2 MHz
    gpio_set_function(EPD_SCK_PIN,  GPIO_FUNC_SPI);
    gpio_set_function(EPD_MOSI_PIN, GPIO_FUNC_SPI);

    // --- Control pins: CS, DC, RST (push-pull outputs) ---
    gpio_init(EPD_CS_PIN);
    gpio_init(EPD_DC_PIN);
    gpio_init(EPD_RST_PIN);
    gpio_set_dir(EPD_CS_PIN, GPIO_OUT);
    gpio_set_dir(EPD_DC_PIN, GPIO_OUT);
    gpio_set_dir(EPD_RST_PIN, GPIO_OUT);

    // --- BUSY pin (input with pull-up) ---
    gpio_init(EPD_BUSY_PIN);
    gpio_set_dir(EPD_BUSY_PIN, GPIO_IN);
    gpio_pull_up(EPD_BUSY_PIN);

    // --- Safe initial states ---
    DEV_Digital_Write(EPD_CS_PIN,  1);
    DEV_Digital_Write(EPD_DC_PIN,  1);
    DEV_Digital_Write(EPD_RST_PIN, 1);
    DEV_Delay_ms(100);

    return 0;
}

// ============================================================
// De-initialise module (pins low, SPI disabled).
// ============================================================
void DEV_Module_Exit(void)
{
    DEV_Digital_Write(EPD_CS_PIN,  0);
    DEV_Digital_Write(EPD_DC_PIN,  0);
    DEV_Digital_Write(EPD_RST_PIN, 0);

    spi_deinit(EPD_SPI_PORT);
}
