/*
 * UDS.c
 *
 *  Created on: Nov 7, 2024
 *      Author: AbdElRahman Khalifa
 */


#include "UDS.h"
#include "bootloader.h"



CAN_RxHeaderTypeDef RxHeader;
static uint8_t RxData[300];

uint32_t * UpdateAddress;

static void CAN_PollingReception(CAN_HandleTypeDef *hcan);
static void UDS_ProcessRequest(uint8_t* request, uint16_t len);
static void UDS_SendResponse(uint8_t* response, uint8_t len);


void CAN_PollingReception(CAN_HandleTypeDef *hcan){
	while(1){
		if (HAL_CAN_GetRxFifoFillLevel(hcan, CAN_RX_FIFO0) > 0) {

			if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &RxHeader, RxData) == HAL_OK) {

				break;
			}else {

			}
		}
		HAL_Delay(10);
	}
}

void UART_PollingReception(UART_HandleTypeDef *huart){


	for(int i=0;i<4;i++){
		uint8_t ok = 0xFF;
		HAL_UART_Transmit(&huart4, &ok, 1, HAL_MAX_DELAY);

		HAL_UART_Receive(huart, &RxData[i], 1, HAL_MAX_DELAY);
	}



	if(*((uint16_t*)&RxData[2])){

		for(int i=0;i<*((uint16_t*)&RxData[2]);i++){
			uint8_t ok = 0xFF;
			HAL_UART_Transmit(&huart4, &ok, 1, HAL_MAX_DELAY);

			HAL_UART_Receive(huart, &RxData[i+4], 1, HAL_MAX_DELAY);
		}
	}

}
#define SYNC_CODE 0xAA
#define ACK_CODE_T  0x55
void syncWithArduino() {
  uint8_t receivedByte;

  while (1) {
    // Wait until data is received over UART
    if (HAL_UART_Receive(&huart4, &receivedByte, 1, HAL_MAX_DELAY) == HAL_OK) {
      // Check if received byte matches SYNC_CODE
      if (receivedByte == SYNC_CODE) {
        // Send ACK_CODE back to Arduino
        HAL_UART_Transmit(&huart4, ACK_CODE_T, 1, HAL_MAX_DELAY);
        return;  // Exit sync function after successful sync
      }
    }
  }
}


void UDS_ReceiveRequest(void){
	//for can
	//CAN_PollingReception(&hcan1);
	//UDS_ProcessRequest(RxData, RxHeader.DLC);
	//for UART
	UART_PollingReception(&huart4); // Wait for UART message
	UDS_ProcessRequest(RxData, *((uint16_t*)&RxData[2]) + 4);  // Process received data

}

static void UDS_ProcessRequest(uint8_t* request, uint16_t len){
	uint8_t sid = request[0];
	uint8_t SubFun = request[1];
	uint8_t response[8]={0};


	switch(sid){
	case 0x10:	//Diagnostic Session Control
		response[0] = 0x50;
		response[1] = SubFun;
		UDS_SendResponse(response, 4);
		break;

	case 0x11:  //ECU Reset
		response[0] = 0x51;
		response[1] = SubFun;
		UDS_SendResponse(response, 4);
		if (request[1] == 0x01) {
			Jump_Application();//make jump ?
		}
		break;

	case 0x22:  //Read version
		response[0] = 0x62;
		response[1] = request[1];
		response[2] = 2;
		*((uint16_t*)&response[4]) = BL_getVersion();

		UDS_SendResponse(response, 6);
		break;

	case 0x14:  //clear Diagnostic Trouble Codes (DTCs) stored in the ECU’s memory
		response[0] = 0x54;
		UDS_SendResponse(response, 4);
		break;

	case 0x34: //Memory address where the update will be written.
		response[0] = 0x74;
		response[1] = request[1];
		UpdateAddress = *((uint32_t *)&request[4]);
		initUploading(UpdateAddress);
		BL_updateVersion(*((uint16_t *)&request[8]));
		UDS_SendResponse(response, 4);
		break;

	case 0x36:	//Send Firmware Data (0x36)
		//use boot-loader here
		response[0] = 0x76;
		response[1] = request[1];
		BL_writeCode(len - 4, request+4);
		UDS_SendResponse(response, 4);
		break;

	case 0x37: 	//End Routine (0x31)
		response[0] = 0x77;
		response[1] = request[1];
		UDS_SendResponse(response, 4);
		break;
	default:
		//send Negative Response
		response[0] = 0x7F;
		response[1] = sid;
		response[2] = 0x11;  // Service not supported
		UDS_SendResponse(response, 4);
		break;
	}
}

static void UDS_SendResponse(uint8_t* response, uint8_t len){
	CAN_TxHeaderTypeDef txHeader;
	txHeader.DLC = len;
	txHeader.StdId = 0x7E8;
	txHeader.IDE = CAN_ID_STD;

	uint32_t txMailbox;
	//for can
	//HAL_CAN_AddTxMessage(&hcan1, &txHeader, response, &txMailbox);
	//for UART
	//syncWithArduino();
	for(int i=0;i<len;i++){
		HAL_UART_Transmit(&huart4, &response[i], 1, HAL_MAX_DELAY);
		HAL_Delay(100);
	}

}

UDS_SendRequestTypeDef UDS_SendRequest(UDS_SendRequestTypeDef *message){
	CAN_TxHeaderTypeDef txHeader;
	uint32_t txMailbox;
	uint8_t data[16];

	data[0]=message->sid;
	data[1]=message->SubFn;
	*((uint16_t*)&data[2])=message->DID;
	for(int i=4;i<12;i++){data[i]=message->data[i-4];}


	txHeader.DLC = sizeof(data);
	txHeader.StdId = 0x7E8;
	txHeader.IDE = CAN_ID_STD;

	//for can
	//HAL_CAN_AddTxMessage(&hcan1, &txHeader, data, &txMailbox);
	//for UART
	syncWithArduino();
	HAL_UART_Transmit(&huart4, data, sizeof(data), HAL_MAX_DELAY);


	//now u will receive the PR or NR

	//for can
	//CAN_PollingReception(&hcan1);
	//for UART
	syncWithArduino();
	UART_PollingReception(&huart4);

	UDS_SendRequestTypeDef ret={RxData[0], RxData[1], *((uint16_t*)&RxData[2])};
	for(int i=4;i<12;i++){ret.data[i-4]=RxData[i];}

	return ret;

}
