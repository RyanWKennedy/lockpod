/*
Copyright 2018 Ryan Kennedy
*/

#include <stdio.h>
#include <libspiflash.h>
#include "board.h"
#include "EPSGlobal.h"
#include "touchCtrl.h"

uint8_t pData[datBufSize]; // data buffer for TWI transfers

static const Pin touchCtrl_shutdownPin[] = {PIO_PB0, PIOB, ID_PIOB, PIO_OUTPUT_0, PIO_DEFAULT}; // EK j3p5
static const Pin touchCtrl_msgRdyPin[] = {PIO_PB6, PIOB, ID_PIOB, PIO_INPUT, PIO_DEFAULT}; // EK j3p17

// used for doing TWI in all software
static const Pin SDA_out[] = {PIO_PA30, PIOA, ID_PIOA, PIO_OUTPUT_1, PIO_OPENDRAIN}; 
static const Pin SDA_in[] = {PIO_PA30, PIOA, ID_PIOA, PIO_INPUT, PIO_DEFAULT};
static const Pin SCL[] = {PIO_PA31, PIOA, ID_PIOA, PIO_OUTPUT_1, PIO_OPENDRAIN}; 

void touchCtrl_setup()
{
	PIO_Configure(touchCtrl_shutdownPin, PIO_LISTSIZE(touchCtrl_shutdownPin));
	PIO_Configure(touchCtrl_msgRdyPin, PIO_LISTSIZE(touchCtrl_msgRdyPin));
	PIO_Set(touchCtrl_shutdownPin);
	delay_msOrUS(10, 1); // Give GSL1680 time to register wake up signal
}

// consider deleting
void touchCtrl_clrReg()
{
	pData[0] = 0x88;
	TWID_Write(&twid, touchCtrl_TWIWAddr, 0xe0, 1, pData, 1, 0);
	// arg reference for last TWID_Write func call:
	// (driver, TWIAddr, periphSlavInternalAddr, periphSlavInternalAddrSizeInBytes, 
	//  pData=pointerToDataToWrite, numBytesToSend, asyncMode)
	delay_msOrUS(50, 1);
	pData[0] = 0x01;
	TWID_Write(&twid, touchCtrl_TWIWAddr, 0x80, 1, pData, 1, 0);
	delay_msOrUS(50, 1);
	pData[0] = 0x04;
	TWID_Write(&twid, touchCtrl_TWIWAddr, 0xe4, 1, pData, 1, 0);
	delay_msOrUS(50, 1);
	pData[0] = 0x00;
	TWID_Write(&twid, touchCtrl_TWIWAddr, 0xe0, 1, pData, 1, 0);
	delay_msOrUS(50, 1);
}

void touchCtrl_loadFw()
{
	printf("\n\rLoading touchpanel controller config data, please wait...");
	
	uint8_t buf[4] = {0};
	uint8_t reg = 0;
	uint32_t source_line = 0;
	struct tsFwDatPack *ptr_fw;
	ptr_fw = touchCtrl_configFw;

	for (source_line = 0; source_line < TSFW_LINES; ++source_line) 
	{
		/* init page trans, set the page val */
		if (ptr_fw[source_line].offset == 0xf0)
		{
			buf[0] = (uint8_t)(ptr_fw[source_line].val & 0x000000ff);
			TWID_Write(&twid, touchCtrl_TWIWAddr, 0xf0, 1, buf, 1, 0);
			//printf("%d\n\r", buf[0]);
		}
		else 
		{
			reg = ptr_fw[source_line].offset;
			buf[0] = (uint8_t)(ptr_fw[source_line].val & 0x000000ff);
			buf[1] = (uint8_t)((ptr_fw[source_line].val & 0x0000ff00) >> 8);
			buf[2] = (uint8_t)((ptr_fw[source_line].val & 0x00ff0000) >> 16);
			buf[3] = (uint8_t)((ptr_fw[source_line].val & 0xff000000) >> 24);

			TWID_Write(&twid, touchCtrl_TWIWAddr, reg, 1, buf, 4, 0);
		}
	}
	//printf("%x\n\r", ptr_fw[source_line-1].val);
	printf("[done]\n\r");
}

