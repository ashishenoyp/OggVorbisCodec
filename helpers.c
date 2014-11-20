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

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
//#include <conio.h>
#include "ogg.h"
#include "vorbis.h"
#define SWAP(x,y) { temp = x; x = y; y = temp; }
#define ogg_uint32_t int
#define _ogg_free free
#define _ogg_malloc malloc

int depth=0;
int flag=0;
int p;
//int nodecnt=0;
int trav_depth=0;

int *huffman_codes(int *lengths,int used);

int ilog(unsigned int v){
  int ret=0;
  while(v){
    ret++;
    v>>=1;
  }
  return(ret);
}

int ilog2(unsigned int v){
  int ret=0;
  if(v)--v;
  while(v){
    ret++;
    v>>=1;
  }
  return(ret);
}

//long _book_maptype1_quantvals(const static_codebook *b)
long lookup1_values(int codebook_entries, int codebook_dimensions)
{
  long vals=floor(pow((float)codebook_entries,1.f/codebook_dimensions));

  /* the above *should* be reliable, but we'll not assume that FP is
     ever reliable when bitstream sync is at stake; verify via integer
     means that vals really is the greatest value of dimensions for which
     vals^codebook_bim <= codebook_entries */
  /* treat the above as an initial guess */
  while(1){
    long acc=1;
    long acc1=1;
    int i;
    for(i=0;i<codebook_dimensions;i++){
      acc*=vals;
      acc1*=vals+1;
    }
    if(acc<=codebook_entries && acc1>codebook_entries){
      return(vals);
    }else{
      if(acc>codebook_entries){
	vals--;
      }else{
	vals++;
      }
    }
  }
}

float float32_unpack(unsigned int val){
  double mant=val&0x1fffff;
  int    sign=val&0x80000000;
  long   exp =(val&0x7fe00000L)>>21;
  if(sign)mant= -mant;
  return(ldexp(mant,exp-788));
}

/*
void mask(unsigned int *used_entries, int entry_no)
{
	

	unsigned int *temp, mod;
	unsigned int masker;

	temp=used_entries+entry_no/32;
	mod = entry_no%32;
	masker=1<<mod;
	*temp |= masker;
} */

/*
int *huffman_codes(int *lengths,int used)
{	
	int i,flag,j;
	int *huffman=malloc(sizeof(int)*used);
	
	memset(huffman,0,sizeof(int)*used);

	for(i=0;i<used;i++)
	{
		flag=1;
		while(flag)
		{
			flag=0;
			for(j=0;j<i;j++)
			{
				if(lengths[i]==lengths[j] && huffman[i]==huffman[j])
				{
					huffman[i]=huffman[j]+1;
					flag=1;
				}

				else if(lengths[i] > lengths[j])
				{
					int temp=huffman[i]>>(lengths[i]-lengths[j]);
					temp=(temp&(~huffman[j])) | ((~temp)&(huffman[j])); // xor operation
					if(temp == 0)
					{
						huffman[i]=(huffman[j]+1) << (lengths[i]-lengths[j]);
						flag=1;
					}
				}
			
				else if(lengths[i] < lengths[j])
				{
					int temp=huffman[j]>>(lengths[j]-lengths[i]);
					temp=(temp&(~huffman[i])) | ((~temp)&(huffman[i])); // xor operation
					if(temp == 0)
					{
						huffman[i]=(huffman[j]>>(lengths[j]-lengths[i]))+1;
						flag=1;
					}
				}

			}
		}
	}

	return (huffman);
} */

/*
int read_scalar_context(vorbis_setup *vs,oggpack_buffer *opb,int codebook_no)
{
	codebook *point=vs->vorbis_codebook_configs[codebook_no];
	int *codes=point->huffman;
	int temp=0, used=point->used_entries_count;
	int len=0, cnt, entry_no, i, j;
	
	
	while(1)
	{
		temp=(temp<<1)|oggpack_read(opb,1);len++;

		for(i=0;i<used;i++)
		{
			if(len==point->codebook_lengths[i])
				if(temp==codes[i])
				{
					temp=-1;
					goto brake;
				}
		}

	brake:
		if(temp==-1)
		{
			temp=i;
			break;
		}
		if(len>32)
		{
			printf("\n No matching codeword found in bitstream .. exiting");
			exit(1);
		}
	}

		
	if(point->used_entries==NULL)
		entry_no=temp;
	else
		entry_no=point->used_entries[temp];
	
	return entry_no;

} */

