/*
 * CliCmdTemplates.h
 *
 *  Created on: 21/03/2017
 *      Author: kbe
 */

#ifndef CLICOMMAND_H_
#define CLICOMMAND_H_

#define VERSION_HI		4	  // 2.5 Last version with FIR filter - version 4.x template 25x5
#define VERSION_LO		0     // 3.0 scale input 2 bits others don't

#if VERSION_HI == 2
#include "TemplateMatch.h"// "TemplateMatch.h" version 2.5
#else
#include "TemplateMatchIIR.h"// version 3.x and 4.x
#endif

#include "DataUDPThread.h"
#include "FileSDCard.h"
#include "TestDataSDCard.h"

#define CMD_BUF_SIZE    8192
#define DOWN_BLOCK_SIZE 4096 // File block size for downloading files from SD-card
#define UPLD_BLOCK_SIZE 1024 // File block size for uploading files to SD-card

class CliCommand {
public:
	CliCommand(TemplateMatch *pTemplateMatch, DataUDPThread *pDataThread, TestDataSDCard *pTestDataSDCard = 0);
	int execute(char *cmd, char *pAnswer, int len, int id);
	int printCommands(void);
	void reset(void);
	int getMode(void) { return m_executeMode; }
private:
	TemplateMatch *m_pTemplateMatch;
	DataUDPThread *m_pDataThread;
	TestDataSDCard *m_pTestDataSDCard;
	int parseTemplate(int *nr, int *W, int *L);
	int parseShortArray(int *nr);
	int parseStrCmd2(char *name, int *value);
	int parseStrCmd2(char *name1, char *name2);
	int parseStrCmd1(char *name);
	int parseCmd3(int *id, int *value1, int *value2);
	int parseCmd2(int *id, int *value);
	int parseCmd2(int *id, float *value);
	int parseCmd1(int *value);
	int okAnswer(char *pAnswer);
	int errorAnswer(char *pAnswer);
	int fileOperation(char *paramStr, char *answer);
	int setParameter(char *paramStr, char *answer);
	int getParameter(char *paramStr, char *answer);
	int openFileRead(char *name, int *size);
	int openFileWrite(char *name);
	int writeToFile(char *data, int len);
	int readFromFile(char *data, int *lenRead, int len);
	int fileTransfer(char *cmd, char *pAnswer, int len);
	int dataTransfer(char *cmd, char *pAnswer, int len);
	bool writeToSampleData(char *data, int len);
	bool checkNr(int nr);
	int majorVer_;
	int minorVer_;
	int m_dataSize; // Used to transfer data samples
	int m_fileSize; // Used to transfer data files
	bool m_uploadFile; // True during upload and false during download of files
	int m_idLocked; // Locked by socket id
	int m_blockCnt;
	int m_executeMode; // 0 = UDP transfer data, 1 = Template matching, 2 = Template matching from SD card
	int m_numSamples;
	char m_fileName[50];
	char m_fileNameNew[50];
	char commandsText[CMD_BUF_SIZE];
	FileSDCard m_file;
    float mTemplate[TEMP_SIZE];
    short mShortArray[TEMP_WIDTH];
};

#endif /* AUDIOCOMMAND_H_ */
