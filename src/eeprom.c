/*
* eeprom.c
* created by: Brad Oraw
* created on: 2025-08-12
*/

#include "eeprom.h"
#include "definitions.h"
#include <string.h>
#include "spi_coordinator.h"

// 25LC1024 SPI Commands
#define EEPROM_CMD_READ    0x03
#define EEPROM_CMD_WRITE   0x02
#define EEPROM_CMD_WREN    0x06
#define EEPROM_CMD_WRDI    0x04
#define EEPROM_CMD_RDSR    0x05
#define EEPROM_CMD_WRSR    0x01
#define EEPROM_CMD_RDID    0xAB  // Fixed: was 0x9F, should be 0xAB
#define EEPROM_CMD_PE      0x42  // Page Erase
#define EEPROM_CMD_SE      0xD8  // Sector Erase
#define EEPROM_CMD_CE      0xC7  // Chip Erase
#define EEPROM_CMD_DPD     0xB9  // Deep Power-Down

static bool eeprom_initialized = false;

static void eeprom_cs_low(void) {
    spi_coordinator_cs_eeprom_low();
}

static void eeprom_cs_high(void) {
    spi_coordinator_cs_eeprom_high();
}

static uint8_t eeprom_spi_transfer(uint8_t data) {
    uint8_t received_data = 0;
    
    // Remove most debug prints, keep only errors
    // SYS_CONSOLE_PRINT("eeprom: SPI transfer 0x%02x\r\n", data);
    
    // Wait for transmit buffer to be empty
    int timeout = 10000;
    while((SPI2STAT & _SPI2STAT_SPITBE_MASK) == 0U && timeout > 0) {
        timeout--;
        for (volatile int i = 0; i < 10; i++);
    }
    
    if (timeout <= 0) {
        SYS_CONSOLE_PRINT("eeprom: TX buffer timeout!\r\n");
        return 0;
    }
    
    // Write data to SPI buffer
    SPI2BUF = data;
    
    // Wait for receive buffer to have data
    timeout = 10000;
    while((SPI2STAT & _SPI2STAT_SPIRBE_MASK) == _SPI2STAT_SPIRBE_MASK && timeout > 0) {
        timeout--;
        for (volatile int i = 0; i < 10; i++);
    }
    
    if (timeout <= 0) {
        SYS_CONSOLE_PRINT("eeprom: RX data timeout!\r\n");
        return 0;
    }
    
    // Read received data
    received_data = SPI2BUF;
    
    // Only print on errors or important events
    // SYS_CONSOLE_PRINT("eeprom: received 0x%02x\r\n", received_data);
    return received_data;
}

static bool eeprom_wait_for_write_complete(void) {
    uint8_t status;
    int timeout = 1000; // Add timeout
    
    // SYS_CONSOLE_PRINT("eeprom: checking write status\r\n");
    
    do {
        eeprom_cs_low();
        eeprom_spi_transfer(EEPROM_CMD_RDSR);
        status = eeprom_spi_transfer(0);
        eeprom_cs_high();
        
        // SYS_CONSOLE_PRINT("eeprom: status=0x%02x\r\n", status);
        
        timeout--;
        if (timeout <= 0) {
            SYS_CONSOLE_PRINT("eeprom: write timeout!\r\n");
            return false;
        }
        
        // Add a small delay
        for (volatile int i = 0; i < 1000; i++);
        
    } while (status & 0x01); // Check WIP bit
    
    // SYS_CONSOLE_PRINT("eeprom: write completed\r\n");
    return true;
}

