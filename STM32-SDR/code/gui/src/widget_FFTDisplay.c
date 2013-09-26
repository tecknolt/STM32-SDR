/*
 * Code for FFT widget
 *
 * STM32-SDR: A software defined HAM radio embedded system.
 * Copyright (C) 2013, STM32-SDR Group
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "widgets.h"
#include <assert.h>
#include "graphicObject.h"
#include <string.h>
#include <stdio.h>
#include "ModeSelect.h"
#include "arm_math.h"
#include "DSP_Processing.h"
#include "TSHal.h"
#include "DMA_IRQ_Handler.h"

static const int FFT_WIDTH   = 240;
static const int FFT_HEIGHT  =  64;
static const int SELFREQ_ADJ =   4;
static const int CHARACTER_WIDTH = 8;
static const int MAX_FREQ_DIGITS = 5;
static const int SMETER_HEIGHT = 12;

static uint16_t WidgetFFT_GetWidth(GL_PageControls_TypeDef* pThis);
static uint16_t WidgetFFT_GetHeight(GL_PageControls_TypeDef* pThis);
static void WidgetFFT_EventHandler(GL_PageControls_TypeDef* pThis);
static void WidgetFFT_DrawHandler(GL_PageControls_TypeDef* pThis, _Bool force);

static void displayFFT(_Bool force, int x, int y);
static void displaySelectedFrequencyText(_Bool force, int x, int y);
static void displaySMeter(_Bool force, int x, int y);


int 	NCO_Point;
void	Acquire ( void );
uint8_t FFT_Display[256];



#define ID_FFTSelFreqNum_LABEL 50105

/*
 * Public Interface
 */

/*
 * Create a new widget and add it to the passed in page
 * Note: May create multiple controls (labels, buttons, ...) and add them to the page.
 */
void Widget_AddToPage_NewFFTDisplay(uint16_t x, uint16_t y, GL_Page_TypeDef *pPage)
{
	GL_PageControls_TypeDef* newFFT = NewCustomWidget(
				0,
				WidgetFFT_GetWidth,
				WidgetFFT_GetHeight,
				WidgetFFT_EventHandler,
				WidgetFFT_DrawHandler,
				0);
	AddPageControlObj(x,    y, newFFT, pPage);
}



/*
 * Private Functions
 */
static uint16_t WidgetFFT_GetWidth(GL_PageControls_TypeDef* pThis)
{
	return FFT_WIDTH;
}
static uint16_t WidgetFFT_GetHeight(GL_PageControls_TypeDef* pThis)
{
	return FFT_HEIGHT;
}


// Write a signed integer to a right-justified string.
void intToCommaString(int16_t number, char *pDest, int numChar)
{
	int index = numChar - 1;
	// Null terminate
	pDest[index--] = 0;

	_Bool isNeg = 0;
	if (number < 0) {
		number = -number;
		isNeg = 1;
	}

	int digitsWritten = 0;
	// Handle 0
	if (number == 0) {
		pDest[index--] = '0';
	}

	while (number > 0) {
		pDest[index--] = number % 10 + 0x30;
		number /= 10;

		digitsWritten++;
		if (digitsWritten > 0 && digitsWritten % 3 == 0 && number > 0)
			pDest[index--] = ',';
	}

	if (isNeg)
		pDest[index--] = '-';

	while (index >= 0) {
		pDest[index--] = ' ';
	}

	// Check that we did not overflow memory.
	assert(index >= -1);
}

static void WidgetFFT_EventHandler(GL_PageControls_TypeDef* pThis)
{
	// Get the coordinates:
	uint16_t X_Point, Y_Point;
	TS_GetTouchEventCoords(&X_Point, &Y_Point);

	//Update PSK NCO Frequency
	int fftLeftEdge = pThis->objCoordinates.MinX;
	NCO_Point = ((int)X_Point - fftLeftEdge) +8;

	NCO_Frequency = (double) ((float) ((X_Point - fftLeftEdge) + 8) * 15.625);
	Acquire();

	//SetRXFrequency(NCO_Frequency);
}

void Acquire (void )
{
	extern int count;
	extern int char_count;
	long i, S1, S2, W;
	double delta;

	/* this is where I  add a correction to the NCO frequency
					based on the nearby spectral peaks */
	S1 = 0;
	S2 = 0;
	delta = 0.;
	//for (i=-2; i<3; i++){
	for (i=-4; i<5; i++){
		W = (long)FFT_Display[NCO_Point + i];
		S1 += W*i;
		S2 += W;
	}
	if (S2 != 0) delta = (double) S1/((double)S2);

	NCO_Frequency +=  (double)((float)delta * 15.625);

	SetRXFrequency (NCO_Frequency );
	count = 0;
	char_count = 0;
}

// Return the x-offset of where the current frequency is selected.
// Return -1 if frequency selection invalid in this mode.
static int getSelectedFrequencyX(void)
{
	if (Mode_GetCurrentMode() == MODE_SSB) {
		return -1;
	} else {
		float exact = (NCO_Frequency - 125) / 15.625;
		return (int) (exact + 0.5) ;
	}
}


/******************************************************
 * Screen Drawing Functions
 ******************************************************/
static void WidgetFFT_DrawHandler(GL_PageControls_TypeDef* pThis, _Bool force)
{
	// Bail if nothing to draw.
	if (!force && !DSP_Flag) {
		return;
	}

	// Extract the FFT's screen coordinates.
	int x = pThis->objCoordinates.MinX;
	int y = pThis->objCoordinates.MinY;

	// Draw the FFT
	displayFFT(force, x, y);

	// Display frequency text and S Meter if we have a selected frequency.
	if (getSelectedFrequencyX() >= 0) {
		displaySelectedFrequencyText(force, x, y);
		displaySMeter(force, x, y);
	}

	// Once the FFT has been drawn, clear the DSP flag.
	DSP_Flag = 0;
}


