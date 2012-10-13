/* kicktrigger.c, (c) 2012, Immanuel Albrecht
 * 
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 * 
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
 * details.
 * 
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <http://www.gnu.org/licenses/>. 
 **/

/*****************************************************************************/

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

/*****************************************************************************/

#include "ladspa.h"

/*****************************************************************************/

/* The port and data numbers for the plugin: */

/*
 * Ports for each channel are:
 */

#define N_PORTS 14

static const char* szPortNames[] = { "Trigger threshold", "Release threshold",
		"Release delay", "Click level", "Click delay", "Click release",
		"Triggered input level", "Stand-by input level",
		"Input trigger release", "Input gain", "Input vs threshold",
		"Trigger count"
/* Input, Output
 */
};

static int isPortLogarithmic[] = { 1, 1, 0, 1, 0, 0, 1, 1, 0, 1, 1, 1 };
static int isPortInput[] = { 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0 };
static int defaultOne[] = { 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 0, 0 };
static int isInteger[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 };
static int hasUpperBound[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 };
static LADSPA_Data upperBound[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 100 };

/*
 * current machine state, reset on activation
 * 
 *  0: triggered?
 *  1: accumulated release time
 *  2: click delay left
 *  3: click release left
 *  4: current input gain
 *  5: click frame count
 */

#define N_DATA 6

/*****************************************************************************/

/* The structure used to hold port connection information and state
 */

typedef LADSPA_Data ** KickTrigger;

/*****************************************************************************/

/* Construct a new plugin instance. */
LADSPA_Handle instantiateKickTrigger(const LADSPA_Descriptor * Descriptor,
		unsigned long SampleRate) {

	KickTrigger instance;
	int i;
	int channels;
	int *pChannels;
	LADSPA_Data* data;

	channels = Descriptor->PortCount / N_PORTS;

	instance = malloc(sizeof(LADSPA_Data*) * (N_PORTS + N_DATA) * channels + 1);

	pChannels = malloc(sizeof(int));

	*instance = (LADSPA_Data*) pChannels;

	*pChannels = channels;

	data = malloc(sizeof(LADSPA_Data) * channels * N_DATA);

	for (i = N_PORTS * channels + 1;
			i < N_PORTS * channels + 1 + N_DATA * channels; i++) {
		instance[i] = data;
		++data;
	}

	return instance;
}

/*****************************************************************************/

/* Connect a port to a data location. */
void connectPortToKickTrigger(LADSPA_Handle Instance, unsigned long Port,
		LADSPA_Data * DataLocation) {

	KickTrigger psKickTrigger;

	psKickTrigger = (KickTrigger) Instance;

	psKickTrigger[Port + 1] = DataLocation;
}

/*****************************************************************************/

void activateKickTrigger(LADSPA_Handle Instance) {
	KickTrigger psKickTrigger;
	int channel, channelCount, i;
	int *pChannelCount;

	psKickTrigger = (KickTrigger) Instance;

	pChannelCount = (int*) *psKickTrigger;

	channelCount = *pChannelCount;

	for (channel = 0; channel < channelCount; ++channel) {
		for (i = 0; i < N_DATA; ++i) {
			if (i == 4) {
				*(psKickTrigger[channelCount * N_PORTS + 1 + N_DATA * channel
						+ i]) = *(psKickTrigger[1 + channel * N_PORTS + 7]);
			} else
				*(psKickTrigger[channelCount * N_PORTS + 1 + N_DATA * channel
						+ i]) = 0.f;
		}
	}
}

/*****************************************************************************/

LADSPA_Data *noise;

