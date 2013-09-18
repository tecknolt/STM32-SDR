/*
 * Header file for Options screen code
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


#include <stdint.h>



typedef enum {
	OPTION_RX_AUDIO = 0,
	OPTION_RX_RF,
	OPTION_Mic_Gain,
	OPTION_Tx_LEVEL,
	OPTION_ST_LEVEL,
	OPTION_RX_AMP,
	OPTION_RX_PHASE,
	OPTION_TX_AMP,
	OPTION_TX_PHASE,
	OPTION_MIC_BIAS,
	OPTION_AGC_THRSH,
	OPTION_SI570_MULT,

	NUM_OPTIONS
} OptionNumber;



// Initialization
void Options_Initialize(void);
void Options_ResetToDefaults(void);

// Work with option data
const char* Options_GetName(int optionIdx);
//uint16_t Options_GetValue(int optionIdx);
int16_t Options_GetValue(int optionIdx);
void     Options_SetValue(int optionIdx, int16_t newValue);
uint16_t Options_GetMinimum(int optionIdx);
uint16_t Options_GetMaximum(int optionIdx);
uint16_t Options_GetChangeRate(int optionIdx);

// Option selection
OptionNumber Options_GetSelectedOption(void);
void         Options_SetSelectedOption(OptionNumber newOption);

// EEPROM Access
void Options_WriteToEEPROM(void);
_Bool Options_HaveValidEEPROMData(void);
void Options_ReadFromEEPROM(void);
