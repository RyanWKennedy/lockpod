/*
Copyright 2018 Ryan Kennedy
*/

#include <stdio.h>
#include <string.h>
#include <libspiflash.h>
#include <assert.h>
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>
#include "board.h"
#include "SPIStor_EPS.h"
#include "textEntry.h"
#include "touchCtrl.h"
#include "aes.h"
#include "EPSGlobal.h"
#include "scrypt.h"

////////////////////////////////////////////////////////////////////////////////
// LCD color pallet code reference (defined in external lcd_color.h):
// COLOR_BLACK is 0x000000
// COLOR_WHITE is 0xFFFFFF
// COLOR_BLUE is 0x0000FF
// COLOR_GREEN is 0x00FF00
// COLOR_RED is 0xFF0000
// COLOR_YELLOW is 0xFFFF00
// COLOR_GRAY is 0x808080
// COLOR_LIGHTGREY is 0xD3D3D3
// COLOR_ORANGE is 0xFFA500

uint8_t *pCanvasBuffer = (uint8_t *)ADDR_LCD_BUFFER_CANVAS; // LCD canvas buffer
/* backlight duty cycle value correlates as a percentage of 255,
so an 83% dutCyc would be 83% of 255, or approx 212 (hex d4)
The LCD PWM is at pin PC26 and I have set it for a freq of roughly 64kHz */
uint8_t bBackLight = 0xdc;
uint8_t key[KEY_SZ];
Twid twid; // TWI driver instance
int currentState, currentViewPage, currentViewAccount;
uint8_t IVsInUse[MAX_TOTL_ACNTS+4][IV_SZ]; // List of initialization vectors being used
// +4 is because of MPW IV+salt plus the fact that those two might get regenerated mid-use in doINIT().
/* NOTE: If making a function that backs up the database, a new used IVs array should be created that
   has enough room for (MAX_TOTL_ACNTS times 2)+4, so that all new IVs can be created without running
   out of memory. Or just make this array sz (MAX_TOTL_ACNTS times 2)+4, and hope that doesnt use too much mem. */
int IVsInUse_arSz;
const Pin shutdown_pin[] = {PIO_PA2, PIOA, ID_PIOA, PIO_OUTPUT_0, PIO_DEFAULT};

// ADC and power check system variables
Adc *pAdc;
uint32_t ADCData;
volatile uint32_t powerCheckCounter;

// Interrupt handler for the ADC.
void ADC_IrqHandler(void)
{
    ADCData = 0;

    ADC_GetItStatus( ADC );
    ADCData = ADC_GetLastData( ADC );
    ADCData = ADCData & ADC_LCDR_LDATA_Msk;
}

// Initialize the ADC for power checking (to monitor battery voltage).
void ADC_init(void)
{

	pAdc = ADC;
	PMC_EnablePeripheral( ID_ADC );
    ADC_SetClock( pAdc, BOARD_ADC_FREQ, BOARD_MCK );
    ADC_SetStartupTime( pAdc, 512 );        /* 512us */
    ADC_SetTrackingTime( pAdc, 20000 );     /* 2ms */
    ADC_SetTriggerPeriod( pAdc, 100000000 );/* 100ms */
    ADC_SetTriggerMode( pAdc, ADC_TRGR_TRGMOD_NO_TRIGGER );
    ADC_SetTagEnable( pAdc, 1 );
    ADC_SetSleepMode( pAdc, 1 );
    ADC_SetFastWakeup( pAdc, 1 );
    ADC_SetLowResolution( pAdc, (MAX_DIGITAL == 0xFF) ? 1 : 0 );
    IRQ_ConfigureIT( ID_ADC, 0, ADC_IrqHandler );
    IRQ_EnableIT( ID_ADC );
	ADC_SetSequenceMode( ADC, 0 ); // normal mode
    ADC_EnableChannel( pAdc, 4 );  // pin PB15
	ADC_EnableIt( ADC, ADC_IER_DRDY );
	
}

void powerCheck_readADC(void)
{
	// first run clears old val, second run captures new val??
	ADC_StartConversion( ADC );
	delay_msOrUS(5, 1);
	ADC_StartConversion( ADC );
	delay_msOrUS(5, 1);
}

// Check power level, and redraw on-screen meter to reflect power level reading.
void powerCheck_redraw(void)
{
	powerCheck_readADC();
	//printf("%d\n\r", ADCData);
	
	LCDD_DrawFilledRectangle(457, 5, 475, 17, COLOR_WHITE);
	LCDD_DrawRectangle(457, 5, 18, 12, COLOR_BLACK);
	LCDD_DrawRectangle(456, 4, 20, 14, COLOR_BLACK);
	LCDD_DrawFilledRectangle(453, 8, 455, 13, COLOR_BLACK);
	
	if(ADCData > BTRY_MED){
		LCDD_DrawFilledRectangle(459, 7, 462, 14, COLOR_GREEN);
		LCDD_DrawFilledRectangle(464, 7, 467, 14, COLOR_GREEN);
		LCDD_DrawFilledRectangle(469, 7, 472, 14, COLOR_GREEN);
	}
	if(ADCData > BTRY_LOW && ADCData <= BTRY_MED){
		LCDD_DrawFilledRectangle(464, 7, 467, 14, COLOR_GREEN);
		LCDD_DrawFilledRectangle(469, 7, 472, 14, COLOR_GREEN);
	}
	if(ADCData <= BTRY_LOW)
		LCDD_DrawFilledRectangle(469, 7, 472, 14, COLOR_RED);
}

// Controls power check sample rate
void powerCheck_iterate(void)
{
	++powerCheckCounter;
	
	if(powerCheckCounter == 1000000){
		powerCheck_redraw();
		powerCheckCounter = 0;
	}
}

// Shutdown if battery too low
void powerCheck_forBadVIn(void)
{
	powerCheck_readADC();
	printf("batVal=%d\n\r", ADCData);
	if(ADCData < BTRY_BAD){
		int horiz = 36;
		int vert = 68;
		LCDD_Fill(COLOR_WHITE);
		powerCheck_redraw();
		LCDD_DrawString(horiz, vert, (uint8_t *)"System power too low.", COLOR_BLACK);
		vert = vert+32;
		LCDD_DrawString(horiz, vert, (uint8_t *)"Shutting down...", COLOR_BLACK);
		delay_msOrUS(3000, 1);
		shutdown();
	}
}

/* Draws a button on the display. Returns a coordinate structure mapping
   the area of the button, to be used when detecting touches to the button.
   Arg btnTxt = the text to be drawn on the button.
   Args xTL, yTL = the display coordinates where the top left corner 
              of the button will be drawn.  */
struct btnTouchArea drawButton(char *btnTxt, int xTL, int yTL)
{
	int txtLen = strlen(btnTxt);
	int bluWidth = (txtLen*12) + 2;
	int blkWidth = bluWidth + 4;
	int bluHeight = 17;
	int blkHeight = 21;
	struct btnTouchArea bTA;
	
	bTA.x1 = xTL + 2;
	bTA.y1 = yTL + 2;
	bTA.x2 = bTA.x1 + bluWidth;
	bTA.y2 = bTA.y1 + bluHeight;
	
	LCDD_DrawFilledRectangle(xTL, yTL, xTL+blkWidth, yTL+blkHeight, COLOR_BLACK);
	LCDD_DrawFilledRectangle(xTL+2, yTL+2, xTL+2+bluWidth, yTL+2+bluHeight, COLOR_BLUE);
	LCDD_DrawString(xTL+5, yTL+4, btnTxt, COLOR_WHITE);
	
