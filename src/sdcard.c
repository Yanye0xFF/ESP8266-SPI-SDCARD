#include "sdcard.h"

void sd_io_init() {
	//CS
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO5_U, FUNC_GPIO5);
	PIN_PULLUP_EN(PERIPHS_IO_MUX_GPIO5_U);
	//SCLK
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO4_U, FUNC_GPIO4);
	PIN_PULLUP_EN(PERIPHS_IO_MUX_GPIO4_U);
	//MOSI
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO0_U, FUNC_GPIO0);
	PIN_PULLUP_EN(PERIPHS_IO_MUX_GPIO0_U);
	//MISO输入模式,不需要上拉
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO2_U, FUNC_GPIO2);
	PIN_PULLUP_DIS(PERIPHS_IO_MUX_GPIO5_U);
	GPIO_DIS_OUTPUT(GPIO_ID_PIN(2));
}

/**
 * 写字节,MSB-->LSB
 * @param data 写入数据
 * */
void sd_write_byte(uint8_t  data) {
	sint8_t i;
	uint8_t tmp;
	for(i = 7; i >= 0; i--) {
		tmp = (data >> i) & 0x1;
		GPIO_OUTPUT_SET(MOSI, tmp);
		GPIO_OUTPUT_SET(SCLK, 0);
		GPIO_OUTPUT_SET(SCLK, 1);
	}
	GPIO_OUTPUT_SET(MOSI, 1);
}

/**
 * 读字节,LSB-->MSB
 * @return 读出的字节数据
 * */
uint8_t sd_read_byte() {
	uint8_t data = 0x00, i;
	for(i = 8; i; i--) {
		GPIO_OUTPUT_SET(SCLK, 0);
		GPIO_OUTPUT_SET(SCLK, 1);
		data <<= 1;
		if(GPIO_INPUT_GET(MISO)){
			data |= 0x1;
		}
	}
	GPIO_OUTPUT_SET(SCLK, 0);
	return data;
}

/**
 * 写命令,参数小端
 * @param command 命令类型
 * @param argument 参数
 * @param crc 校验
 * */
void sd_write_cmd(uint8_t command, uint32_t argument, uint8_t crc) {
	sd_write_byte(command | 0x40);
	sd_write_byte(argument & 0xFF);
	sd_write_byte((argument >> 8) & 0xFF);
	sd_write_byte((argument >> 16) & 0xFF);
	sd_write_byte((argument >> 24) & 0xFF);
	sd_write_byte(crc);
}

/**
 * 写命令,参数大端
 * */
void sd_write_cmd_ex(uint8_t command, uint32_t argument, uint8_t crc) {
	sd_write_byte(command | 0x40);
	sd_write_byte((argument >> 24) & 0xFF);
	sd_write_byte((argument >> 16) & 0xFF);
	sd_write_byte((argument >> 8) & 0xFF);
	sd_write_byte(argument & 0xFF);
	sd_write_byte(crc);
}

/**
 * 读命令响应
 * @return 响应码
 * */
uint8_t sd_response() {
	//额外8个SCLK
	sd_write_byte(0xFF);
	return sd_read_byte();
}

/**
 * 读数据包
 * @param *buffer 读取数据存储区
 * @param length 数据长度
 * @return 成功true, 失败false
 * */
uint8_t sd_read_package(uint8_t *buffer, uint32_t length) {
	int32_t count = 0;
	uint8_t response;
	//等待SD卡发回数据起始令牌0xFE
	do {
		response = sd_read_byte();
		count++;
		if(count > 1024) {
			return false;
		}
	}while(response != 0xFE);
	//开始接收数据
	for(count = 0; count < length; count++) {
		*buffer = sd_read_byte();
		buffer++;
	}
	sd_write_byte(0xFF);
	sd_write_byte(0xFF);
	return true;
}

/**
 * 释放SD卡
 * */
void sd_release() {
	GPIO_OUTPUT_SET(CS, 1);
	//额外8个SCLK
	sd_write_byte(0xFF);
}

/**
 * 片选SD卡
 * */
void sd_select() {
	GPIO_OUTPUT_SET(CS, 0);
}

/**
 * sd卡初始化,完成后sd卡进入数据传输状态
 * @return 0xFF SD V1.0初始化成功
 * */
