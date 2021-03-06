/*
 * Template.h
 *
 *  Created on: 25. dec. 2018
 *      Author: Kim Bjerge
 */

#ifndef SRC_TEMPLATE_H_
#define SRC_TEMPLATE_H_

#include <string>
using namespace std;
#include "FileSDCard.h"

#define DATA_FORMAT_CHAR     7   // Number of bits used for data and template
#define DATA_FORMAT_SHORT    15  // Number of bits used for data and template

#define TEMP_WIDTH  	5	// 9 Template width 8/9(Thesis - optimized)
#define TEMP_LENGTH     25  // 17 Template length 16/17(Thesis - optimized)
#define TEMP_SIZE       (TEMP_WIDTH*TEMP_LENGTH)

// Type of data used for templates and NXCOR
#define TTYPE           int16_t // Version 1.1 - optimized using 16 bit - Version 1.0 uses int32_t
#define SINT            2     	// Number of samples in int32_t

class Template
{
public:
	Template() : mMean(0.0), mVariance(1.0), mChOffset(0),
	 	 	 	 mLength(TEMP_LENGTH), mWidth(TEMP_WIDTH),
				 mIndexTMin(0), mIndexTMax(0), m_file((char *)"0:/") { }

	void clearTemplate(void);
	int loadTemplate(std::string name, int length, int width);
	TTYPE *getTemplate(void) { return mTemplateInt; }
	float getVariance(void) { return mVariance; }
	float getMean(void) { return mMean; }
	int getIndexMin(void) { return mIndexTMin; }
	int getIndexMax(void) { return mIndexTMax; }
	//int getChOffset(void) { return mChOffset; } // NOT USED after Ver. 1.1
	const char *getTemplateName(void) { return mFileName.c_str(); }
	void updateData(float *data, int length, int width, int nr);

private:
	void convertData(void);
	void calcMeanVariance(void);
	void calcMinMaxIndex(void);
	void readChOffset(string name);
    float mTemplate[TEMP_SIZE];
    TTYPE mTemplateInt[TEMP_SIZE];
    float mMean;
    float mVariance;
    int mChOffset;
    int mLength;
    int mWidth;
    int mIndexTMin;
    int mIndexTMax;
    string mFileName;
    FileSDCard m_file;
};

#endif /* SRC_TEMPLATE_H_ */
