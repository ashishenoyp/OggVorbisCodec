#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ogg.h"
#include "byte_op.h"
#include "vorbis.h"
#include "SPORT.h"
#include <signal.h>
#include <def21364.h>
#include <math.h>

int sync[2048];
int stream[2048];
int ext_addr_incr=0;
unsigned int temp[2048];

void main()
{
//	InitPLL(); // set the core clock frequency to required value, default is 147 MHz
	
	ogg_decode OD;
	ogg_decode *od=&OD;

	ogg_packet OP;
	ogg_packet *op=&OP;

	vorbis_info VI;
	vorbis_info *vi=&VI;

	vorbis_comment VC;
	vorbis_comment *vc=&VC;

	vorbis_setup VS;
	vorbis_setup *vs = &VS;

	vorbis_decode VD;
	vorbis_decode *vd=&VD;

	oggpack_buffer OPB;
	oggpack_buffer *opb=&OPB;

	int bytes, i, j, eos,mod,loop_count,cnt=0;
	unsigned int *sync_ptr;
	DATA_PTR buffer;
	
	sync_ptr=temp;
	PP_read(sync_ptr,2048);
	
	memset(od,0,sizeof(*od));
	memset(op,0,sizeof(*op));
	memset(vi,0,sizeof(*vi));
	memset(vs,0,sizeof(*vs));
	memset(vc,0,sizeof(*vc));
	memset(vd,0,sizeof(*vd));

	i=heap_install((void *)0xe0010,6000,1,-1);
	vd->obuf1=heap_malloc(i,2048);
	vd->obuf2=heap_malloc(i,2048);
	vd->buf[0]=vd->obuf1;
	vd->buf[1]=vd->obuf2;
	memset(vd->obuf1,0,2048);
	memset(vd->obuf2,0,2048);
		
	od->sync_buff.data = (unsigned int *)&sync[0];
	od->stream_buff.data = (unsigned int *)&stream[0];
	
	buffer = ogg_sync_buffer(od);

	bytes=0;	
	
	for(i=0;i<2048;i++)
	{		
		incr_ptr_putbyte(buffer,4*i+3,(unsigned int)(sync_ptr[i] & 0xff000000) >> 24);
		incr_ptr_putbyte(buffer,4*i+2,(sync_ptr[i] & 0x00ff0000) >> 16);
		incr_ptr_putbyte(buffer,4*i+1,(sync_ptr[i] & 0x0000ff00) >> 8);
		incr_ptr_putbyte(buffer,4*i  ,(sync_ptr[i] & 0x000000ff));			
		bytes+=4;
		ext_addr_incr+=4;
	}

	ogg_sync_wrote(od,bytes);

	if(ogg_sync_pageout(od)!=1)
	{
		/* error case.  Must not be Vorbis data */
		fprintf(stderr,"Input does not appear to be an Ogg bitstream.\n");
		exit(1);
	}
	
	if(ogg_stream_pagein(od)<0)
	{ 
			/* error; stream version mismatch perhaps */
		fprintf(stderr,"Error reading first page of Ogg bitstream data.\n");
		exit(1);
	}

	if(ogg_stream_packetout(od,op)!=1)
	{ 
		/* no page? must not be vorbis */
		fprintf(stderr,"Error reading initial header packet.\n");
		exit(1);
	}

	if(vorbis_synthesis_headerin(vi, vs, vc, op, opb)<0)
	{ 
		/* error case; not a vorbis header */
		fprintf(stderr,"This Ogg bitstream does not contain Vorbis "
		  "audio data.\n");
		exit(1);
	}

	
	i=0;
	while(i<2)
	{
		while(i<2)
		{
			int result=ogg_sync_pageout(od);
			if(result==0)
				break; /* Need more data */
				/* Don't complain about missing or corrupt data yet.  We'll
				   catch it at the packet output phase */
			if(result==1)
			{
				ogg_stream_pagein(od); /* we can ignore any errors here
								 as they'll also become apparent
								 at packetout */
				while(i<2)
				{
					result=ogg_stream_packetout(od,op);
					if(result==0)
						break;
					if(result<0)
					{
							/* Uh oh; data at some point was corrupted or missing!
							We can't tolerate that in a header.  Die. */
						fprintf(stderr,"Corrupt secondary header.  Exiting.\n");
						exit(1);
					}
					vorbis_synthesis_headerin(vi, vs, vc, op, opb);
					i++;
				}
			}
		}
		
		/* no harm in not checking before adding more */
		sync_ptr=temp;	
		PP_read(sync_ptr,2048);
		
		buffer=ogg_sync_buffer(od);
		bytes=0;
		
		mod = od->sync_fill%4;
		if(mod==0)
			loop_count = (2048-(od->sync_fill/4));
		else
			loop_count = (2048-(od->sync_fill/4))-1;	
		
		
		for(i=0;i<loop_count;i++)
		{
			incr_ptr_putbyte(buffer,4*i+3,(unsigned int)(sync_ptr[i] & 0xff000000) >> 24);
			incr_ptr_putbyte(buffer,4*i+2,(sync_ptr[i] & 0x00ff0000) >> 16);
			incr_ptr_putbyte(buffer,4*i+1,(sync_ptr[i] & 0x0000ff00) >> 8);
			incr_ptr_putbyte(buffer,4*i  ,(sync_ptr[i] & 0x000000ff));				
			bytes+=4;
			ext_addr_incr+=4;
		}

		ogg_sync_wrote(od,bytes);

		if(bytes==0 && i<2)
		{
			fprintf(stderr,"End of file before finding all Vorbis headers!\n");
			exit(1);
		}
		
	}

	/* decode init : allocate memory for audio decode - flor, res etc
		also setup twiddle factors for imdct */
	vorbis_decode_init(vi,vd);
	printf("\n Playing.........\n");
	
	init_DAI();
	Init1835viaSPI();
	initSPORT(vd);
	interrupt(SIG_P3,ISR);
	
	
	/* The rest is just a straight decode loop until end of stream */

	eos = 0;
	while(!eos)
	{
		while(!eos)
		{
			int result=ogg_sync_pageout(od);
			if(result==0)
				break; /* need more data */
			if(result<0)
			{ /* missing or corrupt data at this page position */
				fprintf(stderr,"Corrupt or missing data in bitstream; "
				"continuing...\n");
			}
			else
			{
				ogg_stream_pagein(od); /* can safely ignore errors at this point */
				
				while(1)
				{
					result=ogg_stream_packetout(od, op);

					if(result==0)
						break; /* need more data */
					if(result<0)
					{	/* missing or corrupt data at this page position */
							/* no reason to complain; already complained above */
					}
					else
					{
						oggpack_readinit(opb, op->packet, op->bytes);
						
						/* we have a packet.  Decode it */	
						if(audio_decode(vs,vi,opb,vd))
							write_buffer(vd);			
					}
				}
				
				if(incr_ptr_getbyte(od->header,5)&0x04)
					eos=1;
			}
		}
		
		if(!eos)
		{
			int j;
			
			sync_ptr=temp;	
			PP_read(sync_ptr,2048);
			
			buffer=ogg_sync_buffer(od);
			bytes=0;
			
			mod = od->sync_fill%4;
			if(mod==0)
				loop_count = (2048-(od->sync_fill/4));
			else
				loop_count = (2048-(od->sync_fill/4))-1;				
			
			
			for(i=0;i<loop_count;i++)
			{		
				incr_ptr_putbyte(buffer,4*i+3,(unsigned int)(sync_ptr[i] & 0xff000000) >> 24);
				incr_ptr_putbyte(buffer,4*i+2,(sync_ptr[i] & 0x00ff0000) >> 16);
				incr_ptr_putbyte(buffer,4*i+1,(sync_ptr[i] & 0x0000ff00) >> 8);
				incr_ptr_putbyte(buffer,4*i  ,(sync_ptr[i] & 0x000000ff));					
				bytes+=4;
				ext_addr_incr+=4;
			}
			
			ogg_sync_wrote(od,bytes);
		
			if(bytes==0)
				eos=1;
				
		}
	}
	
	SPORT_off();
	printf("\n Done");
	
	vorbis_decode_clear(vd,vi);
	vorbis_info_clear(vi);
	vorbis_comment_clear(vc);
	vorbis_setup_clear(vs);

}