void touchCtrl_startup(void)
{
	pData[0] = 0x00;
	TWID_Write(&twid, touchCtrl_TWIWAddr, 0xe0, 1, pData, 1, 0);
}

void touchCtrl_reset()
{
	pData[0]=0x88;
	TWID_Write(&twid, touchCtrl_TWIWAddr, 0xe0, 1, pData, 1, 0);
	delay_msOrUS(10, 1);
	
	pData[0]=0x04;
	TWID_Write(&twid, touchCtrl_TWIWAddr, 0xe4, 1, pData, 1, 0);
	delay_msOrUS(10, 1);
	
	pData[0]=0x00;
	TWID_Write(&twid, touchCtrl_TWIWAddr, 0xbc, 1, pData, 1, 0);
	delay_msOrUS(10, 1);
	pData[0]=0x00;
	TWID_Write(&twid, touchCtrl_TWIWAddr, 0xbd, 1, pData, 1, 0);
	delay_msOrUS(10, 1);
	pData[0]=0x00;
	TWID_Write(&twid, touchCtrl_TWIWAddr, 0xbe, 1, pData, 1, 0);
	delay_msOrUS(10, 1);
	pData[0]=0x00;
	TWID_Write(&twid, touchCtrl_TWIWAddr, 0xbf, 1, pData, 1, 0);
	delay_msOrUS(10, 1);
}

void touchCtrl_init()
{
	touchCtrl_reset();
	touchCtrl_loadFw();
	touchCtrl_startup();
	touchCtrl_reset();
	PIO_Clear(touchCtrl_shutdownPin);
	delay_msOrUS(50, 1);
	PIO_Set(touchCtrl_shutdownPin);
	delay_msOrUS(30, 1);
	PIO_Clear(touchCtrl_shutdownPin);
	delay_msOrUS(5, 1);
	PIO_Set(touchCtrl_shutdownPin);
	delay_msOrUS(20, 1);
	touchCtrl_reset();
	touchCtrl_startup();
}

struct touchEvent touchCtrl_getTouch()
{
	autoShutdn_check(AUTOSD_TIMEOUT); // touchCtrl_getTouch usually just waits for user input, so shutdown system if its been waiting too long. 
	struct touchEvent tE;
	tE.touchValid = 0;
	if(PIO_Get(touchCtrl_msgRdyPin)){
		touchCtrl_setInternalRdAddr(0x80);
		delay_msOrUS(1, 1);
		touchCtrl_readBytes(pData, 1);
		delay_msOrUS(1, 1);
		tE.touchValid = pData[0];
		if(tE.touchValid == 1){
			touchCtrl_setInternalRdAddr(0x84);
			delay_msOrUS(1, 1);
			touchCtrl_readBytes(pData, 4);
			delay_msOrUS(1, 1);
			tE.x = pData[0] + pData[1]*256;
			tE.y = pData[2] + (pData[3]&0xf)*256; // the "&" masks out the finger data
			//printf("getTouch() %d %d %d %d %d %d %d %d\n\r", pData[0], pData[1], pData[2], pData[3], pData[4], pData[5], pData[6], pData[7]);
			autoShutdn_timerRst(); // Valid screen touch so reset auto-shutdown timer.
		}
	}
	return tE;
}

