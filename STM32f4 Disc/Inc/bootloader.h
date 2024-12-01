/*
 * bootloader.h
 *
 *  Created on: Nov 4, 2024
 *      Author: AbdElRahman Khalifa
 */

#ifndef INC_BOOTLOADER_H_
#define INC_BOOTLOADER_H_

#include "stm32f4xx_hal.h"
#include <string.h>


// Custom definitions for success and error statuses
#define FLASH_SUCCESS         		0
#define FLASH_ERROR           		1
#define INVALID_SECTOR_NUMBER 		2
#define SUCCESSFUL_ERASE      		0
#define UNSUCCESSFUL_ERASE    		1
#define FLASH_PAYLOAD_WRITE_SUCCESS 0
#define FLASH_PAYLOAD_WRITE_FAILED  1

// Adjust as per the actual flash sectors in your MCU
#define FLASH_SECTOR_2_ADDRESS  0x08008000U  // Example start address for sector 2
#define FLASH_MAX_SECTOR        11          // STM32F4 has 12 sectors

// Custom commands host can use
#define CBL_GET_VER_CMD 		0x10 	//get the version of MCU from MCU
#define CBL_GET_HELP_CMD 		0x11	//get help
#define CBL_GET_CID_CMD			0x12	//read id of bootloader
#define CBL_GO_TO_ADDR_CMD 		0x14	//jump to address
#define CBL_FLASH_ERASE_CMD 	0x15	//erase the flash
#define CBL_MEM_WRITE_CMD 		0x16	//write to flash

// BL version
#define CBL_VENDOR_ID			100
#define CBL_SW_MAJOR_VERSION	1
#define CBL_SW_MINOR_VERSION	1
#define CBL_SW_PATCH_VERSION	0

//return ACK to host
#define SEND_NACK 	0xAB
#define SEND_ACK 	0xCD

#define STM32F4_FLASH_END   0x080FFFFF  // End address of Flash memory
#define STM32F4_SRAM_END    0x2001FFFF  // End address of SRAM

#define ADDRESS_IS_VALID    0x01        // Valid address return value
#define ADDRESS_IS_INVALID  0x00        // Invalid address return value

#define BL_VERSION_ADDRESS 0x08040000U


typedef enum {
	BL_ACK,BL_NACK
}BL_status;
typedef void (*pMainApp)(void);



extern CAN_HandleTypeDef hcan1;
extern CRC_HandleTypeDef hcrc;
extern UART_HandleTypeDef huart4;

BL_status BL_featchHostCommand();
void Jump_Application();
void BL_writeCode(uint16_t dataLength, uint8_t * data);
void initUploading(uint32_t memory_address);
uint16_t BL_getVersion();
uint16_t BL_updateVersion(uint16_t new_version);

#endif /* INC_BOOTLOADER_H_ */
