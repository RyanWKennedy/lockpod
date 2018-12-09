/*
Copyright 2018 Ryan Kennedy
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

const struct keyCharDat keyChars[] = {
	{ // 0
		.primChar = '`',
		.secChar = '~'
	},{ // 1
		.primChar = '1',
		.secChar = '!'
	},{ // 2
		.primChar = '2',
		.secChar = '@'
	},{ // 3
		.primChar = '3',
		.secChar = '#'
	},{ // 4
		.primChar = '4',
		.secChar = '$'
	},{ // 5
		.primChar = '5',
		.secChar = '%'
	},{ // 6
		.primChar = '6',
		.secChar = '^'
	},{ // 7
		.primChar = '7',
		.secChar = '&'
	},{ // 8
		.primChar = '8',
		.secChar = '*'
	},{ // 9
		.primChar = '9',
		.secChar = '('
	},{ // 10
		.primChar = '0',
		.secChar = ')'
	},{ // 11
		.primChar = '-',
		.secChar = '_'
	},{ // 12
		.primChar = '=',
		.secChar = '+'
	},{ // 13
		.primChar = 'q',
		.secChar = 'Q'
	},{ // 14
		.primChar = 'w',
		.secChar = 'W'
	},{ // 15
		.primChar = 'e',
		.secChar = 'E'
	},{ // 16
		.primChar = 'r',
		.secChar = 'R'
	},{ // 17
		.primChar = 't',
		.secChar = 'T'
	},{ // 18
		.primChar = 'y',
		.secChar = 'Y'
	},{ // 19
		.primChar = 'u',
		.secChar = 'U'
	},{ // 20
		.primChar = 'i',
		.secChar = 'I'
	},{ // 21
		.primChar = 'o',
		.secChar = 'O'
	},{ // 22
		.primChar = 'p',
		.secChar = 'P'
	},{ // 23
		.primChar = '[',
		.secChar = '{'
	},{ // 24
		.primChar = ']',
		.secChar = '}'
	},{ // 25
		.primChar = '\\',
		.secChar = '|'
	},{ // 26
		.primChar = 'a',
		.secChar = 'A'
	},{ // 27
		.primChar = 's',
		.secChar = 'S'
	},{ // 28
		.primChar = 'd',
		.secChar = 'D'
	},{ // 29
		.primChar = 'f',
		.secChar = 'F'
	},{ // 30
		.primChar = 'g',
		.secChar = 'G'
	},{ // 31
		.primChar = 'h',
		.secChar = 'H'
	},{ // 32
		.primChar = 'j',
		.secChar = 'J'
	},{ // 33
		.primChar = 'k',
		.secChar = 'K'
	},{ // 34
		.primChar = 'l',
		.secChar = 'L'
	},{ // 35
		.primChar = ';',
		.secChar = ':'
	},{ // 36
		.primChar = '\'',
		.secChar = '"'
	},{ // 37
		.primChar = 'z',
		.secChar = 'Z'
	},{ // 38
		.primChar = 'x',
		.secChar = 'X'
	},{ // 39
		.primChar = 'c',
		.secChar = 'C'
	},{ // 40
		.primChar = 'v',
		.secChar = 'V'
	},{ // 41
		.primChar = 'b',
		.secChar = 'B'
	},{ // 42
		.primChar = 'n',
		.secChar = 'N'
	},{ // 43
		.primChar = 'm',
		.secChar = 'M'
	},{ // 44
		.primChar = ',',
		.secChar = '<'
	},{ // 45
		.primChar = '.',
		.secChar = '>'
	},{ // 46
		.primChar = '/',
		.secChar = '?'
	},{ // 47
		.primChar = ' ',
		.secChar = ' '
	}
};

const struct keyPositDat keyPosits[] = {
	/*0*/{.x=2,.y=147},/*1*/{.x=39,.y=147},/*2*/{.x=76,.y=147},/*3*/{.x=113,.y=147},/*4*/{.x=150,.y=147},
	/*5*/{.x=187,.y=147},/*6*/{.x=224,.y=147},/*7*/{.x=261,.y=147},/*8*/{.x=298,.y=147},/*9*/{.x=335,.y=147},
	/*10*/{.x=372,.y=147},/*11*/{.x=409,.y=147},/*12*/{.x=446,.y=147},/*13*/{.x=2,.y=179},/*14*/{.x=39,.y=179},
	/*15*/{.x=76,.y=179},/*16*/{.x=113,.y=179},/*17*/{.x=150,.y=179},/*18*/{.x=187,.y=179},/*19*/{.x=224,.y=179},
	/*20*/{.x=261,.y=179},/*21*/{.x=298,.y=179},/*22*/{.x=335,.y=179},/*23*/{.x=372,.y=179},/*24*/{.x=409,.y=179},
	/*25*/{.x=446,.y=179},/*26*/{.x=2,.y=211},/*27*/{.x=39,.y=211},/*28*/{.x=76,.y=211},/*29*/{.x=113,.y=211},
	/*30*/{.x=150,.y=211},/*31*/{.x=187,.y=211},/*32*/{.x=224,.y=211},/*33*/{.x=261,.y=211},/*34*/{.x=298,.y=211},
	/*35*/{.x=335,.y=211},/*36*/{.x=372,.y=211},/*37*/{.x=2,.y=243},/*38*/{.x=39,.y=243},/*39*/{.x=76,.y=243},
	/*40*/{.x=113,.y=243},/*41*/{.x=150,.y=243},/*42*/{.x=187,.y=243},/*43*/{.x=224,.y=243},/*44*/{.x=261,.y=243},
	/*45*/{.x=298,.y=243},/*46*/{.x=335,.y=243},/*47*/{.x=372,.y=243}
};

