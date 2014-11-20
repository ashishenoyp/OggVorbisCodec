
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <def21364.h>
#include <cdef21364.h>

void InitPLL()
{
/********************************************************************************************/
//24.576 MHz CLKIN
//(24.576MHz * 6) /2 = 73.728 MHz
	int i, pmctlsetting;
	pmctlsetting = *pPMCTL;   // Read from PMCTL
	pmctlsetting &= ~(0xff);  //Clear old divisor and multiplier setting.
	pmctlsetting |= PLLM6 | PLLD2 | DIVEN ; // Apply multiplier and divider
	*pPMCTL = pmctlsetting;  // Write into PMCTL
	pmctlsetting ^= DIVEN;   // Enable the clock division
	pmctlsetting |= PLLBP ;  // Put PLL in bypass mode until it locks
	*pPMCTL = pmctlsetting;
	for (i=0;i<4096;i++)     // delay loop for PLL to lock
          asm("nop;");
	pmctlsetting = *pPMCTL;
	pmctlsetting ^= PLLBP;   //Clear Bypass Mode
	*pPMCTL = pmctlsetting;
}
