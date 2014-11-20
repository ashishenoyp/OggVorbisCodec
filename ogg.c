#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "byte_op.h"

DATA_PTR ogg_sync_buffer(ogg_decode *od)
{
	int i;
	if(od->sync_returned)
	{
		od->sync_fill-=od->sync_returned;
		if(od->sync_fill>0)
		{
			//memmove(oy->data,oy->data+oy->returned,oy->fill);
			for(i=0; i < od->sync_fill; i++)
			{
				incr_ptr_putbyte(od->sync_buff, i, incr_ptr_getbyte(od->sync_buff, od->sync_returned + i));
			}
			od->sync_returned=0;
		}
	}

	return(incr_ptr_nochange(od->sync_buff, od->sync_fill));
}

int ogg_sync_wrote(ogg_decode *od, long bytes)
{
  if(od->sync_fill+bytes>8192)return(-1);
  od->sync_fill+=bytes;
  return(0);
}

int ogg_sync_pageout(ogg_decode *od)
{

   for(;;)
   {
    long ret=ogg_sync_pageseek(od);
    if(ret>0)
	{
      /* have a page */
      return(1);
    }
    if(ret==0)
	{
      /* need more data */
      return(0);
    }
    
    /* head did not start a synced page... skipped some bytes */
    if(!od->unsynced)
	{
      od->unsynced=1;
      return(-1);
    }
    /* loop. keep looking */
  }
}

long ogg_sync_pageseek(ogg_decode *od)
{
  DATA_PTR page=incr_ptr_nochange(od->sync_buff,od->sync_returned);
  DATA_PTR next;
  long bytes=od->sync_fill-od->sync_returned,i,retval;
  
  if(od->headerbytes==0)
  {
    int headerbytes,i;
    if(bytes<27)
		return(0); /* not enough for a header */
    
    /* verify capture pattern */
    //if(memcmp(page,"OggS",4))goto sync_fail;
	if(incr_ptr_getbyte(page,0)!='O' ||
	   incr_ptr_getbyte(page,1)!='g' ||
	   incr_ptr_getbyte(page,2)!='g' ||
	   incr_ptr_getbyte(page,3)!='S' ) 
	   goto sync_fail; 
    
    headerbytes=(incr_ptr_getbyte(page,26)&0xFF)+27;
    if(bytes<headerbytes)return(0); /* not enough for header + seg table */
    
    /* count up body length in the segment table */
    
    for(i=0;i<incr_ptr_getbyte(page,26);i++)
      od->bodybytes+=incr_ptr_getbyte(page,27+i)&0xFF;//page[27+i];
    od->headerbytes=headerbytes;
  }
  
  if(od->bodybytes+od->headerbytes>bytes)
	  return(0);
  
  {
    DATA_PTR temp,page=incr_ptr_nochange(od->sync_buff,od->sync_returned);
    long bytes;

    od->header.data=page.data;
	od->header.mod=page.mod;

    od->header_len=od->headerbytes;
      
	temp=incr_ptr_nochange(page,od->headerbytes);
	od->body.data=temp.data;
    od->body.mod=temp.mod;

    od->body_len=od->bodybytes;
    
    od->unsynced=0;
    od->sync_returned+=(bytes=od->headerbytes+od->bodybytes);
    od->headerbytes=0;
    od->bodybytes=0;
    return(bytes);
  }
  
 sync_fail:
  
  od->headerbytes=0;
  od->bodybytes=0;
  
  /* search for possible capture */
  //next=memchr(page+1,'O',bytes-1);

  for(i=1;i<bytes;i++)
  {
	if(incr_ptr_getbyte(page,i)=='O')
	{
		next=incr_ptr_nochange(page,i);
		break;
	}
  }

  if(!next.data)
   // next=oy->data+oy->fill;
	next=incr_ptr_nochange(od->sync_buff,od->sync_fill);

  //oy->returned=next-oy->data;

  od->sync_returned=next.data-od->sync_buff.data;

  if(next.mod<od->sync_buff.mod)
  {
	  od->sync_returned-=4;
	  od->sync_returned+=(next.mod-od->sync_buff.mod)+4;
  }
  else
	  od->sync_returned+=next.mod-od->sync_buff.mod;

  retval=next.data-page.data;

  if(next.mod<page.mod)
  {
	  retval-=4;
	  retval+=(next.mod-page.mod)+4;
  }
  else
	  retval+=next.mod-page.mod;

  return(-(retval));
}