uint8_t sd_init() {
	uint8_t i, response;
	int32_t count = 0;
	uint8_t state = 0xFF;

	GPIO_OUTPUT_SET(CS, 1);
	//发送至少74个SCLK
	for (i = 0; i < 10; i++) {
		sd_write_byte(0xFF);
	}

	GPIO_OUTPUT_SET(CS, 0);

	//CMD0
	do {
		sd_write_cmd_ex(0x00, 0, 0x95);
		response = sd_response();
		count++;
		if(count >= 1024){
			sd_release();
			return 0x00;
		}
	}while(response != 0x01);

	//CMD8
	sd_write_cmd(0x08, 0x1AA, 0x87);
	response = sd_response();
	if(response != 0x01) {
		//SD V1.0
		sd_write_cmd_ex(55, 0, 0XFF);//发送CMD55
		//补偿16SCLK,8SCLK等待,8SCLK输出,正常值应(0x01)
		sd_response();
		sd_write_cmd_ex(41, 0, 0XFF);//发送CMD41
		//正常值应(0x01)
		response = sd_response();
		if(response == 0x01) {
			//等待退出IDLE模式
			do {
				sd_write_cmd_ex(55, 0, 0XFF);//发送CMD55
				sd_response();
				sd_write_cmd_ex(41, 0, 0XFF);//发送CMD41
				response = sd_response();
				count++;
				if(count >= 1024) {
					sd_release();
					return 0x41;
				}
			}while(response != 0x00);
			//额外8个SCLK
			sd_write_byte(0xFF);
			//发送CMD16
			sd_write_cmd_ex(16, 512, 0XFF);
			sd_response();
		}else {
			//TODO MMC V3
			sd_release();
			return 0xFD;
		}
	}else {
		//TODO SD V2.0
		sd_release();
		return 0xFE;
	}
	sd_release();
	return state;
}

/**
 * 读CID数据
 * @return CID_DETAIL结构
 * */
CID_DETAIL sd_read_cid() {
	uint8_t response;
	uint8_t *buffer = NULL;
	CID_DETAIL cid_info;

	sd_select();

	sd_write_cmd(10, 0, 0x01);
	response = sd_response();

	if(response == 0x00) {
		buffer = (uint8_t *)os_malloc(sizeof(uint8_t) * 16);
		//接收16个字节的数据
		if(sd_read_package(buffer, 16)) {
			//0x1b 0x53 0x4d 0x30 0x30 0x30 0x30 0x30 0x10 0x2 0x4d 0x9f 0x1a 0x0 0xc3 0x2b
			cid_info.mid = *(buffer + 0);
			//oid ascii little endia
			cid_info.oid = *(buffer + 2) << 8;
			cid_info.oid |= *(buffer + 1);
			//pnm ascii little endia
			cid_info.pnm = *(buffer + 7) << 24;
			cid_info.pnm |= *(buffer + 6) << 16;
			cid_info.pnm |= *(buffer + 5) << 8;
			cid_info.pnm |= *(buffer + 4);
			cid_info.pnm <<= 8;
			cid_info.pnm |= *(buffer + 3);
			//product reversion
			cid_info.prv = *(buffer + 8);
			//psn 32bit unsigned int
			cid_info.psn = *(buffer + 9) << 24;
			cid_info.psn |= *(buffer + 10) << 16;
			cid_info.psn |= *(buffer + 11) << 8;
			cid_info.psn |= *(buffer + 12);
			//忽略13字节高四位保留位
			cid_info.mdt = (*(buffer + 13) & 0x0F) << 8;
			cid_info.mdt |= *(buffer + 14);
			//CRC与最后一保留位放在一起
			cid_info.crc = *(buffer + 15);
		}
		os_free(buffer);
		buffer = NULL;
	}
	sd_release();
	return cid_info;
}

/**
 * 读CSD数据
 * @return CSD数据指针,使用完成务必释放内存
 * */
uint8_t* sd_read_csd() {
	uint8_t response;
	uint8_t *buffer = NULL;

	sd_select();

	sd_write_cmd(9, 0, 0x01);
	response = sd_response();
	if(response == 0x00) {
		buffer = (uint8_t *)os_malloc(sizeof(uint8_t) * 16);
		//0x00 0x2f 0x00 0x32 0x5b 0x5a 0x83 0xba 0x6d 0xb7 0xff 0xbf 0x16 0x80 0x00 0xf1
		//接收16个字节的数据
		if(!sd_read_package(buffer, 16)) {
			os_free(buffer);
			buffer = NULL;
		}
	}
	sd_release();
	return buffer;
}

/**
 * 计算SD卡容量
 * @return 卡容量(字节)
 * */