	return bTA;
}

// Initialize the auto-shutdown system
void autoShutdn_init(void)
{
	// configure shutdown signal pin, and set to low
	PIO_Configure(shutdown_pin, PIO_LISTSIZE(shutdown_pin));
	PIO_Clear(shutdown_pin);
	
	/* Configure timer */
	
	volatile uint32_t dummy;
	// configure the PMC to enable the Timer Counter clock for TC1 channel 1
	PMC_EnablePeripheral(ID_TC1);
	REG_TC1_CCR1 = TC_CCR_CLKDIS; // disable counter
	REG_TC1_IDR1 = 0xFFFFFFFF; //disable interrupts
	dummy = REG_TC1_SR1; // clear status register
	// configure channel mode register
	REG_TC1_CMR1 = TC_CMR_TCCLKS_TIMER_CLOCK4 // ~1MHz
					| TC_CMR_WAVE
					| TC_CMR_WAVSEL_UP;
	REG_TC1_CCR1 = TC_CCR_CLKEN; // enable counter
}

void autoShutdn_timerRst(void)
{
	REG_TC1_CCR1 = TC_CCR_SWTRG; // restart counter
	while(REG_TC1_CV1 > 256); // wait for reset to take hold
}

/* Check if the auto-shutdown timer has been running longer than the specified
   millisecond 'timeout' count argument.  If so, execute a system shutdown. */
void autoShutdn_check(int timeout)
{
	assert(timeout > 0 && timeout < 4000000);
	uint32_t timeoutTot = timeout*1040; // 1ms is roughly 1040 counts
	if(REG_TC1_CV1 < timeoutTot)
		return;
	shutdown();
}

// Execute a system shutdown (device power off)
void shutdown(void)
{
	// Wipe out any old key information in RAM.
	int i;
	for(i = 0; i < KEY_SZ; ++i)
		key[i] = 0;
		
	LCDD_Fill(COLOR_BLACK);
		
	PIO_Set(shutdown_pin);
	while(1);
}

void delay_msOrUS_init()
{
	volatile uint32_t dummy;
	// configure the PMC to enable the Timer Counter clock for TC0 channel 1
	PMC_EnablePeripheral(ID_TC0);
	REG_TC0_CCR1 = TC_CCR_CLKDIS; // disable counter
	REG_TC0_IDR1 = 0xFFFFFFFF; //disable interrupts
	dummy = REG_TC0_SR1; // clear status register
	// configure channel mode register
	REG_TC0_CMR1 = TC_CMR_TCCLKS_TIMER_CLOCK4 // ~1MHz
					| TC_CMR_WAVE
					| TC_CMR_WAVSEL_UP;
	REG_TC0_CCR1 = TC_CCR_CLKEN; // enable counter
}

// set mode to 1 for ms delay, else it will do a (clumsy) micro-sec delay
void delay_msOrUS(int dly, int mode)
{
	assert(dly > 0 && dly < 4000000);
	int dlyTot;
	if(mode == 1)
		dlyTot = dly*1040; // 1ms is roughly 1040 counts
	else{
		if(dly == 1)
			dlyTot = dly+1;
		else
			dlyTot = dly;
	}
	REG_TC0_CCR1 = TC_CCR_SWTRG; // restart counter
	while(REG_TC0_CV1 > 256); // wait for reset to take hold
	while(REG_TC0_CV1 < dlyTot);
}	

// TWI interrupt handler. Forwards the interrupt to the TWI driver handler.
void TWI0_IrqHandler(void)
{
    TWID_Handler(&twid);
}

// Get an initial system startup read of the IVs in use
void getIVsInUse(void)
{
	int i;
	uint16_t totAcnts = SPIStor_getTotlAcnts();
	printf("totAcnts=%d\n\r", totAcnts);
	uint32_t addr;
	uint8_t buf[IV_SZ];
	AT25D_Read(&at25, buf, 1, SADDR_INIT);
	
	IVsInUse_arSz = 0;
	/* If system not in 'blank' condition, find the IVs stored in it */
	if(buf[0] != 0){
		AT25D_Read(&at25, buf, IV_SZ, SADDR_MPW_SALT);
		for(i = 0; i < IV_SZ; ++i)
			IVsInUse[IVsInUse_arSz][i] = buf[i];
		++IVsInUse_arSz;
		AT25D_Read(&at25, buf, IV_SZ, SADDR_MPW_IV);
		for(i = 0; i < IV_SZ; ++i)
			IVsInUse[IVsInUse_arSz][i] = buf[i];
		++IVsInUse_arSz;
		if(totAcnts > 0)
			for(addr = SADDR_ACNT_1; addr < SADDR_ACNT_1 + (totAcnts*ACNT_BLOCK_SZ); addr = addr + ACNT_BLOCK_SZ){
				//printf("addr=%d\n\r", addr);
				AT25D_Read(&at25, buf, IV_SZ, addr);
				for(i = 0; i < IV_SZ; ++i)
					IVsInUse[IVsInUse_arSz][i] = buf[i];
				++IVsInUse_arSz;
			}
	}
}

void getNewIV(uint8_t *newIV16B)
{
	assert(RAND_MAX >= 65536);
	
	int i, randRes;
	
	// Generate new IV
	for(i = 0; i < IV_SZ; i=i+2){
		randRes = rand() % 65536;
		newIV16B[i] = randRes/256;
		newIV16B[i+1] = randRes - (newIV16B[i]*256);
		//printf("randRes=%d b1=%d b2=%d\n\r", randRes, newIV16B[i], newIV16B[i+1]);
	}
	
	int matchFlag = 0;
	int doneFlag = 0;
	/* Loop the makes sure new IV is not identical to any existing ones */
	while(doneFlag == 0){
		for(i = 0; i < IVsInUse_arSz; ++i)
			// Compare new IV with existing IVs
			if(memcmp(newIV16B, IVsInUse[i], IV_SZ) == 0){
				matchFlag = 1;
				break;
			}
		if(matchFlag == 1){ // If new IV matches an old IV, generate another new IV
			for(i = 0; i < IV_SZ; i=i+2){
				randRes = rand() % 65536;
				newIV16B[i] = randRes/256;
				newIV16B[i+1] = randRes - (newIV16B[i]*256);
			}
			matchFlag = 0;
		}
		else
			doneFlag = 1;
	}
	for(i = 0; i < IV_SZ; ++i)
		IVsInUse[IVsInUse_arSz][i] = newIV16B[i];
	++IVsInUse_arSz;
}

/* Draw 'yes' and 'no' buttons on the screen, and wait for the user to touch
one of them.  Return 1 if choice is yes, return 0 if choice is no.
*/
int getYesOrNo(void)
{
	LCDD_DrawFilledRectangle(60, 170, 180, 238, COLOR_BLUE); // for 'yes'
	LCDD_DrawFilledRectangle(300, 170, 420, 238, COLOR_BLUE); // for 'no'
	LCDD_DrawString(102, 197, (uint8_t *)"YES", COLOR_YELLOW);
	LCDD_DrawString(348, 197, (uint8_t *)"NO", COLOR_YELLOW);
	struct touchEvent tE;
	while(1){  // loop until 'yes' or 'no' is touched
		powerCheck_iterate();  // sample power level in this wait loop
		tE = touchCtrl_getTouch();
		if(tE.touchValid == 1){
			if(tE.x > 60 && tE.x < 180 && tE.y > 170 && tE.y < 238)
				return 1;
			if(tE.x > 300 && tE.x < 420 && tE.y > 170 && tE.y < 238)
				return 0;
		}
	}
}

