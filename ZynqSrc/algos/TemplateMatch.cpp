/*
 * TemplateMatch.cpp
 *
 *  Created on: 25. dec. 2018
 *      Author: Kim Bjerge
 */

#include <math.h>
#include "TemplateMatch.h"
#include "xparameters.h"
#include "FIRFilter_coeffs.h"

#define DEBUG_FILES // When disabled doesn't save time

TemplateMatch::~TemplateMatch()
{
	if (mNumSamples > 0) {
		for (int i = 0; i < FIR_NUM; i++) {
			delete(pFirFilter[i]);
		}
		for (int i = 0; i < TEMP_NUM; i++) {
			delete(pNXCOR[i]);
			delete(pTemplate[i]);
		}
#ifdef DEBUG_FILES
		for (int i = 0; i < TEMP_NUM; i++)
			delete(pResultNXCOR[i]);
		delete(pResultFIR);
#endif
	}
}

int TemplateMatch::Init(Config *pConfig, int numSamples, IRQ* pIrq)
{

	for (int i = 0; i < FIR_NUM; i++) {
		pFirFilter[i] = new FirFilter(XPAR_FIRFILTER_0_DEVICE_ID+i, FIR_SIZE, FIR_TAPS);
		pFirFilter[i]->Init(pIrq, XPAR_FABRIC_FIRFILTER_0_INTERRUPT_INTR+i);
	}
	for (int i = 0; i < TEMP_NUM; i++) {
		pNXCOR[i] = new NXCOR(XPAR_NXCOR_0_DEVICE_ID+i, TEMP_LENGTH, TEMP_WIDTH);
		pNXCOR[i]->Init(pIrq, XPAR_FABRIC_NXCOR_0_INTERRUPT_INTR+i);
		pTemplate[i] = new Template();

		if (i < pConfig->getNumTemplates()) {
			// Load template from file defined by pTempNames
			pTemplate[i]->loadTemplate(pConfig->getTemplateName(i));
			pNXCOR[i]->setNXCORThreshold(pConfig->getThreshold(i));
			pNXCOR[i]->setPeakThreshold(pConfig->getMin(i), pConfig->getMax(i));
			// TODO - set counter
			pNXCOR[i]->printSettings();
		} else
			pTemplate[i]->clearTemplate();
	}

#ifdef DEBUG_FILES
	for (int i = 0; i < TEMP_NUM; i++) {
		pResultNXCOR[i] = new ResultFile<float>();
		pResultNXCOR[i]->allocateContent(numSamples);
	}
	pResultFIR = new ResultFile<int>();
	pResultFIR->allocateContent(numSamples*NUM_CHANNELS);
#endif

	mNumCfgTemplates = pConfig->getNumTemplates();
	mNumSamples = numSamples;

	return 0;
}

int TemplateMatch::updateCoefficients()
{
	// Updating coefficients based on table with double numbers
	for (int i = 0; i < FIR_TAPS; i++) {
		mCoeff[i] = (int)round(FIR_coeffs[i]*pow(2,FIR_FORMAT)); // Convert to format 1.FIR_FORMAT
	}
	for (int i = 0; i < FIR_NUM; i++) {
		pFirFilter[i]->updateCoefficients(mCoeff);
	}
	for (int i = 0; i < NUM_CHANNELS; i++)
		mFiltered[i] = 0;
	return 0;
}

int TemplateMatch::updateTemplates()
{
	// Update template in HLS IP core
	for (int i = 0; i < TEMP_NUM; i++) {
		pNXCOR[i]->updateTemplate(pTemplate[i]->getTemplate(), round(pTemplate[i]->getMean()));
		for (int j = 0; j < TEMP_LENGTH; j++) { // Clear IP Core memory
			pNXCOR[i]->executeNXCOR(mFiltered, pTemplate[i]->getVariance());
		}
	}
	return 0;
}

