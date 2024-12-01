/*
 * UDS.h
 *
 *  Created on: Nov 7, 2024
 *      Author: AbdElRahman Khalifa
 */

#ifndef INC_UDS_H_
#define INC_UDS_H_


#include "stm32f4xx_hal.h"

typedef struct{
	uint8_t sid;
	uint8_t SubFn;
	uint16_t DID;
	uint8_t data[8];
}UDS_SendRequestTypeDef;

extern CAN_HandleTypeDef hcan1;
extern UART_HandleTypeDef huart4;

extern uint16_t version;

void UDS_ReceiveRequest(void);

/*
 * this function initiate request and return the response
 */
UDS_SendRequestTypeDef UDS_SendRequest(UDS_SendRequestTypeDef *message);

#endif /* INC_UDS_H_ */