uint64_t sd_capacity() {
	uint64_t capacity = 0;
	uint8_t c_size_mult, read_bl_len;
	uint16_t c_size;
	uint8_t *buffer = NULL;

	buffer = sd_read_csd();

	if(buffer != NULL) {
		read_bl_len = (buffer[5] & 15);

		c_size_mult = ((buffer[10] & 128) >> 7) + ((buffer[9] & 3) << 1);
		c_size = (buffer[8] >> 6) + ((uint16_t)buffer[7] << 2) + ((uint16_t)(buffer[6] & 3) << 10);

		capacity = (c_size + 1)<<(c_size_mult + 2 + read_bl_len);
	}
	os_free(buffer);
	buffer = NULL;
	return capacity;
}

/**
 * SD卡读扇区(512字节)
 * @param sector_addr 扇区字节地址,如使用sector_id,需要sector_id << 9
 * @return 扇区数据指针,使用完成务必释放内存
 * */
uint8_t* sd_read_sector(uint32_t sector_addr) {
	uint8_t response;
	uint8_t *buffer = NULL;

	sd_select();
	//读命令
	sd_write_cmd_ex(17, sector_addr, 0x01);
	response = sd_response();

	if(response == 0x00) {
		buffer = (uint8_t *)os_malloc(sizeof(uint8_t) * 512);
		//接收512字节的数据
		response = sd_read_package(buffer, 512);
		if(!response) {
			os_free(buffer);
			buffer = NULL;
		}
	}
	sd_release();
	return buffer;
}

/**
 * SD卡读扇区(512字节)
 * @param *buffer 存储数据缓冲区指针
 * @param sector_addr 扇区字节地址,如使用sector_id,需要sector_id << 9
 * @return 存储数据缓冲区指针
 * */
uint8_t* sd_read_sector_ex(uint8_t *buffer, uint32_t sector_addr) {
	uint8_t response;

	sd_select();
	//读命令
	sd_write_cmd_ex(17, sector_addr, 0x01);
	response = sd_response();

	if(response == 0x00) {
		//接收512字节的数据
		response = sd_read_package(buffer, 512);
		if(!response) {
			buffer = NULL;
		}
	}
	sd_release();
	return buffer;
}

/**
 * SD卡写扇区(512字节)
 * @param *buffer 存储数据缓冲区指针
 * @param sector_addr 扇区字节地址,如使用sector_id,需要sector_id << 9
 * @return true 写入成功, false 写入失败
 * */
uint8_t sd_write_sector(const uint8_t* buffer, uint32_t sector_addr) {
	uint8_t response, state = true;
	uint32_t i = 0, count = 0;

	sd_select();
	//写命令
	do {
		sd_write_cmd_ex(24, sector_addr, 0x00);
		response = sd_response();
		count++;
		if(count > 200) {
			break;
		}
	}while(response != 0x00);

	count = 0;
	if(response == 0x00) {
		//发24个SCLK，等待SD卡准备好
		sd_write_byte(0xFF);
		sd_write_byte(0xFF);
		sd_write_byte(0xFF);
		//发起始令牌0xFE
		sd_write_byte(0xFE);
		//发一个sector数据(512字节)
		for(i = 0; i < 512; i++){
			sd_write_byte(*(buffer + i));
		}
		sd_write_byte(0xFF);
		sd_write_byte(0xFF);
		//接收响应
		response = sd_read_byte();
		if((response & 0x1F) != 0x05) {
			sd_release();
			return false;
		}
		//发送SCLK等待sd卡固化数据
		do {
			response = sd_read_byte();
			count++;
			if(count > 65535) {
				state = false;
				break;
			}
		}while(response != 0xFF);

		sd_release();
		return state;
	}
	sd_release();
	return false;
}

/**
 * SD卡读多扇区(512字节 * length)
 * @param sector_addr 扇区字节地址,如使用sector_id,需要sector_id << 9
 * @param length 扇区数量
 * @return 扇区数据指针,使用完成务必释放内存
 * */
uint8_t* sd_read_multisector(uint32_t sector_addr, uint32_t length) {
	uint8_t response;
	uint8_t *buffer = NULL;

	if(length == 1) {
		return sd_read_sector(sector_addr);
	}

	sd_select();
	//连续读命令
	sd_write_cmd(18, sector_addr, 0x01);
	response = sd_response();

	if(response == 0x00) {
		buffer = (uint8_t *)os_malloc(sizeof(uint8_t) * 512 * length);
		do {
			//接收512字节的数据
			response = sd_read_package(buffer, 512);
			length--;
			buffer += 512;
		}while(response && length);
		//发送停止命令
		sd_write_cmd_ex(12, 0, 0X01);
		sd_write_byte(0xFF);

		if(length > 0) {
			os_free(buffer);
			buffer = NULL;
		}
	}
	sd_release();
	return buffer;
}

