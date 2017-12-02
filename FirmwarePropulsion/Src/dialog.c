/*
 * dialog.c
 *
 *  Created on: 31 oct. 2017
 *      Author: Jérémy
 */

/* ----- Includes ----- */
#include "dialog.h"
#include "frame.h"

/* ----- Defines ----- */
#define UART_BUFFER_SIZE 8
#define STDOUT_BUFFER_SIZE 128

#define ASCII_STX 0x02
#define ASCII_ETX 0x03

/* ----- Global variables ----- */
uint8_t  _stdout_buff[STDOUT_BUFFER_SIZE] = {0};
uint32_t _stdout_head = 0;
uint32_t _stdout_tail = 0;

UART_HandleTypeDef* _huart = NULL;

volatile uint8_t _uart_tx_buff[UART_BUFFER_SIZE] = {0};
volatile uint8_t _uart_rx_buff[UART_BUFFER_SIZE] = {0};
uint32_t _uart_tx_index = 0;
uint32_t _uart_rx_index = 0;

HAL_UART_StateTypeDef HAL_UART_tx_State = HAL_UART_STATE_READY;

frame_t frame;
frame_t frame2;

/* ----- Private prototype functions */

int __io_putchar(int ch)
{
	Dialog_Putc(ch);
    return ch;
}

/*int __io_getchar(void)
{
  uint8_t ch = 0;

  return ch;
}*/

uint8_t _decode_frame(frame_t* pframe, uint8_t* ptr);
uint8_t _encode_frame(frame_t* pframe, uint8_t* ptr);
void _send_frame(frame_t* pframe);
void _do_action(frame_t* pframe);

/* ----- Public functions ----- */

void Dialog_Init(UART_HandleTypeDef* huart)
{
	// Add uart handle
	_huart = huart;

	// Init HAL on Rx buffer
	HAL_UART_Receive_IT(_huart, (uint8_t*)_uart_rx_buff, UART_BUFFER_SIZE);

}

void Dialog_Process(void)
{
	// Look for frame to decode
	if(_decode_frame(&frame, (uint8_t*)_uart_rx_buff))
	{
		// Frame found
		_do_action(&frame);
	}

	// Send data
	if(HAL_UART_tx_State == HAL_UART_STATE_READY)
	{
		if(_stdout_tail != _stdout_head)
		{
			// Char to print
			frame.type = TYPE_ASCII;
			frame.frame.ascii.ch = _stdout_buff[_stdout_tail++];
			_stdout_tail %= STDOUT_BUFFER_SIZE;
			_send_frame(&frame);
		}
	}
	else
	{
		// HAL is busy
	}
}

void Dialog_Putc(uint8_t ch)
{
	_stdout_buff[_stdout_head++] = ch;
	_stdout_head %= STDOUT_BUFFER_SIZE;
}

/* ----- Private functions ----- */

/*
 * Types of frame to decode:
 *   - Packet         [STX|I|L|Data|CRC|ETX]
 *   - Ascii cmd      [ASCII|\r|\n]
 *   - Ascii shortcut [ASCII]
 */
