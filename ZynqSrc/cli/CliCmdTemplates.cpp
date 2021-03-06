/*
 * CliCmdTemplates.cpp
 *
 *  Created on: 21/03/2019
 *      Author: kbe
 */
#include "CliCmdTemplates.h"
#include "TestDataSDCard.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "Gpio.h"
#include "Switch.h"

void TestFileSDCard(void);

#define CMD_DELIMITER    ","
#define FILE_NAME_LEN    14 // Max. length of file names

CliCommand::CliCommand(TemplateMatch *pTemplateMatch, DataUDPThread *pDataThread, TestDataSDCard *pTestDataSDCard) : m_file((char *)"0:/")
{
	m_pTemplateMatch = pTemplateMatch;
	m_pDataThread = pDataThread;
	m_pTestDataSDCard = pTestDataSDCard;
	m_numSamples = MAX_NUM_SAMPLES; // Default 60 seconds
	majorVer_ = VERSION_HI;
	minorVer_ = VERSION_LO;
	m_fileSize = 0;
	m_uploadFile = true;
	m_dataSize = 0;
	m_idLocked = 0;
	m_executeMode = 2; // Default mode - template matching using data from SD card
}

void CliCommand::reset(void) // Reset transfer if connection lost
{
	m_fileSize = 0;
	m_dataSize = 0;
	m_blockCnt = 0;
	m_uploadFile = true;
	m_file.close();
}

//------------------------------------------------------------------------------------------------
// Parsing Commands
// -----------------------------------------------------------------------------------------------
int CliCommand::parseTemplate(int *nr, int *W, int *L)
{
	int w, l, ok = 0;
	char *nrStr, *WStr, *LStr, *dStr;
	nrStr = strtok(NULL, CMD_DELIMITER);
	WStr = strtok(NULL, CMD_DELIMITER);
	LStr = strtok(NULL, CMD_DELIMITER);
	if ((nrStr != 0) && (WStr != 0) && (LStr != 0)) {
		*nr = atoi(nrStr);
		w = atoi(WStr);
		*W = w;
		l = atoi(LStr);
		*L = l;
		memset(mTemplate, 0, sizeof(mTemplate));
		ok = 1;
		for (int i = 0; i < w*l && i < TEMP_SIZE; i++) {
			dStr = strtok(NULL, CMD_DELIMITER);
			if (dStr != 0)
				mTemplate[i] = atof(dStr);
			else {
				printf("Insufficient data for template %d of size %d*%d end %d\r\n", *nr, w, l, i);
				ok = 0;
				break;
			}
		}
	}
	return ok;
}

int CliCommand::parseShortArray(int *nr)
{
	int ok = 0;
	char *nrStr, *dStr;
	nrStr = strtok(NULL, CMD_DELIMITER);
	if (nrStr != 0) {
		*nr = atoi(nrStr);
		dStr = strtok(NULL, CMD_DELIMITER);
		if (dStr != 0) {
			memset(mShortArray, 0, sizeof(mShortArray));
			ok = 1;
			for (int i = 0; i < TEMP_WIDTH; i++) {
				if (dStr != 0)
					mShortArray[i] = (short)atoi(dStr);
				else
					break;
				dStr = strtok(NULL, CMD_DELIMITER);
			}
		}
	}
	return ok;
}

int CliCommand::parseStrCmd2(char *name, int *value)
{
	int ok = 0;
	char *nameStr, *valueStr;
	nameStr = strtok(NULL, CMD_DELIMITER);
	valueStr = strtok(NULL, CMD_DELIMITER);
	if ((nameStr != 0) && (valueStr != 0)) {
		strcpy(name, nameStr);
		*value = atoi(valueStr);
		ok = 1;
	}
	return ok;
}

int CliCommand::parseStrCmd2(char *name1, char *name2)
{
	int ok = 0;
	char *name1Str, *name2Str;
	name1Str = strtok(NULL, CMD_DELIMITER);
	name2Str = strtok(NULL, CMD_DELIMITER);
	if ((name1Str != 0) && (name2Str != 0)) {
		strcpy(name1, name1Str);
		strcpy(name2, name2Str);
		ok = 1;
	}
	return ok;
}

