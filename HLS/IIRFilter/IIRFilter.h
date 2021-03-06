///////////////////////////////////////////////////////////
//  IIRFilter.h
//  Header:          IIR Filter
//  Created on:      02-05-2019
//  Original author: Kim Bjerge
///////////////////////////////////////////////////////////
#ifndef IIRFiler_H
#define IIRFiler_H

#include "stdint.h"

#define 				DATA_CHANNELS 						8
#define					NUM_TAPS							3 // 13 Number of b or a coefficients
#define                 NUM_SOS								6 // 1 Number of second order sections (IIR)
#define                 INIT_VALUE                          0x00000000
#define 				ALGO_BITS							23  // 24 bits resolution of coefficients with max. 22 bits input
#define                 SHIFT_BITS                          5   // Number of bits to shift samples to improve accuracy, input max. 18 bits
//#define                 sigType                             int32_t
#define                 sigType                             int16_t
#define                 MAX_VALUE                           32767   // Max. positive 16 bits value
#define                 MIN_VALUE                           -32768  // Min. negative 16 bits value

void initIIR(void);

// Processing number of DATA_CHANNELS in parallel NUM_TAPS IIR filters
void IIRFilter (sigType results[DATA_CHANNELS],
				sigType samples[DATA_CHANNELS],
				int32_t coeff[NUM_TAPS*2*NUM_SOS], // First b coefficients then a coefficients
				int32_t operation);
#endif
