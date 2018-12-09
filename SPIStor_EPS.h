/*
Copyright 2018 Ryan Kennedy
*/

#ifndef SPISTOR_EPS_H
#define SPISTOR_EPS_H


/* Account block size will match minimum num of bytes that can be erased in one op*/
#define ACNT_BLOCK_SZ 0x1000
/* Maximum device page size in bytes. */
#define MAXPAGESIZE 256
/* SPI peripheral pins to configure to access the serial flash. */
#define SPI_PINS PINS_SPI0, PIN_SPI0_NPCS0
/* Storage addresses */
#define SADDR_INIT 0x90000
#define SADDR_MPW_SALT 0x90040
#define SADDR_MPW_IV 0x90080
#define SADDR_MPW 0x90100
#define SADDR_TOTL_ACNTS 0x91000
#define SADDR_KB_SET 0x91010
#define SADDR_ACNT_1 0x92000


int SPI_init(void);
unsigned char SPIStor_Erase4KBlock(At25 *pAt25, unsigned int address);
void SPIStor_verifiedWrite(unsigned char *pData, unsigned int size, unsigned int wrAddr);
void SPIStor_dataReset(void);
uint16_t SPIStor_getTotlAcnts(void);
void SPIStor_setTotlAcnts(uint16_t num);
void SPIStor_writeAcnt(uint32_t acntAddr, char *acNa, char *usNa, char *paWd, char *note);
void SPIStor_readAcntName(uint32_t acntAddr, char *acntName);

#endif