// normal layout reference
const uint8_t keyOrd_normalRef[keyArSize] = {
	0,1,2,3,4,5,6,7,8,9,10,11,12, // first row
	13,14,15,16,17,18,19,20,21,22,23,24,25, // second row
	26,27,28,29,30,31,32,33,34,35,36, // third row
	37,38,39,40,41,42,43,44,45,46,47}; // fourth row

// random layout reference 1
const uint8_t keyOrd_randomRef1[keyArSize] = {
	37,10,3,4,18,6,47,8,2,9,11,13,40,15,0,17,5,19,20,43,22,33,24,30,27,28,
	29,26,31,32,23,34,35,36,1,38,39,14,46,42,21,44,45,41,7,25,12,16};
	
// random layout reference 2
const uint8_t keyOrd_randomRef2[keyArSize] = {
	20,44,21,42,27,24,7,25,16,12,40,31,26,29,11,22,34,35,36,1,4,38,
	15,2,19,5,17,46,43,23,33,41,30,45,28,14,37,13,3,39,18,6,32,8,0,9,47,10};
	
uint8_t keyOrd[2][keyArSize];

struct keyDisp currentKb[keyArSize];

/* Sets the keyboard layouts (both random and normal).  It is called once, just at
   system startup; this function is where the random number generator is seeded. */
void setKbLayout(void)
{
	int i;
	for(i = 0; i < keyArSize; ++i)
		keyOrd[0][i] = keyOrd_normalRef[i];
	
	int vert = 8;
	LCDD_Fill(COLOR_WHITE);
	powerCheck_redraw();
	LCDD_DrawString(8, vert, (uint8_t *)"Device start up...", COLOR_BLACK);
	vert = vert+16;
	LCDD_DrawString(8, vert, (uint8_t *)"For security, provide some", COLOR_BLACK);
	vert = vert+16;
	LCDD_DrawString(8, vert, (uint8_t *)"random seed data by tapping", COLOR_BLACK);
	vert = vert+16;
	LCDD_DrawString(8, vert, (uint8_t *)"anywhere on the screen in a", COLOR_BLACK);
	vert = vert+16;
	LCDD_DrawString(8, vert, (uint8_t *)"random location.", COLOR_BLACK);
	struct touchEvent tE;
	while(1){  // loop until screen touched
		powerCheck_iterate();  // sample power level in this wait loop
		tE = touchCtrl_getTouch();
		if(tE.touchValid == 1)
			break;
	}
	int seedVal = tE.x*tE.y;
	int xtra = powerCheckCounter%100;
	seedVal = seedVal + xtra;
	//printf("touch= %d %d, xtra=%d, seedVal=%d\n\r", tE.x, tE.y, xtra, seedVal);
	srand(seedVal);
	unsigned int anchorPoint;
	unsigned int range = keyArSize; // range of random numbers I want
	unsigned int buckets = RAND_MAX / keyArSize; 
	unsigned int limit = buckets * range;
	/* Create equal size buckets all in a row, then fire randomly towards
	 * the buckets until you land in one of them. All buckets are equally
	 * likely. If you land off the end of the line of buckets, try again. */
	do{
		anchorPoint = rand();
	}while(anchorPoint >= limit);
	anchorPoint = anchorPoint / buckets;
	//printf("In range rand result: %d\n\r", anchorPoint);
	
	uint8_t kbSetting;
	AT25D_Read(&at25, &kbSetting, 1, SADDR_KB_SET);
	//printf("kbSetting=%d\n\r", kbSetting);
	if(kbSetting == 0xFF){
		int tossup = rand()%2;
		if(tossup == 0){
			for(i = 0; i < anchorPoint; ++i)
				keyOrd[1][i] = keyOrd_randomRef1[keyArSize-anchorPoint+i];
			for(i = anchorPoint; i < keyArSize; ++i)
				keyOrd[1][i] = keyOrd_randomRef1[i-anchorPoint];
			printf("kbSetRand1\n\r");
		}
		else{
			for(i = 0; i < anchorPoint; ++i)
				keyOrd[1][i] = keyOrd_randomRef2[keyArSize-anchorPoint+i];
			for(i = anchorPoint; i < keyArSize; ++i)
				keyOrd[1][i] = keyOrd_randomRef2[i-anchorPoint];
			printf("kbSetRand2\n\r");
		}
	}
	else{
		for(i = 0; i < keyArSize; ++i)
			keyOrd[1][i] = keyOrd_normalRef[i];
		printf("kbSetRandNorm\n\r");
	}
	//for(i = 0; i < keyArSize; ++i)
	//	printf("%d,", keyOrd[1][i]);
	//printf("\n\r");
}