int CliCommand::parseStrCmd1(char *name)
{
	int ok = 0;
	char *nameStr;
	nameStr = strtok(NULL, CMD_DELIMITER);
	if (nameStr != 0) {
		strcpy(name, nameStr);
		ok = 1;
	}
	return ok;
}

int CliCommand::parseCmd3(int *id, int *value1, int *value2)
{
	int ok = 0;
	char *idStr, *valueStr1, *valueStr2;
	idStr = strtok(NULL, CMD_DELIMITER);
	valueStr1 = strtok(NULL, CMD_DELIMITER);
	valueStr2 = strtok(NULL, CMD_DELIMITER);
	if ((idStr != 0) && (valueStr1 != 0) && (valueStr2 != 0)) {
		*id = atoi(idStr);
		*value1 = atoi(valueStr1);
		*value2 = atoi(valueStr2);
		ok = 1;
	}
	return ok;
}

int CliCommand::parseCmd2(int *id, int *value)
{
	int ok = 0;
	char *idStr, *valueStr;
	idStr = strtok(NULL, CMD_DELIMITER);
	valueStr = strtok(NULL, CMD_DELIMITER);
	if ((idStr != 0) && (valueStr != 0)) {
		*id = atoi(idStr);
		*value = atoi(valueStr);
		ok = 1;
	}
	return ok;
}

int CliCommand::parseCmd2(int *id, float *value)
{
	int ok = 0;
	char *idStr, *valueStr;
	idStr = strtok(NULL, CMD_DELIMITER);
	valueStr = strtok(NULL, CMD_DELIMITER);
	if ((idStr != 0) && (valueStr != 0)) {
		*id = atoi(idStr);
		*value = atof(valueStr);
		ok = 1;
	}
	return ok;
}

int CliCommand::parseCmd1(int *value)
{
	int ok = 0;
	char *valueStr;
	valueStr = strtok(NULL, CMD_DELIMITER);
	if (valueStr != 0) {
		*value = atoi(valueStr);
		ok = 1;
	}
	return ok;
}

int CliCommand::okAnswer(char *pAnswer)
{
	strcpy(pAnswer, "ok\n");
	return 4;
}

int CliCommand::errorAnswer(char *pAnswer)
{
	strcpy(pAnswer, "error\n");
	return 7;
}

bool CliCommand::writeToSampleData(char *data, int len)
{
	bool ok = true;
	int samples;

	if (len%4 != 0) {
		printf("Transmitted data block size invalid %d - terminated\n", len);
		m_dataSize = 0;
		ok = false;
	} else {
		samples = len/4;
		if (m_dataSize > samples) {
			m_dataSize -= samples;
		} else {
			m_dataSize = 0;
		}
		m_pTestDataSDCard->appendDataSamples((float*)data, samples);
	}
	return ok;
}

int CliCommand::dataTransfer(char *cmd, char *pAnswer, int len)
{
	int length = len;

	// Handling of updating test sample data
	if (writeToSampleData(cmd, len)) {
		m_blockCnt++;
		xil_printf("%06d-%07d\r", m_blockCnt, m_dataSize);
		sprintf(pAnswer, "%06d\r", m_blockCnt);
		length = strlen(pAnswer); // Block count reply
		//length = okAnswer(pAnswer);
		if (m_dataSize == 0)
			printf("\nData transfer completed\r\n");
	} else {
		m_dataSize = 0;
		length = errorAnswer(pAnswer);
	}
	return length;
}

int CliCommand::openFileRead(char *name, int *size)
{
	int result;

	result = m_file.mount();
	if (result != XST_SUCCESS) printf("Failed to mount SD card\r\n");

	// Read size of file
	*size = m_file.size(name);

	// Open existing file<<
	result = m_file.open(name, FA_OPEN_EXISTING | FA_READ);
	if (result != XST_SUCCESS)
		printf("Failed open file for reading\r\n");

	return result;
}