uint8_t touchCtrl_innerReadByte(uint8_t *pAckInfo, int i, int lastFlag)
{
	uint8_t bits[8] = {0, 0, 0, 0, 0, 0, 0, 0};
	
	// readByte-bit1
	PIO_Set(SCL);
	delay_msOrUS(500, 0);
	bits[7] = PIO_Get(SDA_in);
	PIO_Clear(SCL);
	delay_msOrUS(500, 0);
	
	// readByte-bit2
	PIO_Set(SCL);
	delay_msOrUS(500, 0);
	bits[6] = PIO_Get(SDA_in);
	PIO_Clear(SCL);
	delay_msOrUS(500, 0);
	
	// readByte-bit3
	PIO_Set(SCL);
	delay_msOrUS(500, 0);
	bits[5] = PIO_Get(SDA_in);
	PIO_Clear(SCL);
	delay_msOrUS(500, 0);
	
	// readByte-bit4
	PIO_Set(SCL);
	delay_msOrUS(500, 0);
	bits[4] = PIO_Get(SDA_in);
	PIO_Clear(SCL);
	delay_msOrUS(500, 0);
	
	// readByte-bit5
	PIO_Set(SCL);
	delay_msOrUS(500, 0);
	bits[3] = PIO_Get(SDA_in);
	PIO_Clear(SCL);
	delay_msOrUS(500, 0);
	
	// readByte-bit6
	PIO_Set(SCL);
	delay_msOrUS(500, 0);
	bits[2] = PIO_Get(SDA_in);
	PIO_Clear(SCL);
	delay_msOrUS(500, 0);
	
	// readByte-bit7
	PIO_Set(SCL);
	delay_msOrUS(500, 0);
	bits[1] = PIO_Get(SDA_in);
	PIO_Clear(SCL);
	delay_msOrUS(500, 0);
	
	// readByte-bit8
	PIO_Set(SCL);
	delay_msOrUS(500, 0);
	bits[0] = PIO_Get(SDA_in);
	PIO_Clear(SCL);
	delay_msOrUS(500, 0);
	
	// readByte-ackOrNack
	if(lastFlag == 1){
		PIO_Configure(SDA_out, PIO_LISTSIZE(SDA_out));
		PIO_Set(SDA_out); // nack
		delay_msOrUS(500, 0);
		PIO_Set(SCL);
		delay_msOrUS(500, 0);
		PIO_Configure(SDA_in, PIO_LISTSIZE(SDA_in));
		pAckInfo[i] = PIO_Get(SDA_in);
		PIO_Clear(SCL);
		delay_msOrUS(500, 0);
	}
	else{
		PIO_Configure(SDA_out, PIO_LISTSIZE(SDA_out));
		PIO_Clear(SDA_out); // ack
		delay_msOrUS(500, 0);
		PIO_Set(SCL);
		delay_msOrUS(500, 0);
		pAckInfo[i] = 0;
		PIO_Clear(SCL);
		delay_msOrUS(500, 0);
		PIO_Clear(SDA_out);
		PIO_Configure(SDA_in, PIO_LISTSIZE(SDA_in));
	}
	
	uint8_t retVal = bits[0] + (bits[1]*2) + (bits[2]*4) + (bits[3]*8)  + (bits[4]*16) + (bits[5]*32)
		+ (bits[6]*64) + (bits[7]*128);
	//printf("touchCtrl_innerReadByte() %d\n\r", retVal);
	return retVal;
}

