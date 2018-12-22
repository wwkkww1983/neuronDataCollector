//  NeuronData.h
//  Implementation of the Class NeuronData
//  Created on:      7-december-2018 11:01:53
//  Original author: Kim Bjerge
///////////////////////////////////////////////////////////

#if !defined(NEURON_DATA_INCLUDED_)
#define NEURON_DATA_INCLUDED_

#include "LxRecord.h"

class NeuronData
{

public:
	NeuronData();
	virtual ~NeuronData();

	void SetPulseActive(bool generate) {
		m_generatePulse = generate;
	}

	virtual void GenerateSampleRecord(LRECORD *pLxRecord);

protected:
	void AddCheckSum(LRECORD *pLxRecord);
	bool m_generatePulse;
	int m_n;
};

#endif // !defined(NEURON_DATA_INCLUDED_)