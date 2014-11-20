/********************************************************************
 *                                                                  *
 * THIS FILE IS PART OF THE OggVorbis SOFTWARE CODEC SOURCE CODE.   *
 * USE, DISTRIBUTION AND REPRODUCTION OF THIS LIBRARY SOURCE IS     *
 * GOVERNED BY A BSD-STYLE SOURCE LICENSE INCLUDED WITH THIS SOURCE *
 * IN 'COPYING'. PLEASE READ THESE TERMS BEFORE DISTRIBUTING.       *
 *                                                                  *
 * Authors:                                                         *
 *          Ashish Shenoy P                                         *
 *          Sanjay Rajashekar                                       *
 ********************************************************************

 ********************************************************************/
#ifndef _vorbis_h
#define _vorbis_h

#include "ogg.h"
#include "mdct.h"

typedef struct vorbis_info {

  int version;
  int channels;
  long rate;

  long bitrate_upper;
  long bitrate_nominal;
  long bitrate_lower;
  long bitrate_window;

  int blocksize[2];

} vorbis_info;

typedef struct vorbis_comment{
  /* unlimited user comment fields.  libvorbis writes 'libvorbis'
     whatever vendor is set to in encode */
  char **user_comments;
  int   *comment_lengths;
  int    comments;
  char  *vendor;

} vorbis_comment;


typedef struct {

//	int *codebook_lengths;
	int codebook_entries;
//	unsigned int *used_entries;
//	int used_entries_count;
	unsigned int codebook_min_value;
	unsigned int codebook_delta_value;
	int codebook_lookup_type;
	int *codebook_multiplicands;
	int codebook_dimensions;
	int codebook_lookup_values;
	int codebook_sequence_p;
	int *huffman;

} codebook;


typedef struct {

	int *floor1_class_dimensions;
	int *floor1_class_subclasses;
	int *floor1_class_masterbooks;
	int **floor1_subclass_books;
	int *floor1_partition_class;
	int floor1_x_list[100];
	int sorted_index[100];
	int lo_n[100];
	int hi_n[100];
	int floor1_mult;
	int floor1_partitions;
	int floor1_values;
	int maxclass;

} floor1;

typedef struct {

	int residue_begin;
	int residue_end;
	int residue_classifications;
	int residue_classbook;
	int residue_partition_size;
	int **residue_books;
	int vorbis_residue_type;

} residue;

typedef struct {

	int vorbis_mapping_mux[2];
	int *vorbis_mapping_submap_floor;
	int *vorbis_mapping_submap_residue;
	int *vorbis_mapping_magnitude;
	int *vorbis_mapping_angle;
	int vorbis_mapping_submaps;
	int vorbis_mapping_coupling_steps;

} mapping;

typedef struct {

	int mode_blockflag;
	int mode_mapping;

} mode;

typedef struct {

	codebook ** vorbis_codebook_configs;
	floor1 **vorbis_floor1_configs;	
	residue ** vorbis_residue_configs;
	mapping **vorbis_mapping_configs;
	mode **vorbis_mode_configs;

	int vorbis_codebook_count;
	int vorbis_mode_count;
	int vorbis_floor_count;
	int vorbis_residue_count;
	int vorbis_mapping_count;

} vorbis_setup;

typedef struct
{
	float **right_hand_data;
	int **flor;
	float **res;
	int write;

	/* window data */
	int window_center;
	int left_window_start;
	int left_window_end;
	int right_window_start;
	int right_window_end;
	int left_n;
	int right_n;
	int prev;

	/* channel pcm data */
	int *data;

	mdct_lookup **mdct;
	
	int *obuf1;
	int *obuf2;
	int *buf[2];

} vorbis_decode;

#define OV_FALSE      -1
#define OV_EOF        -2
#define OV_HOLE       -3

#define OV_EREAD      -128
#define OV_EFAULT     -129
#define OV_EIMPL      -130
#define OV_EINVAL     -131
#define OV_ENOTVORBIS -132
#define OV_EBADHEADER -133
#define OV_EVERSION   -134
#define OV_ENOTAUDIO  -135
#define OV_EBADPACKET -136
#define OV_EBADLINK   -137
#define OV_ENOSEEK    -138

#define LNULL 0x7FFF0000
#define RNULL 0x00007FFF
#define RMASK 0xFFFF0000
#define LMASK 0x0000FFFF
#define MSB   0x80000000
#define MSB_MASK 0x7FFFFFFF

extern int vorbis_synthesis_headerin(vorbis_info *vi, vorbis_setup *vs, vorbis_comment *vc, ogg_packet *op, oggpack_buffer *opb);
extern int ilog(unsigned int v);
extern int ilog2(unsigned int v);
extern long lookup1_values(int codebook_entries, int codebook_dimensions);
extern void mask(int *used_entries, int entry_no);
extern float float32_unpack(unsigned int val);
extern void render_line(int x0,int x1,int y0,int y1,float *d);
extern int render_point(int x0,int x1,int y0,int y1,int x);
extern int high_neighbor(int *v,int x);
extern int low_neighbor(int *v,int x);
extern void Sort(int *a, int *b, int size);
extern int read_scalar_context(vorbis_setup *vs, oggpack_buffer *opb, int codebook_no);
extern void read_vector_context(vorbis_setup *vs,oggpack_buffer *opb,int codebook_no, float *value);
extern void vorbis_decode_init(vorbis_info *vi, vorbis_decode *vd);
extern int audio_decode(vorbis_setup *vs, vorbis_info *vi, oggpack_buffer *opb, vorbis_decode *vd);

extern void vorbis_info_clear(vorbis_info *vi);
extern void vorbis_comment_clear(vorbis_comment *vc);
extern void vorbis_setup_clear(vorbis_setup *vs);
extern void vorbis_decode_clear(vorbis_decode *vd,vorbis_info *vi);
//extern int *huffman_codes(int *lengths,int used);
extern void inorder(int *, int, int);
extern int *huffman_tree_build(int *lengths, int *entries, int no_of_entries);

void write_buffer(vorbis_decode *vd);
extern void delay(int i);
extern void InitPLL(void);
extern void SPORT_off(void);
extern void PP_read(unsigned int *, int);
extern void init_DAI(void);
extern void Init1835viaSPI(void);
extern void initSPORT(vorbis_decode *);

#endif

