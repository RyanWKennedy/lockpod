/*
Copyright 2018 Ryan Kennedy

Helper functions/variables for interfacing the AT91SAM9G15 MPU with the following flash IC:
AT25DF321A-SH-B (digikey 1265-1029-5-ND, IC FLASH 32MBIT 100MHZ 8SOIC)
*/

#include <board.h>
#include <libspiflash.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <stdint.h>
#include "SPIStor_EPS.h"
#include "aes.h"
#include "EPSGlobal.h"

/* DMA driver instance for DMA transfers. */
static sDmad dmad;
/* Serial flash driver instance. */
At25 at25;
/* SPI driver instance. */
static Spid spid;
/* Pins to configure for the application. */
static Pin SPIPins[] = {SPI_PINS};

/*
 * ISR for DMA interrupt
 */
static void ISR_DMA(void)
{
    DMAD_Handler(&dmad);
}

int SPI_init(void)
{
	uint32_t jedecId;
    
    /* Initialize DMA driver instance (polling mode = 0) */
    DMAD_Initialize( &dmad, 0);
    /* Enable interrupts */
    IRQ_ConfigureIT(ID_DMAC0, 0, ISR_DMA);
    IRQ_ConfigureIT(ID_DMAC1, 0, ISR_DMA);
    IRQ_EnableIT(ID_DMAC0);
    IRQ_EnableIT(ID_DMAC1);
    printf("DMA driver initialized with IRQ\n\r");

    /* Initialize the SPI and serial flash */
    PIO_Configure(SPIPins, PIO_LISTSIZE(SPIPins));
    SPID_Configure(&spid, SPI0, ID_SPI0, &dmad);
    
    AT25_Configure(&at25, &spid, 0, 0);
    
    printf("SPI and AT25 drivers initialized\n\r");
    
    /* Read the JEDEC ID of the device to identify it*/
    while(1)
    {
        jedecId = AT25D_ReadJedecId(&at25);
        printf("ID read: %x\n\r", jedecId);
        break;
    }
    if (AT25_FindDevice(&at25, jedecId)) {

        printf("%s serial flash detected\n\r", AT25_Name(&at25));
    }
    else {

        printf("Failed to detec the device (JEDEC ID 0x%08X).\n\r", jedecId);
        return 1;
    }
    assert (MAXPAGESIZE >= AT25_PageSize(&at25));
	
	/* Unprotected the flash */
    AT25D_Unprotect(&at25);
    printf("Flash unprotected; SPI_init() complete.\n\r");
	
	return 0;
}

/*
 * Erases the specified 4KB block of the serial dataflash.
 *
 * param\ pAt25  Pointer to an AT25 driver instance.
 * param\ address  Address of the block to erase.
 *
 * return 0 if successful; otherwise returns AT25_ERROR_PROTECTED if the
 * device is protected or AT25_ERROR_BUSY if it is busy executing a command.
 */
unsigned char SPIStor_Erase4KBlock(At25 *pAt25, unsigned int address)
{
    unsigned char status;
    unsigned char error;

    assert(pAt25);

    /* Check that the flash is ready and unprotected */
    AT25_SendCommand(pAt25, AT25_READ_STATUS, 1, &status, 1, 0, 0, 0);
    while (AT25_IsBusy(pAt25)) // wait
    {
    }
    if ((status & AT25_STATUS_RDYBSY) != AT25_STATUS_RDYBSY_READY) {
        //TRACE_ERROR("AT25D_EraseBlock : Flash busy\n\r");
        return AT25_ERROR_BUSY;
    }
    else if ((status & AT25_STATUS_SWP) != AT25_STATUS_SWP_PROTNONE) {
        //TRACE_ERROR("AT25D_EraseBlock : Flash protected\n\r");
        return AT25_ERROR_PROTECTED;
    }

    /* Enable critical write operation */
      AT25D_EnableWrite(pAt25);

    /* Start the block erase command */
    error = AT25_SendCommand(pAt25, 0x20, 4, 0, 0, address, 0, 0);
    assert(!error);

    /* Wait for transfer to finish */
    while (AT25_IsBusy(pAt25))
    {
    }
    /* Poll the Serial flash status register until the operation is achieved */
    AT25D_WaitReady(pAt25);

    return 0;
}

/* 
Writes data to the At25 SPI storage at the specified address, then verifies that the data was
properly written by reading it back and comparing read data to what was supposed to be 
written.  An error message is given if data can't be successfully written.
The page(s) to program must have been erased prior to writing. This function handles 
page boundary crossing automatically.
Arguments:
* pData =  Data buffer.
* size = Number of bytes in buffer.
* wrAddr =  Write address.
*/
void SPIStor_verifiedWrite(unsigned char *pData, unsigned int size, unsigned int wrAddr)
{
	uint8_t cpyBuf[size];
	uint8_t cmpBuf[size];
	memcpy(cpyBuf, pData, size);
	AT25D_Write(&at25, pData, size, wrAddr);
	AT25D_Read(&at25, cmpBuf, size, wrAddr);
	if(memcmp(cpyBuf, cmpBuf, size) == 0)
		return;
	memcpy(pData, cpyBuf, size);
	AT25D_Write(&at25, pData, size, wrAddr);
	int i;
	for(i = 0; i < 1000; ++i){
		AT25D_Read(&at25, cmpBuf, size, wrAddr);
		if(memcmp(cpyBuf, cmpBuf, size) == 0)
			return;
		memcpy(pData, cpyBuf, size);
		AT25D_Write(&at25, pData, size, wrAddr);
	}
	printf("writeFail\n\r");
}

