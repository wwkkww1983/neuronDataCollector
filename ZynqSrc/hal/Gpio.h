/*
 * Gpio.h
 *
 *  Created on: 9. aug. 2017
 *      Author: Kim Bjerge
 */

#ifndef SRC_HAL_GPIO_H_
#define SRC_HAL_GPIO_H_

#include "xparameters.h"
#include "xgpio.h"

class Gpio
{
public:
	Gpio(int deviceID);
	int readio(void);
	void writeio(int val);
protected:
	XGpio mGpioHandle;
	int mChannel;
};

#endif /* SRC_HAL_GPIO_H_ */