int CliCommand::openFileWrite(char *name)
{
	int result;

	result = m_file.mount();
	if (result != XST_SUCCESS) printf("Failed to mount SD card\r\n");

	// Create a new file
	result = m_file.open(name, FA_CREATE_ALWAYS | FA_WRITE);
	if (result != XST_SUCCESS) printf("Failed open file for writing\r\n");

	return result;
}

int CliCommand::writeToFile(char *data, int len)
{
	int result;

	if (m_fileSize > len) {
		m_fileSize -= len;
	} else {
		m_fileSize = 0;
	}

	// Write to file
	result = m_file.write((void *)data, len, true);
	if (result != XST_SUCCESS) {
		printf("Failed writing to file\r\n");
		m_file.close();
	}

	if (m_fileSize == 0) {
		result = m_file.close();
		if (result != XST_SUCCESS)
			printf("Failed closing file\r\n");
	}

	return result;
}

int CliCommand::readFromFile(char *data, int *lenRead, int len)
{
	int result;

	// Read from file
	result = m_file.read((void *)data, len, false);
	if (result != XST_SUCCESS) {
		printf("Failed reading data size %d from file\r\n", len);
		m_file.close();
		m_fileSize = 0;
		return result;
	}

	// Get actual number of bytes read from file
    *lenRead = m_file.getReadSize();

    // Decrease file size
	if (m_fileSize > *lenRead) {
		m_fileSize -= *lenRead;
	} else {
		m_fileSize = 0;
	}

	// Close file when done
	if (m_fileSize == 0) {
		result = m_file.close();
		if (result != XST_SUCCESS)
			printf("Failed closing file\r\n");
	}

	return result;
}

int CliCommand::fileTransfer(char *cmd, char *pAnswer, int len)
{
	int length = len;

	if (m_uploadFile) {
		// Handling of file transfer, upload of files to SD card from computer
		if (writeToFile(cmd, len) == XST_SUCCESS) {
			m_blockCnt++;
			xil_printf("%06d-%06d\r", m_blockCnt, m_fileSize/UPLD_BLOCK_SIZE); // Kilo bytes
			sprintf(pAnswer, "%06d\r", m_blockCnt);
			length = strlen(pAnswer); // Block count reply
			//length = 0; // No reply
			if (m_fileSize == 0)
				printf("\nFile transfer completed\r\n");
		} else {
			m_fileSize = 0;
			length = errorAnswer(pAnswer);
		}
	} else {
		// Handling of file transfer, download files from SD card to computer
		if (readFromFile(pAnswer, &length, DOWN_BLOCK_SIZE) == XST_SUCCESS) {
			m_blockCnt++;
			xil_printf("%06d-%06d\r", m_blockCnt, m_fileSize/DOWN_BLOCK_SIZE); // x4 Kilo bytes
			if (m_fileSize == 0)
				printf("\nFile transfer completed\r\n");
		} else {
			strcpy(pAnswer, "f,error\n");
			length = strlen(pAnswer);
			m_fileSize = 0;
		}
	}

	return length;
}

bool CliCommand::checkNr(int nr)
{
	return (nr > 0 && nr <= TEMP_NUM);
}

