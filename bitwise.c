/********************************************************************
 *                                                                  *
 * THIS FILE IS PART OF THE OggVorbis SOFTWARE CODEC SOURCE CODE.   *
 * USE, DISTRIBUTION AND REPRODUCTION OF THIS LIBRARY SOURCE IS     *
 * GOVERNED BY A BSD-STYLE SOURCE LICENSE INCLUDED WITH THIS SOURCE *
 * IN 'COPYING'. PLEASE READ THESE TERMS BEFORE DISTRIBUTING.       *
 *                                                                  *
 * THE OggVorbis SOURCE CODE IS (C) COPYRIGHT 1994-2002             *
 * by the Xiph.Org Foundation http://www.xiph.org/                  *
 *                                                                  *
 ********************************************************************

  function: packing variable sized words into an octet stream
  last mod: $Id: bitwise.c 7675 2004-09-01 00:34:39Z xiphmont $

 ********************************************************************/

/* We're 'LSb' endian; if we write a word but read individual bits,
   then we'll read the lsb first */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "ogg.h"
#include "byte_op.h"

#define BUFFER_INCREMENT 256

static const unsigned long mask[]=
{0x00000000,0x00000001,0x00000003,0x00000007,0x0000000f,
 0x0000001f,0x0000003f,0x0000007f,0x000000ff,0x000001ff,
 0x000003ff,0x000007ff,0x00000fff,0x00001fff,0x00003fff,
 0x00007fff,0x0000ffff,0x0001ffff,0x0003ffff,0x0007ffff,
 0x000fffff,0x001fffff,0x003fffff,0x007fffff,0x00ffffff,
 0x01ffffff,0x03ffffff,0x07ffffff,0x0fffffff,0x1fffffff,
 0x3fffffff,0x7fffffff,0xffffffff };

static const unsigned int mask8B[]=
{0x00,0x80,0xc0,0xe0,0xf0,0xf8,0xfc,0xfe,0xff};


void oggpack_readinit(oggpack_buffer *b,DATA_PTR buf,int bytes)
{
  memset(b,0,sizeof(*b));
  b->buffer=b->ptr=buf;
  //b->endbit=8*buf.mod;
  b->storage=bytes;
}

long oggpack_look(oggpack_buffer *b,int bits){
  unsigned long ret;
  unsigned long m=mask[bits];

 // printf("Entering oggpack_look\n");

  bits+=b->endbit;

  if(b->endbyte+4>=b->storage){
    /* not the main path */
    if(b->endbyte*8+bits>b->storage*8)return(-1);
  }
  
  //ret=b->ptr[0]>>b->endbit;
  ret = (incr_ptr_getbyte(b->ptr, 0) &0x000000ff) >> b->endbit;
  if(bits>8){
    ret|=(incr_ptr_getbyte(b->ptr, 1) &0x000000ff)<<(8-b->endbit);  
    if(bits>16){
      ret|=(incr_ptr_getbyte(b->ptr, 2) &0x000000ff)<<(16-b->endbit);  
      if(bits>24){
	ret|=(incr_ptr_getbyte(b->ptr, 3) &0x000000ff)<<(24-b->endbit);  
	if(bits>32 && b->endbit)
	  ret|=(incr_ptr_getbyte(b->ptr, 4) &0x000000ff)<<(32-b->endbit);
      }
    }
  }
  return(m&ret);
}



void oggpack_adv(oggpack_buffer *b,int bits){
	//printf("Entering oggpack_adv\n");
  bits+=b->endbit;
 // b->ptr+=bits/8;
  incr_ptr(&b->ptr, bits/8);
  b->endbyte+=bits/8;
  b->endbit=bits&7;
}


/* bits <= 32 */
long oggpack_read(oggpack_buffer *b,int bits)
{
  long ret;
  unsigned long m=mask[bits];

  bits+=b->endbit;

  if(b->endbyte+4>=b->storage)
  {
    /* not the main path */
    ret=-1L;
    if(b->endbyte*8+bits>b->storage*8)goto overflow;
  }
  
  //ret=b->ptr[0]>>b->endbit;
  ret=(incr_ptr_getbyte(b->ptr,0) & 0x000000FF)>>b->endbit;

  if(bits>8)
  {
    //ret|=b->ptr[1]<<(8-b->endbit);
	  ret|=(incr_ptr_getbyte(b->ptr,1) & 0x000000FF)<<(8-b->endbit);
    if(bits>16)
	{
      ret|=(incr_ptr_getbyte(b->ptr,2) & 0x000000FF)<<(16-b->endbit);  
      if(bits>24)
	  {
		ret|=(incr_ptr_getbyte(b->ptr,3) & 0x000000FF)<<(24-b->endbit);  
		if(bits>32 && b->endbit)
		{
			ret|=(incr_ptr_getbyte(b->ptr,4) & 0x000000FF)<<(32-b->endbit);
		}
      }
    }
  }

  ret&=m;
  
 overflow:

  //b->ptr+=bits/8;
  incr_ptr(&b->ptr,bits/8);
  b->endbyte+=bits/8;
  b->endbit=bits&7;
  return(ret);
}


#undef BUFFER_INCREMENT