void touchCtrl_readBytes(uint8_t *pData, int num)
{
	PIO_Configure(SCL, PIO_LISTSIZE(SCL));
	PIO_Configure(SDA_out, PIO_LISTSIZE(SDA_out));
	uint8_t arrAck[8] = {2, 2, 2, 2, 2, 2, 2, 2};
	
	// initRead-start
	PIO_Clear(SDA_out);
	delay_msOrUS(500, 0);
	PIO_Clear(SCL);
	delay_msOrUS(500, 0);
	
	// initRead-bit1:1
	PIO_Set(SDA_out);
	delay_msOrUS(500, 0);
	PIO_Set(SCL);
	delay_msOrUS(500, 0);
	PIO_Clear(SCL);
	delay_msOrUS(500, 0);
	
	// initRead-bit2:0
	PIO_Clear(SDA_out);
	delay_msOrUS(500, 0);
	PIO_Set(SCL);
	delay_msOrUS(500, 0);
	PIO_Clear(SCL);
	delay_msOrUS(500, 0);
	
	// initRead-bit3:0
	PIO_Clear(SDA_out);
	delay_msOrUS(500, 0);
	PIO_Set(SCL);
	delay_msOrUS(500, 0);
	PIO_Clear(SCL);
	delay_msOrUS(500, 0);
	
	// initRead-bit4:0
	PIO_Clear(SDA_out);
	delay_msOrUS(500, 0);
	PIO_Set(SCL);
	delay_msOrUS(500, 0);
	PIO_Clear(SCL);
	delay_msOrUS(500, 0);
	
	// initRead-bit5:0
	PIO_Clear(SDA_out);
	delay_msOrUS(500, 0);
	PIO_Set(SCL);
	delay_msOrUS(500, 0);
	PIO_Clear(SCL);
	delay_msOrUS(500, 0);
	
	// initRead-bit6:0
	PIO_Clear(SDA_out);
	delay_msOrUS(500, 0);
	PIO_Set(SCL);
	delay_msOrUS(500, 0);
	PIO_Clear(SCL);
	delay_msOrUS(500, 0);
	
	// initRead-bit7:0
	PIO_Clear(SDA_out);
	delay_msOrUS(500, 0);
	PIO_Set(SCL);
	delay_msOrUS(500, 0);
	PIO_Clear(SCL);
	delay_msOrUS(500, 0);
	
	// initRead-bit8:1
	PIO_Set(SDA_out);
	delay_msOrUS(500, 0);
	PIO_Set(SCL);
	delay_msOrUS(500, 0);
	PIO_Clear(SCL);
	delay_msOrUS(500, 0);
	
	// initRead-bit9:ack
	PIO_Set(SDA_out);
	delay_msOrUS(500, 0);
	PIO_Set(SCL);
	delay_msOrUS(500, 0);
	PIO_Configure(SDA_in, PIO_LISTSIZE(SDA_in));
	arrAck[0] = PIO_Get(SDA_in);
	delay_msOrUS(500, 0);
	PIO_Clear(SCL);
	delay_msOrUS(500, 0);
	
	int numI;
	int sendNACK = 0;
	for(numI = 0; numI < num; ++numI){
		if(numI == num-1)
			sendNACK = 1;
		pData[numI] = touchCtrl_innerReadByte(arrAck, numI+1, sendNACK);
	}
	
	// stop
	PIO_Configure(SDA_out, PIO_LISTSIZE(SDA_out));
	PIO_Clear(SDA_out);
	delay_msOrUS(500, 0);
	PIO_Set(SCL);
	delay_msOrUS(500, 0);
	PIO_Set(SDA_out);
	delay_msOrUS(500, 0);
	
	//printf("touchCtrl_read8Byte() %d %d %d %d %d %d %d %d\n\r", arrAck[0], arrAck[1], arrAck[2], arrAck[3], arrAck[4], arrAck[5], arrAck[6], arrAck[7]);
	//printf("touchCtrl_read8Byte() %d %d %d %d %d %d %d %d\n\r", pData[0], pData[1], pData[2], pData[3], pData[4], pData[5], pData[6], pData[7]);
	
}

