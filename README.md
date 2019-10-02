# ESP8266-SPI-SDCARD 

已完成功能:  
esp8266 GPIO模拟SPI输入/输出配置  
读取Sd卡CID, CSD信息  
计算SD v1.0 卡容量  
读写单个扇区  
读写多个扇区  
在user_main.c中提供简单测试  
##### 串口返回示例(以2G samsung SD卡为例):    

```
SD_IO_INIT:OK
SD_INIT:ff
MID:0x1b
COMPANY:Samsung
OID:SM
BRANDS:ProGrade,Samsung
PNM:00000
PRV:10
PSN:38641434
MDT:09-3
csd:
0x00 0x2f 0x00 0x32 0x5b 0x5a 0x83 0xba 
0x6d 0xb7 0xff 0xbf 0x16 0x80 0x00 0xf1 
card size:2001731584
```