#ifndef __SDCARD_H__
#define __SDCARD_H__

/*
 * cs    GPIO05  D1
 * mosi  GPIO04  D2
 * sck   GPIO00  D3
 * miso  GPIO02  D4
 * */

#include "c_types.h"
#include "eagle_soc.h"
#include "gpio.h"
#include "osapi.h"
#include "mem.h"

#define CS 5
#define MOSI 4
#define SCLK 0
#define MISO 2

typedef struct cid_detail {
	uint8_t mid;
	uint16_t oid;
	uint64_t pnm;
	uint8_t prv;
	uint32_t psn;
	uint16_t mdt;
	uint8_t crc;
} CID_DETAIL;

void sd_io_init();
uint8_t sd_init();

CID_DETAIL sd_read_cid();
uint8_t* sd_read_csd();

uint64_t sd_capacity();

uint8_t* sd_read_sector(uint32_t sector_addr);
uint8_t* sd_read_sector_ex(uint8_t *buffer, uint32_t sector_addr);

uint8_t sd_write_sector(const uint8_t* buffer, uint32_t sector_addr);

uint8_t* sd_read_multisector(uint32_t sector_addr, uint32_t length);
uint8_t* sd_read_multisector_ex(uint8_t *buffer, uint32_t sector_addr, uint32_t length);

uint8_t sd_write_multisector(const uint8_t* buffer, uint32_t sector_addr, uint32_t length);

uint8_t sd_fill_multisector(uint8_t data, uint32_t sector_addr, uint32_t length);

#endif
