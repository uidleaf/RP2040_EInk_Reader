#include "hw_config.h"

// Hardware Configuration of the SD Card "objects"
static sd_sdio_if_t sdio_if = {
    // CLK is 10
    // CMD is 18
    // D0 is 19
    // D1 is 20
    // D2 is 21
    // D3 is 22
    .CMD_gpio = 18,
    .D0_gpio = 19,
    .SDIO_PIO = pio0,
    .DMA_IRQ_num = DMA_IRQ_0,
    .baud_rate = 125 * 1000 * 1000 / 10  // 12.5 MHz (safer for jumper wires/high capacity)
};

static sd_card_t sd_card = {
    .type = SD_IF_SDIO, 
    .sdio_if_p = &sdio_if,
    .use_card_detect = false
};

size_t sd_get_num() { 
    return 1; 
}

sd_card_t* sd_get_by_num(size_t num) {
    if (0 == num) {
        return &sd_card;
    } else {
        return NULL;
    }
}