int ogg_stream_pagein(ogg_decode *od)
{
  DATA_PTR header=od->header;
  DATA_PTR body=od->body;
  long     bodysize=od->body_len;
  int      segptr=0;

  int continued=(int)(incr_ptr_getbyte(od->header,4)&0x000000FF);
  int bos=(int)(incr_ptr_getbyte(od->header,5)&0x02);
  int eos=(int)(incr_ptr_getbyte(od->header,5)&0x04);
 
  long pageno=(incr_ptr_getbyte(od->header,18)&0x000000ff) |
	 ((incr_ptr_getbyte(od->header,19)&0x000000ff)<<8) |
	 ((incr_ptr_getbyte(od->header,20)&0x000000ff)<<16) |
	 ((incr_ptr_getbyte(od->header,21)&0x000000ff)<<24);
  int segments=incr_ptr_getbyte(od->header,26)&0x000000FF;


  /* clean up 'returned data' */
  {
    long lr=od->lacing_returned;
    long br=od->body_returned;

    /* body data */
    if(br)
	{
	  int i;
      od->body_fill-=br;
      if(od->body_fill)
		//memmove(os->body_data,os->body_data+br,os->body_fill);
		for(i=0; i < od->body_fill; i++)
			incr_ptr_putbyte(od->stream_buff, i, incr_ptr_getbyte(od->stream_buff, br+i));

      od->body_returned=0;
    }

    if(lr)
	{
      /* segment table */
      if(od->lacing_fill-lr)
	  {
		int *temp = &od->lacing_vals[0];
		memmove(temp,temp+lr,(od->lacing_fill-lr)*sizeof(*temp));
	  }
      od->lacing_fill-=lr;
      od->lacing_packet-=lr;
      od->lacing_returned=0;
    }
  }

  /* are we in sequence? */
  if(pageno!=od->pageno)
  {
    int i;

    /* unroll previous partial packet (if any) */
    for(i=od->lacing_packet;i<od->lacing_fill;i++)
      od->body_fill-=od->lacing_vals[i]&0xff;
    od->lacing_fill=od->lacing_packet;

    /* make a note of dropped data in segment table */
    if(od->pageno!=-1){
      od->lacing_vals[od->lacing_fill++]=0x400;
      od->lacing_packet++;
    }
  }

  /* are we a 'continued packet' page?  If so, we may need to skip
     some segments */
  if(continued)
  {
    if(od->lacing_fill<1 || od->lacing_vals[od->lacing_fill-1]==0x400)
	{
      bos=0;
      for(;segptr<segments;segptr++)
	  {
		int val=incr_ptr_getbyte(header,27+segptr) & 0x000000FF; //header[27+segptr];
		//body+=val;
		incr_ptr(&body, val);
		bodysize-=val;
		if(val<255)
		{
			segptr++;
			break;
		}
      }
    }
  }
  
  if(bodysize)
  {
	  int i;
	//memcpy(os->body_data+os->body_fill,body,bodysize);
	  for(i=0;i<bodysize;i++)
		//*(os->body_data+os->body_fill+i)=incr_ptr_getbyte(body,i);
		incr_ptr_putbyte(od->stream_buff, od->body_fill+i, incr_ptr_getbyte(body,i));
		
	  od->body_fill+=bodysize;
  }

  {
    int saved=-1;
    while(segptr<segments)
	{
      int val=incr_ptr_getbyte(header,27+segptr) & 0x000000FF;
      od->lacing_vals[od->lacing_fill]=val;
      
      if(bos)
	  {
			od->lacing_vals[od->lacing_fill]|=0x100;
			bos=0;
      }
      
      if(val<255)saved=od->lacing_fill;
      
      od->lacing_fill++;
      segptr++;
      
      if(val<255)
		  od->lacing_packet=od->lacing_fill;
    }
  
  
  }

  if(eos)
  {    
    if(od->lacing_fill>0)
      od->lacing_vals[od->lacing_fill-1]|=0x200;
  }

  od->pageno=pageno+1;

  return(0);
}

static int _packetout(ogg_decode *od,ogg_packet *op,int adv){

  /* The last part of decode. We have the stream broken into packet
     segments.  Now we need to group them into packets (or return the
     out of sync markers) */

  int ptr=od->lacing_returned;

  if(od->lacing_packet<=ptr)return(0);

  if(od->lacing_vals[ptr]&0x400)
  {
    /* we need to tell the codec there's a gap; it might need to
       handle previous packet dependencies. */
    od->lacing_returned++;
    od->packetno++;
    return(-1);
  }

  if(!op && !adv)return(1); /* just using peek as an inexpensive way
                               to ask if there's a whole packet
                               waiting */

  /* Gather the whole packet. We'll have no holes or a partial packet */
  {
    int size=od->lacing_vals[ptr]&0xff;
    int bytes=size;
    int eos=od->lacing_vals[ptr]&0x200; /* last packet of the stream? */
    int bos=od->lacing_vals[ptr]&0x100; /* first packet of the stream? */

    while(size==255){
      int val=od->lacing_vals[++ptr];
      size=val&0xff;
      if(val&0x200)eos=0x200;
      bytes+=size;
    }

    if(op)
	{
      op->e_o_s=eos;
      op->b_o_s=bos;
      //op->packet=os->body_data+os->body_returned;
	  op->packet=incr_ptr_nochange(od->stream_buff,od->body_returned);
      op->packetno=od->packetno;
      //op->granulepos=os->granule_vals[ptr];
      op->bytes=bytes;
    }

    if(adv){
      od->body_returned+=bytes;
      od->lacing_returned=ptr+1;
      od->packetno++;
    }
  }
  return(1);
}

int ogg_stream_packetout(ogg_decode *od,ogg_packet *op)
{
  return _packetout(od,op,1);
}