static void displayFFT(_Bool force, int x, int y)
{
	/*
	 * Calculate the data to draw:
	 * - Use FFT_Magnitude[0..127] which is 0 to +4kHz of frequency
	 *   (don't use last half which is -4kHz to 0kHz)
	 * - Scale the values up to fill a wider portion of the display.
	 *   (by averaging with neighboring data). This helps allow the
	 *   user to tap on frequencies to select them with better resolution.
	 * - Average with "old" data from the previous pass to give it an
	 *   effective time-based smoothing (as in, display does not change
	 *   as abruptly as it would when using new data samples each time).
	 */
	static uint8_t FFT_Output[128];   // static because use last rounds data now.

	// TODO: Where are all these FFT constants from?
	for (int16_t j = 0; j < 128; j++) {
		// Changed for getting right display with SR6.3
		// Convert from Q15 (fractional numeric representation) into integer
		//FFT_Output[j] = (uint8_t) (6 * log((float32_t) (FFT_Magnitude[j] + 1)));
		FFT_Output[j] = (uint8_t) (6 * log( (Smoothed_FFT_Magnitude[j] + 1)));

		if (FFT_Output[j] > 64)
			FFT_Output[j] = 64;
		FFT_Display[2 * j] = FFT_Output[j];
		// Note that calculation uses values from last pass through.
		//FFT_Display[2 * j + 1] = 0;
		FFT_Display[2 * j + 1] = (FFT_Output[j] + FFT_Output[j + 1]) / 2;
	}


	/*
	 * Display the FFT
	 * - Drop the bottom 8, and top 8 frequency-display bins to discard
	 *   noisy sections near band edges due to filtering.
	 */
	// Draw the FFT using direct memory writes (fast).
	LCD_SetDisplayWindow(x, y, FFT_HEIGHT, FFT_WIDTH);
	LCD_WriteRAM_PrepareDir(LCD_WriteRAMDir_Down);

	for (int x = 0; x < FFT_WIDTH; x++) {
		// Plot this column of the FFT.
		for (int y = 0; y < FFT_HEIGHT; y++) {

			// Draw red line for selected frequency
			if (x == getSelectedFrequencyX()) {
				// Leave some white at the top
				if (y <= SELFREQ_ADJ) {
					LCD_WriteRAM(LCD_COLOR_WHITE);
				} else {
					LCD_WriteRAM(LCD_COLOR_RED);
				}
			}

			// Draw data
			else if (FFT_HEIGHT - y < FFT_Display[x + 8]) {
				LCD_WriteRAM(LCD_COLOR_BLUE);
			}

			// Draw background
			else {
				LCD_WriteRAM(LCD_COLOR_WHITE);
			}
		}
	}
}

static void displaySelectedFrequencyText(_Bool force, int x, int y)
{
	// Update the frequency offset displayed (text):
	static double oldSelectedFreq = -1;
	if (force || oldSelectedFreq != NCO_Frequency) {
		oldSelectedFreq = NCO_Frequency;

		//int textY = y + FFT_HEIGHT + TEXT_OFFSET_BELOW_FFT;
		int textY = FFT_HEIGHT - 18;
		//int numberX = x + FFT_WIDTH - MAX_FREQ_DIGITS * CHARACTER_WIDTH;
		int numberX = 4 * CHARACTER_WIDTH;
		//int labelX = numberX - CHARACTER_WIDTH * 8;	// 7=# letters in label w/ a space
		int labelX = 0;

		// Display frequency select label
		GL_SetFont(GL_FONTOPTION_8x16);
		GL_SetBackColor(LCD_COLOR_BLACK);
		GL_SetTextColor(LCD_COLOR_WHITE);
		GL_PrintString(labelX, textY, "AF", 0);

		// Display select frequency number
		GL_SetTextColor(LCD_COLOR_RED);
		char number[MAX_FREQ_DIGITS + 1];
		intToCommaString((int)NCO_Frequency, number, MAX_FREQ_DIGITS + 1);
		GL_PrintString(numberX, textY, number, 0);
	}
}

static void displaySMeter(_Bool force, int x, int y)
{
	// Override where it is located.
	// TODO: Make this relative to the X,Y of the component, not a hard coded value.
	x = 80;
	y = 68;

	// Draw the S Meter bar
	LCD_SetDisplayWindow(x, y, SMETER_HEIGHT, FFT_WIDTH);
	LCD_WriteRAM_PrepareDir(LCD_WriteRAMDir_Down);

	int signalLevel = 3*FFT_Display[getSelectedFrequencyX() + 8];
	for (int x = 0; x < FFT_WIDTH; x++){
		for (int y = 0; y < SMETER_HEIGHT; y++){
			if (x <= signalLevel) {
				LCD_WriteRAM(LCD_COLOR_GREEN);
			}
			else {
				LCD_WriteRAM(LCD_COLOR_DGRAY);
			}
		}
	}

	// Construct output string:
	char SMeter$[7];
	if (signalLevel < 120) {
		int S = signalLevel / 12;
		sprintf(SMeter$,"S%i    ", S);
	}
	else {
		int S = 9;
		int R = (signalLevel-100)/3;
		sprintf(SMeter$,"S%i+%i", S, R);
	}

	// Display string
	GL_SetBackColor(LCD_COLOR_BLACK);
	GL_SetTextColor(LCD_COLOR_WHITE);
	GL_PrintString (1,67,SMeter$,0);
}
