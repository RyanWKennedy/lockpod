/*
Copyright 2018 Ryan Kennedy
*/

#ifndef EPSGLOBAL_H
#define EPSGLOBAL_H

// LCD canvas buffer address: DRAM + 1MB
#define ADDR_LCD_BUFFER_CANVAS (EBI_DDRSDRC_ADDR + 0x100000)
// size based on dimensions
//#define SIZE_LCD_BUFFER (BOARD_LCD_WIDTH * BOARD_LCD_HEIGHT * 4)

#define TWIClock 400000 // TWI clock frequency in Hz
#define TWIPeriphSlavAddr 0x51 // I used this for TWI sys testing
#define datBufSize 64 // data buffer size (value choice arbitrary)
#define maxLineInput 64 // maximum length of a user inputed text line
#define ACNTS_PER_VIEW_PG 6 // Max number of accounts shown on a single 'view' page
#define IV_SZ 16 // Initialization vector size
#define KEY_SZ 32 // 32 byte key size (for AES 256-bit)
#define MAX_TOTL_ACNTS 875 // Maximum number of accounts
#define AUTOSD_TIMEOUT 240000 // milliseconds of inactivity before auto-shutdown occurs
#define BOARD_ADC_FREQ 300000 // ADC controller frequency
#define MAX_DIGITAL 0x3FF // for ADC

/* Battery level threshold values (compared to ADC digital readings). Hardware under voltage lockout
   shuts down system at around a value of 460. */
#define BTRY_BAD 500
#define BTRY_LOW 570
#define BTRY_MED 680

/* States */
//#define TEST 0
#define INIT 1
#define LOCKED 2
#define HOME 3
#define ADD 4
#define VIEW 5
#define VIEW2 6

/* Can hold the coordinates defining the touch area of a button. */
struct btnTouchArea{
	int x1;
	int y1;
	int x2;
	int y2;
};

extern uint8_t *pCanvasBuffer;
extern uint8_t bBackLight;

extern Twid twid;
extern At25 at25;
extern uint8_t key[KEY_SZ];
extern int currentState, currentViewPage, currentViewAccount;
extern uint8_t IVsInUse[MAX_TOTL_ACNTS+4][IV_SZ]; 
extern int IVsInUse_arSz;
extern Adc *pAdc;
extern uint32_t ADCData;
extern volatile uint32_t powerCheckCounter;

void ADC_IrqHandler(void);
void ADC_init(void);
void powerCheck_readADC(void);
void powerCheck_redraw(void);
void powerCheck_iterate(void);
void powerCheck_forBadVIn(void);
struct btnTouchArea drawButton(char *btnTxt, int xTL, int yTL);
void autoShutdn_init(void);
void autoShutdn_timerRst(void);
void autoShutdn_check(int timeout);
void shutdown(void);
void delay_msOrUS_init(void);
void delay_msOrUS(int dly, int mode);
void TWI0_IrqHandler(void);
void getIVsInUse(void);
void getNewIV(uint8_t *newIV16B);
int getYesOrNo(void);
void getOk(void);
void autogenPassword(char *paWd);
void drawAngleUp(int x, int y);
void drawAngleDown(int x, int y);
void moveAcnt(int origNum);
void deleteAcnt(void);

void doINIT(void);
void doLOCKED(void);
void doHOME(void);
void doADD(void);
void doVIEW(void);
void doVIEW2(void);
//void doTEST(void);

#endif