/*
Copyright 2018 Ryan Kennedy
EPS_mainsafe program
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libspiflash.h>
#include "board.h"
#include "EPSGlobal.h"
#include "textEntry.h"
#include "touchCtrl.h"
#include "SPIStor_EPS.h"
#include "aes.h"

extern int main(void)
{
	WDT_Disable(WDT); // disable watchdog timer

	printf("--------\n\r");
	printf("Begin application program debug console:\n\r");
	printf("program compiled %s %s \n\r\n\r", __DATE__, __TIME__ );
	
	// for control of program start time
	// printf("Press a character key to continue...");
	// DBGU_GetChar();
	// printf("\n\r");
	
	///////////////////////////////////////////////////////////////////////////
	// Early setup functions
	
	autoShutdn_init(); // Initialize auto-shutdown system.
	delay_msOrUS_init(); // To use the delay function.
	SPI_init(); // For interacting with SPI flash storage device.
	
	///////////////////////////////////////////////////////////////////////////
	// Touch-panel prep (apparently needs to precede LCD prep)
	
	/* Configure TWI pins. */
	Pin TWIPins[] = {PINS_TWI0}; // TWI PIO pins to configure
	PIO_Configure(TWIPins, PIO_LISTSIZE(TWIPins));

	/* Enable TWI peripheral clock */
	PMC_EnablePeripheral(ID_TWI0);

	/* Configure TWI */
	TWI_ConfigureMaster(TWI0, TWIClock, BOARD_MCK);
	TWID_Initialize(&twid, TWI0);

	/* Configure TWI interrupts */
	IRQ_ConfigureIT(ID_TWI0, 0, TWI0_IrqHandler);
	IRQ_EnableIT(ID_TWI0);
	
	touchCtrl_setup();
	touchCtrl_init();
	
	//////////////////////////////////////////////////////////////////////////////////////////////
	// LCD prep
	
	LCDD_Initialize();
	LCDD_SetBacklight(bBackLight);
	LCDD_CreateCanvas(LCDD_OVR1, pCanvasBuffer, 24, 0, 0, BOARD_LCD_WIDTH, BOARD_LCD_HEIGHT);
	
	//////////////////////////////////////////////////////////////////////////////////////////////
	// General prep
	
	ADC_init(); // For checking system input voltage (battery power check).
	powerCheckCounter = 0;
	getIVsInUse();
	autoShutdn_timerRst(); // Start running auto-shutdown timer here.
	setKbLayout(); // Random seeding done here.
	uint8_t initialState;
	AT25D_Read(&at25, &initialState, 1, SADDR_INIT);
	if(initialState == 0)
		currentState = INIT;
	else
		currentState = LOCKED;
	
	//////////////////////////////////////////////////////////////////////////////////////////////
	// Execute state-machine loop
	
	while(1){
		switch(currentState){
			case INIT: doINIT(); break;
			case LOCKED: doLOCKED(); break;
			case HOME: doHOME(); break;
			case VIEW: doVIEW(); break;
			case VIEW2: doVIEW2(); break;
			case ADD: doADD(); break;
			//case TEST: doTEST(); break;
		}
	}
	
	return 0;
}
