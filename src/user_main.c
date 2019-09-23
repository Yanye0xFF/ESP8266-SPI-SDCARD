#include "user_main.h"

const uint8_t *COMPANY[] = {"Panasonic", "Toshiba", "SanDisk", "Samsung",
						"AData", "Phison", "Lexar", "Silicon Power",
						"Kingston", "Transcend", "Patriot", "Sony"};

const uint8_t MID[] = {0x01, 0x02, 0x03, 0x1B, 0x1D, 0x27, 0x28, 0x31, 0x41, 0x74,
						0x76, 0x82};

const uint8_t *OEMID[] = {"PA", "TM", "SD", "SM", "AD", "PH",
							"BE", "SP" ,"42", "JE", "JT", "SO", "BE"};

const uint8_t *BRANDS[] = {"Panasonic", "Toshiba", "SanDisk", "ProGrade,Samsung",
							"AData", "AgfaPhoto,Delkin,PNY", "Lexar,PNY,ProGrade",
							"Silicon Power", "Kingston" ,"Transcend",
							"Gobe,Sony", "Anglebird(V60),Hoodman", "Anglebird(V90)"};

void array_cpy(uint8_t *src, uint8_t *target, uint32_t len);
void array_fill(uint8_t *target, uint8_t data, uint32_t len);
uint8_t bcd_to_dec(uint8_t bcd);

uint32_t str_len(const uint8_t *arg0);
uint8_t str_equals(const uint8_t *arg0, const uint8_t *arg1);

const uint8_t *mid_to_company(uint8_t mid);
const uint8_t *oemid_to_brand(uint8_t *oemid);


uint32 ICACHE_FLASH_ATTR user_rf_cal_sector_set(void) {
    enum flash_size_map size_map = system_get_flash_size_map();
    uint32 rf_cal_sec = 0;
    switch (size_map) {
        case FLASH_SIZE_4M_MAP_256_256:
            rf_cal_sec = 128 - 5;
            break;
        case FLASH_SIZE_8M_MAP_512_512:
            rf_cal_sec = 256 - 5;
            break;
        case FLASH_SIZE_16M_MAP_512_512:
        case FLASH_SIZE_16M_MAP_1024_1024:
            rf_cal_sec = 512 - 5;
            break;
        case FLASH_SIZE_32M_MAP_512_512:
        case FLASH_SIZE_32M_MAP_1024_1024:
            rf_cal_sec = 1024 - 5;
            break;
        case FLASH_SIZE_64M_MAP_1024_1024:
            rf_cal_sec = 2048 - 5;
            break;
        case FLASH_SIZE_128M_MAP_1024_1024:
            rf_cal_sec = 4096 - 5;
            break;
        default:
            rf_cal_sec = 0;
            break;
    }
    return rf_cal_sec;
}

void ICACHE_FLASH_ATTR user_rf_pre_init(void) {
}

void ICACHE_FLASH_ATTR user_init(void) {

	uint8_t state;
	uint8_t temp[7] = {0};
	uint8_t *buffer = NULL;

	uint32_t capacity;
    uint8_t c_size_mult, read_bl_len;
    uint16_t c_size;

	CID_DETAIL cid_info;

	uint32_t i = 0, j = 0;


	uart_init(BIT_RATE_115200, BIT_RATE_115200);
	os_printf("BIT_RATE_115200\n");

	//关闭wifi
	wifi_set_opmode(NULL_MODE);
	os_printf("NULL_MODE\n");

	sd_io_init();
	os_printf("SD_IO_INIT:OK\n");

	state = sd_init();
	os_printf("SD_INIT:%x\n",state);

	cid_info = sd_read_cid();

	if(cid_info.mid != 0x00) {
		os_printf("MID:0x%x\n", cid_info.mid);
		os_printf("COMPANY:%s\n", mid_to_company(cid_info.mid));

		array_cpy((uint8_t *)&cid_info.oid, temp, sizeof(cid_info.oid));
		temp[sizeof(cid_info.oid) + 1] = 0x00;
		os_printf("OID:%s\n",temp);

		os_printf("BRANDS:%s\n",oemid_to_brand(temp));

		array_fill(temp, 0x00, 7);
		array_cpy((uint8_t *)&cid_info.pnm, temp, 5);
		temp[6] = 0x00;
		os_printf("PNM:%s\n",temp);

		os_printf("PRV:%d%d\n",bcd_to_dec((cid_info.prv >> 4) & 0x0F), bcd_to_dec((cid_info.prv & 0x0F)));
		os_printf("PSN:%d\n",cid_info.psn);

		os_printf("MDT:%d%d-%d\n",bcd_to_dec((cid_info.mdt>>8 & 0x0F)),
			bcd_to_dec(cid_info.mdt >> 4 & 0x0F), bcd_to_dec(cid_info.mdt & 0x0F));
	}else {
		os_printf("read cid:failed!\n");
	}

	buffer = sd_read_csd();
	if(buffer != NULL) {
		os_printf("csd:\n");
		for(i = 0; i < 16; i++) {
			if((i != 0) && (i % 8 == 0)) {
				os_printf("\n");
			}
			if(*(buffer + i) < 0x10) {
				os_printf("0x0%x ", *(buffer + i));
			}else {
				os_printf("0x%x ", *(buffer + i));
			}
		}
		os_printf("\n");

		read_bl_len = (buffer[5] & 15);
		c_size_mult = ((buffer[10] & 128) >> 7) + ((buffer[9] & 3) << 1);
		c_size = (buffer[8] >> 6) + ((uint16_t)buffer[7] << 2) + ((uint16_t)(buffer[6] & 3) << 10);

		capacity = (c_size + 1)<<(c_size_mult + 2 + read_bl_len);

		os_printf("card size:%d\n",capacity);

	}else {
		os_printf("read csd:failed!\n");
	}
	os_free(buffer);
	buffer = NULL;

}

void array_cpy(uint8_t *src, uint8_t *target, uint32_t len) {
	uint32_t i;
    for(i = 0; i < len; i++) {
        *(target + i) =  *(src + i);
    }
}

void array_fill(uint8_t *target, uint8_t data, uint32_t len) {
	uint32_t i;
    for(i = 0; i < len; i++) {
        *(target + i) =  data;
    }
}

//decode by 5421
uint8_t bcd_to_dec(uint8_t bcd) {
    const uint8_t map[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x08, 0x09, 0x0A, 0x0B, 0x0C};
	uint8_t i = 9;
    while((map[i] != bcd) && i) {
        i--;
    }
    return i;
}

const uint8_t *mid_to_company(uint8_t mid) {
	uint32_t i;
    for(i = 0; i < 14; i++) {
        if(mid == MID[i]) return COMPANY[i];
    }
    return NULL;
}

const uint8_t *oemid_to_brand(uint8_t *oemid) {
	uint32_t i;
    for(i = 0; i < 14; i++) {
        if(str_equals(OEMID[i], oemid)) return BRANDS[i];
    }
    return NULL;
}


uint8_t str_equals(const uint8_t *arg0, const uint8_t *arg1) {
	uint32_t i, length;
    if((length = str_len(arg0)) != str_len(arg1)) {
        return 0;
    }
    for(i = 0; i < length; i++) {
        if(*(arg0 + i) != *(arg1 + i)) return 0;
    }
    return 1;
}

uint32_t str_len(const uint8_t *arg0) {
    uint32_t len = 0;
    while(*arg0++) len++;
    return len;
}
