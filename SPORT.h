#ifndef _SPORT_H
#define _SPORT_H

#include "vorbis.h"

void Init1835viaSPI();
void init_DAI();
void initSPORT(vorbis_decode *vd);
void ISR();

#endif