// Deletes all data.  Device set to blank state.
void SPIStor_dataReset(void)
{
	int horiz = 60;
	int vert = 32;
	LCDD_Fill(COLOR_WHITE);
	powerCheck_redraw();
	LCDD_DrawString(horiz, vert, (uint8_t *)"Are you sure you", COLOR_BLACK);
	vert = vert+16;
	LCDD_DrawString(horiz, vert, (uint8_t *)"want to delete all data?", COLOR_BLACK);
	vert = vert+32;
	LCDD_DrawString(horiz, vert, (uint8_t *)"Device would be reset", COLOR_BLACK);
	vert = vert+16;
	LCDD_DrawString(horiz, vert, (uint8_t *)"to a blank state.", COLOR_BLACK);
	if(getYesOrNo()){
		powerCheck_forBadVIn();
		uint8_t buf[2];
		uint16_t totAcnts = SPIStor_getTotlAcnts();
		
		horiz = 60;
		vert = 68;
		LCDD_Fill(COLOR_WHITE);
		LCDD_DrawString(horiz, vert, (uint8_t *)"Deleting information...", COLOR_BLACK);
		vert = vert+32;
		
		SPIStor_Erase4KBlock(&at25, SADDR_INIT);
		buf[0] = 0x00;
		SPIStor_verifiedWrite(buf, 1, SADDR_INIT);
		
		SPIStor_Erase4KBlock(&at25, SADDR_TOTL_ACNTS);
		buf[0] = 0x00;
		buf[1] = 0x00;
		SPIStor_verifiedWrite(buf, 2, SADDR_TOTL_ACNTS);
		
		uint32_t addr = SADDR_ACNT_1;
		if(totAcnts > 0)
			for(addr = SADDR_ACNT_1; addr < SADDR_ACNT_1 + (totAcnts*ACNT_BLOCK_SZ); addr = addr + ACNT_BLOCK_SZ)
				SPIStor_Erase4KBlock(&at25, addr);
		
		LCDD_DrawString(horiz, vert, (uint8_t *)"Shutting down...", COLOR_BLACK);
		printf("All data deleted. finAddr=%d. shutting down...\n\r", addr);
		delay_msOrUS(2000, 1);
		shutdown();
	}
}

uint16_t SPIStor_getTotlAcnts(void)
{
	uint8_t buf[2];
	AT25D_Read(&at25, buf, 2, SADDR_TOTL_ACNTS);
	return (buf[0]*256) + buf[1];
}

// Sets the total account accounts flash storage location to the number passed as 'num'
void SPIStor_setTotlAcnts(uint16_t num)
{
	uint8_t buf[2];
	buf[0] = num/256;
	buf[1] = num - (buf[0]*256);
	
	SPIStor_Erase4KBlock(&at25, SADDR_TOTL_ACNTS);
	SPIStor_verifiedWrite(buf, 2, SADDR_TOTL_ACNTS);
}

void SPIStor_writeAcnt(uint32_t acntAddr, char *acNa, char *usNa, char *paWd, char *note)
{
	powerCheck_forBadVIn();
	uint8_t newIV[IV_SZ];
	getNewIV(newIV);
	
	/* Gather account info strings together into a buffer, then encrypt the buffer. */
	uint8_t buf[maxLineInput*4];
	memcpy(buf, acNa, maxLineInput);
	memcpy(&(buf[maxLineInput]), usNa, maxLineInput);
	memcpy(&(buf[maxLineInput*2]), paWd, maxLineInput);
	memcpy(&(buf[maxLineInput*3]), note, maxLineInput);
	struct AES_ctx ctx;
	AES_init_ctx_iv(&ctx, key, newIV);
    AES_CBC_encrypt_buffer(&ctx, buf, maxLineInput*4);
	
	/* Erase the account space, then write the IV and buffer */
	SPIStor_Erase4KBlock(&at25, acntAddr);
	SPIStor_verifiedWrite(newIV, IV_SZ, acntAddr);
	SPIStor_verifiedWrite(buf, maxLineInput*4, acntAddr+IV_SZ);
}

/* Given an account address, and a string; 
   the name of the account associated with the address will be filled into the string. */
void SPIStor_readAcntName(uint32_t acntAddr, char *acntName)
{
	uint8_t buf[maxLineInput];
	uint8_t acntIV[IV_SZ];
	int i;
	
	AT25D_Read(&at25, acntIV, IV_SZ, acntAddr);
	
	AT25D_Read(&at25, buf, maxLineInput, acntAddr+IV_SZ);
	struct AES_ctx ctx;
    AES_init_ctx_iv(&ctx, key, acntIV);
    AES_CBC_decrypt_buffer(&ctx, buf, maxLineInput);
	
	memcpy(acntName, buf, maxLineInput);
	for(i = 0; i < maxLineInput; ++i)
		if(acntName[i] == 0)
			break;
	if(i == maxLineInput) // if string is 64 chars long, the null char must be post appended
		acntName[maxLineInput] = 0;
}