int CliCommand::fileOperation(char *paramStr, char *answer)
{
	int value;
	char *param = strtok(paramStr, CMD_DELIMITER);
	int ok = 0;

	if (param != NULL) {

		switch (param[0]) {

			case 'd': // Delete file on SD card
				if (parseStrCmd1(m_fileName)) {
					if (strlen(m_fileName) < FILE_NAME_LEN) { // Filenames max. 8 chars + extension 4
						if (m_file.del(m_fileName) == XST_SUCCESS) {
							ok = 1;
						}
					}  else {
						printf("Too long file name %s\n", m_fileName);
					}
				}
				break;

			case 'n': // Rename file on SD Card
				if (parseStrCmd2(m_fileName, m_fileNameNew)) {
					if (strlen(m_fileName) < FILE_NAME_LEN && strlen(m_fileNameNew) < FILE_NAME_LEN ) { // Filenames max. 8 chars + extension 4
						if (m_file.rename(m_fileName, m_fileNameNew) == XST_SUCCESS) {
							ok = 1;
						}
					}  else {
						printf("Too long file names  %s -> %s\n", m_fileName, m_fileNameNew);
					}
				}
				break;

			case 'u': // Upload file of size
				if (parseStrCmd2(m_fileName, &value)) {
					if (strlen(m_fileName) < FILE_NAME_LEN) { // Filenames max. 8 chars + extension 4
						if (openFileWrite(m_fileName) == XST_SUCCESS) {
							m_fileSize = value;
							printf("Start upload file %s of size %d\n", m_fileName, m_fileSize);
							m_uploadFile = true;
							m_blockCnt = 0;
							ok = 1;
						} else {
							printf("Could not open file %s\n", m_fileName);
						}
					}  else {
						printf("Invalid file name %s length (max. 8+3)\n", m_fileName);
					}
				}
				break;

			case 'w': // Download file to computer
				if (parseStrCmd1(m_fileName)) {
					if (strlen(m_fileName) < FILE_NAME_LEN) { // Filenames max. 8 chars + extension 4
						if (openFileRead(m_fileName, &m_fileSize) == XST_SUCCESS) {
							printf("Start download file %s of size %d\n", m_fileName, m_fileSize);
							m_uploadFile = false;
							m_blockCnt = 0;
							sprintf(answer, "f,ok,%d\n",m_fileSize);
							ok = 2;
						} else {
							m_fileSize = 0;
							printf("Could not open file %s\n", m_fileName);
						}
					}  else {
						printf("Invalid file name %s length (max. 8+3)\n", m_fileName);
					}
				}
				break;

		}
	}

	// Return answer to file command
	if (ok == 1) strcpy(answer, "f,ok\n");
	if (ok == 0) strcpy(answer, "f,error\n");
	return strlen(answer)+1;
}