void read_vector_context(vorbis_setup *vs,oggpack_buffer *opb,int codebook_no, float *value)
{
	int lookup_offset,index_divisor,j,i,multiplicand_offset;
	codebook * pointer=vs->vorbis_codebook_configs[codebook_no];
	float delta_value=float32_unpack(pointer->codebook_delta_value);
	float min_value=float32_unpack(pointer->codebook_min_value), last;

	lookup_offset=read_scalar_context(vs,opb,codebook_no);
	

	/* assumed that 'value' vector has been malloced in caller function */
	switch(pointer->codebook_lookup_type)
	{
		case 0:
			printf("\nInvalid request... no vector lookup in codebook %d.." 
				"... exiting",codebook_no); 
			exit(1);

		case 1:
		
			last=0;
			index_divisor=1;

			for(i=0;i<pointer->codebook_dimensions;i++)
			{
				multiplicand_offset = (lookup_offset/index_divisor)%pointer->codebook_lookup_values;
				value[i]=(float)pointer->codebook_multiplicands[multiplicand_offset]*delta_value+min_value+last;

				if(pointer->codebook_sequence_p)
					last=value[i];
				index_divisor*=pointer->codebook_lookup_values;
			}
			break;
			
			
		case 2:
			
			last=0;
			multiplicand_offset=lookup_offset*pointer->codebook_dimensions;

			for(i=0;i<pointer->codebook_dimensions;i++)
				value[i]=(float)pointer->codebook_multiplicands[multiplicand_offset]*delta_value+min_value+last;

			if(pointer->codebook_sequence_p)
				last=value[i];
				
			multiplicand_offset++;
			
			break;
			
		default:
			printf("\n Codebook lookup type is greater than 2");
			exit(1);
	} 
}