void touchCtrl_setInternalRdAddr(int intrAddr)
{
	PIO_Configure(SCL, PIO_LISTSIZE(SCL));
	PIO_Configure(SDA_out, PIO_LISTSIZE(SDA_out));
	uint8_t arrAck[2] = {2, 2};
	
	// byte1-start
	PIO_Clear(SDA_out);
	delay_msOrUS(500, 0);
	PIO_Clear(SCL);
	delay_msOrUS(500, 0);
	
	// byte1-bit1:1
	PIO_Set(SDA_out);
	delay_msOrUS(500, 0);
	PIO_Set(SCL);
	delay_msOrUS(500, 0);
	PIO_Clear(SCL);
	delay_msOrUS(500, 0);
	
	// byte1-bit2:0
	PIO_Clear(SDA_out);
	delay_msOrUS(500, 0);
	PIO_Set(SCL);
	delay_msOrUS(500, 0);
	PIO_Clear(SCL);
	delay_msOrUS(500, 0);
	
	// byte1-bit3:0
	PIO_Clear(SDA_out);
	delay_msOrUS(500, 0);
	PIO_Set(SCL);
	delay_msOrUS(500, 0);
	PIO_Clear(SCL);
	delay_msOrUS(500, 0);
	
	// byte1-bit4:0
	PIO_Clear(SDA_out);
	delay_msOrUS(500, 0);
	PIO_Set(SCL);
	delay_msOrUS(500, 0);
	PIO_Clear(SCL);
	delay_msOrUS(500, 0);
	
	// byte1-bit5:0
	PIO_Clear(SDA_out);
	delay_msOrUS(500, 0);
	PIO_Set(SCL);
	delay_msOrUS(500, 0);
	PIO_Clear(SCL);
	delay_msOrUS(500, 0);
	
	// byte1-bit6:0
	PIO_Clear(SDA_out);
	delay_msOrUS(500, 0);
	PIO_Set(SCL);
	delay_msOrUS(500, 0);
	PIO_Clear(SCL);
	delay_msOrUS(500, 0);
	
	// byte1-bit7:0
	PIO_Clear(SDA_out);
	delay_msOrUS(500, 0);
	PIO_Set(SCL);
	delay_msOrUS(500, 0);
	PIO_Clear(SCL);
	delay_msOrUS(500, 0);
	
	// byte1-bit8:0
	PIO_Clear(SDA_out);
	delay_msOrUS(500, 0);
	PIO_Set(SCL);
	delay_msOrUS(500, 0);
	PIO_Clear(SCL);
	delay_msOrUS(500, 0);
	
	// byte1-bit9:ack
	PIO_Set(SDA_out);
	delay_msOrUS(500, 0);
	PIO_Set(SCL);
	delay_msOrUS(500, 0);
	PIO_Configure(SDA_in, PIO_LISTSIZE(SDA_in));
	arrAck[0] = PIO_Get(SDA_in);
	PIO_Configure(SDA_out, PIO_LISTSIZE(SDA_out));
	PIO_Set(SDA_out);
	PIO_Clear(SCL);
	delay_msOrUS(500, 0);
	
	// byte2-bit1:1
	PIO_Set(SDA_out);
	delay_msOrUS(500, 0);
	PIO_Set(SCL);
	delay_msOrUS(500, 0);
	PIO_Clear(SCL);
	delay_msOrUS(500, 0);
	
	// byte2-bit2:0
	PIO_Clear(SDA_out);
	delay_msOrUS(500, 0);
	PIO_Set(SCL);
	delay_msOrUS(500, 0);
	PIO_Clear(SCL);
	delay_msOrUS(500, 0);
	
	// byte2-bit3:0
	PIO_Clear(SDA_out);
	delay_msOrUS(500, 0);
	PIO_Set(SCL);
	delay_msOrUS(500, 0);
	PIO_Clear(SCL);
	delay_msOrUS(500, 0);
	
	// byte2-bit4:0
	PIO_Clear(SDA_out);
	delay_msOrUS(500, 0);
	PIO_Set(SCL);
	delay_msOrUS(500, 0);
	PIO_Clear(SCL);
	delay_msOrUS(500, 0);
	
	// byte2-bit5:0
	PIO_Clear(SDA_out);
	delay_msOrUS(500, 0);
	PIO_Set(SCL);
	delay_msOrUS(500, 0);
	PIO_Clear(SCL);
	delay_msOrUS(500, 0);
	
	// byte2-bit6:0
	if(intrAddr == 0x84)
		PIO_Set(SDA_out);
	else
		PIO_Clear(SDA_out);
	delay_msOrUS(500, 0);
	PIO_Set(SCL);
	delay_msOrUS(500, 0);
	PIO_Clear(SCL);
	delay_msOrUS(500, 0);
	
	// byte2-bit7:0
	PIO_Clear(SDA_out);
	delay_msOrUS(500, 0);
	PIO_Set(SCL);
	delay_msOrUS(500, 0);
	PIO_Clear(SCL);
	delay_msOrUS(500, 0);
	
	// byte2-bit8:0
	PIO_Clear(SDA_out);
	delay_msOrUS(500, 0);
	PIO_Set(SCL);
	delay_msOrUS(500, 0);
	PIO_Clear(SCL);
	delay_msOrUS(500, 0);
	
	// byte1-bit9:ack
	PIO_Set(SDA_out);
	delay_msOrUS(500, 0);
	PIO_Set(SCL);
	delay_msOrUS(500, 0);
	PIO_Configure(SDA_in, PIO_LISTSIZE(SDA_in));
	arrAck[1] = PIO_Get(SDA_in);
	PIO_Configure(SDA_out, PIO_LISTSIZE(SDA_out));
	PIO_Set(SDA_out);
	PIO_Clear(SCL);
	delay_msOrUS(500, 0);
	
	// stop
	PIO_Clear(SDA_out);
	delay_msOrUS(500, 0);
	PIO_Set(SCL);
	delay_msOrUS(500, 0);
	PIO_Set(SDA_out);
	delay_msOrUS(500, 0);
	
	//printf("touchCtrl_setInternalRdAddr() %d %d\n\r", arrAck[0], arrAck[1]);
}