void displayKey(struct keyDisp currentKey)
{
	//LCDD_DrawFilledRectangle(currentKey.x, currentKey.y, currentKey.x + 31, currentKey.y + 26, COLOR_WHITE); // replaced by a single 'fill LCD white' command
	if(currentKey.primChar == ' '){
		LCDD_DrawRectangle(currentKey.x + 4, currentKey.y + 17, 2, 6, COLOR_BLACK);
		LCDD_DrawRectangle(currentKey.x + 6, currentKey.y + 21, 20, 2, COLOR_BLACK);
		LCDD_DrawRectangle(currentKey.x + 26, currentKey.y + 17, 2, 6, COLOR_BLACK);
	}
	else
		LCDD_DrawChar(currentKey.x + 4, currentKey.y + 9, currentKey.primChar, COLOR_BLACK);
		LCDD_DrawChar(currentKey.x + 18, currentKey.y + 4, currentKey.secChar, COLOR_LIGHTGREY);
}

/* Draws the keyboard on the display.
   If arg layout=0, a 'normal' (QWERTY) looking keyboard is shown.
   If arg layout=1, a random key layout is shown. */
void displayKb(uint8_t layout)
{
	LCDD_Fill(COLOR_WHITE);

	// key frame horizontals
	LCDD_DrawFilledRectangle(0, 105, 480, 146, COLOR_BLUE);
	LCDD_DrawFilledRectangle(0, 174, 480, 178, COLOR_BLUE);
	LCDD_DrawFilledRectangle(0, 206, 480, 210, COLOR_BLUE);
	LCDD_DrawFilledRectangle(0, 238, 480, 242, COLOR_BLUE);
	LCDD_DrawFilledRectangle(0, 270, 480, 271, COLOR_BLUE);
	
	// key frame verticals
	LCDD_DrawFilledRectangle(0, 147, 1, 269, COLOR_BLUE);
	LCDD_DrawFilledRectangle(34, 147, 38, 269, COLOR_BLUE);
	LCDD_DrawFilledRectangle(71, 147, 75, 269, COLOR_BLUE);
 	LCDD_DrawFilledRectangle(108, 147, 112, 269, COLOR_BLUE);
	LCDD_DrawFilledRectangle(145, 147, 149, 269, COLOR_BLUE);
	LCDD_DrawFilledRectangle(182, 147, 186, 269, COLOR_BLUE);
	LCDD_DrawFilledRectangle(219, 147, 223, 269, COLOR_BLUE);
	LCDD_DrawFilledRectangle(256, 147, 260, 269, COLOR_BLUE);
	LCDD_DrawFilledRectangle(293, 147, 297, 269, COLOR_BLUE);
	LCDD_DrawFilledRectangle(330, 147, 334, 269, COLOR_BLUE);
	LCDD_DrawFilledRectangle(367, 147, 371, 269, COLOR_BLUE);
	LCDD_DrawFilledRectangle(404, 147, 408, 269, COLOR_BLUE);
	LCDD_DrawFilledRectangle(441, 147, 445, 210, COLOR_BLUE);
	LCDD_DrawFilledRectangle(478, 147, 479, 269, COLOR_BLUE);

	int i;
	for(i = 0; i < keyArSize; ++i){
		currentKb[i].primChar = keyChars[keyOrd[layout][i]].primChar;
		currentKb[i].secChar = keyChars[keyOrd[layout][i]].secChar;
		currentKb[i].x = keyPosits[i].x;
		currentKb[i].y = keyPosits[i].y;
	}
	for(i = 0; i < keyArSize; ++i)
		displayKey(currentKb[i]);
	
	// control keys
	LCDD_DrawFilledRectangle(10, 115, 76, 137, COLOR_WHITE);
	LCDD_DrawString(14, 119, (uint8_t *)"Enter", COLOR_BLACK);
	LCDD_DrawFilledRectangle(87, 115, 244, 137, COLOR_WHITE);
	LCDD_DrawString(91, 119, (uint8_t *)"Clear", COLOR_BLACK);
	LCDD_DrawString(159, 119, (uint8_t *)"textbox", COLOR_BLACK);
	LCDD_DrawFilledRectangle(255, 115, 469, 137, COLOR_WHITE);
	LCDD_DrawString(259, 119, (uint8_t *)"Move", COLOR_BLACK);
	LCDD_DrawString(315, 119, (uint8_t *)"cursor", COLOR_BLACK);
	LCDD_DrawString(405, 119, (uint8_t *)"<  >", COLOR_BLACK);
	LCDD_DrawString(413, 220, (uint8_t *)"Delet", COLOR_BLACK);
	LCDD_DrawString(413, 252, (uint8_t *)"Shift", COLOR_LIGHTGREY);
}

