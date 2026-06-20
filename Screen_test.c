#include "screen_test.h"
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/dma.h"

// Global variable for DMA identifier
static int DMA_id;

int main()
{
    stdio_init_all();

    // Start up SPI for the screen
    spi_init(SPI_PORT, BAUD);
    gpio_set_function(SCLK, GPIO_FUNC_SPI);
    gpio_set_function(MOSI, GPIO_FUNC_SPI);

    // Set up GPIO
    gpio_init(CS);
    gpio_init(DC);
    gpio_init(RST);
    gpio_set_dir(CS, GPIO_OUT);
    gpio_set_dir(DC, GPIO_OUT);
    gpio_set_dir(RST, GPIO_OUT);

    // Put the LCD into a known state before doing work
    gpio_put(RST, 1);
    gpio_put(RST, 0);
    sleep_ms(10);
    gpio_put(RST, 1);
    sleep_ms(120);

    lcd_init();

    lcd_fill(0xFF00);

    // Initialize DMA
    lcd_DMA_init();

    uint16_t color = 0x0000;

    while (true){
        lcd_DMA_fill(color);
        sleep_ms(10);
        color = (color + 10 < color)? 0x0000 : color + 10;
    }
}

void lcd_send_command(uint8_t cmd){
    // Set up DC and CS
    gpio_put(CS, 0);
    gpio_put(DC, 0);

    spi_write_blocking(SPI_PORT, &cmd, sizeof(cmd));

    gpio_put(CS, 1);
}

void lcd_send_data(uint8_t * data, size_t size){
    // Set up DC and CS
    gpio_put(CS, 0);
    gpio_put(DC, 1);

    // Write the data
    spi_write_blocking(SPI_PORT, data, size);

    // Indicate that the screen is done
    gpio_put(CS, 1);
}

void lcd_init(){
    // Table of commands to send to the board
    screen_lcd_init_t init_cmds[] = {
		{0xCF, {0x00, 0x83, 0X30}, 3},
		{0xED, {0x64, 0x03, 0X12, 0X81}, 4},
		{0xE8, {0x85, 0x01, 0x79}, 3},
		{0xCB, {0x39, 0x2C, 0x00, 0x34, 0x02}, 5},
		{0xF7, {0x20}, 1},
		{0xEA, {0x00, 0x00}, 2},
		{0xC0, {0x26}, 1},		 /*Power control*/
		{0xC1, {0x11}, 1},		 /*Power control */
		{0xC5, {0x35, 0x3E}, 2}, /*VCOM control*/
		{0xC7, {0xBE}, 1},		 /*VCOM control*/
		{0x36, {0x28}, 1},		 /*Memory Access Control*/
		{0x3A, {0x05}, 1},		 /*Pixel Format Set*/
		{0xB1, {0x00, 0x1B}, 2},
		{0xF2, {0x08}, 1},
		{0x26, {0x01}, 1},
		{0xE0, {0x1F, 0x1A, 0x18, 0x0A, 0x0F, 0x06, 0x45, 0X87, 0x32, 0x0A, 0x07, 0x02, 0x07, 0x05, 0x00}, 15},
		{0XE1, {0x00, 0x25, 0x27, 0x05, 0x10, 0x09, 0x3A, 0x78, 0x4D, 0x05, 0x18, 0x0D, 0x38, 0x3A, 0x1F}, 15},
		{0x2A, {0x00, 0x00, 0x00, 0xEF}, 4},
		{0x2B, {0x00, 0x00, 0x01, 0x3f}, 4},
		{0x2C, {0}, 0},
		{0xB7, {0x07}, 1},
		{0xB6, {0x0A, 0x82, 0x27, 0x00}, 4},
		{0x11, {0}, 0x80},
		{0x29, {0}, 0x80},
		{0, {0}, 0xff},
	};

    uint16_t i = 0;
    while (init_cmds[i].databytes != 0xFF){
        lcd_send_command(init_cmds[i].command);
        lcd_send_data(init_cmds[i].data, init_cmds[i].databytes & 0x1F);
        if (init_cmds[i].databytes & 0x80){
            sleep_ms(100);
        }

        i++;
    }

    lcd_send_command(0x36);
    lcd_send_data((uint8_t[]){0x28}, 1);

    lcd_send_command(0x21);
}

void lcd_fill(uint16_t color){
    lcd_send_command(0x2A);
    lcd_send_data((uint8_t[]){0x00, 0x00, ((WIDTH-1) >> 8) & 0xFF, (WIDTH-1) & 0xFF}, 4);
    lcd_send_command(0x2B);
    lcd_send_data((uint8_t[]){0x00, 0x00, ((HEIGHT-1) >> 8) & 0xFF, (HEIGHT-1) & 0xFF}, 4);
    lcd_send_command(0x2C);
    
    uint8_t hi = (color >> 8) & 0xFF;
    uint8_t lo = color & 0xFF;
    
    gpio_put(CS, 0);
    gpio_put(DC, 1);
    for (int i = 0; i < WIDTH * HEIGHT; i++){
        spi_write_blocking(SPI_PORT, &hi, 1);
        spi_write_blocking(SPI_PORT, &lo, 1);
    }
    gpio_put(CS, 1);
}

void lcd_DMA_fill(uint16_t color){
    // Select column
    lcd_send_command(0x2A);
    lcd_send_data((uint8_t[]){0x00, 0x00, ((WIDTH-1) >> 8) & 0xFF, (WIDTH-1) & 0xFF}, 4);
    
    // Select row
    lcd_send_command(0x2B);
    lcd_send_data((uint8_t[]){0x00, 0x00, ((HEIGHT-1) >> 8) & 0xFF, (HEIGHT-1) & 0xFF}, 4);
    
    // Get in a state thats ready to accept data
    lcd_send_command(0x2C);
    
    // Tell SPI to expect data
    gpio_put(CS, 0);
    gpio_put(DC, 1);

    // Go through and fire off every row
    for (int i = 0 ; i < HEIGHT; i++){
        lcd_dma_fill_row(color);
    }

    gpio_put(CS, 1);
}

void lcd_dma_fill_row(uint16_t color){
    uint16_t DMA_BUFFER[WIDTH];

    uint16_t swapped_endian_color = endian_swap(color);

    for (int i = 0 ; i < WIDTH; i ++){
        DMA_BUFFER[i] = swapped_endian_color;
    }

    // Fire off from DMA
    dma_channel_transfer_from_buffer_now(DMA_id, DMA_BUFFER, dma_encode_transfer_count(2 * WIDTH));
    dma_channel_wait_for_finish_blocking(DMA_id);
}

void lcd_DMA_init(void){
    DMA_id = dma_claim_unused_channel(true);

    // Configure the DMA channel
    dma_channel_config_t config_dma = dma_channel_get_default_config(DMA_id);

    // Update the config and ensure certain default behaviours
    channel_config_set_transfer_data_size(&config_dma, DMA_SIZE_8);
    channel_config_set_write_increment(&config_dma, false);
    channel_config_set_dreq(&config_dma, spi_get_dreq(SPI_PORT, true));

    // Cant believe you forgot this
    dma_channel_set_write_addr(DMA_id, &spi_get_hw(SPI_PORT)->dr, false);

    // Update the DMA channel to use specified config settings
    dma_channel_set_config(DMA_id, &config_dma, false);
}

uint16_t endian_swap(uint16_t num){
    // Split into upper and lower btyes
    uint16_t upper = 0xFF00 & num;
    uint16_t lower = 0x00FF & num;

    upper = upper >> 8;
    lower = lower << 8;

    return upper | lower;
}