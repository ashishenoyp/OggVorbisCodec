#include <stdio.h>
/* Register Definitions */
#define PPCTL       0x1800
#define EIPP        0x1810
#define EMPP        0x1811
#define ECPP        0x1812
#define IIPP        0x1818
#define IMPP        0x1819
#define ICPP        0x181a
#define RXPP        0x1809

/* Register Bit Definitions */
#define PPEN    0x00000001
#define PPDUR4  0x00000008
#define PPBHC   0x00000040
#define PPDEN   0x00000100
#define PPTRAN  0x00000000
#define PPBS    0x00020000
#define PPDS    0x00010000
#define PPDUR2  0x00000004
#define PPDUR3  0x00000006
#define PPDUR6  0x00000014
#define PP16    0x00000080
#define PPFLMD  0x00004000
#define PPALEPL 0x00002000
#define PPDUR32 0x0000003E

/* External Memory Location */
#define EXTBUFF 0x1000000

extern int ext_addr_incr;
int first_time=0;

void PP_read(int *dest_addr, int dest_size)
{
int i;
/* Turn off the Parallel Port */
* (volatile int *)PPCTL = 0;

/* initiate parallel port DMA registers*/
* (volatile int *)IIPP = (int)dest_addr;
* (volatile int *)IMPP = 1;
* (volatile int *)ICPP = dest_size;

* (volatile int *)EMPP = 1;
* (volatile int *)EIPP = EXTBUFF+(ext_addr_incr);
/* For 8-bit external memory, the External count is
    four times the internal count */
* (volatile int *)ECPP = dest_size * 4;

* (volatile int *)PPCTL =  PPBHC|PPDUR6;  /* implement a bus hold cycle */
            									/* make pp data cycles last for a */
                    							/* duration of 4 cclk cycles */
                    							/* enable flash mode for slower access */

/* initiate PP DMA*/
/*Enable Parallel Port and PP DMA in same cycle*/
* (volatile int *)PPCTL |= PPDEN|PPEN;

//*(dest_addr) = * (volatile int *)RXPP;
/*poll to ensure parallel port has completed the transfer*/
//delay(1000000);

//while( (* (volatile int *)PPCTL & (PPBS|PPDS)) != 0){} 


}

