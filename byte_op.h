
#ifndef _BYTE_OP_H
#define _BYTE_OP_H

#include<stdio.h>
#include<stdlib.h>

#include "ogg.h"

extern char getbyte(DATA_PTR *);

extern void putbyte(DATA_PTR *, char );

extern void incr_ptr(DATA_PTR *, int );

extern void incr_ptr_putbyte(DATA_PTR , int , char );

extern char incr_ptr_getbyte(DATA_PTR , int );

extern DATA_PTR incr_ptr_nochange(DATA_PTR , int);
#endif