uint8_t _decode_frame(frame_t* pframe, uint8_t* ptr)
{
	static uint8_t state = 0;
	static uint8_t i = 0;

	uint8_t ret = 0;

	uint16_t head = 0;
	head = _huart->RxXferSize - _huart->RxXferCount;

	while( (head != _uart_rx_index) && (ret == 0))
	{
		switch(state)
		{
		case 0:	// Idle
			state = 1;
			break;
		case 1:	// Wait STX or Ascii cmd
			if(_uart_rx_buff[_uart_rx_index] == ASCII_STX)
			{
                // Start of Text (STX)
				pframe->type = TYPE_PACKET;
				_uart_rx_index += 1U;
				state = 2;
			}
			else if( ((_uart_rx_buff[_uart_rx_index] >= 'A') && (_uart_rx_buff[_uart_rx_index] <= 'Z')) ||
					 ((_uart_rx_buff[_uart_rx_index] >= 'a') && (_uart_rx_buff[_uart_rx_index] <= 'z')) ||
					 ((_uart_rx_buff[_uart_rx_index] >= '0') && (_uart_rx_buff[_uart_rx_index] <= '9')) ||
					 (_uart_rx_buff[_uart_rx_index] == ' ')  ||
					 (_uart_rx_buff[_uart_rx_index] == '.')  ||
					 (_uart_rx_buff[_uart_rx_index] == '-')  ||
					 (_uart_rx_buff[_uart_rx_index] == '\r') ||
					 (_uart_rx_buff[_uart_rx_index] == '\n')
				   )
			{
                // Ascii cmd
				pframe->type = TYPE_ASCII;
				pframe->frame.ascii.ch = _uart_rx_buff[_uart_rx_index];
				_uart_rx_index += 1U;

				ret = 1;
			}
			else
			{
				// Discard byte
				_uart_rx_index += 1U;
			}
			break;
		case 2:	// Wait I
			pframe->frame.packet.Id = _uart_rx_buff[_uart_rx_index];
			_uart_rx_index += 1U;
			state = 3;
			break;
		case 3:	// Wait L
			pframe->frame.packet.Length = _uart_rx_buff[_uart_rx_index];
			_uart_rx_index += 1U;
			state = 4;
			i = 0;	// Reset index for data
			break;
		case 4:	// Wait DATA
			pframe->frame.packet.Data[i] = _uart_rx_buff[_uart_rx_index];
			i += 1U;
			_uart_rx_index += 1U;
			if(i == pframe->frame.packet.Length)
				state = 5;
			i = 0;	// Reset index for crc
			break;
		case 5:	// Wait CRC
			// CRC TODO
			pframe->frame.packet.Crc[i] = _uart_rx_buff[_uart_rx_index];
			i += 1U;
			_uart_rx_index += 1U;
			if(i == 2)
				state = 6;
			break;
		case 6:	// Wait ETX
			if(_uart_rx_buff[_uart_rx_index] == ASCII_ETX)
			{
				// Good : ETX occur
				ret = 1;	// TODO : Send frame length
			}
			else
			{
				// Error : ETX doesn't occur
				// Discard frame (and maybe should I send frame Error)
			}
			_uart_rx_index += 1U;
			state = 1;
			break;
		default:
			break;
		} // End Switch

		// circular index
		_uart_rx_index %= UART_BUFFER_SIZE;
	} // End while fresh data

	return ret;
}

uint8_t _encode_frame(frame_t* pframe, uint8_t* ptr)
{
	uint8_t i=0, j=0;

	switch (pframe->type)
	{
	case TYPE_PACKET:
		ptr[i++] = ASCII_STX;

		ptr[i++] = pframe->frame.packet.Id;
		ptr[i++] = pframe->frame.packet.Length;

		for(j = 0; j < pframe->frame.packet.Length; j++)
			ptr[i++] = pframe->frame.packet.Data[j];

		ptr[i++] = pframe->frame.packet.Crc[0];
		ptr[i++] = pframe->frame.packet.Crc[1];

		ptr[i++] = ASCII_ETX;
		break;
	case TYPE_ASCII:
		ptr[i++] = pframe->frame.ascii.ch;
		break;
	default:
		break;
	}
	return i;
}


void _do_action(frame_t* pframe)
{
	if(pframe->type == TYPE_PACKET)
	{
		switch(pframe->frame.packet.Id)
		{
		case ID_RESET:
			break;
		case ID_PING:
			// Prepare frame
			pframe->type = TYPE_PACKET;
			pframe->frame.packet.Id      = ID_PONG;
			pframe->frame.packet.Length  = 0;
			pframe->frame.packet.Data[0] = 0;
			pframe->frame.packet.Crc[0]  = 0;
			pframe->frame.packet.Crc[1]  = 0;
			_send_frame(pframe);
			break;
		default:
			break;
		}
	}
	else if(pframe->type == TYPE_ASCII)
	{
		// Echo to terminal
		pframe->type = TYPE_ASCII;
		//pframe->frame.ascii.ch = pframe->frame.ascii.ch;
		_send_frame(pframe);
	}
}

void _send_frame(frame_t* pframe)
{
	uint8_t size;

	// Wait last frame completely transmit
	while(HAL_UART_tx_State != HAL_UART_STATE_READY);
	// Lock tx buff
	HAL_UART_tx_State = HAL_UART_STATE_BUSY_TX;
	// Encode frame to buff
	size = _encode_frame(pframe, (uint8_t*)_uart_tx_buff);
	// Send buff on uart
	HAL_UART_Transmit_IT(_huart, (uint8_t*)_uart_tx_buff, size);
}

/* ----- Callback ----- */

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if(huart == _huart)
    {
    	HAL_UART_Receive_IT(_huart, (uint8_t*)_uart_rx_buff, UART_BUFFER_SIZE);
    }
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if(huart == _huart)
    {
    	HAL_UART_tx_State = HAL_UART_STATE_READY;
    }
}