bool eeprom_test_communication(void) {
    SYS_CONSOLE_PRINT("eeprom: testing communication\r\n");
    
    // First, let's just test if we can access SPI registers
    SYS_CONSOLE_PRINT("eeprom: checking SPI2STAT=0x%08lx\r\n", SPI2STAT);
    SYS_CONSOLE_PRINT("eeprom: checking SPI2CON=0x%08lx\r\n", SPI2CON);
    
    // Add a delay before starting
    for (volatile int i = 0; i < 10000; i++);
    
    // Test 1: Write Enable (single byte command - might return 0xFF)
    SYS_CONSOLE_PRINT("eeprom: test 1 - Write Enable\r\n");
    eeprom_cs_low();
    for (volatile int i = 0; i < 1000; i++);
    uint8_t wren_response = eeprom_spi_transfer(EEPROM_CMD_WREN);
    for (volatile int i = 0; i < 1000; i++);
    eeprom_cs_high();
    for (volatile int i = 0; i < 10000; i++);
    SYS_CONSOLE_PRINT("eeprom: WREN response=0x%02x\r\n", wren_response);
    
    // Test 2: Write a byte to address 0
    SYS_CONSOLE_PRINT("eeprom: test 2 - Write to addr 0\r\n");
    eeprom_cs_low();
    for (volatile int i = 0; i < 1000; i++);
    eeprom_spi_transfer(EEPROM_CMD_WRITE);  // Write command
    eeprom_spi_transfer(0x00);              // Address high
    eeprom_spi_transfer(0x00);              // Address mid
    eeprom_spi_transfer(0x00);              // Address low
    uint8_t write_response = eeprom_spi_transfer(0xAA); // Write data 0xAA
    for (volatile int i = 0; i < 1000; i++);
    eeprom_cs_high();
    for (volatile int i = 0; i < 50000; i++); // Wait for write to complete
    SYS_CONSOLE_PRINT("eeprom: WRITE response=0x%02x\r\n", write_response);
    
    // Test 3: Read back the data we just wrote
    SYS_CONSOLE_PRINT("eeprom: test 3 - Read back from addr 0\r\n");
    eeprom_cs_low();
    for (volatile int i = 0; i < 1000; i++);
    eeprom_spi_transfer(EEPROM_CMD_READ);   // Read command
    eeprom_spi_transfer(0x00);              // Address high
    eeprom_spi_transfer(0x00);              // Address mid
    eeprom_spi_transfer(0x00);              // Address low
    uint8_t read_data = eeprom_spi_transfer(0x00); // Read data
    for (volatile int i = 0; i < 1000; i++);
    eeprom_cs_high();
    for (volatile int i = 0; i < 10000; i++);
    SYS_CONSOLE_PRINT("eeprom: READ back data=0x%02x\r\n", read_data);
    
    // If write/read cycle works, the EEPROM is working!
    if (read_data == 0xAA) {
        SYS_CONSOLE_PRINT("eeprom: Write/Read test PASSED!\r\n");
        SYS_CONSOLE_PRINT("eeprom: EEPROM is working correctly!\r\n");
        return true;
    } else {
        SYS_CONSOLE_PRINT("eeprom: Write/Read test failed\r\n");
        return false;
    }
}

void eeprom_init(void) {
    SYS_CONSOLE_PRINT("eeprom: init\r\n");
    
    if (eeprom_initialized) {
        SYS_CONSOLE_PRINT("eeprom: already initialized\r\n");
        return;
    }
    
    // Initialize SPI coordinator if not already done
    spi_coordinator_init();
    
    eeprom_initialized = true;
    
    // Test basic communication (write/read cycle)
    // if (!eeprom_test_communication()) {
    //     SYS_CONSOLE_PRINT("eeprom: communication test failed!\r\n");
    //     eeprom_initialized = false;
    //     return;
    // }
    
    // Check if EEPROM is initialized with magic number
    uint32_t current_magic = eeprom_get_magic();
    if (current_magic != EEPROM_MAGIC_NUMBER) {
        // SYS_CONSOLE_PRINT("eeprom: initializing with magic number\r\n");
        if (!eeprom_set_magic(EEPROM_MAGIC_NUMBER)) {
            SYS_CONSOLE_PRINT("eeprom: failed to set magic number!\r\n");
            eeprom_initialized = false;
            return;
        }
        //SYS_CONSOLE_PRINT("eeprom: magic number set successfully\r\n");
    } else {
        //SYS_CONSOLE_PRINT("eeprom: magic number already valid\r\n");
    }
    
    // SYS_CONSOLE_PRINT("eeprom: init complete\r\n");
}