// Draw an 'ok' button on the screen, and wait for the user to touch it.
void getOk(void)
{
	LCDD_DrawFilledRectangle(180, 170, 300, 238, COLOR_BLUE);
	LCDD_DrawString(228, 197, (uint8_t *)"OK", COLOR_YELLOW);
	struct touchEvent tE;
	while(1){  // loop until 'ok' is touched
		powerCheck_iterate();  // sample power level in this wait loop
		tE = touchCtrl_getTouch();
		if(tE.touchValid == 1)
			if(tE.x > 180 && tE.x < 300 && tE.y > 170 && tE.y < 238)
				return;
	}
}

/*
Auto-generates a password using random characters.  User can choose number of characters.
Generated password is filled into the string argument that's passed in.
*/
void autogenPassword(char *paWd)
{	
	char charCountTxt[maxLineInput+1];
	int i, allDigits, rangeGood, charCount;
	int horiz = 60;
	int vert = 68;
	LCDD_Fill(COLOR_WHITE);
	powerCheck_redraw();
	LCDD_DrawString(horiz, vert, (uint8_t *)"Would you like to auto-generate", COLOR_BLACK);
	vert = vert+16;
	LCDD_DrawString(horiz, vert, (uint8_t *)"a new password?", COLOR_BLACK);

	if(getYesOrNo()){
		while(1){
			allDigits = 1;
			rangeGood = 0;
			charCountTxt[0] = 0;
			displayKb(0);
			powerCheck_redraw();
			LCDD_DrawString(4, 4, (uint8_t *)"Choose the Character Count", COLOR_BLACK);
			LCDD_DrawString(4, 32, (uint8_t *)"Enter a number between 8 and 64:", COLOR_ORANGE);
			putGetLine(charCountTxt, 0);
			
			// Input validation
			for(i = 0; i < strlen(charCountTxt); ++i)
				if(isdigit(charCountTxt[i]) == 0)
					allDigits = 0;
			
			// Input validation
			charCount = atoi(charCountTxt);
			if(charCount >= 8 && charCount <= 64)
				rangeGood = 1;
			
			if(allDigits && rangeGood)
				break;
				
			// If loop break above wasn't reached, the input was invalid. Loop continues...
			LCDD_Fill(COLOR_WHITE);
			powerCheck_redraw();
			horiz = 162;
			vert = 136;
			LCDD_DrawString(horiz, vert, (uint8_t *)"Invalid input.", COLOR_BLACK);
			getOk();
		}
		paWd[0] = 0;
		char gluChar[2];
		unsigned int r;
		unsigned int range = (keyArSize-1)*2; // range of random numbers I want
		unsigned int buckets = RAND_MAX / range; 
		unsigned int limit = buckets * range;
		for(i = 0; i < charCount; ++i){
			/* Create equal size buckets all in a row, then fire randomly towards
			 * the buckets until you land in one of them. All buckets are equally
			 * likely. If you land off the end of the line of buckets, try again. 
			 * The 'for' loop repeats this for the number of desired valid random results. */
			do{
				r = rand();
			}while(r >= limit);
			r = r / buckets;
			gluChar[0] = randCharHelper(r);
			gluChar[1] = 0;
			strcat(paWd, gluChar);
			//printf("In range rand result: %d %c\n\r", r, gluChar[0]);
		}
	}
}

/* Draws a line that slopes up from left to right (used for up/down buttons).
   It gets drawn with respect to the 'x' and 'y' coordinate arguments. */
void drawAngleUp(int x, int y)
{
	int i;
	for(i = 0; i < 7; ++i){
		LCDD_DrawPixel(x+i, y-i, COLOR_WHITE);
		LCDD_DrawPixel(x+i, y-i-1, COLOR_WHITE);
		LCDD_DrawPixel(x+i, y-i-2, COLOR_WHITE);
	}
}

/* Draws a line that slopes down from left to right (used for up/down buttons).
   It gets drawn with respect to the 'x' and 'y' coordinate arguments. */
void drawAngleDown(int x, int y)
{
	int i;
	for(i = 0; i < 7; ++i){
		LCDD_DrawPixel(x+i, y-6+i, COLOR_WHITE);
		LCDD_DrawPixel(x+i, y-7+i, COLOR_WHITE);
		LCDD_DrawPixel(x+i, y-8+i, COLOR_WHITE);
	}
}

/* Moves the account at database index passed as 'origNum' argument 
   to a new (user desired) location index. */
void moveAcnt(int origNum)
{
	uint16_t totAcnts = SPIStor_getTotlAcnts();
	
	// Prepare user prompt text
	char totAcntsTxt[maxLineInput+1] = "";
	sprintf(totAcntsTxt, "%d:", totAcnts);
	char line[maxLineInput+1] = "Enter a number between 1 and ";
	strcat(line, totAcntsTxt);
	
	// Prompt user for desired new account destination index.
	char destNumTxt[maxLineInput+1];
	int i, allDigits, rangeGood, destNum;
	int cancel = 0;
	while(1){
		allDigits = 1;
		rangeGood = 0;
		destNumTxt[0] = 0;
		displayKb(0);
		powerCheck_redraw();
		LCDD_DrawString(4, 4, (uint8_t *)"Move to New List Position", COLOR_BLACK);
		LCDD_DrawString(4, 32, line, COLOR_ORANGE);
		cancel = putGetLine(destNumTxt, 1);
		
		if(cancel)
			break;
		
		// Input validation
		for(i = 0; i < strlen(destNumTxt); ++i)
			if(isdigit(destNumTxt[i]) == 0)
				allDigits = 0;
		
		// Input validation
		destNum = atoi(destNumTxt);
		if(destNum >= 1 && destNum <= totAcnts)
			rangeGood = 1;
		
		if(allDigits && rangeGood)
			break;
			
		// If loop break directly above wasn't reached, the input was invalid. Loop continues...
		LCDD_Fill(COLOR_WHITE);
		powerCheck_redraw();
		LCDD_DrawString(162, 136, (uint8_t *)"Invalid input.", COLOR_BLACK);
		getOk();
	}
	//printf("origNum=%d destNum=%d\n\r", origNum, destNum);
	if(cancel == 0){
		powerCheck_forBadVIn();
		int horiz = 60;
		int vert = 68;
		LCDD_Fill(COLOR_WHITE);
		LCDD_DrawString(horiz, vert, (uint8_t *)"Moving account.", COLOR_BLACK);
		vert = vert+32;
		LCDD_DrawString(horiz, vert, (uint8_t *)"Please wait...", COLOR_BLACK);
	
		uint32_t addr = SADDR_ACNT_1 + ((origNum-1)*ACNT_BLOCK_SZ); // Original address of the account to move
		uint16_t acntBufSz = IV_SZ + (maxLineInput*4); // Total number of bytes for a full account record
		uint8_t acntBufMove[acntBufSz];
		uint8_t acntBufShift[acntBufSz];
		
		AT25D_Read(&at25, acntBufMove, acntBufSz, addr); // Read the data of the account to move into memory
		
		int count, countStop;
		if(origNum > destNum){ // This will mean the account must be moved to a smaller account number.
			countStop = origNum - destNum;
			// Loop that shifts accounts "down" the list, to make room for the moved account.
			for(count = 0; count < countStop; ++count){
				AT25D_Read(&at25, acntBufShift, acntBufSz, addr-ACNT_BLOCK_SZ);
				SPIStor_Erase4KBlock(&at25, addr);
				SPIStor_verifiedWrite(acntBufShift, acntBufSz, addr);
				addr = addr - ACNT_BLOCK_SZ;
			}
			SPIStor_Erase4KBlock(&at25, addr);
			SPIStor_verifiedWrite(acntBufMove, acntBufSz, addr);
		}
		if(origNum < destNum){ // This will mean the account must be moved to a larger account number.
			countStop = destNum - origNum;
			// Loop that shifts accounts "up" the list, to make room for the moved account.
			for(count = 0; count < countStop; ++count){
				AT25D_Read(&at25, acntBufShift, acntBufSz, addr+ACNT_BLOCK_SZ);
				SPIStor_Erase4KBlock(&at25, addr);
				SPIStor_verifiedWrite(acntBufShift, acntBufSz, addr);
				addr = addr + ACNT_BLOCK_SZ;
			}
			SPIStor_Erase4KBlock(&at25, addr);
			SPIStor_verifiedWrite(acntBufMove, acntBufSz, addr);
		}
		autoShutdn_timerRst();
	}
}

