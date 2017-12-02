/*
 * frame.h
 *
 *  Created on: 31 oct. 2017
 *      Author: Jérémy
 */

#ifndef FRAME_H_
#define FRAME_H_

#define FRAME_DATA_LENGTH_MAX 16

enum frameType {
	TYPE_PACKET,
	TYPE_ASCII,
	TYPE_ASCII_CMD,
	TYPE_ASCII_SHORTCUT,
};

enum frameId {
	ID_RESET,
	ID_PING,
	ID_PONG,
	ID_ASK_STATUS,
	ID_REP_STATUS,
	ID_CHECKUP,

	ID_MAX
};

const uint8_t frameLength[ID_MAX] = {
		0,
		0,
		0,
		0,
		0,
		0,
};


union frame {
	struct {
		enum frameId  Id;
		uint8_t       Length;
		uint8_t       Data[FRAME_DATA_LENGTH_MAX];
		uint8_t       Crc[2];
	} packet;
	struct {
		uint8_t       ch;
	}ascii;
};


typedef struct frameDef {
	enum frameType type;
	union frame frame;
} frame_t;


#endif /* FRAME_H_ */
