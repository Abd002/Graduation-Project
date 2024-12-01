#include "bootloader.h"

static char Host_buffer[] = {

    0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D,
    0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
    0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x30, 0x31,
    0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B,
    0x3C, 0x3D, 0x3E, 0x3F, 0x40, 0x41, 0x42, 0x43, 0x44, 0x45,
    0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F,
    0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59,
    0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F, 0x60, 0x61, 0x62, 0x63,
    0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D,
    0x6E, 0x6F, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
    0x78, 0x79, 0x7A, 0x7B, 0x7C, 0x7D, 0x7E, 0x7F, 0x80, 0x81,
    0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8A, 0x8B,
    0x8C, 0x8D, 0x8E, 0x8F, 0x90, 0x91, 0x92, 0x93, 0x94, 0x95,
    0x96, 0x97, 0x98, 0x99, 0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F,
    0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9,
    0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF, 0xB0, 0xB1, 0xB2, 0xB3,
    0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD,
    0xBE, 0xBF, 0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7,
    0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF, 0xD0, 0xD1,
    0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xDB,
    0xDC, 0xDD, 0xDE, 0xDF, 0xE0, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5,
    0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEE, 0xEF,
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09,
	0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13
};



static uint8_t performFlashErase(uint32_t sector);
static uint8_t performFlashWrite(uint32_t address, uint16_t dataLength, uint8_t* data);

static BL_status BL_CRC_verify(uint32_t * pdata, uint32_t dataLength, uint32_t HostCRC);


uint16_t BL_getVersion();
static void BL_GetCID();
static void BL_flashErase();
static uint8_t BL_AddressVerification(uint32_t address);
void BL_writeCode(uint16_t dataLength, uint8_t * data);
void initUploading(uint32_t address);


static uint8_t BL_CRC_verify(uint32_t * pdata, uint32_t dataLength, uint32_t HostCRC) {
    uint8_t i = 0;
    uint32_t MCU_CRC = 0;

    // Accumulate CRC for the provided data length
    for (i = 0; i < dataLength; i++) {
        MCU_CRC = HAL_CRC_Accumulate(&hcrc, &pdata[i], 1); // Process one 32-bit integer at a time
    }

    // Reset CRC peripheral to avoid carry-over
    __HAL_CRC_DR_RESET(&hcrc);

    // Compare calculated CRC with the received HostCRC
    if (MCU_CRC == HostCRC) {
        return BL_ACK; // CRC match
    } else {
        return BL_NACK; // CRC mismatch
    }
}


uint16_t BL_getVersion(){
	return (*(uint16_t *)BL_VERSION_ADDRESS);
}
uint16_t BL_updateVersion(uint16_t new_version){
	performFlashErase(FLASH_SECTOR_6);
	HAL_FLASH_Unlock();
	if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, BL_VERSION_ADDRESS, new_version) != HAL_OK) {
	        // Handle error (e.g., through a debugger or error flag)
	}

	    // Lock the Flash to prevent further writes
	HAL_FLASH_Lock();

}

/*
 *
 * function is designed to read a chip ID from a hardware register
 *
 */
static void BL_GetCID(){
	uint16_t chipID=(uint16_t)(DBGMCU->IDCODE&0x0fff);

	BL_sendACK(2);
	HAL_UART_Transmit(&huart4, (uint8_t*)&chipID, sizeof(chipID), HAL_MAX_DELAY);
}

/*
 *
 * jump to Application which located at FLASH_SECTOR_2_ADDRESS
 *
 */
void Jump_Application()
{
	Deinit_Peripherals();

	uint32_t MSP_Value = *((volatile uint32_t*)FLASH_SECTOR_2_ADDRESS);
	uint32_t MainAppAdd = *((volatile uint32_t*)(FLASH_SECTOR_2_ADDRESS+4));

	pMainApp ResetHandler_Address=(pMainApp)MainAppAdd;
	HAL_RCC_DeInit();
	__set_MSP(MSP_Value);
	ResetHandler_Address();
}

/*
 *
 * Erase flash before installing new code
 *
 */
static void BL_flashErase(){
	uint8_t erase_statu = UNSUCCESSFUL_ERASE;

	erase_statu = performFlashErase(FLASH_SECTOR_2);

}
static uint32_t address;
static uint32_t addressRef;
void initUploading(uint32_t memory_address){
	address = memory_address;
	addressRef=0;
	BL_flashErase();
}

/*
 *
 * write new code using UART
 *
 */