int CliCommand::setParameter(char *paramStr, char *answer)
{
	int value,nr,W,L,tmp;
	float floatValue;
	char *param = strtok(paramStr, CMD_DELIMITER);
	int ok = 0;

	if (param != NULL) {

		switch (param[0]) {

			case 'a': // Set min/max difference above limits
				if (parseShortArray(&nr)) {
					if (checkNr(nr)) {
						m_pTemplateMatch->getConfig()->setPeakMinMaxLimits(nr-1, mShortArray);
						printf("Template %d min/max difference limits updated [%d, %d, %d, %d..]\n",
								nr, mShortArray[0], mShortArray[1], mShortArray[2], mShortArray[3]);
						ok = 1;
					}
				}
				break;

			case 'b': // Set max. limit for coherency
				if (parseCmd3(&nr, &value, &tmp)) {
					if (checkNr(nr)) {
						m_pTemplateMatch->getConfig()->setMinCoherncy(nr-1, value);
						m_pTemplateMatch->getConfig()->setMaxCoherncy(nr-1, tmp);
						printf("Template %d coherency limits set to min. %d and max. %d\n", nr, value, tmp);
						ok = 1;
					}
				}
				break;

			case 'c': // Set trigger output when template 1 and 2 seen within samples
				if (parseCmd1(&value)) {
					m_pTemplateMatch->getConfig()->setCounter(0, value); // Only set for template 1
					printf("Trigger output set when template 1 and 2 seen within %d samples\n", value);
					ok = 1;
				}
				break;

			case 'd': // Update template data
				if (parseTemplate(&nr, &W, &L)) {
					if (checkNr(nr)) {
						m_pTemplateMatch->updateTemplateData(nr-1, mTemplate, L, W);
						printf("Template %d of size %d*%d updated\n", nr, W, L);
						ok = 1;
					}
				}
				break;

			case 'e': // Set duration of experiment in seconds
				if (parseCmd1(&value)) {
					m_numSamples = value*Fs_RATE; // Sample rate = 30 kHz
					m_pTestDataSDCard->setNumSamplesCollect(m_numSamples);
					printf("Duration of experiment set to %d sec. processing %d samples\n", value, m_numSamples);
					ok = 1;
				}
				break;

			case 'f': // Load binary filter coefficients from SD card (*.bin)
				if (parseStrCmd1(m_fileName)) {
					if (strlen(m_fileName) < FILE_NAME_LEN) { // Filenames max. 8 chars + extension 4
						if (m_pTemplateMatch->getConfig()->loadCoeffBin(m_fileName) == XST_SUCCESS) {
							ok = 1;
						}
					}  else {
						printf("Too long file name %s\n", m_fileName);
					}
				}
				break;

			case 'g': // Set gradient for template
				if (parseCmd2(&nr, &value)) {
					if (checkNr(nr)) {
						m_pTemplateMatch->getConfig()->setMinGradient(nr-1, value);
						printf("Template %d gradient set to %d\n", nr, value);
						ok = 1;
					}
				}
				break;

			case 'h': // Set peak high limits
				if (parseShortArray(&nr)) {
					if (checkNr(nr)) {
						m_pTemplateMatch->getConfig()->setPeakMaxLimits(nr-1, mShortArray);
						printf("Template %d peak high limits updated [%d, %d, %d, %d..]\n",
								nr, mShortArray[0], mShortArray[1], mShortArray[2], mShortArray[3]);
						ok = 1;
					}
				}
				break;

			case 'l': // Set peak low limits
				if (parseShortArray(&nr)) {
					if (checkNr(nr)) {
						m_pTemplateMatch->getConfig()->setPeakMinLimits(nr-1, mShortArray);
						printf("Template %d peak low limits updated [%d, %d, %d, %d..]\n",
								nr, mShortArray[0], mShortArray[1], mShortArray[2], mShortArray[3]);
						ok = 1;
					}
				}
				break;

			case 'k': // Set enable printing counters during template matching
				if (parseCmd1(&value)) {
					if (value >= 0) {
						m_pTemplateMatch->setPrintDebug(value > 0);
						printf("%d printing counters during template matching\n", value);
						ok = 1;
					}
				}
				break;

			case 'm': // Set channel mapping
				if (parseShortArray(&nr)) {
					if (checkNr(nr)) {
						m_pTemplateMatch->getConfig()->setChannelMap(nr-1, mShortArray);
						printf("Template %d channel map updated [%d, %d, %d, %d..]\n",
								nr, mShortArray[0], mShortArray[1], mShortArray[2], mShortArray[3]);
						ok = 1;
					}
				}
				break;

			case 'n': // Set number of template to use
				if (parseCmd1(&value)) {
					if (0 < value && value <= TEMP_NUM) {
						m_pTemplateMatch->getConfig()->setNumTemplates(value);
						printf("Number of templates to use %d\n", value);
						ok = 1;
					}
				}
				break;

			case 'o': // Open and load sample data from file name using specified number of samples
			 	if (parseStrCmd2(m_fileName, &value)) {
					if (strlen(m_fileName) < FILE_NAME_LEN && m_pTestDataSDCard != 0) { // Filenames max. 8 chars + extension 4
						printf("Load test data from file %s using %d samples\n", m_fileName, value);
						if (m_pTestDataSDCard->readFile(m_fileName) == XST_SUCCESS) {
							m_numSamples = value;
							ok = 1;
						}
					}  else {
						printf("Invalid file %s or not possible\n", m_fileName);
					}
				}
				break;

			case 'p': // Set processing mode
				if (parseCmd1(&value)) {
					if (0 <= value && value <= 3) {
						printf("Processing mode %d\n", value);
						m_executeMode = value;
						ok = 1;
					}
				}
				break;

			case 't': // Set threshold for template
				if (parseCmd2(&nr, &floatValue)) {
					if (checkNr(nr)) {
						m_pTemplateMatch->getConfig()->setThreshold(nr-1, floatValue);
						printf("Template %d threshold set to %f\n", nr, floatValue);
						ok = 1;
					}
				}
				break;

			case 'u': // Upload sample data used for testing
				if (parseCmd1(&value)) {
					if (value < int(MAX_NUM_SAMPLES*NUM_CHANNELS) &&
						m_pTestDataSDCard != 0) {
						m_pTestDataSDCard->resetDataBuffer();
						m_dataSize = value;
						m_blockCnt = 0;
						printf("Start upload sample data of size %d\n", value);
						ok = 1;
					}
				}
				break;

			case 'w': // Set saving raw or filtered sample data
				if (parseCmd1(&value)) {
					if (value >= 0) {
						m_pTemplateMatch->setSaveRawData(value > 0);
						printf("%d saving raw (1) or filtered (0) sample data\n", value);
						ok = 1;
					}
				}
				break;
		}
	}

	// Return answer to set command
	if (ok) strcpy(answer, "s,ok\n");
	else strcpy(answer, "s,error\n");
	return strlen(answer)+1;
}