float FLOOR1_fromdB_LOOKUP[256]={
  1.0649863e-07F, 1.1341951e-07F, 1.2079015e-07F, 1.2863978e-07F, 
  1.3699951e-07F, 1.4590251e-07F, 1.5538408e-07F, 1.6548181e-07F, 
  1.7623575e-07F, 1.8768855e-07F, 1.9988561e-07F, 2.128753e-07F, 
  2.2670913e-07F, 2.4144197e-07F, 2.5713223e-07F, 2.7384213e-07F, 
  2.9163793e-07F, 3.1059021e-07F, 3.3077411e-07F, 3.5226968e-07F, 
  3.7516214e-07F, 3.9954229e-07F, 4.2550680e-07F, 4.5315863e-07F, 
  4.8260743e-07F, 5.1396998e-07F, 5.4737065e-07F, 5.8294187e-07F, 
  6.2082472e-07F, 6.6116941e-07F, 7.0413592e-07F, 7.4989464e-07F, 
  7.9862701e-07F, 8.5052630e-07F, 9.0579828e-07F, 9.6466216e-07F, 
  1.0273513e-06F, 1.0941144e-06F, 1.1652161e-06F, 1.2409384e-06F, 
  1.3215816e-06F, 1.4074654e-06F, 1.4989305e-06F, 1.5963394e-06F, 
  1.7000785e-06F, 1.8105592e-06F, 1.9282195e-06F, 2.0535261e-06F, 
  2.1869758e-06F, 2.3290978e-06F, 2.4804557e-06F, 2.6416497e-06F, 
  2.8133190e-06F, 2.9961443e-06F, 3.1908506e-06F, 3.3982101e-06F, 
  3.6190449e-06F, 3.8542308e-06F, 4.1047004e-06F, 4.3714470e-06F, 
  4.6555282e-06F, 4.9580707e-06F, 5.2802740e-06F, 5.6234160e-06F, 
  5.9888572e-06F, 6.3780469e-06F, 6.7925283e-06F, 7.2339451e-06F, 
  7.7040476e-06F, 8.2047000e-06F, 8.7378876e-06F, 9.3057248e-06F, 
  9.9104632e-06F, 1.0554501e-05F, 1.1240392e-05F, 1.1970856e-05F, 
  1.2748789e-05F, 1.3577278e-05F, 1.4459606e-05F, 1.5399272e-05F, 
  1.6400004e-05F, 1.7465768e-05F, 1.8600792e-05F, 1.9809576e-05F, 
  2.1096914e-05F, 2.2467911e-05F, 2.3928002e-05F, 2.5482978e-05F, 
  2.7139006e-05F, 2.8902651e-05F, 3.0780908e-05F, 3.2781225e-05F, 
  3.4911534e-05F, 3.7180282e-05F, 3.9596466e-05F, 4.2169667e-05F, 
  4.4910090e-05F, 4.7828601e-05F, 5.0936773e-05F, 5.4246931e-05F, 
  5.7772202e-05F, 6.1526565e-05F, 6.5524908e-05F, 6.9783085e-05F, 
  7.4317983e-05F, 7.9147585e-05F, 8.4291040e-05F, 8.9768747e-05F, 
  9.5602426e-05F, 0.00010181521F, 0.00010843174F, 0.00011547824F, 
  0.00012298267F, 0.00013097477F, 0.00013948625F, 0.00014855085F, 
  0.00015820453F, 0.00016848555F, 0.00017943469F, 0.00019109536F, 
  0.00020351382F, 0.00021673929F, 0.00023082423F, 0.00024582449F, 
  0.00026179955F, 0.00027881276F, 0.00029693158F, 0.00031622787F, 
  0.00033677814F, 0.00035866388F, 0.00038197188F, 0.00040679456F, 
  0.00043323036F, 0.00046138411F, 0.00049136745F, 0.00052329927F, 
  0.00055730621F, 0.00059352311F, 0.00063209358F, 0.00067317058F, 
  0.00071691700F, 0.00076350630F, 0.00081312324F, 0.00086596457F, 
  0.00092223983F, 0.00098217216F, 0.0010459992F, 0.0011139742F, 
  0.0011863665F, 0.0012634633F, 0.0013455702F, 0.0014330129F, 
  0.0015261382F, 0.0016253153F, 0.0017309374F, 0.0018434235F, 
  0.0019632195F, 0.0020908006F, 0.0022266726F, 0.0023713743F, 
  0.0025254795F, 0.0026895994F, 0.0028643847F, 0.0030505286F, 
  0.0032487691F, 0.0034598925F, 0.0036847358F, 0.0039241906F, 
  0.0041792066F, 0.0044507950F, 0.0047400328F, 0.0050480668F, 
  0.0053761186F, 0.0057254891F, 0.0060975636F, 0.0064938176F, 
  0.0069158225F, 0.0073652516F, 0.0078438871F, 0.0083536271F, 
  0.0088964928F, 0.009474637F, 0.010090352F, 0.010746080F, 
  0.011444421F, 0.012188144F, 0.012980198F, 0.013823725F, 
  0.014722068F, 0.015678791F, 0.016697687F, 0.017782797F, 
  0.018938423F, 0.020169149F, 0.021479854F, 0.022875735F, 
  0.024362330F, 0.025945531F, 0.027631618F, 0.029427276F, 
  0.031339626F, 0.033376252F, 0.035545228F, 0.037855157F, 
  0.040315199F, 0.042935108F, 0.045725273F, 0.048696758F, 
  0.051861348F, 0.055231591F, 0.058820850F, 0.062643361F, 
  0.066714279F, 0.071049749F, 0.075666962F, 0.080584227F, 
  0.085821044F, 0.091398179F, 0.097337747F, 0.10366330F, 
  0.11039993F, 0.11757434F, 0.12521498F, 0.13335215F, 
  0.14201813F, 0.15124727F, 0.16107617F, 0.17154380F, 
  0.18269168F, 0.19456402F, 0.20720788F, 0.22067342F, 
  0.23501402F, 0.25028656F, 0.26655159F, 0.28387361F, 
  0.30232132F, 0.32196786F, 0.34289114F, 0.36517414F, 
  0.38890521F, 0.41417847F, 0.44109412F, 0.46975890F, 
  0.50028648F, 0.53279791F, 0.56742212F, 0.60429640F, 
  0.64356699F, 0.68538959F, 0.72993007F, 0.77736504F, 
  0.82788260F, 0.88168307F, 0.9389798F, 1.F, 
};

int render_point(int x0,int x1,int y0,int y1,int x){
  y0&=0x7fff; /* mask off flag */
  y1&=0x7fff;

  {
    int dy=y1-y0;
    int adx=x1-x0;
    int ady=abs(dy);
    int err=ady*(x-x0);
    
    int off=err/adx;
    if(dy<0)return(y0-off);
    return(y0+off);
  }
}