void BL_writeCode(uint16_t dataLength, uint8_t * data){
	uint8_t addressVerify = ADDRESS_IS_INVALID;

	//uint32_t address = address_memory;


	uint8_t retOfFlash = FLASH_PAYLOAD_WRITE_FAILED;


	addressVerify = BL_AddressVerification(address);

	//BL_sendACK(1);
	if(addressVerify == ADDRESS_IS_VALID){
		retOfFlash = performFlashWrite(address, dataLength, data);
		address+=dataLength;

		//HAL_UART_Transmit(&huart4, (uint8_t*)&retOfFlash, sizeof(retOfFlash), HAL_MAX_DELAY);

		addressRef++;
	}else{
		//HAL_UART_Transmit(&huart4, (uint8_t*)&addressVerify, sizeof(addressVerify), HAL_MAX_DELAY);
	}
}

static uint8_t BL_AddressVerification(uint32_t address) {
    uint8_t addressRet = ADDRESS_IS_INVALID; // Default to invalid address

    // Check if the address is within the Flash memory range
    if (address >= FLASH_BASE && address <= STM32F4_FLASH_END) {
        addressRet = ADDRESS_IS_VALID;
    }
    // Check if the address is within the SRAM range
    else if (address >= SRAM_BASE && address <= STM32F4_SRAM_END) {
        addressRet = ADDRESS_IS_VALID;
    }

    return addressRet; // Return the verification result
}
/*
	FLASH_SECTOR_0     0U
	FLASH_SECTOR_1     1U
	FLASH_SECTOR_2     2U
	FLASH_SECTOR_3     3U
	FLASH_SECTOR_4     4U
	FLASH_SECTOR_5     5U
	FLASH_SECTOR_6     6U
	FLASH_SECTOR_7     7U
	FLASH_SECTOR_8     8U
	FLASH_SECTOR_9     9U
	FLASH_SECTOR_10    10U
	FLASH_SECTOR_11    11U
 */
static uint8_t performFlashErase(uint32_t sector) {
    FLASH_EraseInitTypeDef eraseInitStruct;
    uint32_t sectorError = 0;
    HAL_StatusTypeDef halStatus;

    // Check for valid sector number
    if (sector > FLASH_MAX_SECTOR) {
        return INVALID_SECTOR_NUMBER;
    }

    // Configure Erase structure for a specific sector
    eraseInitStruct.TypeErase = FLASH_TYPEERASE_SECTORS;
    eraseInitStruct.Sector = sector;
    eraseInitStruct.NbSectors = 1;
    eraseInitStruct.VoltageRange = FLASH_VOLTAGE_RANGE_3;  // Voltage range may vary based on STM32F4 series

    // Unlock the Flash for erasing
    HAL_FLASH_Unlock();

    // Perform the Erase
    halStatus = HAL_FLASHEx_Erase(&eraseInitStruct, &sectorError);

    // Lock the Flash after erasing
    HAL_FLASH_Lock();

    // Check Erase Status
    return (halStatus == HAL_OK) ? SUCCESSFUL_ERASE : UNSUCCESSFUL_ERASE;
}
static uint8_t performFlashWrite(uint32_t address, uint16_t dataLength, uint8_t* data) {
    uint16_t count = 0;
    HAL_StatusTypeDef halStatus;
    uint8_t result = FLASH_PAYLOAD_WRITE_FAILED;

    // Unlock the Flash for writing
    HAL_FLASH_Unlock();

    // Write data in word (8-bit) chunks
    for (count = 0; count < dataLength ; count++) {
        halStatus = HAL_FLASH_Program(FLASH_TYPEPROGRAM_BYTE, address + (count), *((uint8_t*)&data[count]));

        if (halStatus != HAL_OK) {
            result = FLASH_PAYLOAD_WRITE_FAILED;
            break;
        } else {
            result = FLASH_PAYLOAD_WRITE_SUCCESS;
        }
    }

    // Lock the Flash after writing
    HAL_FLASH_Lock();

    return result;
}

/**
  * @brief Deinitialize all configured peripherals
  * @retval None
  */
void Deinit_Peripherals(void)
{
  // Deinitialize CAN peripheral
  if (HAL_CAN_DeInit(&hcan1) != HAL_OK)
  {
    Error_Handler();
  }

  // Deinitialize CRC peripheral
  if (HAL_CRC_DeInit(&hcrc) != HAL_OK)
  {
    Error_Handler();
  }

  // Deinitialize UART peripheral
  if (HAL_UART_DeInit(&huart4) != HAL_OK)
  {
    Error_Handler();
  }

  // Deinitialize GPIO pins
  HAL_GPIO_WritePin(GPIOD, GPIO_PIN_14, GPIO_PIN_RESET);
  HAL_GPIO_DeInit(GPIOD, GPIO_PIN_14);

  // Additional GPIO Ports Clock Disable (optional if you want to save power)
  __HAL_RCC_GPIOA_CLK_DISABLE();
  __HAL_RCC_GPIOD_CLK_DISABLE();

  // Disable clocks for peripherals to save power (if needed)
  __HAL_RCC_CAN1_CLK_DISABLE();
  __HAL_RCC_CRC_CLK_DISABLE();
  __HAL_RCC_UART4_CLK_DISABLE();
}



