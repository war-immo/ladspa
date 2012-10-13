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

/*****************************************************************************/

#include "ladspa.h"

/*****************************************************************************/

/* The port and data numbers for the plugin: */

#define N_PORTS 3
#define N_DATA 1



/*****************************************************************************/

/* The structure used to hold port connection information and state
 */

typedef LADSPA_Data ** KickTrigger;

/*****************************************************************************/

/* Construct a new plugin instance. */
LADSPA_Handle 
instantiateKickTrigger(const LADSPA_Descriptor * Descriptor,
		     unsigned long             SampleRate) {
		 
  KickTrigger instance;
  int i;
  int channels;
  int *pChannels;
  float* data;
  
  channels = 
	Descriptor->PortCount/N_PORTS;

  instance 
    = malloc(sizeof(LADSPA_Data*)*(N_PORTS+N_DATA)*channels+1);
    
  pChannels 
    = malloc(sizeof(int));
  
  *instance 
    = (LADSPA_Data*) pChannels;
  
  *pChannels 
    = channels;
    
  data = malloc(sizeof(float)*channels*N_DATA);
    
  for (i=N_PORTS*channels+1;i<N_PORTS*channels+1+N_DATA*channels;i++) {
	 instance[i] = data;
	 ++data;
  }
    
  return instance;
}

/*****************************************************************************/

/* Connect a port to a data location. */
void 
connectPortToKickTrigger(LADSPA_Handle Instance,
		       unsigned long Port,
		       LADSPA_Data * DataLocation) {

  KickTrigger psKickTrigger;

  psKickTrigger = (KickTrigger)Instance;
  
  psKickTrigger[Port+1] = DataLocation;
}

/*****************************************************************************/

void 
activateKickTrigger(LADSPA_Handle Instance) {
  KickTrigger  psKickTrigger;
  int channel, channelCount, i;
  int *pChannelCount;
  
  psKickTrigger = (KickTrigger)Instance;
  
  pChannelCount = (int*) *psKickTrigger;
  
  channelCount = *pChannelCount;
  
  for (channel=0;channel<channelCount;++channel) {
	  for (i = 0;i<N_DATA;++i) {
	    *(psKickTrigger[channelCount*N_PORTS+1+N_DATA*channel +i])
	      = 0.f;
	  }
  }
}


/*****************************************************************************/

void 
runKickTrigger(LADSPA_Handle Instance,
		   unsigned long SampleCount) {
  LADSPA_Data * pfInput;
  LADSPA_Data * pfOutput;

  KickTrigger  psKickTrigger;
  unsigned long lSampleIndex;
  int channel, channelCount;
  int *pChannelCount;

  psKickTrigger = (KickTrigger)Instance;
  
  pChannelCount = (int*) *psKickTrigger;
  
  channelCount = *pChannelCount;
  
  for (channel=0;channel<channelCount;++channel) {
	  pfInput = psKickTrigger[1+N_PORTS*channel+N_PORTS-2];
	  pfOutput = psKickTrigger[1+N_PORTS*channel+N_PORTS-1];
	  
	  for (lSampleIndex = 0; lSampleIndex < SampleCount; lSampleIndex++) 
		*(pfOutput++) = *(pfInput++);
  }
}

/*****************************************************************************/

/* Throw away a simple delay line. */
void 
cleanupKickTrigger(LADSPA_Handle Instance) {
  KickTrigger kInstance;
  int* pChannels;
  
  kInstance = (KickTrigger) Instance;
  pChannels = (int*) *kInstance;
  
  free(*kInstance);
  free(kInstance[*pChannels*N_PORTS+1]);
  free(kInstance);
}

/*****************************************************************************/

LADSPA_Descriptor * g_psMonoDescriptor = NULL;
LADSPA_Descriptor * g_psStereoDescriptor = NULL;

/*****************************************************************************/

