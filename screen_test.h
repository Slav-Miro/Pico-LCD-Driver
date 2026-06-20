#pragma once
#include <stdio.h>
#include <stdint.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/dma.h"

typedef struct {
    uint8_t command;    // Command bytes
    uint8_t data[16];   // 16 Parameter bytes
    uint8_t databytes;  // Number of parameters: (0x80 = delay) (0xFF = end of command stream)
}screen_lcd_init_t;

// Declarations for pins
/*
    - SCLK: Serial Clock
    - MOSI: Master Out Slave In
    - CS:   Chip Select
    - DC:   Data/Command
    - RST: Reset
*/
#define SPI_PORT    spi0
#define SCLK        2
#define MOSI        3
#define CS          5
#define DC          6
#define RST         7

// Declarations for command codes
#define SWRESET     0x01
#define SLEEP_IN    0x10
#define SLEEP_OUT   0x11
#define MEM_ACCSS   0x36
#define COLOR_MOD   0x3A
#define TABLE2_P2   0x96
#define TABLE2_P1   0xC3
#define COMMAND_CTL 0xF0

// Baudrate definition
#define BAUD        40*1000*1000

// Display dimensions
#define WIDTH      480 
#define HEIGHT     320

// Function definitions
/***
 * @brief Handles the command part of the DC pin where a 0 registers the incoming byte as a command and a 1 as the data to display.
 * @param cmd An unsigned 8 bit integer to represent the command to send to the LCD.
 */
void lcd_send_command(uint8_t cmd);

/***
 * @brief Send dynamic data to the SPI interface
 * @param data The array of data to send to the SPI interface
 * @param len The size of the array data
 */
void lcd_send_data(uint8_t * data, size_t len);

/***
 * @brief Initialize the LCD screen
 */
void lcd_init();

/***
 * @brief Sets the screen to a solid color
 * @param color A 16 bit value to set the color of the screen
 */
void lcd_fill(uint16_t);

/***
 * @brief Sets the screen to a solid color
 * @param color A 16 bit value to set the color of the screen
 */
void lcd_fill(uint16_t);

/***
 * @brief Sets the screen to a solid color through DMA
 * @param color A 16 bit value to set the color of the screen
 */
void lcd_fill_dma(uint16_t);