// Deletes currentViewAccount
void deleteAcnt(void)
{
	powerCheck_forBadVIn();
	int horiz = 60;
	int vert = 68;
	LCDD_Fill(COLOR_WHITE);
	LCDD_DrawString(horiz, vert, (uint8_t *)"Deleting account.", COLOR_BLACK);
	vert = vert+32;
	LCDD_DrawString(horiz, vert, (uint8_t *)"Please wait...", COLOR_BLACK);

	uint16_t totAcnts = SPIStor_getTotlAcnts();
	uint32_t addr = SADDR_ACNT_1 + (currentViewAccount*ACNT_BLOCK_SZ);
	
	--totAcnts;
	SPIStor_setTotlAcnts(totAcnts);
	
	/* If it's the last account, just delete it; no accounts must move to fill gap. */
	if(currentViewAccount == (totAcnts)){
		SPIStor_Erase4KBlock(&at25, addr);
		return;
	}
	
	/* If it's not the last account, shift all following accounts */
	uint32_t shiftAddr = addr + ACNT_BLOCK_SZ;
	uint16_t acntBufSz = IV_SZ + (maxLineInput*4); // Total number of bytes for a full account record
	uint8_t buf[acntBufSz];
	for(addr; addr < SADDR_ACNT_1 + (totAcnts*ACNT_BLOCK_SZ); addr = addr + ACNT_BLOCK_SZ){
		AT25D_Read(&at25, buf, acntBufSz, shiftAddr);
		SPIStor_Erase4KBlock(&at25, addr);
		SPIStor_verifiedWrite(buf, acntBufSz, addr);
		shiftAddr = shiftAddr + ACNT_BLOCK_SZ;
	}
	
	autoShutdn_timerRst();
}

void doINIT(void)
{
	int i;
	int cancel = 0;
	char paWdTxt[maxLineInput+1] = "";
	uint8_t initialState;
	AT25D_Read(&at25, &initialState, 1, SADDR_INIT);
	
	autogenPassword(paWdTxt); // give option to auto-generate
	displayKb(0);
	powerCheck_redraw();
	LCDD_DrawString(4, 4, (uint8_t *)"Master Password Setup", COLOR_BLACK);
	LCDD_DrawString(4, 32, (uint8_t *)"Enter desired master password:", COLOR_ORANGE);
	if(initialState == 1){
		// If system already initialized, give option to cancel new pw setup
		cancel = putGetLine(paWdTxt, 1);
		if(cancel == 1){
			currentState = HOME;
			return;
		}
	}
	else
		putGetLine(paWdTxt, 2);
		
	// Make a temporary backup of key
	uint8_t oldKey[KEY_SZ];
	for(i = 0; i < KEY_SZ; ++i)
		oldKey[i] = key[i];
	
	// Prepare crypto inputs
	int paWdLen = strlen(paWdTxt);
	uint8_t paWdBytes[65]; // TODO: retry VLA and test reliability
	memcpy(paWdBytes, paWdTxt, paWdLen);
	uint8_t newIV[IV_SZ];
	getNewIV(newIV);
	uint8_t salt[IV_SZ];
	for(i = 0; i < IV_SZ; ++i)
		salt[i] = newIV[i]; // Just use IV generator to make a master password salt
	getNewIV(newIV); // Get another new IV for master password key encryption
	
	// Derive encryption key from password.
	int horiz = 36;
	int vert = 68;
	LCDD_Fill(COLOR_WHITE);
	LCDD_DrawString(horiz, vert, (uint8_t *)"Deriving master key from password.", COLOR_BLACK);
	vert = vert+32;
	LCDD_DrawString(horiz, vert, (uint8_t *)"May take a few minutes...", COLOR_BLACK);
	
	// Run twice to make sure no mem errors during derivation.
	uint8_t checkKey[KEY_SZ];
	while(1){
		crypto_scrypt(paWdBytes, paWdLen, salt, IV_SZ, key, KEY_SZ);
		crypto_scrypt(paWdBytes, paWdLen, salt, IV_SZ, checkKey, KEY_SZ);
		if(memcmp(key, checkKey, KEY_SZ) == 0)
			break;
	}
	
	// Encrypt the key data
	struct AES_ctx ctx;
	uint8_t encKey[KEY_SZ];
	for(i = 0; i < KEY_SZ; ++i)
		encKey[i] = key[i];
	AES_init_ctx_iv(&ctx, key, newIV);
	AES_CBC_encrypt_buffer(&ctx, encKey, KEY_SZ);
	
	// Check that there's sufficient system power
	powerCheck_forBadVIn();
	
	// Commit all to storage
	SPIStor_Erase4KBlock(&at25, SADDR_INIT);
	initialState = 1;
	SPIStor_verifiedWrite(&initialState, 1, SADDR_INIT);
	SPIStor_verifiedWrite(salt, IV_SZ, SADDR_MPW_SALT);
	SPIStor_verifiedWrite(newIV, IV_SZ, SADDR_MPW_IV);
	SPIStor_verifiedWrite(encKey, KEY_SZ, SADDR_MPW);
	
	uint16_t totAcnts = SPIStor_getTotlAcnts();
	uint32_t addr;
	uint8_t buf[maxLineInput*4];
	// If there's already information stored, it all has to be re-encrypted based on the new password.
	if(totAcnts > 0){
		horiz = 60;
		vert = 68;
		LCDD_Fill(COLOR_WHITE);
		LCDD_DrawString(horiz, vert, (uint8_t *)"Re-encrypting the database", COLOR_BLACK);
		vert = vert+16;
		LCDD_DrawString(horiz, vert, (uint8_t *)"with new password key.", COLOR_BLACK);
		vert = vert+16;
		LCDD_DrawString(horiz, vert, (uint8_t *)"May take a few minutes...", COLOR_BLACK);
		for(addr = SADDR_ACNT_1; addr < SADDR_ACNT_1 + (totAcnts*ACNT_BLOCK_SZ); addr = addr + ACNT_BLOCK_SZ){
			AT25D_Read(&at25, newIV, IV_SZ, addr);
			AT25D_Read(&at25, buf, maxLineInput*4, addr+IV_SZ);
			AES_init_ctx_iv(&ctx, oldKey, newIV);
			AES_CBC_decrypt_buffer(&ctx, buf, maxLineInput*4);
			AES_init_ctx_iv(&ctx, key, newIV);
			AES_CBC_encrypt_buffer(&ctx, buf, maxLineInput*4);
			SPIStor_Erase4KBlock(&at25, addr);
			SPIStor_verifiedWrite(newIV, IV_SZ, addr);
			SPIStor_verifiedWrite(buf, maxLineInput*4, addr+IV_SZ);
		}
	}
	
	getIVsInUse();
	for(i = 0; i < KEY_SZ; ++i){
		oldKey[i] = 0;
		checkKey[i] = 0;
	}
		
	autoShutdn_timerRst();
	currentState = HOME;
}