bool eeprom_write(uint32_t address, const uint8_t *data, uint16_t length) {
    if (!eeprom_initialized || address + length > EEPROM_SIZE) {
        SYS_CONSOLE_PRINT("eeprom: write failed - invalid params\r\n");
        return false;
    }
    
    // Acquire SPI2 for EEPROM use
    if (!spi_coordinator_acquire(SPI_DEVICE_EEPROM)) {
        SYS_CONSOLE_PRINT("eeprom: failed to acquire SPI2\r\n");
        return false;
    }
    
    // SYS_CONSOLE_PRINT("eeprom: write addr=0x%lx len=%d\r\n", address, length);
    
    // Write data page by page
    uint32_t current_addr = address;
    uint16_t remaining = length;
    const uint8_t *current_data = data;
    
    while (remaining > 0) {
        // Calculate bytes to write in this page
        uint16_t page_offset = current_addr % EEPROM_PAGE_SIZE;
        uint16_t bytes_in_page = EEPROM_PAGE_SIZE - page_offset;
        if (bytes_in_page > remaining) {
            bytes_in_page = remaining;
        }
        
        // SYS_CONSOLE_PRINT("eeprom: writing page at 0x%lx, %d bytes\r\n", current_addr, bytes_in_page);
        
        // Enable write
        eeprom_cs_low();
        // SYS_CONSOLE_PRINT("eeprom: CS low\r\n");
        
        eeprom_spi_transfer(EEPROM_CMD_WREN);
        // SYS_CONSOLE_PRINT("eeprom: WREN sent\r\n");
        eeprom_cs_high();
        
        // Write page
        eeprom_cs_low();
        // SYS_CONSOLE_PRINT("eeprom: CS low for write\r\n");
        
        eeprom_spi_transfer(EEPROM_CMD_WRITE);
        // SYS_CONSOLE_PRINT("eeprom: WRITE cmd sent\r\n");
        
        eeprom_spi_transfer((current_addr >> 16) & 0xFF);
        eeprom_spi_transfer((current_addr >> 8) & 0xFF);
        eeprom_spi_transfer(current_addr & 0xFF);
        // SYS_CONSOLE_PRINT("eeprom: address sent\r\n");
        
        for (int i = 0; i < bytes_in_page; i++) {
            eeprom_spi_transfer(current_data[i]);
        }
        // SYS_CONSOLE_PRINT("eeprom: data sent\r\n");
        
        eeprom_cs_high();
        // SYS_CONSOLE_PRINT("eeprom: CS high\r\n");
        
        // Wait for write to complete
        // SYS_CONSOLE_PRINT("eeprom: waiting for write complete\r\n");
        eeprom_wait_for_write_complete();
        // SYS_CONSOLE_PRINT("eeprom: write complete\r\n");
        
        current_addr += bytes_in_page;
        current_data += bytes_in_page;
        remaining -= bytes_in_page;
    }
    
    // SYS_CONSOLE_PRINT("eeprom: write finished successfully\r\n");
    // Release SPI2 when done
    spi_coordinator_release();
    return true;
}

bool eeprom_read(uint32_t address, uint8_t *data, uint16_t length) {
    if (!eeprom_initialized || address + length > EEPROM_SIZE) {
        SYS_CONSOLE_PRINT("eeprom: read failed - invalid params\r\n");
        return false;
    }
    
    // Acquire SPI2 for EEPROM use
    if (!spi_coordinator_acquire(SPI_DEVICE_EEPROM)) {
        SYS_CONSOLE_PRINT("eeprom: failed to acquire SPI2\r\n");
        return false;
    }
    
    // SYS_CONSOLE_PRINT("eeprom: read addr=0x%lx len=%d\r\n", address, length);
    
    eeprom_cs_low();
    eeprom_spi_transfer(EEPROM_CMD_READ);
    eeprom_spi_transfer((address >> 16) & 0xFF);
    eeprom_spi_transfer((address >> 8) & 0xFF);
    eeprom_spi_transfer(address & 0xFF);
    
    for (int i = 0; i < length; i++) {
        data[i] = eeprom_spi_transfer(0);
    }
    eeprom_cs_high();
    
    // Release SPI2 when done
    spi_coordinator_release();
    return true;
}

bool eeprom_erase_page(uint32_t address) {
    if (!eeprom_initialized || address >= EEPROM_SIZE) {
        return false;
    }
    
    // Erase page by writing 0xFF to all bytes
    uint8_t erase_buffer[EEPROM_PAGE_SIZE];
    memset(erase_buffer, 0xFF, EEPROM_PAGE_SIZE);
    
    return eeprom_write(address, erase_buffer, EEPROM_PAGE_SIZE);
}

void eeprom_deinit(void) {
    SYS_CONSOLE_PRINT("eeprom: deinit\r\n");
    eeprom_initialized = false;
}

bool eeprom_is_initialized(void) {
    return eeprom_initialized;
}

uint32_t eeprom_get_magic(void) {
    uint32_t magic = 0;
    // SYS_CONSOLE_PRINT("eeprom: reading magic number\r\n");
    
    if (!eeprom_read(EEPROM_MAGIC_OFFSET, (uint8_t*)&magic, sizeof(magic))) {
        SYS_CONSOLE_PRINT("eeprom: magic read failed\r\n");
        return 0;
    }
    
    // SYS_CONSOLE_PRINT("eeprom: magic=0x%08lx\r\n", magic);
    return magic;
}

bool eeprom_set_magic(uint32_t magic) {
    SYS_CONSOLE_PRINT("eeprom: setting magic number 0x%08lx\r\n", magic);
    
    bool result = eeprom_write(EEPROM_MAGIC_OFFSET, (uint8_t*)&magic, sizeof(magic));
    
    if (result) {
        SYS_CONSOLE_PRINT("eeprom: magic write successful\r\n");
    } else {
        SYS_CONSOLE_PRINT("eeprom: magic write failed!\r\n");
    }
    
    return result;
}