void TemplateMatch::processResults(void)
{
	for (int i = 0; i < mNumCfgTemplates; i++) {
		int state = pNXCOR[i]->verifyActivation();
		if (state == 1) {
			printf("%06d %03d %s %.3f P%05d\r\n",
					mCount,
					pNXCOR[i]->getNumActivations(),
					pTemplate[i]->getTemplateName(),
					pNXCOR[i]->getNXCORResult(),
					pNXCOR[i]->getMaxPeak());
			leds.setOn((Leds::LedTypes)i, true);
			testOut.setOn((TestIO::IOTypes)i, true);
		}
		if (state == 3) {
			leds.setOn((Leds::LedTypes)i, false);
			testOut.setOn((TestIO::IOTypes)i, false);
		}
	}
}

void TemplateMatch::run()
{
	int count = mNumSamples;
    bool firstTime = true;
    int *pSampleData = (int *)lxRecord.board[0].data;
    int start_tick, end_tick;

	printf("Updating FIR coefficients and templates\r\n");
    updateCoefficients();
    updateTemplates();
	printf("Neuron template matching running\r\n");

	while (1)
	{

		printf("Turn SW0 on to start\r\n");
		while (!sw.isOn(Switch::SW0))
			Sleep(100);

		pNeuronData->reset();
		pResultFIR->reset();
		for (int i = 0; i < TEMP_NUM; i++)
			pResultNXCOR[i]->reset();


		// LED + Hardware signals for debugging
		leds.setOn(Leds::LED7 , true);
		testOut.setOn(TestIO::JB10, true);
		mCount = 0;
		count = mNumSamples;
		firstTime = true;

		start_tick = xTaskGetTickCount();
		while (count > 0) {

			// Get next sample from data generator
			pNeuronData->GenerateSampleRecord(&lxRecord);

			if (!firstTime) {
				// After fist iteration read filtered samples
				for (int i = 0; i < FIR_NUM; i++) {
					pFirFilter[i]->readFiltered(&mFiltered[i*FIR_SIZE]);
				}
				// Read result of normalized cross core correlation
				for (int i = 0; i < mNumCfgTemplates; i++) {
					pNXCOR[i]->readResultNXCOR(pTemplate[i]->getVariance());
				}
			}

			// Start filtering next NUM_CHANNELS of samples
			for (int i = 0; i < FIR_NUM; i++) {
				pFirFilter[i]->startFilter((int *)&pSampleData[i*FIR_SIZE]);
			}
			// Start normalized cross core correlation of filtered samples
			for (int i = 0; i < mNumCfgTemplates; i++) {
				pNXCOR[i]->startNXCOR((int *)&mFiltered[pTemplate[i]->getChOffset()]);
			}

			processResults();

			// Append test result to memory
#ifdef DEBUG_FILES
			pResultFIR->appendData(mFiltered, NUM_CHANNELS);
			for (int i = 0; i < mNumCfgTemplates; i++) {
				float NXCORRes = pNXCOR[i]->getNXCORResult();
				if (pNXCOR[i]->getActiveState() == 1)
					NXCORRes = 1.0; // Set to max when active
				pResultNXCOR[i]->appendData(&NXCORRes, 1);
			}
#endif
			// Wait for one sample delay
			//printf(".");
			//vTaskDelay( pdMS_TO_TICKS( 1 ) );
			//vTaskDelay( pdMS_TO_TICKS( 0.0333333 ) );
			firstTime = false;
			count--;
			mCount++;
		}
		end_tick = xTaskGetTickCount();

		testOut.setOn(TestIO::JB10, false);
		leds.setOn(Leds::LED7 , false);

		printf("Tick start %d and tick end %d, duration = %d ms\r\n", start_tick, end_tick, (1000*(end_tick-start_tick))/configTICK_RATE_HZ);
		printf("Neuron template matching completed on %d samples\r\n", mNumSamples);

#ifdef DEBUG_FILES
		// Save test result from memory to files
		pResultFIR->saveContent("FIRFilt.bin");
		for (int i = 0; i < mNumCfgTemplates; i++) {
			char name[20];
			sprintf(name, "NXCORT%d.bin", i+1);
			pResultNXCOR[i]->saveContent(name);
		}
		printf("Saved result to files FIRFilt.bin and NXCORT1-%d.bin\r\n", mNumCfgTemplates);
#endif

	} // while (1)

}