void runKickTrigger(LADSPA_Handle Instance, unsigned long SampleCount) {
	LADSPA_Data * pfInput;
	LADSPA_Data * pfOutput;

	KickTrigger psKickTrigger;
	unsigned long lSampleIndex;
	int channel, channelCount;
	int *pChannelCount;

	/*
	 * current machine state, reset on activation
	 *
	 *  0: triggered?
	 *  1: accumulated release time
	 *  2: click delay left
	 *  3: click release left
	 *  4: current input gain
	 *  5: click count
	 */
	LADSPA_Data *pTrig, *pAccRel, *pClDel, *pClRel, *pInput, *pClickFrame;

	/* "Trigger threshold",
	 * "Release threshold",
	 * "Release delay",
	 * "Click level",
	 * "Click delay",
	 * "Click release",
	 * "Triggered input level",
	 * "Stand-by input level",
	 * "Input trigger release"
	 */
	LADSPA_Data *pTrigThr, *pRelThr, *pRelDelay, *pClLvl, *pClDelay,
			*pClRelease, *pTriggered, *pStandby, *pTrigRelease;

	LADSPA_Data smp;
	LADSPA_Data max_smp;
	LADSPA_Data smp_abs;
	LADSPA_Data amount, clickFactor, releaseThreshold, triggerThreshold;

	psKickTrigger = (KickTrigger) Instance;

	pChannelCount = (int*) *psKickTrigger;

	channelCount = *pChannelCount;

	for (channel = 0; channel < channelCount; ++channel) {
		pTrig = psKickTrigger[1 + N_PORTS * channelCount + N_DATA * channel];
		pAccRel = psKickTrigger[1 + N_PORTS * channelCount + N_DATA * channel
				+ 1];
		pClDel =
				psKickTrigger[1 + N_PORTS * channelCount + N_DATA * channel + 2];
		pClRel =
				psKickTrigger[1 + N_PORTS * channelCount + N_DATA * channel + 3];
		pInput =
				psKickTrigger[1 + N_PORTS * channelCount + N_DATA * channel + 4];
		pClickFrame = psKickTrigger[1 + N_PORTS * channelCount
				+ N_DATA * channel + 5];

		pTrigThr = psKickTrigger[1 + N_PORTS * channel];
		pRelThr = psKickTrigger[1 + N_PORTS * channel + 1];
		pRelDelay = psKickTrigger[1 + N_PORTS * channel + 2];
		pClLvl = psKickTrigger[1 + N_PORTS * channel + 3];
		pClDelay = psKickTrigger[1 + N_PORTS * channel + 4];
		pClRelease = psKickTrigger[1 + N_PORTS * channel + 5];
		pTriggered = psKickTrigger[1 + N_PORTS * channel + 6];
		pStandby = psKickTrigger[1 + N_PORTS * channel + 7];
		pTrigRelease = psKickTrigger[1 + N_PORTS * channel + 8];

		pfInput = psKickTrigger[1 + N_PORTS * channel + N_PORTS - 2];
		pfOutput = psKickTrigger[1 + N_PORTS * channel + N_PORTS - 1];

		amount = fabsf(*pStandby - *pTriggered)
				/ (4.f * 1024.f * *pTrigRelease);

		if (amount < 0.000001f)
			amount = 0.000001f;

		clickFactor = *pClLvl / (*pClRelease * 1024.f);

		triggerThreshold = 0.9f * *pTrigThr;

		releaseThreshold = *pRelThr * triggerThreshold * 0.4f;
		max_smp = 0;

		for (lSampleIndex = 0; lSampleIndex < SampleCount; lSampleIndex++) {
			smp = *(pfInput++);
			smp_abs = fabsf(smp);

			if (max_smp < smp_abs)
				max_smp = smp_abs;

			*pfOutput = *pInput * smp;

			if (*pTrig == 0.f) {

				if (*pInput != *pStandby) {
					if (fabsf(*pInput - *pStandby) <= amount)
						*pInput = *pStandby;
					else if (*pInput < *pStandby)
						*pInput += amount;
					else
						*pInput -= amount;
				}

				if (smp_abs >= triggerThreshold) {
					*pTrig = 1.f;
					*pAccRel = 0.f;
					*pClickFrame = 0.f;
					*pClDel = *pClDelay * 64.f;
					*pClRel = *pClRelease * 512.f;
					*pInput = *pTriggered;
					*psKickTrigger[1 + N_PORTS * channel + 11] = (((int)*psKickTrigger[1 + N_PORTS * channel + 11])+1)%101;
				}
			} else {
				if (smp_abs < releaseThreshold) {
					*pAccRel += 1.f;
					if (*pAccRel >= *pRelDelay * 256.f)
						*pTrig = 0.f;
				} else {
					*pAccRel = 0.f;
				}
			}

			if (*pClDel >= 0.f) {
				*pfOutput += *pClLvl * noise[((int) *pClickFrame) % 1024]
						* 0.4f;

				*pClDel -= 1.f;
				*pClickFrame += 1.f;
			} else if (*pClRel >= 0.f) {
				*pfOutput += *pClRel * clickFactor
						* noise[((int) *pClickFrame) % 1024] * 0.4f;

				*pClRel -= 1.f;
				*pClickFrame += 1.f;
			}

			pfOutput++;
		}

		*psKickTrigger[1 + N_PORTS * channel + 9] = *pInput;
		*psKickTrigger[1 + N_PORTS * channel + 10] = max_smp / triggerThreshold;

	}
}

