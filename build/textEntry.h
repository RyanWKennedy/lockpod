/*
Copyright 2018 Ryan Kennedy
*/

#ifndef TEXTENTRY_H
#define TEXTENTRY_H

#define keyArSize 48

/////////////////////////////////////////////////////////////////////////////////////////
// macros instead of functions to increase performance

#define BLACKOUT_TEXTBOX \
	LCDD_DrawString(x, y, lineRow1, COLOR_BLACK); \
	LCDD_DrawString(x, y+17, lineRow2, COLOR_BLACK); \
	LCDD_DrawRectangle(cursorX, cursorY, 2, 16, COLOR_BLACK); \

#define SHOW_CHAR_KEY_ACTIVE \
	LCDD_DrawRectangle(keyPosits[keyI].x, keyPosits[keyI].y, 32, 27, COLOR_RED); \
	LCDD_DrawRectangle(keyPosits[keyI].x + 1, keyPosits[keyI].y + 1, 30, 25, COLOR_RED); \
	LCDD_DrawRectangle(keyPosits[keyI].x + 2, keyPosits[keyI].y + 2, 28, 23, COLOR_RED); \

#define SHOW_CHAR_KEY_INACTIVE \
	delay_msOrUS(200, 1); \
	LCDD_DrawRectangle(keyPosits[keyI].x, keyPosits[keyI].y, 32, 27, COLOR_WHITE); \
	LCDD_DrawRectangle(keyPosits[keyI].x + 1, keyPosits[keyI].y + 1, 30, 25, COLOR_WHITE); \
	LCDD_DrawRectangle(keyPosits[keyI].x + 2, keyPosits[keyI].y + 2, 28, 23, COLOR_WHITE); \

// To debug touched chars, paste the following function between the last two curly braces in this macro: printf("%c\n\r", c[0]).
// Also note: failed using strncpy(linePar1, line, cursorI)... investigate strncpy bug
#define PARSE_LINE \
	if(strlen(line) < maxLineInput){ \
		if(shiftActive == 0) \
			c[0] = currentKb[keyI].primChar; \
		else \
			c[0] = currentKb[keyI].secChar; \
		for(i = 0; i < cursorI; ++i) \
			linePar1[i] = line[i]; \
		linePar1[cursorI] = 0; \
		strcat(linePar2, line+cursorI); \ 
		strcat(linePar1, c); \
		strcat(linePar1, linePar2); \
		strcpy(line, linePar1); \
		++cursorI; \
		lineLen = strlen(line); \
		if(lineLen <= 32){ \
			for(i = 0; i < lineLen; ++i) \
				lineRow1[i] = line[i]; \
			lineRow1[lineLen] = 0; \
			lineRow2[0] = 0; \
		} \
		else{ \
			for(i = 0; i < 32; ++i) \
				lineRow1[i] = line[i]; \
			lineRow1[32] = 0; \
			strcpy(lineRow2, ""); \
			strcat(lineRow2, line+32); \
		} \
	} \
	LCDD_DrawString(x, y, lineRow1, COLOR_WHITE); \
	LCDD_DrawString(x, y+17, lineRow2, COLOR_WHITE); \
	
#define CALC_CURSOR_POS \
	if(cursorI < 32){ \
		cursorX = (x-2) + cursorI*12; \
		cursorY = y-1; \
	} \
	else{ \
		cursorX = (x-2) + (cursorI-32)*12; \
		cursorY = y+16; \
	} \

struct keyCharDat{
	char primChar;
	char secChar;
};

struct keyPositDat{
	int x;
	int y;
};

struct keyDisp{
	char primChar;
	char secChar;
	int x;
	int y;
};

void setKbLayout(void);
void displayKey(struct keyDisp currentKey);
void displayKb(uint8_t layout);
int putGetLine(char * line, uint8_t xtraButn);
char randCharHelper(int i);

#endif