/*
 * dialog.h
 *
 *  Created on: 31 oct. 2017
 *      Author: Jérémy
 */

#ifndef DIALOG_H_
#define DIALOG_H_

#include "usart.h"
#include "cmsis_os.h"

void Dialog_Init(UART_HandleTypeDef* huart);

void Dialog_Process(void);

void Dialog_Putc(uint8_t ch);

#endif /* DIALOG_H_ */