void render_line(int x0,int x1,int y0,int y1,float *d){
  int dy=y1-y0;
  int adx=x1-x0;
  int ady=abs(dy);
  int base=dy/adx;
  int sy=(dy<0?base-1:base+1);
  int x=x0;
  int y=y0;
  int err=0;
 
  ady-=abs(base*adx);

  d[x]*=FLOOR1_fromdB_LOOKUP[y];
  while(++x<x1){
    err=err+ady;
    if(err>=adx){
      err-=adx;
      y+=sy;
    }else{
      y+=base;
    }
    d[x]*=FLOOR1_fromdB_LOOKUP[y];
  }
}

int low_neighbor(int *v,int x)
{
	int n=0,i;
	int max=0;

	for(i=0;i<x;i++)
	{
		if(v[i]>max && v[i]<v[x])
		{
			max=v[i];n=i;
		}
	}

	return n;
}

int high_neighbor(int *v,int x)
{
	int n,i;
	int min=0x0fffffff;

	n=1;

	for(i=0;i<x;i++)
	{
		if(v[i]<min && v[i]>v[x])
		{
			min=v[i];
			n=i;
		}
	}

	return n;
}



void QuickerSort(int *a, int *b, int size, int start, int end) {
	int pivot = start;
	int p = start + 1;
	int q = end;
	int temp = 0;
	
	if(size > 0 && start < end) {
		while(p < q) {
			while(a[p] < a[pivot]) {
				p++;
				if(p > end) break;
			}
			while(a[q] > a[pivot]) {
				q--;
				if(q < start + 1) break;
			}	
			if(p > end || q < start + 1) break;
			
			if(p < q) {
				SWAP(a[p], a[q])
				if(b != a) SWAP(b[p], b[q])
				//if(c != a) SWAP(c[p], c[q])
			}
		}
		p--;
		
		SWAP(a[p], a[pivot])
		if(b != a) SWAP(b[p], b[pivot])
		//if(c != a) SWAP(c[p], c[pivot])

		QuickerSort(a, b, size, start, p - 1);
		QuickerSort(a, b, size, p + 1, end);
	}
}

void Sort(int *a, int *b, int size) {
	QuickerSort(a, b, size, 0, size -1);
}

void vorbis_info_clear(vorbis_info *vi)
{
	memset(vi,0,sizeof(*vi));
}

void vorbis_comment_clear(vorbis_comment *vc)
{
	int i;

	for(i=0;i<vc->comments;i++)
		free(vc->user_comments[i]);
	free(vc->user_comments);
	free(vc->comment_lengths);

	memset(vc,0,sizeof(*vc));
}

void vorbis_codebook_clear(codebook *c)
{
	//free(c->codebook_lengths);
	free(c->codebook_multiplicands);
	free(c->huffman);
	//free(c->used_entries);

	memset(c,0,sizeof(*c));
}

void vorbis_floor_clear(floor1 *f)
{
	int i;

	free(f->floor1_class_dimensions);
	free(f->floor1_class_subclasses);
	free(f->floor1_class_masterbooks);
	free(f->floor1_partition_class);

	for(i=0;i<f->maxclass;i++)
		free(f->floor1_subclass_books[i]);
	free(f->floor1_subclass_books);

	memset(f,0,sizeof(*f));
}
 
void vorbis_residue_clear(residue *r)
{
	int i;

	for(i=0;i<r->residue_classifications;i++)
		free(r->residue_books[i]);
	free(r->residue_books);

	memset(r,0,sizeof(*r));
}

void vorbis_mapping_clear(mapping *m)
{
	free(m->vorbis_mapping_submap_floor);
	free(m->vorbis_mapping_submap_residue);
	free(m->vorbis_mapping_magnitude);
	free(m->vorbis_mapping_angle);

	memset(m,0,sizeof(*m));
}

void vorbis_setup_clear(vorbis_setup *vs)
{
	int i;
	
	for(i=0;i<vs->vorbis_codebook_count;i++)
		vorbis_codebook_clear(vs->vorbis_codebook_configs[i]);
	free(vs->vorbis_codebook_configs);

	for(i=0;i<vs->vorbis_floor_count;i++)
		vorbis_floor_clear(vs->vorbis_floor1_configs[i]);
	free(vs->vorbis_floor1_configs);

	for(i=0;i<vs->vorbis_residue_count;i++)
		vorbis_residue_clear(vs->vorbis_residue_configs[i]);
	free(vs->vorbis_residue_configs);

	for(i=0;i<vs->vorbis_mapping_count;i++)
		vorbis_mapping_clear(vs->vorbis_mapping_configs[i]);
	free(vs->vorbis_mapping_configs);

	for(i=0;i<vs->vorbis_mode_count;i++)
		memset(vs->vorbis_mode_configs[i],0,sizeof(*vs->vorbis_mode_configs));
	free(vs->vorbis_mode_configs);

	memset(vs,0,sizeof(*vs));

}