int CliCommand::getParameter(char *paramStr, char *answer)
{
	int value;
	char *param = strtok(paramStr, CMD_DELIMITER);
	int ok = 0;
	Switch sw;

	switch (param[0]) {

		case 'c': // Read configuration
			m_pTemplateMatch->updateConfig(m_numSamples);
			memset(commandsText, 0, sizeof(commandsText));
			m_pTemplateMatch->printSettings(commandsText);
			strcpy(answer, commandsText);
			//strcpy(answer, "Printed on USB-UART\n");
			ok = 1;
			break;

		case 'e': // Read execution time
			printf("Execution time %d sec, samples %d\n", m_numSamples/Fs_RATE, m_numSamples);
			sprintf(answer, "Time,%d\n", m_numSamples/Fs_RATE);
			ok = 1;
			break;

		case 'p': // Read processing mode
			printf("Processing mode %d\n", m_executeMode);
			sprintf(answer, "Mode,%d\n", m_executeMode);
			ok = 1;
			break;

		case 's': // Read switch settings on ZedBoard
			value = sw.readio();
			printf("Switches 0x%02X\n", value);
			sprintf(answer, "Switches,0x%02X\n", value);
			ok = 1;
			break;

		case 't': // Get first high and low time stamps of
			printf("TimeStamp %d,%d\n", m_pTemplateMatch->getFirstTimeStampHigh(),
					                    m_pTemplateMatch->getFirstTimeStampLow());
			sprintf(answer, "TimeStamp,%d,%d\n", m_pTemplateMatch->getFirstTimeStampHigh(),
					                             m_pTemplateMatch->getFirstTimeStampLow());
			ok = 1;
			break;

		case 'v': // Read version number
			printf("Software version number %d.%d\n", majorVer_, minorVer_);
			sprintf(answer, "Version,%d.%d\n", majorVer_, minorVer_);
			ok = 1;
			break;

		case 'w': // Set saving raw or filtered sample data
			printf("Saving (%d) sample data as raw (1) or filtered (0)\n", m_pTemplateMatch->getSaveRawData());
			sprintf(answer, "SaveRawData,%d\n",  m_pTemplateMatch->getSaveRawData());
			ok = 1;
			break;


	}

	// Return answer to get command
	if (!ok) strcpy(answer, "g,error\n");
	return strlen(answer)+1;
}