/*****************************************************************************/

/* Throw away a simple delay line. */
void cleanupKickTrigger(LADSPA_Handle Instance) {
	KickTrigger kInstance;
	int* pChannels;

	kInstance = (KickTrigger) Instance;
	pChannels = (int*) *kInstance;

	free(*kInstance);
	free(kInstance[*pChannels * N_PORTS + 1]);
	free(kInstance);
}

/*****************************************************************************/

LADSPA_Descriptor * g_psMonoDescriptor = NULL;
LADSPA_Descriptor * g_psStereoDescriptor = NULL;

/*****************************************************************************/

void fillDescriptor(LADSPA_Descriptor *g_psDescriptor, int channels, int id) {
	char ** pcPortNames;
	LADSPA_PortDescriptor * piPortDescriptors;
	LADSPA_PortRangeHint * psPortRangeHints;

	char label[1024];
	char name[1024];
	char portname[1024];

	int i, j;

	strcpy(label, "kicktrigger_x");
	sprintf(label + strlen(label), "%d", channels);

	strcpy(name, "Kick Trigger ");
	sprintf(name + strlen(name), "%d", channels);
	strcat(name, " Channels");

	g_psDescriptor->UniqueID = id;
	g_psDescriptor->Label = strdup(label);
	g_psDescriptor->Properties = LADSPA_PROPERTY_HARD_RT_CAPABLE;
	g_psDescriptor->Name = strdup(name);
	g_psDescriptor->Maker = strdup("Immanuel Albrecht");
	g_psDescriptor->Copyright = strdup("(c) 2012, GPLv3");

	g_psDescriptor->PortCount = N_PORTS * channels;

	piPortDescriptors = (LADSPA_PortDescriptor *) calloc(N_PORTS * channels,
			sizeof(LADSPA_PortDescriptor));
	g_psDescriptor->PortDescriptors =
			(const LADSPA_PortDescriptor *) piPortDescriptors;

	for (i = 0; i < channels; ++i) {
		for (j = 0; j < N_PORTS - 2; ++j) {
			piPortDescriptors[i * N_PORTS + j] = (
					isPortInput[j] ? LADSPA_PORT_INPUT : LADSPA_PORT_OUTPUT)
					| LADSPA_PORT_CONTROL;
		}

		piPortDescriptors[i * N_PORTS + N_PORTS - 2] = LADSPA_PORT_INPUT
				| LADSPA_PORT_AUDIO;
		piPortDescriptors[i * N_PORTS + N_PORTS - 1] = LADSPA_PORT_OUTPUT
				| LADSPA_PORT_AUDIO;
	}

	pcPortNames = (char **) calloc(N_PORTS * channels, sizeof(char *));
	g_psDescriptor->PortNames = (const char **) pcPortNames;

	for (i = 0; i < channels; ++i) {
		for (j = 0; j < N_PORTS - 2; ++j) {
			strcpy(portname, szPortNames[j]);
			strcat(portname, " for channel ");
			sprintf(portname + strlen(portname), "%d", i);

			pcPortNames[i * N_PORTS + j] = strdup(portname);
		}

		strcpy(portname, "Input ");
		sprintf(portname + strlen(portname), "%d", i);

		pcPortNames[i * N_PORTS + N_PORTS - 2] = strdup(portname);

		strcpy(portname, "Output ");
		sprintf(portname + strlen(portname), "%d", i);

		pcPortNames[i * N_PORTS + N_PORTS - 1] = strdup(portname);
	}

	psPortRangeHints = ((LADSPA_PortRangeHint *) calloc(N_PORTS * channels,
			sizeof(LADSPA_PortRangeHint)));
	g_psDescriptor->PortRangeHints =
			(const LADSPA_PortRangeHint *) psPortRangeHints;

	for (i = 0; i < channels; ++i) {
		for (j = 0; j < N_PORTS - 2; ++j) {
			psPortRangeHints[i * N_PORTS + j].HintDescriptor =
					(LADSPA_HINT_BOUNDED_BELOW
							| (isPortLogarithmic[j] ?
									LADSPA_HINT_LOGARITHMIC : 0)
							| (isInteger[j] ? LADSPA_HINT_INTEGER : 0)
							| (hasUpperBound[j] ? LADSPA_HINT_BOUNDED_ABOVE : 0)
							| (defaultOne[j] ?
									LADSPA_HINT_DEFAULT_1 :
									LADSPA_HINT_DEFAULT_0));

			psPortRangeHints[i * N_PORTS + j].LowerBound = 0;
			psPortRangeHints[i * N_PORTS + j].UpperBound = upperBound[j];
		}
		psPortRangeHints[i * N_PORTS + N_PORTS - 2].HintDescriptor = 0;
		psPortRangeHints[i * N_PORTS + N_PORTS - 1].HintDescriptor = 0;
	}

	g_psDescriptor->instantiate = instantiateKickTrigger;
	g_psDescriptor->connect_port = connectPortToKickTrigger;
	g_psDescriptor->activate = activateKickTrigger;
	g_psDescriptor->run = runKickTrigger;
	g_psDescriptor->run_adding = NULL;
	g_psDescriptor->set_run_adding_gain = NULL;
	g_psDescriptor->deactivate = NULL;
	g_psDescriptor->cleanup = cleanupKickTrigger;
}