void doLOCKED(void)
{
	int i;
	// Wipe out any old key information in RAM.
	for(i = 0; i < KEY_SZ; ++i)
		key[i] = 0;
	
	// Get user input.
	char paWdTxt[maxLineInput+1] = "";
	displayKb(1);
	powerCheck_redraw();
	LCDD_DrawString(4, 4, (uint8_t *)"Password Safe Locked", COLOR_BLACK);
	LCDD_DrawString(4, 32, (uint8_t *)"Enter master password to proceed:", COLOR_ORANGE);
	putGetLine(paWdTxt, 2);
	
	// Derive a key from the entered password.
	uint8_t salt[IV_SZ];
	int horiz = 60;
	int vert = 68;
	LCDD_Fill(COLOR_WHITE);
	LCDD_DrawString(horiz, vert, (uint8_t *)"Unlocking device.", COLOR_BLACK);
	vert = vert+32;
	LCDD_DrawString(horiz, vert, (uint8_t *)"Takes a minute or two...", COLOR_BLACK);
	AT25D_Read(&at25, salt, IV_SZ, SADDR_MPW_SALT);
	int paWdLen = strlen(paWdTxt);
	uint8_t paWdBytes[65]; // TODO: retry VLA and test reliability
	memcpy(paWdBytes, paWdTxt, paWdLen);
	uint8_t enteredKey[KEY_SZ];
	crypto_scrypt(paWdBytes, paWdLen, salt, IV_SZ, enteredKey, KEY_SZ);
	
	// Perform a decryption on the (encrypted) stored key using the entered key.
	uint8_t paWdIV[IV_SZ];
	uint8_t storedKey[KEY_SZ];
	AT25D_Read(&at25, paWdIV, IV_SZ, SADDR_MPW_IV);
	AT25D_Read(&at25, storedKey, KEY_SZ, SADDR_MPW);
	struct AES_ctx ctx;
	AES_init_ctx_iv(&ctx, enteredKey, paWdIV);
	AES_CBC_decrypt_buffer(&ctx, storedKey, KEY_SZ);
	
	autoShutdn_timerRst();
	
	// If the keys don't match, don't permit entry.
	if(memcmp(enteredKey, storedKey, KEY_SZ) != 0){
		currentState = LOCKED;
		LCDD_Fill(COLOR_WHITE);
		powerCheck_redraw();
		horiz = 158;
		vert = 136;
		LCDD_DrawString(horiz, vert, (uint8_t *)"Access Denied!", COLOR_BLACK);
		LCDD_DrawFilledRectangle(180, 170, 300, 238, COLOR_BLUE);
		LCDD_DrawString(228, 197, (uint8_t *)"OK", COLOR_YELLOW);
		struct touchEvent tE;
		while(1){ 
			powerCheck_iterate();  // sample power level in this wait loop
			tE = touchCtrl_getTouch();
			if(tE.touchValid == 1){
				if(tE.x > 180 && tE.x < 300 && tE.y > 170 && tE.y < 238)
					break; // 'ok' touched
				if(tE.x > 0 && tE.x < 50 && tE.y > 0 && tE.y < 50){
					//test_scrypt(); // diagnostic
					//perhaps add a data reset touch sequence here
					//break;
					delay_msOrUS(1, 1);
				}
			}
		}
		return;
	}

	// If the keys do match, execution reaches here (key is valid).
	for(i = 0; i < KEY_SZ; ++i){
		key[i] = enteredKey[i];
		enteredKey[i] = 0;
		storedKey[i] = 0;
	}
	
	currentState = HOME;
}

void doHOME(void)
{
	//printf("Current state = HOME (%d),\n\r", currentState);
	
	// reset 'view' state upon returning 'home'; zero equals page 1, account 1
	currentViewPage = 0;
	currentViewAccount = 0;
	
	LCDD_Fill(COLOR_WHITE);
	powerCheck_redraw();
	int vert = 10;
	LCDD_DrawString(180, vert, (uint8_t *)"Home Menu", COLOR_BLACK);
	vert = vert+32;
	struct btnTouchArea btnPwr = drawButton("Power off", 60, vert);
	struct btnTouchArea btnLock = drawButton("Lock", 240, vert);
	vert = vert+32;
	struct btnTouchArea btnView = drawButton("View all accounts", 60, vert);
	vert = vert+32;
	struct btnTouchArea btnAdd = drawButton("Add new account", 60, vert);
	vert = vert+48;
	struct btnTouchArea btnInit = drawButton("Change master password", 60, vert);
	vert = vert+32;
	struct btnTouchArea btnDataRst = drawButton("Delete all information", 60, vert);
	
	struct touchEvent tE;
	while(1){  // loop until a button is touched
		powerCheck_iterate();  // sample power level in this wait loop
		tE = touchCtrl_getTouch();
		if(tE.touchValid == 1){
			if(tE.x > btnPwr.x1 && tE.x < btnPwr.x2 && tE.y > btnPwr.y1 && tE.y < btnPwr.y2)
				shutdown();
			if(tE.x > btnLock.x1 && tE.x < btnLock.x2 && tE.y > btnLock.y1 && tE.y < btnLock.y2){
				currentState = LOCKED;
				return;
			}
			if(tE.x > btnView.x1 && tE.x < btnView.x2 && tE.y > btnView.y1 && tE.y < btnView.y2){
				currentState = VIEW;
				return;
			}
			if(tE.x > btnAdd.x1 && tE.x < btnAdd.x2 && tE.y > btnAdd.y1 && tE.y < btnAdd.y2){
				currentState = ADD;
				return;
			}
			if(tE.x > btnInit.x1 && tE.x < btnInit.x2 && tE.y > btnInit.y1 && tE.y < btnInit.y2){
				currentState = INIT;
				return;
			}
			if(tE.x > btnDataRst.x1 && tE.x < btnDataRst.x2 && tE.y > btnDataRst.y1 && tE.y < btnDataRst.y2){
				SPIStor_dataReset();
				return;
			}
			//if(tE.x > 470 && tE.x < 480 && tE.y > 262 && tE.y < 272){
			//	currentState = TEST;
			//	return;
			//}
		}
	}
}