//------------------------------------------------------------------------------------------------
// Parse and execute commands
// -----------------------------------------------------------------------------------------------
int CliCommand::execute(char *cmd, char *pAnswer, int len, int id)
{
	int length = len;
	//TimeMeasure time;

	if (m_fileSize > 0) {
		// Handling of file transfer (upload and download files on SD-card)
		length = fileTransfer(cmd, pAnswer, len);
	} else if (m_dataSize > 0) {
		// Handling of updating test sample data (in memory)
		length = dataTransfer(cmd, pAnswer, len);
	} else {

		if (length > 0) cmd[length-1] = 0; // Remove newline character
		// Handling of ASCII commands
		switch (cmd[0]) {

			case 's': // Set parameter
				length = setParameter(&cmd[1], pAnswer);
				break;

			case 'g': // Get parameter
				length = getParameter(&cmd[1], pAnswer);
				break;

			case 'f': // File operations
				length = fileOperation(&cmd[1], pAnswer);
				break;

			case 'l':
				if (m_file.list(commandsText, sizeof(commandsText)) == 0)
					strcpy(commandsText, (char*)"No files on SD card\n");
			    length = strlen(commandsText)+1;
				strcpy(pAnswer, commandsText);
				break;

			case 'b': // Start processing neuron samples
				if (m_executeMode == 0) { // Transmitting UDP samples to PC
					m_pDataThread->runThread(Thread::PRIORITY_ABOVE_NORMAL, "DataUDPThread");
					length = okAnswer(pAnswer);
				} else if (m_executeMode == 3) { // Collecting and writing sample to "HPPDATA.BIN" file
					if (m_pTestDataSDCard->isRunning()) {
						strcpy(pAnswer, "Running\n");
						length = strlen(pAnswer)+1;
					} else {
						m_pTestDataSDCard->runThread(Thread::PRIORITY_NORMAL, "DataSDCard");
						length = okAnswer(pAnswer);
					}
				} else { // Performing template matching
					if (m_pTemplateMatch->isRunning()) {
						strcpy(pAnswer, "Running\n");
						length = strlen(pAnswer)+1;
					} else {
						m_pTemplateMatch->updateConfig(m_numSamples);
						//m_pTemplateMatch->runThread(Thread::PRIORITY_ABOVE_NORMAL, "TemplateMatch");
						m_pTemplateMatch->runThread(Thread::PRIORITY_NORMAL, "TemplateMatch");
						length = okAnswer(pAnswer);
					}
				}
				break;

			case 'e': // Stop processing of neuron samples
				if (m_executeMode == 0)
					m_pDataThread->setStreaming(false);
				else if (m_executeMode == 3)
					m_pTestDataSDCard->stopRunning();
				else
					m_pTemplateMatch->stopRunning();
				length = okAnswer(pAnswer);
				break;

			case 'k': // Kill processing of neuron samples
				if (m_executeMode == 0) {
					m_pDataThread->setStreaming(false);
					m_pDataThread->kill();
				} else if (m_executeMode == 3) {
					m_pTestDataSDCard->stopAndKill();
				} else {
					m_pTemplateMatch->stopAndKill(); // Stop and kill thread
				}
				length = okAnswer(pAnswer);
				break;

			case '?':
				length = printCommands();
				//printf(commandsText);
				strcpy(pAnswer, commandsText);
				break;
		}

	}

	return length;
}