/**
 * SD卡读多扇区(512字节 * length)
 * @param *buffer 存储数据缓冲区指针
 * @param sector_addr 扇区字节地址,如使用sector_id,需要sector_id << 9
 * @param length 扇区数量
 * @return 扇区数据指针
 * */
uint8_t* sd_read_multisector_ex(uint8_t *buffer, uint32_t sector_addr, uint32_t length) {
	uint8_t response;

	if(length == 1) {
		return sd_read_sector_ex(buffer, sector_addr);
	}

	sd_select();
	//连续读命令
	sd_write_cmd_ex(18, sector_addr, 0x01);
	response = sd_response();

	if(response == 0x00) {
		do {
			//接收512字节的数据
			response = sd_read_package(buffer, 512);
			length--;
			buffer += 512;
		}while(response && length);
		//发送停止命令
		sd_write_cmd_ex(12, 0, 0X01);
		sd_write_byte(0xFF);

		if(length > 0) {
			buffer = NULL;
		}
	}
	sd_release();
	return buffer;
}

/**
 * SD卡写多扇区(512字节 * length)
 * @param *buffer 存储数据缓冲区指针
 * @param sector_addr 扇区字节地址,如使用sector_id,需要sector_id << 9
 * @param length 扇区数量
 * @return true 写入成功, false 写入失败
 * */
uint8_t sd_write_multisector(const uint8_t* buffer, uint32_t sector_addr, uint32_t length) {
	uint8_t response, state = true;
	uint32_t i = 0, count = 0;
	if(length == 1) {
		return sd_write_sector(buffer, sector_addr);
	}

	sd_select();
	//连续写命令
	sd_write_cmd_ex(25, sector_addr, 0x00);
	response = sd_response();

	if(response == 0x00) {
		//发24个SCLK，等待SD卡准备好
		sd_write_byte(0xFF);
		sd_write_byte(0xFF);
		sd_write_byte(0xFF);
		do {
			//发起始令牌0xFC
			sd_write_byte(0xFC);
			//发一个sector数据(512字节)
			for(i = 0; i < 512; i++){
				sd_write_byte(*buffer++);
			}
			sd_write_byte(0xFF);
			sd_write_byte(0xFF);
			//接收响应
			response = sd_read_byte();
			if((response & 0x1F) != 0x05) {
				sd_release();
				return false;
			}
			//发送SCLK等待sd卡固化数据
			do {
				response = sd_read_byte();
				count++;
				if(count > 65535) {
					state = false;
					break;
				}
			}while(response != 0xFF);
			length--;
			count = 0;
		}while(length);
		sd_write_byte(0xFD);
		sd_write_byte(0xFF);
	}
	sd_release();
	return state;
}

/**
 * SD卡填充多扇区(512字节 * length)
 * @param data 用于填充的数据
 * @param sector_addr 扇区字节地址,如使用sector_id,需要sector_id << 9
 * @param length 扇区数量
 * @return true 数据填充成功, false 数据填充失败
 * */
uint8_t sd_fill_multisector(uint8_t data, uint32_t sector_addr, uint32_t length) {
	uint8_t response, state = true;
	uint32_t i = 0, count = 0;
	uint8_t *buffer = NULL;
	if(length == 1) {
		buffer = (uint8_t *)os_malloc(sizeof(uint8_t) * 512);
		for(i = 0; i < 512; i++){
			*(buffer+i) = data;
		}
		state = sd_write_sector(buffer, sector_addr);
		os_free(buffer);
		buffer = NULL;
		return state;
	}

	sd_select();
	//连续写命令
	sd_write_cmd_ex(25, sector_addr, 0x00);
	response = sd_response();

	if(response == 0x00) {
		//发24个SCLK，等待SD卡准备好
		sd_write_byte(0xFF);
		sd_write_byte(0xFF);
		sd_write_byte(0xFF);
		do {
			//发起始令牌0xFC
			sd_write_byte(0xFC);
			//发一个sector数据(512字节)
			for(i = 0; i < 512; i++){
				sd_write_byte(data);
			}
			sd_write_byte(0xFF);
			sd_write_byte(0xFF);
			//接收响应
			response = sd_read_byte();
			if((response & 0x1F) != 0x05) {
				sd_release();
				return false;
			}
			//发送SCLK等待sd卡固化数据
			do {
				response = sd_read_byte();
				count++;
				if(count > 65535) {
					state = false;
					break;
				}
			}while(response != 0xFF);
			length--;
			count = 0;
		}while(length);
		sd_write_byte(0xFD);
		sd_write_byte(0xFF);
	}
	sd_release();
	return state;
}