// Add new account to database
void doADD(void)
{
	//printf("Current state = ADD (%d),\n\r", currentState);
	
	uint16_t totAcnts = SPIStor_getTotlAcnts();
	int i, acNaLen;
	int cancel = 0;
	
	if(totAcnts == MAX_TOTL_ACNTS){
		LCDD_Fill(COLOR_WHITE);
		powerCheck_redraw();
		int x = 30;
		int y = 68;
		LCDD_DrawString(x, y, (uint8_t *)"Can't add another account.", COLOR_BLACK);
		y = y+32;
		LCDD_DrawString(x, y, (uint8_t *)"Total account number maximum reached.", COLOR_BLACK);
		getOk();
		currentState = HOME;
		return;
	}
	
	char acNa[maxLineInput+1] = "";
	char usNa[maxLineInput+1] = "";
	char paWd[maxLineInput+1] = "";
	char note[maxLineInput+1] = "";
	
	// Get account name from user
	displayKb(0);
	powerCheck_redraw();
	LCDD_DrawString(4, 4, (uint8_t *)"Add New Account", COLOR_BLACK);
	LCDD_DrawString(4, 32, (uint8_t *)"Enter account name...", COLOR_ORANGE);
	cancel = putGetLine(acNa, 1);
	
	// Prepare account name string for display in next stages
	char acNaRow1[maxLineInput+1] = "";
	char acNaRow2[maxLineInput+1] = "";
	acNaLen = strlen(acNa);
	if(acNaLen <= 32){
		for(i = 0; i < acNaLen; ++i)
			acNaRow1[i] = acNa[i];
		acNaRow1[acNaLen] = 0;
	}
	else{
		for(i = 0; i < 32; ++i)
			acNaRow1[i] = acNa[i];
		acNaRow1[32] = 0;
		strcat(acNaRow2, acNa+32);
	}
	
	if(cancel == 0){
		// Get username for new account
		displayKb(0);
		powerCheck_redraw();
		LCDD_DrawString(4, 4, (uint8_t *)"Add", COLOR_BLACK);
		LCDD_DrawString(4, 32, (uint8_t *)"username...", COLOR_ORANGE);
		LCDD_DrawString(52, 4, acNaRow1, COLOR_LIGHTGREY);
		LCDD_DrawString(52, 20, acNaRow2, COLOR_LIGHTGREY);
		cancel = putGetLine(usNa, 1);
	}
	
	if(cancel == 0){
		// Get password for new account
		autogenPassword(paWd); // give option to auto-generate
		displayKb(0);
		powerCheck_redraw();
		LCDD_DrawString(4, 4, (uint8_t *)"Add", COLOR_BLACK);
		LCDD_DrawString(4, 32, (uint8_t *)"passwor", COLOR_ORANGE);
		
		// We're in a tight spot... so draw a shortend 'd'
		LCDD_DrawRectangle(87, 42, 1, 2, COLOR_ORANGE); // d construct
		LCDD_DrawRectangle(88, 41, 1, 4, COLOR_ORANGE); // d construct
		LCDD_DrawRectangle(89, 40, 1, 6, COLOR_ORANGE); // d construct
		LCDD_DrawRectangle(90, 40, 5, 2, COLOR_ORANGE); // d construct
		LCDD_DrawRectangle(90, 44, 5, 2, COLOR_ORANGE); // d construct
		LCDD_DrawRectangle(95, 36, 2, 10, COLOR_ORANGE); // d construct
		
		LCDD_DrawString(100, 32, (uint8_t *)"...", COLOR_ORANGE);
		LCDD_DrawString(52, 4, acNaRow1, COLOR_LIGHTGREY);
		LCDD_DrawString(52, 20, acNaRow2, COLOR_LIGHTGREY);
		cancel = putGetLine(paWd, 1);
	}
	
	if(cancel == 0){
		int horiz = 60;
		int vert = 68;
		LCDD_Fill(COLOR_WHITE);
		powerCheck_redraw();
		LCDD_DrawString(horiz, vert, (uint8_t *)"Would you like to add any notes", COLOR_BLACK);
		vert = vert+16;
		LCDD_DrawString(horiz, vert, (uint8_t *)"for this account?", COLOR_BLACK);
		if(getYesOrNo()){
			// Get notes for new account
			displayKb(0);
			powerCheck_redraw();
			LCDD_DrawString(4, 4, (uint8_t *)"Add", COLOR_BLACK);
			LCDD_DrawString(4, 32, (uint8_t *)"notes...", COLOR_ORANGE);
			LCDD_DrawString(52, 4, acNaRow1, COLOR_LIGHTGREY);
			LCDD_DrawString(52, 20, acNaRow2, COLOR_LIGHTGREY);
			cancel = putGetLine(note, 1);
		}
	}
	
	if(cancel == 0){
		uint32_t addr = SADDR_ACNT_1 + (totAcnts*ACNT_BLOCK_SZ);
		SPIStor_writeAcnt(addr, acNa, usNa, paWd, note);
		++totAcnts;
		SPIStor_setTotlAcnts(totAcnts);
		int totalViewPages = totAcnts/ACNTS_PER_VIEW_PG;
		if((totAcnts%ACNTS_PER_VIEW_PG) != 0)
			++totalViewPages;
		currentViewPage = totalViewPages - 1;
		currentState = VIEW;
		return;
	}
	
	currentState = HOME;
}