/* _init() is called automatically when the plugin library is first
 loaded. */
void _init() {

	int i;

	noise = malloc(sizeof(LADSPA_Data) * 1024);

	for (i = 0; i < 1024; ++i) {
		if (i < 64)
			noise[i] = 1.f;
		else if (i < 128)
			noise[i] = -1.f;
		else if (i < 150)
			noise[i] = 1.f;
		else if (i < 182)
			noise[i] = -1.f;
		else if (i < 198)
			noise[i] = 1.f;
		else if (i < 214)
			noise[i] = -1.f;
		else
			noise[i] = (rand() % 1024 - 512) / 512.f;
	}

	g_psMonoDescriptor = (LADSPA_Descriptor *) malloc(
			sizeof(LADSPA_Descriptor));
	g_psStereoDescriptor = (LADSPA_Descriptor *) malloc(
			sizeof(LADSPA_Descriptor));

	if (g_psMonoDescriptor) {
		fillDescriptor(g_psMonoDescriptor, 1, 4861);
	}

	if (g_psStereoDescriptor) {
		fillDescriptor(g_psStereoDescriptor, 2, 4862);

	}
}

/*****************************************************************************/

void deleteDescriptor(LADSPA_Descriptor * psDescriptor) {
	unsigned long lIndex;
	if (psDescriptor) {
		free((char *) psDescriptor->Label);
		free((char *) psDescriptor->Name);
		free((char *) psDescriptor->Maker);
		free((char *) psDescriptor->Copyright);
		free((LADSPA_PortDescriptor *) psDescriptor->PortDescriptors);
		for (lIndex = 0; lIndex < psDescriptor->PortCount; lIndex++)
			free((char *) (psDescriptor->PortNames[lIndex]));
		free((char **) psDescriptor->PortNames);
		free((LADSPA_PortRangeHint *) psDescriptor->PortRangeHints);
		free(psDescriptor);
	}
}

/*****************************************************************************/

/* _fini() is called automatically when the library is unloaded. */
void _fini() {
	deleteDescriptor(g_psMonoDescriptor);
	deleteDescriptor(g_psStereoDescriptor);
}

/*****************************************************************************/

/* Return a descriptor of the requested plugin type. There are two
 plugin types available in this library (mono and stereo). */
const LADSPA_Descriptor *
ladspa_descriptor(unsigned long Index) {
	/* Return the requested descriptor or null if the index is out of
	 range. */
	switch (Index) {
	case 0:
		return g_psMonoDescriptor;
	case 1:
		return g_psStereoDescriptor;
	default:
		return NULL;
	}
}

/*****************************************************************************/

/* EOF */