void
fillDescriptor(LADSPA_Descriptor *g_psDescriptor, int channels, int id) {
  char ** pcPortNames;
  LADSPA_PortDescriptor * piPortDescriptors;
  LADSPA_PortRangeHint * psPortRangeHints;
  
  char label[1024];
  char name[1024];
  char portname[1024];
  
  int i,j;
  
  strcpy(label, "kicktrigger_x");
  sprintf(label+strlen(label),"%d",channels);
  
  strcpy(name, "Kick Trigger ");
  sprintf(name+strlen(name),"%d",channels);
  strcat(name, " Channels");
  
  g_psDescriptor->UniqueID
      = id;
  g_psDescriptor->Label
      = strdup(label);
  g_psDescriptor->Properties
      = LADSPA_PROPERTY_HARD_RT_CAPABLE;
  g_psDescriptor->Name 
      = strdup(name);
    g_psDescriptor->Maker
      = strdup("Immanuel Albrecht");
    g_psDescriptor->Copyright
      = strdup("(c) 2012, GPLv3");
      
    g_psDescriptor->PortCount
      = N_PORTS*channels;
      
    piPortDescriptors
      = (LADSPA_PortDescriptor *)calloc(N_PORTS*channels, sizeof(LADSPA_PortDescriptor));
    g_psDescriptor->PortDescriptors
      = (const LADSPA_PortDescriptor *)piPortDescriptors;
      
    for (i=0; i<channels;++i) 
    {
	  for (j=0;j<N_PORTS-2;++j)
	  {
		piPortDescriptors[i*N_PORTS+j]
          = LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL;
	  }
	  
      piPortDescriptors[i*N_PORTS+N_PORTS-2]
         = LADSPA_PORT_INPUT | LADSPA_PORT_AUDIO;
      piPortDescriptors[i*N_PORTS+N_PORTS-1]
         = LADSPA_PORT_OUTPUT | LADSPA_PORT_AUDIO;
	}
      
    
    pcPortNames
      = (char **)calloc(N_PORTS*channels, sizeof(char *));
    g_psDescriptor->PortNames 
      = (const char **)pcPortNames;
      
    for (i=0; i<channels;++i) 
    {
	  for (j=0;j<N_PORTS-2;++j)
	  {
		strcpy(portname,"Control ");
		sprintf(portname+strlen(portname),"%d",j);
        strcat(portname," of ");
        sprintf(portname+strlen(portname),"%d",i);
        
		pcPortNames[i*N_PORTS+j]
          = strdup(portname);
	  }
	  
	  strcpy(portname,"Input ");
	  sprintf(portname+strlen(portname),"%d",i);
	  
	  pcPortNames[i*N_PORTS+N_PORTS-2]
        = strdup(portname);
        
      strcpy(portname,"Output ");
      sprintf(portname+strlen(portname),"%d",i);
	  
      pcPortNames[i*N_PORTS+N_PORTS-1]
        = strdup(portname);
	}
      

    psPortRangeHints = ((LADSPA_PortRangeHint *)
			calloc(N_PORTS*channels, sizeof(LADSPA_PortRangeHint)));
    g_psDescriptor->PortRangeHints
      = (const LADSPA_PortRangeHint *)psPortRangeHints;
      
    for (i=0; i<channels;++i) 
    {
	  for (j=0;j<N_PORTS-2;++j)
	  {
		  psPortRangeHints[i*N_PORTS+j].HintDescriptor
			= (LADSPA_HINT_BOUNDED_BELOW 
				| LADSPA_HINT_LOGARITHMIC
				| LADSPA_HINT_DEFAULT_1);
		  psPortRangeHints[i*N_PORTS+j].LowerBound 
			= 0;
	  }
	  psPortRangeHints[i*N_PORTS+N_PORTS-2].HintDescriptor
        = 0;
      psPortRangeHints[i*N_PORTS+N_PORTS-1].HintDescriptor
        = 0;
    }
      
    
    
    g_psDescriptor->instantiate 
      = instantiateKickTrigger;
    g_psDescriptor->connect_port 
      = connectPortToKickTrigger;
    g_psDescriptor->activate
      = activateKickTrigger;
    g_psDescriptor->run
      = runKickTrigger;
    g_psDescriptor->run_adding
      = NULL;
    g_psDescriptor->set_run_adding_gain
      = NULL;
    g_psDescriptor->deactivate
      = NULL;
    g_psDescriptor->cleanup
      = cleanupKickTrigger;
}

/* _init() is called automatically when the plugin library is first
   loaded. */
void 
_init() {



  g_psMonoDescriptor
    = (LADSPA_Descriptor *)malloc(sizeof(LADSPA_Descriptor));
  g_psStereoDescriptor 
    = (LADSPA_Descriptor *)malloc(sizeof(LADSPA_Descriptor));

  if (g_psMonoDescriptor) {
     fillDescriptor(g_psMonoDescriptor, 1, 1668666); // TODO: Change Unique ID to something else
  }
  
  if (g_psStereoDescriptor) {
	  fillDescriptor(g_psStereoDescriptor, 2, 1668667); // TODO: Change Unique ID to something else
    
  }
}

/*****************************************************************************/

void
deleteDescriptor(LADSPA_Descriptor * psDescriptor) {
  unsigned long lIndex;
  if (psDescriptor) {
    free((char *)psDescriptor->Label);
    free((char *)psDescriptor->Name);
    free((char *)psDescriptor->Maker);
    free((char *)psDescriptor->Copyright);
    free((LADSPA_PortDescriptor *)psDescriptor->PortDescriptors);
    for (lIndex = 0; lIndex < psDescriptor->PortCount; lIndex++)
      free((char *)(psDescriptor->PortNames[lIndex]));
    free((char **)psDescriptor->PortNames);
    free((LADSPA_PortRangeHint *)psDescriptor->PortRangeHints);
    free(psDescriptor);
  }
}

/*****************************************************************************/

/* _fini() is called automatically when the library is unloaded. */
void
_fini() {
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