// View list of accounts
void doVIEW(void)
{
	//printf("Current state = VIEW (%d),\n\r", currentState);
	
	uint16_t totAcnts = SPIStor_getTotlAcnts();
	int i, x, y;
	
	if(totAcnts == 0){
		LCDD_Fill(COLOR_WHITE);
		powerCheck_redraw();
		LCDD_DrawString(30, 84, (uint8_t *)"No accounts exist to view right now.", COLOR_BLACK);
		getOk();
		currentState = HOME;
		return;
	}
	int totalViewPages = totAcnts/ACNTS_PER_VIEW_PG;
	if((totAcnts%ACNTS_PER_VIEW_PG) != 0)
		++totalViewPages;
	
	// UI constructs for all pages
	LCDD_Fill(COLOR_WHITE);
	powerCheck_redraw();
	LCDD_DrawString(165, 4, (uint8_t *)"Account List", COLOR_BLACK);
	LCDD_DrawRectangle(0, 23, 430, 2, COLOR_BLACK);
	LCDD_DrawRectangle(41, 0, 2, 272, COLOR_BLACK);
	LCDD_DrawString(275, 247, (uint8_t *)"Go to Home Menu", COLOR_BLUE);
	LCDD_DrawRectangle(275, 263, 178, 2, COLOR_BLUE);
	
	// Draw page-up button
	if(currentViewPage > 0){
		LCDD_DrawFilledRectangle(0, 0, 40, 22, COLOR_BLUE);
		x = 13;
		y = 14;
		drawAngleUp(x, y);
		x = x+7;
		drawAngleDown(x, y);
	}
	
	uint32_t addr;
	char acNa[maxLineInput+1] = "";
	char acNumTxt[maxLineInput+1] = "";
	char acNaRow1[maxLineInput+1];
	char acNaRow2[maxLineInput+1];
	int acNaLen, vertRef, acntI;
	uint8_t rowActive[6] = {0, 0, 0, 0, 0, 0};
	int rowI = 0;
	// Loop that displays account listings
	for(acntI = currentViewPage*ACNTS_PER_VIEW_PG; acntI < (currentViewPage*ACNTS_PER_VIEW_PG) + ACNTS_PER_VIEW_PG; ++acntI){
		vertRef = 25 + (rowI*36);
		rowActive[rowI] = 1;
		++rowI;
		
		LCDD_DrawFilledRectangle(43, vertRef, 427, vertRef+33, COLOR_BLUE);
		LCDD_DrawRectangle(0, vertRef+34, 430, 2, COLOR_BLACK);
		LCDD_DrawRectangle(428, vertRef, 2, 36, COLOR_BLACK);
		
		// Draw 'move' button
		if(totAcnts > 1){
			LCDD_DrawString(433, vertRef+7, (uint8_t *)"m", COLOR_BLUE);
			LCDD_DrawString(444, vertRef+7, (uint8_t *)"o", COLOR_BLUE);
			LCDD_DrawString(455, vertRef+7, (uint8_t *)"v", COLOR_BLUE);
			LCDD_DrawString(466, vertRef+7, (uint8_t *)"e", COLOR_BLUE);
			LCDD_DrawRectangle(433, vertRef+23, 44, 2, COLOR_BLUE);
		}
		
		// Prepare account name and number strings for display
		addr = SADDR_ACNT_1 + (acntI*ACNT_BLOCK_SZ);
		SPIStor_readAcntName(addr, acNa);
		acNaRow1[0] = 0;
		acNaRow2[0] = 0;
		acNaLen = strlen(acNa);
		if(acNaLen <= 32){
			for(i = 0; i < acNaLen; ++i)
				acNaRow1[i] = acNa[i];
			acNaRow1[acNaLen] = 0;
		}
		else{
			for(i = 0; i < 32; ++i)
				acNaRow1[i] = acNa[i];
			acNaRow1[32] = 0;
			strcat(acNaRow2, acNa+32);
		}
		sprintf(acNumTxt, "%d", acntI+1);
		
		// Display number and name
		LCDD_DrawString(4, vertRef+2, acNumTxt, COLOR_BLACK);
		LCDD_DrawString(45, vertRef+2, acNaRow1, COLOR_WHITE);
		LCDD_DrawString(45, vertRef+18, acNaRow2, COLOR_WHITE);
		
		if(acntI == totAcnts-1)
			break;
	}
	
	// Draw page-down button
	if(currentViewPage < totalViewPages-1){
		LCDD_DrawFilledRectangle(0, 241, 40, 272, COLOR_BLUE);
		x = 13;
		y = 266;
		drawAngleDown(x, y);
		x = x+7;
		drawAngleUp(x, y);
	}
	
	struct touchEvent tE;
	while(1){  // loop until a button is touched
		powerCheck_iterate();  // sample power level in this wait loop
		tE = touchCtrl_getTouch();
		if(tE.touchValid == 1){
			if(tE.x > 275 && tE.x < 453 && tE.y > 247 && tE.y < 270){
				// 'Go to Home' button
				currentState = HOME;
				return;
			}
			if(tE.x > 0 && tE.x < 40 && tE.y > 0 && tE.y < 22 && currentViewPage > 0){
				// page-up button
				--currentViewPage;
				currentState = VIEW;
				return;
			}
			if(tE.x > 0 && tE.x < 40 && tE.y > 241 && tE.y < 272 && currentViewPage < (totalViewPages-1)){
				// page-down button
				++currentViewPage;
				currentState = VIEW;
				return;
			}
			if(rowActive[0] == 1 && tE.x > 43 && tE.x < 427 && tE.y > 25 && tE.y < 58){ // row0 acnt button
				currentViewAccount = currentViewPage*ACNTS_PER_VIEW_PG;
				currentState = VIEW2;
				return;
			}
			if(rowActive[0] == 1 && tE.x > 433 && tE.x < 477 && tE.y > 32 && tE.y < 52 && totAcnts > 1){ // row0 move button
				currentState = VIEW;
				moveAcnt(1 + (currentViewPage*ACNTS_PER_VIEW_PG));
				return;
			}
			if(rowActive[1] == 1 && tE.x > 43 && tE.x < 427 && tE.y > 61 && tE.y < 94){ // row1 acnt button
				currentViewAccount = currentViewPage*ACNTS_PER_VIEW_PG + 1;
				currentState = VIEW2;
				return;
			}
			if(rowActive[1] == 1 && tE.x > 433 && tE.x < 477 && tE.y > 68 && tE.y < 88){ // row1 move button
				currentState = VIEW;
				moveAcnt(1 + (currentViewPage*ACNTS_PER_VIEW_PG + 1));
				return;
			}
			if(rowActive[2] == 1 && tE.x > 43 && tE.x < 427 && tE.y > 97 && tE.y < 130){ // row2 acnt button
				currentViewAccount = currentViewPage*ACNTS_PER_VIEW_PG + 2;
				currentState = VIEW2;
				return;
			}
			if(rowActive[2] == 1 && tE.x > 433 && tE.x < 477 && tE.y > 104 && tE.y < 124){ // row2 move button
				currentState = VIEW;
				moveAcnt(1 + (currentViewPage*ACNTS_PER_VIEW_PG + 2));
				return;
			}
			if(rowActive[3] == 1 && tE.x > 43 && tE.x < 427 && tE.y > 133 && tE.y < 166){ // row3 acnt button
				currentViewAccount = currentViewPage*ACNTS_PER_VIEW_PG + 3;
				currentState = VIEW2;
				return;
			}
			if(rowActive[3] == 1 && tE.x > 433 && tE.x < 477 && tE.y > 140 && tE.y < 160){ // row3 move button
				currentState = VIEW;
				moveAcnt(1 + (currentViewPage*ACNTS_PER_VIEW_PG + 3));
				return;
			}
			if(rowActive[4] == 1 && tE.x > 43 && tE.x < 427 && tE.y > 169 && tE.y < 202){ // row4 acnt button
				currentViewAccount = currentViewPage*ACNTS_PER_VIEW_PG + 4;
				currentState = VIEW2;
				return;
			}
			if(rowActive[4] == 1 && tE.x > 433 && tE.x < 477 && tE.y > 176 && tE.y < 196){ // row4 move button
				currentState = VIEW;
				moveAcnt(1 + (currentViewPage*ACNTS_PER_VIEW_PG + 4));
				return;
			}
			if(rowActive[5] == 1 && tE.x > 43 && tE.x < 427 && tE.y > 205 && tE.y < 238){ // row5 acnt button
				currentViewAccount = currentViewPage*ACNTS_PER_VIEW_PG + 5;
				currentState = VIEW2;
				return;
			}
			if(rowActive[5] == 1 && tE.x > 433 && tE.x < 477 && tE.y > 212 && tE.y < 232){ // row5 move button
				currentState = VIEW;
				moveAcnt(1 + (currentViewPage*ACNTS_PER_VIEW_PG + 5));
				return;
			}
		}
	}
}