void vorbis_decode_clear(vorbis_decode *vd, vorbis_info *vi)
{
	int i;

	for(i=0;i<vi->channels;i++)
	{
		free(vd->flor[i]);
		free(vd->res[i]);
		free(vd->right_hand_data[i]);
	}
	free(vd->flor);
	free(vd->res);
	free(vd->right_hand_data);

	free(vd->data);

	mdct_clear(vd->mdct[0]);
	mdct_clear(vd->mdct[1]);
	free(vd->mdct);
}

int *huffman_tree_build(int *lengths, int *entries, int no_of_entries)
{	
	int i,j;
	 
	int *huffman_tree=(int *)malloc(1*sizeof(int));
	int nodecnt=0;
	nodecnt++;
	p=0;
	huffman_tree[0]=0;
	huffman_tree[0] |= LNULL|RNULL;

	for(i=0;i<no_of_entries;i++)
	{
		depth=0;flag=0;trav_depth=0;
		inorder(huffman_tree,0,lengths[i]);

		for(j=depth;j<lengths[i];j++)
		{
			if((huffman_tree[p] & RMASK) == LNULL)
			{
				huffman_tree=(int *)realloc(huffman_tree,sizeof(int)*(nodecnt+1));
				huffman_tree[p] &= LMASK; huffman_tree[p] |= nodecnt<<16;
				p = nodecnt;
				nodecnt++;
			}
			else
			{
				huffman_tree=(int *)realloc(huffman_tree,sizeof(int)*(nodecnt+1));
				huffman_tree[p] &= RMASK;	huffman_tree[p] |= nodecnt;
				p = nodecnt;
				nodecnt++;
			}
			if(j==lengths[i]-1)
			{
				huffman_tree[p] = 0; 
				huffman_tree[p] |= MSB|entries[i];  
			}
			else
			{ 
				huffman_tree[p] = 0;
				huffman_tree[p] |= LNULL|RNULL;
			}
		}
	}

	//traverse(0);
	//free(huffman_tree);
	return(huffman_tree);
}

void inorder(int *huffman_tree, int ptr, int curr_length)
{
	 //non leaf node
	if((huffman_tree[ptr] & MSB) == 0)
	{
		if((huffman_tree[ptr] & RMASK) != LNULL)
		{
			trav_depth++;
			inorder(huffman_tree,((huffman_tree[ptr] & RMASK) >> 16), curr_length);
			if(flag==1){depth++; return;}
		}
		if((huffman_tree[ptr] & LMASK) != RNULL)
		{
			trav_depth++;
			inorder(huffman_tree,(huffman_tree[ptr] & LMASK), curr_length);
			if(flag==1){depth++; return;}
		}
	}
	else
	{
		trav_depth--;
		return; // if leaf node then return
	}

	if(((huffman_tree[ptr] & RMASK) == LNULL) && trav_depth < curr_length)// found an opening for the new codeword
	{ p=ptr; flag=1; trav_depth--;return; }

	if(((huffman_tree[ptr] & LMASK) == RNULL) && trav_depth < curr_length)
	{ p=ptr; flag=1; trav_depth--;return; }
	trav_depth--;
}

int read_scalar_context(vorbis_setup *vs,oggpack_buffer *opb,int codebook_no)
{
	codebook *point=vs->vorbis_codebook_configs[codebook_no];
	int *huff_tree=point->huffman;
	int temp=0,incr=0,pointer=0,entry_no;

	while(1)
	{
		temp = oggpack_read(opb,1);
		
		/* now traverse the tree bit by bit */

		if(temp==0)
		{
			incr = huff_tree[pointer] & RMASK;
			if(incr != LNULL)
				pointer = (incr>>16);
			if(((huff_tree[pointer] & MSB)>>31) == 1)  /* leaf node */
			{
				entry_no = huff_tree[pointer] & MSB_MASK;
				break;
			}
		}
		if(temp==1)
		{
			incr = huff_tree[pointer] & LMASK;
			if(incr != RNULL)
				pointer = (incr);
			if(((huff_tree[pointer] & MSB)>>31) == 1)  /* leaf node */
			{
				entry_no = huff_tree[pointer] & MSB_MASK;
				break;
			}
		}
	}

	return(entry_no);
	
}

