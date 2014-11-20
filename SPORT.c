#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <sru.h>
#include "ad1835.h"
#include <def21364.h>
#include <cdef21364.h>
#include <signal.h>
#include "vorbis.h"
#include "ogg.h"

#define OFFSET 0x00080000
#define PCI 0x00080000

int reading[2], buf_no=1, ind=0, iflag=0;
unsigned int TCB_1[4] = { 0, 2048, 1, 0 };
unsigned int TCB_2[4] = { 0, 2048, 1, 0 };

void write_buffer(vorbis_decode *vd)
{
	int i=0;

	while(i<vd->write)
	{		
		if(ind<2048)
			vd->buf[buf_no][ind]=vd->data[i];
		else
		{
			buf_no=buf_no^1;
			while(reading[buf_no]==1){asm("nop;");}
			ind=0;
			vd->buf[buf_no][ind]=vd->data[i];
		}
		i++;ind++;
	}	
}

void init_DAI()
{
	
    SRU(LOW,PBEN06_I); // DAI pin 6 takes signal from outside the dsp
    SRU(LOW,PBEN07_I);
    SRU(LOW,PBEN08_I);
    SRU(HIGH,PBEN09_I);
    SRU(HIGH,PBEN13_I);
    SRU(HIGH,PBEN14_I);

	SRU(SPORT1_DB_O,DAI_PB09_I); // connect sport data out to dac4 data input
    SRU(DAI_PB07_O,DAI_PB13_I); // connect adc clk to dac clk
    SRU(DAI_PB08_O,DAI_PB14_I); // connect adc fs to dac fs
 
    SRU(DAI_PB07_O,SPORT1_CLK_I); // connect adc clk to sport clk
    SRU(DAI_PB08_O,SPORT1_FS_I); // connect acd fs to sport fs

}


void initSPORT(vorbis_decode *vd)
{
	reading[0]=1;reading[1]=0;
	
	TCB_1[0] = (unsigned int)TCB_2 + 3 - OFFSET + PCI;
	TCB_1[3] = (unsigned int)vd->obuf1 - OFFSET;
	
	TCB_2[0] = (unsigned int)TCB_1 + 3 - OFFSET + PCI;
	TCB_2[3] = (unsigned int)vd->obuf2 - OFFSET;
	
	*pSPCTL1 = 0;
	
	*pSPCTL1 |= SPTRAN | OPMODE | SLEN16 | SPEN_B | BHD | SCHEN_B | SDEN_B | PACK;
	
	*pCPSP1B = (unsigned int) TCB_1 + 3 - OFFSET;
}

void ISR()
{
	reading[0]=reading[0]^1;
	reading[1]=reading[1]^1;
	return;
}

void SPORT_off()
{
	*pSPCTL1 = 0;
}