// View a single account
void doVIEW2(void)
{
	//printf("Current state = VIEW2 (%d),\n\r", currentState);
	
	int i, fieldI;
	uint8_t buf[maxLineInput*4];
	uint8_t acntIV[IV_SZ];
	uint32_t addr = SADDR_ACNT_1 + (currentViewAccount*ACNT_BLOCK_SZ);
	AT25D_Read(&at25, acntIV, IV_SZ, addr);
	
	// Strings in 2D arrays for easy looping
	char fieldTitle[4][maxLineInput+1];
	char field[4][maxLineInput+1];
	strcpy(fieldTitle[0], "Account name:");
	field[0][0] = 0;
	strcpy(fieldTitle[1], "Username:");
	field[1][0] = 0;
	strcpy(fieldTitle[2], "Password:");
	field[2][0] = 0;
	strcpy(fieldTitle[3], "Notes:");
	field[3][0] = 0;
	
	// Decrypt account data
	AT25D_Read(&at25, buf, maxLineInput*4, addr+IV_SZ);
	struct AES_ctx ctx;
    AES_init_ctx_iv(&ctx, key, acntIV);
    AES_CBC_decrypt_buffer(&ctx, buf, maxLineInput*4);
	
	LCDD_Fill(COLOR_WHITE);
	powerCheck_redraw();
	
	// Parse account data into strings, and display them.  Also display 'edit' buttons.
	char stRow1[maxLineInput+1];
	char stRow2[maxLineInput+1];
	int stLen;
	int vert = 4;
	int horiz = 28;
	for(fieldI = 0; fieldI < 4; ++fieldI){
		memcpy(field[fieldI], &(buf[maxLineInput*fieldI]), maxLineInput);
		for(i = 0; i < maxLineInput; ++i)
			if(field[fieldI][i] == 0)
				break;
		if(i == maxLineInput) // if string is 64 chars long, the null char must be post appended
			field[fieldI][maxLineInput] = 0;
		
		stRow1[0] = 0;
		stRow2[0] = 0;
		stLen = strlen(field[fieldI]);
		if(stLen <= 32){
			for(i = 0; i < stLen; ++i)
				stRow1[i] = field[fieldI][i];
			stRow1[stLen] = 0;
		}
		else{
			for(i = 0; i < 32; ++i)
				stRow1[i] = field[fieldI][i];
			stRow1[32] = 0;
			strcat(stRow2, field[fieldI]+32);
		}
		
		LCDD_DrawString(horiz+336, vert, (uint8_t *)"edit", COLOR_BLUE);
		LCDD_DrawRectangle(horiz+336, vert+16, 48, 2, COLOR_BLUE);
		LCDD_DrawString(horiz, vert, fieldTitle[fieldI], COLOR_ORANGE);
		vert = vert+20;
		LCDD_DrawString(horiz, vert, stRow1, COLOR_BLACK);
		vert = vert+16;
		LCDD_DrawString(horiz, vert, stRow2, COLOR_BLACK);
		vert = vert+24;
	}
	
	// Draw 'back' button
	horiz = 340;
	vert = 243;
	LCDD_DrawFilledRectangle(horiz, vert+1, horiz+75, vert+22, COLOR_BLUE);
	LCDD_DrawRectangle(horiz, vert, 77, 23, COLOR_BLACK);
	LCDD_DrawRectangle(horiz+1, vert+1, 75, 21, COLOR_BLACK);
	LCDD_DrawString(horiz+5, vert+4, (uint8_t *)"<", COLOR_YELLOW);
	LCDD_DrawString(horiz+27, vert+4, (uint8_t *)"Back", COLOR_WHITE);
	
	// Draw 'delete account' button
	horiz = 80;
	vert = 243;
	LCDD_DrawFilledRectangle(horiz, vert+1, horiz+195, vert+22, COLOR_BLUE);
	LCDD_DrawRectangle(horiz, vert, 197, 23, COLOR_BLACK);
	LCDD_DrawRectangle(horiz+1, vert+1, 195, 21, COLOR_BLACK);
	LCDD_DrawString(horiz+6, vert+2, (uint8_t *)"x", COLOR_RED);
	LCDD_DrawString(horiz+27, vert+4, (uint8_t *)"Delete Account", COLOR_WHITE);
	
	int cancel = 0;
	struct touchEvent tE;
	while(1){  // loop until a button is touched
		powerCheck_iterate();  // sample power level in this wait loop
		tE = touchCtrl_getTouch();
		if(tE.touchValid == 1){
			if(tE.x > 340 && tE.x < 417 && tE.y > 243 && tE.y < 270){ // 'back' button
				currentState = VIEW;
				return;
			}
			if(tE.x > 80 && tE.x < 277 && tE.y > 243 && tE.y < 270){ // 'delete' button
				horiz = 60;
				vert = 68;
				LCDD_Fill(COLOR_WHITE);
				powerCheck_redraw();
				LCDD_DrawString(horiz, vert, (uint8_t *)"Are you sure you want to", COLOR_BLACK);
				vert = vert+16;
				LCDD_DrawString(horiz, vert, (uint8_t *)"delete this entire account?", COLOR_BLACK);
				if(getYesOrNo()){
					deleteAcnt();
					uint16_t totAcnts = SPIStor_getTotlAcnts();
					if(totAcnts == 0)
						currentState = HOME;
					else
						currentState = VIEW;
					return;
				}
				currentState = VIEW2;
				return;
			}
			if(tE.x > 364 && tE.x < 412 && tE.y > 4 && tE.y < 24){ // Edit account name button
				currentState = VIEW2;
				displayKb(0);
				powerCheck_redraw();
				LCDD_DrawString(4, 4, (uint8_t *)"Edit", COLOR_BLACK);
				LCDD_DrawString(4, 32, fieldTitle[0], COLOR_ORANGE);
				cancel = putGetLine(field[0], 1);
				if(cancel == 0)
					SPIStor_writeAcnt(addr, field[0], field[1], field[2], field[3]);
				return;
			}
			if(tE.x > 364 && tE.x < 412 && tE.y > 64 && tE.y < 84){ // Edit username button
				currentState = VIEW2;
				displayKb(0);
				powerCheck_redraw();
				LCDD_DrawString(4, 4, (uint8_t *)"Edit", COLOR_BLACK);
				LCDD_DrawString(4, 32, fieldTitle[1], COLOR_ORANGE);
				cancel = putGetLine(field[1], 1);
				if(cancel == 0)
					SPIStor_writeAcnt(addr, field[0], field[1], field[2], field[3]);
				return;
			}
			if(tE.x > 364 && tE.x < 412 && tE.y > 124 && tE.y < 144){ // Edit password button
				currentState = VIEW2;
				autogenPassword(field[2]); // give option to auto-generate new password
				displayKb(0);
				powerCheck_redraw();
				LCDD_DrawString(4, 4, (uint8_t *)"Edit", COLOR_BLACK);
				LCDD_DrawString(4, 32, fieldTitle[2], COLOR_ORANGE);
				cancel = putGetLine(field[2], 1);
				if(cancel == 0)
					SPIStor_writeAcnt(addr, field[0], field[1], field[2], field[3]);
				return;
			}
			if(tE.x > 364 && tE.x < 412 && tE.y > 184 && tE.y < 204){ // Edit notes button
				currentState = VIEW2;
				displayKb(0);
				powerCheck_redraw();
				LCDD_DrawString(4, 4, (uint8_t *)"Edit", COLOR_BLACK);
				LCDD_DrawString(4, 32, fieldTitle[3], COLOR_ORANGE);
				cancel = putGetLine(field[3], 1);
				if(cancel == 0)
					SPIStor_writeAcnt(addr, field[0], field[1], field[2], field[3]);
				return;
			}
		}
	}
}

// void doTEST(void)
// {
	// printf("Current state = TEST (%d),\n\r", currentState);
	
	// int i = 0;
	
	// LCDD_Fill(COLOR_WHITE);
	// powerCheck_redraw();
	// int vert = 4;
	// LCDD_DrawString(4, vert, (uint8_t *)"test", COLOR_BLACK);
	
	// while(1){
		// ++powerCheckCounter;
		// if(powerCheckCounter == 1000000){
			// powerCheck_redraw();
			// printf("%d\n\r", ADCData);
			// powerCheckCounter = 0;
			// ++i;
		// }
		// if(i == 5)
			// break;
	// }
	
	// currentState = HOME;
// }