/*
This function gets a string of text input from the user, while also showing it on the display.
That string is the argument 'line'.  The argument 'xtraButn' is a flag which allows for 
configuring the display of an additional button on screen.
This function returns zero if 'enter' is pressed, or one if 'cancel' is pressed.
*/
int putGetLine(char * line, uint8_t xtraButn){
	int x, y;
	x = 4;
	y = 50;
	LCDD_DrawFilledRectangle(x, y, x+388, y+36, COLOR_BLACK); // textbox
	char backupLine[maxLineInput+1] = "";
	if(xtraButn == 1){ 
		// draw cancel button if required
		LCDD_DrawFilledRectangle(x+392, y+9, x+467, y+30, COLOR_BLUE);
		LCDD_DrawRectangle(x+392, y+8, 77, 23, COLOR_BLACK);
		LCDD_DrawRectangle(x+393, y+9, 75, 21, COLOR_BLACK);
		LCDD_DrawString(x+397, y+12, (uint8_t *)"Cancel", COLOR_WHITE);
		// backup original line in case cancel is touched
		strcpy(backupLine, line);
	}
	if(xtraButn == 2){ 
		// draw 'power off' button if required
		LCDD_DrawFilledRectangle(x+404, y+2, x+468, y+36, COLOR_BLUE);
		LCDD_DrawRectangle(x+404, y, 67, 38, COLOR_BLACK);
		LCDD_DrawRectangle(x+405, y+1, 65, 36, COLOR_BLACK);
		LCDD_DrawString(x+409, y+3, (uint8_t *)"Power", COLOR_WHITE);
		LCDD_DrawString(x+409, y+20, (uint8_t *)"off", COLOR_WHITE);
	}
	x = x+4;
	y = y+3;
	volatile uint32_t cursorDly = 0;
	uint8_t cursorVis = 1;
	uint8_t cursorI = 0;
	int cursorX, cursorY;
	struct touchEvent tE;
	uint8_t keyI = 255;
	uint8_t shiftActive = 0;
	uint8_t i, lineLen;
	char c[2];
	// line parts 1 and 2 are for splicing line of text if cursor is in middle
	char linePar1[maxLineInput+1] = "";
	char linePar2[maxLineInput+1] = "";
	// line rows 1 and 2 are because the text box has two rows of text
	char lineRow1[(maxLineInput/2)+1] = "";
	char lineRow2[(maxLineInput/2)+1] = "";
	lineLen = strlen(line);
	if(lineLen > 0){ // if pre-populating textbox
		cursorI = lineLen;
		if(lineLen <= 32){
			for(i = 0; i < lineLen; ++i)
				lineRow1[i] = line[i];
			lineRow1[lineLen] = 0;
			lineRow2[0] = 0;
		}
		else{
			for(i = 0; i < 32; ++i)
				lineRow1[i] = line[i];
			lineRow1[32] = 0;
			strcpy(lineRow2, "");
			strcat(lineRow2, line+32);
		}
		LCDD_DrawString(x, y, lineRow1, COLOR_WHITE);
		LCDD_DrawString(x, y+17, lineRow2, COLOR_WHITE);
	}
	while(1){
		powerCheck_iterate();  // sample power level in this wait loop
		tE = touchCtrl_getTouch();
		if(tE.touchValid == 1){
			if(tE.y < 105){ // touch outside key area
				if(tE.x > 395 && tE.x < 473 && tE.y > 57 && tE.y < 81 && xtraButn == 1){ // 'cancel' pressed
					strcpy(line, backupLine);
					return 1;
				}
				if(tE.x > 408 && tE.x < 475 && tE.y > 50 && tE.y < 88 && xtraButn == 2) // 'power off' pressed
					shutdown();
			}
			else{ // touch inside key area
				strcpy(c, " ");
				strcpy(linePar1, "");
				strcpy(linePar2, "");
				if(tE.x > 10 && tE.x < 77 && tE.y > 105 && tE.y < 140){ // 'enter' pressed
					keyI = 53;
					
					// touch animation
					LCDD_DrawRectangle(10, 115, 67, 23, COLOR_RED);
					LCDD_DrawRectangle(11, 116, 65, 21, COLOR_RED);
					LCDD_DrawRectangle(12, 117, 63, 19, COLOR_RED);
					delay_msOrUS(200, 1);
					LCDD_DrawRectangle(10, 115, 67, 23, COLOR_WHITE);
					LCDD_DrawRectangle(11, 116, 65, 21, COLOR_WHITE);
					LCDD_DrawRectangle(12, 117, 63, 19, COLOR_WHITE);
					
					return 0;
				}
				else if(tE.x > 87 && tE.x < 244 && tE.y > 105 && tE.y < 140){ // 'clear textbox' pressed
					keyI = 52;
					// touch animation
					LCDD_DrawRectangle(87, 115, 158, 23, COLOR_RED);
					LCDD_DrawRectangle(88, 116, 156, 21, COLOR_RED);
					LCDD_DrawRectangle(89, 117, 154, 19, COLOR_RED);
					
					BLACKOUT_TEXTBOX
					strcpy(line, ""); // erase old line
					cursorI = 0;
					
					// touch animation
					delay_msOrUS(200, 1);
					LCDD_DrawRectangle(87, 115, 158, 23, COLOR_WHITE);
					LCDD_DrawRectangle(88, 116, 156, 21, COLOR_WHITE);
					LCDD_DrawRectangle(89, 117, 154, 19, COLOR_WHITE);
				}
				else if(tE.x > 392 && tE.x < 427 && tE.y > 105 && tE.y < 140){ // 'move cursor left' pressed
					keyI = 51;
					// touch animation
					LCDD_DrawRectangle(392, 115, 36, 23, COLOR_RED);
					LCDD_DrawRectangle(393, 116, 34, 21, COLOR_RED);
					LCDD_DrawRectangle(394, 117, 32, 19, COLOR_RED);
					
					if(cursorI > 0){
						LCDD_DrawRectangle(cursorX, cursorY, 2, 16, COLOR_BLACK); // erase cursor artifacts
						--cursorI;
					}
					
					// give user time to see cursor on screen if moving fast
					CALC_CURSOR_POS
					LCDD_DrawRectangle(cursorX, cursorY, 2, 16, COLOR_YELLOW);
					
					// touch animation
					delay_msOrUS(200, 1);
					LCDD_DrawRectangle(392, 115, 36, 23, COLOR_WHITE);
					LCDD_DrawRectangle(393, 116, 34, 21, COLOR_WHITE);
					LCDD_DrawRectangle(394, 117, 32, 19, COLOR_WHITE);
				}
				else if(tE.x > 428 && tE.x < 463 && tE.y > 105 && tE.y < 140){ // 'move cursor right' pressed
					keyI = 50;
					// touch animation
					LCDD_DrawRectangle(428, 115, 36, 23, COLOR_RED);
					LCDD_DrawRectangle(429, 116, 34, 21, COLOR_RED);
					LCDD_DrawRectangle(430, 117, 32, 19, COLOR_RED);
					
					if(cursorI < strlen(line)){
						LCDD_DrawRectangle(cursorX, cursorY, 2, 16, COLOR_BLACK); // erase cursor artifacts
						++cursorI;
					}
					
					// give user time to see cursor on screen if moving fast
					CALC_CURSOR_POS
					LCDD_DrawRectangle(cursorX, cursorY, 2, 16, COLOR_YELLOW);
					
					// touch animation
					delay_msOrUS(200, 1);
					LCDD_DrawRectangle(428, 115, 36, 23, COLOR_WHITE);
					LCDD_DrawRectangle(429, 116, 34, 21, COLOR_WHITE);
					LCDD_DrawRectangle(430, 117, 32, 19, COLOR_WHITE);
				}
				else if(tE.x > 406 && tE.x < 480 && tE.y > 208 && tE.y < 240){ // 'delete' pressed
					keyI = 49;
					// touch animation
					LCDD_DrawRectangle(409, 211, 69, 27, COLOR_RED);
					LCDD_DrawRectangle(410, 212, 67, 25, COLOR_RED);
					LCDD_DrawRectangle(411, 213, 65, 23, COLOR_RED);
					
					if(cursorI > 0){
						BLACKOUT_TEXTBOX
						strcpy(linePar1, line);
						linePar1[cursorI-1] = 0;
						strcat(linePar2, line+cursorI);
						strcat(linePar1, linePar2);
						strcpy(line, linePar1);
						--cursorI;
						lineLen = strlen(line);
						if(lineLen <= 32){
							for(i = 0; i < lineLen; ++i)
								lineRow1[i] = line[i];
							lineRow1[lineLen] = 0;
							lineRow2[0] = 0;
						}
						else{
							for(i = 0; i < 32; ++i)
								lineRow1[i] = line[i];
							lineRow1[32] = 0;
							strcpy(lineRow2, "");
							strcat(lineRow2, line+32);
						}
						LCDD_DrawString(x, y, lineRow1, COLOR_WHITE);
						LCDD_DrawString(x, y+17, lineRow2, COLOR_WHITE);
					}
					
					// touch animation
					delay_msOrUS(200, 1);
					LCDD_DrawRectangle(409, 211, 69, 27, COLOR_WHITE);
					LCDD_DrawRectangle(410, 212, 67, 25, COLOR_WHITE);
					LCDD_DrawRectangle(411, 213, 65, 23, COLOR_WHITE);
				}
				else if(tE.x > 406 && tE.x < 480 && tE.y > 240 && tE.y < 272){ // 'shift' pressed
					keyI = 48;
					if(shiftActive == 1){
						shiftActive = 0;
						LCDD_DrawRectangle(409, 243, 69, 27, COLOR_WHITE);
						LCDD_DrawRectangle(410, 244, 67, 25, COLOR_WHITE);
						LCDD_DrawRectangle(411, 245, 65, 23, COLOR_WHITE);
					}
					else{
						shiftActive = 1;
						LCDD_DrawRectangle(409, 243, 69, 27, COLOR_RED);
						LCDD_DrawRectangle(410, 244, 67, 25, COLOR_RED);
						LCDD_DrawRectangle(411, 245, 65, 23, COLOR_RED);
					}
					delay_msOrUS(200, 1); // slow sample rate
				}
				else if(tE.y > 144 && tE.y < 176){ // touch in first row
					if(tE.x > 0 && tE.x < 36){
						keyI = 0;
						SHOW_CHAR_KEY_ACTIVE
						BLACKOUT_TEXTBOX
						PARSE_LINE
					}
					else if(tE.x > 36 && tE.x < 73){
						keyI = 1;
						SHOW_CHAR_KEY_ACTIVE
						BLACKOUT_TEXTBOX
						PARSE_LINE
					}
					else if(tE.x > 73 && tE.x < 110){
						keyI = 2;
						SHOW_CHAR_KEY_ACTIVE
						BLACKOUT_TEXTBOX
						PARSE_LINE
					}
					else if(tE.x > 110 && tE.x < 147){
						keyI = 3;
						SHOW_CHAR_KEY_ACTIVE
						BLACKOUT_TEXTBOX
						PARSE_LINE
					}
					else if(tE.x > 147 && tE.x < 184){
						keyI = 4;
						SHOW_CHAR_KEY_ACTIVE
						BLACKOUT_TEXTBOX
						PARSE_LINE
					}
					else if(tE.x > 184 && tE.x < 221){
						keyI = 5;
						SHOW_CHAR_KEY_ACTIVE
						BLACKOUT_TEXTBOX
						PARSE_LINE
					}
					else if(tE.x > 221 && tE.x < 258){
						keyI = 6;
						SHOW_CHAR_KEY_ACTIVE
						BLACKOUT_TEXTBOX
						PARSE_LINE
					}
					else if(tE.x > 258 && tE.x < 295){
						keyI = 7;
						SHOW_CHAR_KEY_ACTIVE
						BLACKOUT_TEXTBOX
						PARSE_LINE
					}
					else if(tE.x > 295 && tE.x < 332){
						keyI = 8;
						SHOW_CHAR_KEY_ACTIVE
						BLACKOUT_TEXTBOX
						PARSE_LINE
					}
					else if(tE.x > 332 && tE.x < 369){
						keyI = 9;
						SHOW_CHAR_KEY_ACTIVE
						BLACKOUT_TEXTBOX
						PARSE_LINE
					}
					else if(tE.x > 369 && tE.x < 406){
						keyI = 10;
						SHOW_CHAR_KEY_ACTIVE
						BLACKOUT_TEXTBOX
						PARSE_LINE
					}
					else if(tE.x > 406 && tE.x < 443){
						keyI = 11;
						SHOW_CHAR_KEY_ACTIVE
						BLACKOUT_TEXTBOX
						PARSE_LINE
					}
					else if(tE.x > 443 && tE.x < 480){
						keyI = 12;
						SHOW_CHAR_KEY_ACTIVE
						BLACKOUT_TEXTBOX
						PARSE_LINE
					}
					SHOW_CHAR_KEY_INACTIVE
				}
				else if(tE.y > 176 && tE.y < 208){ // touch in second row
					if(tE.x > 0 && tE.x < 36){
						keyI = 13;
						SHOW_CHAR_KEY_ACTIVE
						BLACKOUT_TEXTBOX
						PARSE_LINE
					}
					else if(tE.x > 36 && tE.x < 73){
						keyI = 14;
						SHOW_CHAR_KEY_ACTIVE
						BLACKOUT_TEXTBOX
						PARSE_LINE
					}
					else if(tE.x > 73 && tE.x < 110){
						keyI = 15;
						SHOW_CHAR_KEY_ACTIVE
						BLACKOUT_TEXTBOX
						PARSE_LINE
					}
					else if(tE.x > 110 && tE.x < 147){
						keyI = 16;
						SHOW_CHAR_KEY_ACTIVE
						BLACKOUT_TEXTBOX
						PARSE_LINE
					}
					else if(tE.x > 147 && tE.x < 184){
						keyI = 17;
						SHOW_CHAR_KEY_ACTIVE
						BLACKOUT_TEXTBOX
						PARSE_LINE
					}
					else if(tE.x > 184 && tE.x < 221){
						keyI = 18;
						SHOW_CHAR_KEY_ACTIVE
						BLACKOUT_TEXTBOX
						PARSE_LINE
					}
					else if(tE.x > 221 && tE.x < 258){
						keyI = 19;
						SHOW_CHAR_KEY_ACTIVE
						BLACKOUT_TEXTBOX
						PARSE_LINE
					}
					else if(tE.x > 258 && tE.x < 295){
						keyI = 20;
						SHOW_CHAR_KEY_ACTIVE
						BLACKOUT_TEXTBOX
						PARSE_LINE
					}
					else if(tE.x > 295 && tE.x < 332){
						keyI = 21;
						SHOW_CHAR_KEY_ACTIVE
						BLACKOUT_TEXTBOX
						PARSE_LINE
					}
					else if(tE.x > 332 && tE.x < 369){
						keyI = 22;
						SHOW_CHAR_KEY_ACTIVE
						BLACKOUT_TEXTBOX
						PARSE_LINE
					}
					else if(tE.x > 369 && tE.x < 406){
						keyI = 23;
						SHOW_CHAR_KEY_ACTIVE
						BLACKOUT_TEXTBOX
						PARSE_LINE
					}
					else if(tE.x > 406 && tE.x < 443){
						keyI = 24;
						SHOW_CHAR_KEY_ACTIVE
						BLACKOUT_TEXTBOX
						PARSE_LINE
					}
					else if(tE.x > 443 && tE.x < 480){
						keyI = 25;
						SHOW_CHAR_KEY_ACTIVE
						BLACKOUT_TEXTBOX
						PARSE_LINE
					}
					SHOW_CHAR_KEY_INACTIVE
				}
				else if(tE.y > 208 && tE.y < 240 && tE.x < 406){ // touch in third row
					if(tE.x > 0 && tE.x < 36){
						keyI = 26;
						SHOW_CHAR_KEY_ACTIVE
						BLACKOUT_TEXTBOX
						PARSE_LINE
					}
					else if(tE.x > 36 && tE.x < 73){
						keyI = 27;
						SHOW_CHAR_KEY_ACTIVE
						BLACKOUT_TEXTBOX
						PARSE_LINE
					}
					else if(tE.x > 73 && tE.x < 110){
						keyI = 28;
						SHOW_CHAR_KEY_ACTIVE
						BLACKOUT_TEXTBOX
						PARSE_LINE
					}
					else if(tE.x > 110 && tE.x < 147){
						keyI = 29;
						SHOW_CHAR_KEY_ACTIVE
						BLACKOUT_TEXTBOX
						PARSE_LINE
					}
					else if(tE.x > 147 && tE.x < 184){
						keyI = 30;
						SHOW_CHAR_KEY_ACTIVE
						BLACKOUT_TEXTBOX
						PARSE_LINE
					}
					else if(tE.x > 184 && tE.x < 221){
						keyI = 31;
						SHOW_CHAR_KEY_ACTIVE
						BLACKOUT_TEXTBOX
						PARSE_LINE
					}
					else if(tE.x > 221 && tE.x < 258){
						keyI = 32;
						SHOW_CHAR_KEY_ACTIVE
						BLACKOUT_TEXTBOX
						PARSE_LINE
					}
					else if(tE.x > 258 && tE.x < 295){
						keyI = 33;
						SHOW_CHAR_KEY_ACTIVE
						BLACKOUT_TEXTBOX
						PARSE_LINE
					}
					else if(tE.x > 295 && tE.x < 332){
						keyI = 34;
						SHOW_CHAR_KEY_ACTIVE
						BLACKOUT_TEXTBOX
						PARSE_LINE
					}
					else if(tE.x > 332 && tE.x < 369){
						keyI = 35;
						SHOW_CHAR_KEY_ACTIVE
						BLACKOUT_TEXTBOX
						PARSE_LINE
					}
					else if(tE.x > 369 && tE.x < 406){
						keyI = 36;
						SHOW_CHAR_KEY_ACTIVE
						BLACKOUT_TEXTBOX
						PARSE_LINE
					}
					SHOW_CHAR_KEY_INACTIVE
				}
				else if(tE.y > 240 && tE.y < 272 && tE.x < 406){ // touch in fourth row
					if(tE.x > 0 && tE.x < 36){
						keyI = 37;
						SHOW_CHAR_KEY_ACTIVE
						BLACKOUT_TEXTBOX
						PARSE_LINE
					}
					else if(tE.x > 36 && tE.x < 73){
						keyI = 38;
						SHOW_CHAR_KEY_ACTIVE
						BLACKOUT_TEXTBOX
						PARSE_LINE
					}
					else if(tE.x > 73 && tE.x < 110){
						keyI = 39;
						SHOW_CHAR_KEY_ACTIVE
						BLACKOUT_TEXTBOX
						PARSE_LINE
					}
					else if(tE.x > 110 && tE.x < 147){
						keyI = 40;
						SHOW_CHAR_KEY_ACTIVE
						BLACKOUT_TEXTBOX
						PARSE_LINE
					}
					else if(tE.x > 147 && tE.x < 184){
						keyI = 41;
						SHOW_CHAR_KEY_ACTIVE
						BLACKOUT_TEXTBOX
						PARSE_LINE
					}
					else if(tE.x > 184 && tE.x < 221){
						keyI = 42;
						SHOW_CHAR_KEY_ACTIVE
						BLACKOUT_TEXTBOX
						PARSE_LINE
					}
					else if(tE.x > 221 && tE.x < 258){
						keyI = 43;
						SHOW_CHAR_KEY_ACTIVE
						BLACKOUT_TEXTBOX
						PARSE_LINE
					}
					else if(tE.x > 258 && tE.x < 295){
						keyI = 44;
						SHOW_CHAR_KEY_ACTIVE
						BLACKOUT_TEXTBOX
						PARSE_LINE
					}
					else if(tE.x > 295 && tE.x < 332){
						keyI = 45;
						SHOW_CHAR_KEY_ACTIVE
						BLACKOUT_TEXTBOX
						PARSE_LINE
					}
					else if(tE.x > 332 && tE.x < 369){
						keyI = 46;
						SHOW_CHAR_KEY_ACTIVE
						BLACKOUT_TEXTBOX
						PARSE_LINE
					}
					else if(tE.x > 369 && tE.x < 406){
						keyI = 47;
						SHOW_CHAR_KEY_ACTIVE
						BLACKOUT_TEXTBOX
						PARSE_LINE
					}
					SHOW_CHAR_KEY_INACTIVE
				}
			}
		}
		else{ // just blink cursor
			CALC_CURSOR_POS
			if(cursorVis == 1)
				LCDD_DrawRectangle(cursorX, cursorY, 2, 16, COLOR_YELLOW);
			else
				LCDD_DrawRectangle(cursorX, cursorY, 2, 16, COLOR_BLACK);
			++cursorDly;
			if (cursorDly%2048 == 0)
				cursorVis = cursorVis^0x01;
		}	
	}
}

// Returns a character from the keyChars array based on the 'i' index argument.
char randCharHelper(int i)
{
	assert(i >= 0 && i <= 93);
	
	if(i <= 46)
		return keyChars[i].primChar;
		
	i = i-47;
	return keyChars[i].secChar;
}