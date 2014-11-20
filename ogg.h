
#ifndef _OGG_H
#define _OGG_H

typedef struct {
	unsigned int *data;
	int mod;
} DATA_PTR;


typedef struct {
DATA_PTR sync_buff;
DATA_PTR stream_buff;

int sync_fill;
int stream_fill;
int sync_returned;
int stream_returned;

int headerbytes;
int bodybytes;

DATA_PTR header;
long header_len;
DATA_PTR body;
long body_len;

int unsynced;

int lacing_vals[2048];
int lacing_fill;
int lacing_packet;
int lacing_returned;

int body_fill;
int body_returned;
int pageno;
int packetno;

} ogg_decode;

typedef struct {
  DATA_PTR packet;
  long  bytes;
  long  b_o_s;
  long  e_o_s;

  //ogg_int64_t  granulepos;
  
  long  packetno;     /* sequence number for decode; the framing
				knows where there's a hole in the data,
				but we need coupling so that the codec
				(which is in a seperate abstraction
				layer) also knows about the gap */
} ogg_packet;

typedef struct {
  long endbyte;
  int  endbit;

  DATA_PTR buffer;
  DATA_PTR ptr;
  long storage;
} oggpack_buffer;


extern int ogg_sync_pageout(ogg_decode *od);
extern int ogg_sync_wrote(ogg_decode *od, long bytes);
extern DATA_PTR ogg_sync_buffer(ogg_decode *od);
extern long ogg_sync_pageseek(ogg_decode *od);
extern int ogg_stream_pagein(ogg_decode *od);
extern int ogg_stream_packetout(ogg_decode *od,ogg_packet *op);
extern void oggpack_readinit(oggpack_buffer *b,DATA_PTR buf,int bytes);
extern long oggpack_read(oggpack_buffer *b,int bits);

#endif