int CliCommand::printCommands(void)
{
	char string[200];
	commandsText[0] = 0;

	sprintf(string, "\r\nNeuron Data Analyzer Version %d.%d:\r\n", majorVer_, minorVer_);
	strcat(commandsText, string);
	sprintf(string, "-----------------------------------\r\n");
	strcat(commandsText, string);
	sprintf(string, "? - display this command menu\r\n");
	strcat(commandsText, string);
	sprintf(string, "b - begin processing neuron samples\r\n");
	strcat(commandsText, string);
	sprintf(string, "e - end processing neuron samples\r\n");
	strcat(commandsText, string);
	sprintf(string, "k - kill processing neuron samples (Used when stucked)\r\n");
	strcat(commandsText, string);

	sprintf(string, "\r\nFile operations SD card:\r\n");
	strcat(commandsText, string);
	sprintf(string, "------------------------\r\n");
	strcat(commandsText, string);

	sprintf(string, "l - list files on SD card\r\n");
	strcat(commandsText, string);
	sprintf(string, "f,d,<filename> - delete file on SD card\r\n");
	strcat(commandsText, string);
	sprintf(string, "f,n,<oldName>,<newName> - rename file on SD card\r\n");
	strcat(commandsText, string);
	sprintf(string, "f,u,<filename>(,<size>) - upload file to SD card (size in bytes and binary data to be send after command)\r\n");
	strcat(commandsText, string);
	sprintf(string, "f,w,<filename> - download a file from SD card (binary data to be received after command)\r\n");
	strcat(commandsText, string);

	sprintf(string, "\r\nSet(s)/Get(g) parameters:\r\n");
	strcat(commandsText, string);
	sprintf(string, "-------------------------\r\n");
	strcat(commandsText, string);

	sprintf(string, "s,a,<nr>,<l0>,<l1>,<l2>..<l4> - set template (1-6) min/max difference above limits for mapped channels (l0-l4)\r\n");
	strcat(commandsText, string);
	sprintf(string, "s,b,<nr>,<min>,<max> - set coherency min/max limits for template (1-6) to be valid\r\n");
	strcat(commandsText, string);
	sprintf(string, "s,d,<nr>,<W>,<L>,<d1>,<d2>..<dN> - update template (1-6) of size N=W*L with flattered data d1..dN of floats\r\n");
	strcat(commandsText, string);
	sprintf(string, "s,c,<counter> - set trigger output when template 1 and 2 seen within samples\r\n");
	strcat(commandsText, string);
	sprintf(string, "s,e,<sec> - set duration of experiment in seconds\r\n");
	strcat(commandsText, string);
	sprintf(string, "s,f,<filename> - open and load filter coefficients from SD card (only *.bin format)\r\n");
	strcat(commandsText, string);
	// Removed in version 4.x
	//sprintf(string, "s,g,<nr>,<grad> - set gradient for template (1-6) where min. peak and peak(n-6) must be greater than <grad>\r\n"); // For all channels
	//strcat(commandsText, string);
	sprintf(string, "s,h,<nr>,<h0>,<h1>,<h2>..<h4> - set template (1-6) peak high limits for mapped channels (h0-h4)\r\n");
	strcat(commandsText, string);
	sprintf(string, "s,l,<nr>,<l0>,<l1>,<l2>..<l4> - set template (1-6) peak low limits for mapped channels (l0-l4)\r\n");
	strcat(commandsText, string);
	sprintf(string, "s,k,<0|1> - set enable (1) printing counters during template matching\r\n");
	strcat(commandsText, string);
	sprintf(string, "s,m,<nr>,<c0>,<c1>,<c2>..<c4> - set template (1-6) channel mapping to neuron channels (cX = 0-31)\r\n");
	strcat(commandsText, string);
	sprintf(string, "s,n,<num> - set number (1-6) of templates to be used\r\n");
	strcat(commandsText, string);
	sprintf(string, "s,o,<filename>,<samples> - open and load sample data file from SD card\r\n");
	strcat(commandsText, string);
	sprintf(string, "s,p,<mode> - processing mode: tx UDP samples(0), real-time trigger(1), trigger SD card(2), collect on SD card(3)\r\n");
	strcat(commandsText, string);
	sprintf(string, "s,t,<nr>,<thres> - set threshold for template (1-6) used to trigger neuron activation using NXCOR\r\n"); // Normalized cross correlation
	strcat(commandsText, string);
	sprintf(string, "s,u,<size> - upload sample data (32 channels) of size in bytes - binary floats to be send after command\r\n");
	strcat(commandsText, string);
	sprintf(string, "s,w,<0|1> - set saving raw RAWDATA.BIN (1) or filtered IIRFILT.BIN/FIRFILT.BIN (0) sample data\r\n");
	strcat(commandsText, string);

	sprintf(string, "g,c - read configuration for template matching using NXCOR\r\n");
	strcat(commandsText, string);
	sprintf(string, "g,e - read execution time\r\n");
	strcat(commandsText, string);
	sprintf(string, "g,p - read processing mode: tx UDP samples(0), real-time trigger(1), trigger SD card(2), collect on SD card(3)\r\n");
	strcat(commandsText, string);
	sprintf(string, "g,s - read switch settings only on ZedBoard\r\n");
	strcat(commandsText, string);
	sprintf(string, "g,t - get first high and low time stamps of sample data\r\n");
	strcat(commandsText, string);
	sprintf(string, "g,v - read software version\r\n");
	strcat(commandsText, string);
	sprintf(string, "g,w - read saving raw (1) or filtered (0) sample data\r\n");
	strcat(commandsText, string);

	return strlen(commandsText)+1;
}
