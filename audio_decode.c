#include <stdio.h>
#include <string.h>
#include <math.h>
#include "vorbis.h"
#include "byte_op.h"
#include "mdct.h"

extern float FLOOR1_fromdB_LOOKUP[256];
extern float *vwin[8];
extern float vwin8192[4096];
extern float vwin4096[2048];
extern float vwin2048[1024];
extern float vwin1024[512];
extern float vwin512[256];
extern float vwin256[128];
extern float vwin128[64];
extern float vwin64[32];


void window_setup(vorbis_setup *vs, vorbis_info *vi, oggpack_buffer *opb, int mode_number, vorbis_decode *vd, int n)
{
	int prev_win_flag, next_win_flag;

	if(vs->vorbis_mode_configs[mode_number]->mode_blockflag == 1)
	{
		prev_win_flag=oggpack_read(opb,1);
		next_win_flag=oggpack_read(opb,1);

		vd->window_center = n/2;
		
		if(prev_win_flag == 0)
		{
			vd->left_window_start=(n/4)-(vi->blocksize[0]/4);
			vd->left_window_end=(n/4)+(vi->blocksize[0]/4);
			vd->left_n=vi->blocksize[0]/2;
		}
		else
		{
			vd->left_window_start=0;
			vd->left_window_end=vd->window_center;
			vd->left_n=n/2;
		}

		if(next_win_flag == 0)
		{
			vd->right_window_start=(3*n/4)-(vi->blocksize[0]/4);
			vd->right_window_end=(3*n/4)+(vi->blocksize[0]/4);
			vd->right_n=vi->blocksize[0]/2;
		}
		else
		{
			vd->right_window_start=vd->window_center;
			vd->right_window_end=n;
			vd->right_n=n/2;	
		}
	}
	else
	{
		vd->window_center=n/2;
		vd->left_n=n/2;
		vd->left_window_end=n/2;
		vd->left_window_start=0;
		vd->right_n=n/2;
		vd->right_window_end=n;
		vd->right_window_start=n/2;
	}	

	vd->write=vd->right_window_start-vd->left_window_start;
}


void post_window(vorbis_decode *vd, vorbis_info *vi, int n)
{
	int i=0,j=0,k=0,temp1,temp2,cnt1=0,cnt2=0;
	float **a=vd->res;
	float *left_win;
	float *right_win;
	int *data=vd->data;

	temp1=vd->left_n/64;
	while((temp1=temp1>>1) != 0)
		cnt1++;

	temp2=vd->right_n/64;
	while((temp2=temp2>>1) != 0)
		cnt2++;

	left_win=vwin[cnt1+1];
	right_win=vwin[cnt2+1];

	for(i=0;i<vi->channels;i++)
	{
		j=vd->left_window_start;
		k=0;

		while(j<vd->left_window_end)
		{
			a[i][j]=a[i][j]*left_win[k]+vd->right_hand_data[i][k];
			j++;k++;
		}

		k=0;
		j=vd->right_window_start;

		while(j<vd->right_window_end)
		{
			vd->right_hand_data[i][k]=a[i][j]*right_win[vd->right_n-k-1];
			j++;k++;
		}

		k=0;
		j=vd->left_window_start;

		while(j<vd->right_window_start)
		{
			if(i)
				data[k] |= (int)(32767*a[i][j])&0xffff;
			else
				data[k] = (int)(32767*a[i][j])<<16;
			j++;k++;
		}	
	}
} 

void floor_curve_decode(vorbis_setup *vs, vorbis_info *vi, oggpack_buffer *opb, int mode_number, vorbis_decode *vd, int n)
{
	int i, j, k,fixed_vals[4], clas, cdim, cbits, csub, cval, offset;
	int submap_number, floor_number, range;
	floor1* pointer;
	int book, low_n_offset, high_n_offset,predicted,val,highroom,lowroom;
	int room,hx,lx,hy,ly;
	
	int **flor=vd->flor;

	fixed_vals[0] = 256;
	fixed_vals[1] = 128;
	fixed_vals[2] = 86;
	fixed_vals[3] = 64;

	for(i=0;i<vi->channels;i++)
	{
		submap_number=vs->vorbis_mapping_configs[vs->vorbis_mode_configs[mode_number]->mode_mapping]->vorbis_mapping_mux[i];
		floor_number=vs->vorbis_mapping_configs[vs->vorbis_mode_configs[mode_number]->mode_mapping]->vorbis_mapping_submap_floor[submap_number];

		pointer=vs->vorbis_floor1_configs[floor_number];

		if(oggpack_read(opb, 1) == 1)
		{
			range = fixed_vals[(vs->vorbis_floor1_configs[floor_number]->floor1_mult)-1];
			flor[i][0] = oggpack_read(opb, ilog(range-1));
			flor[i][1] = oggpack_read(opb, ilog(range-1));
			
			offset = 2;

			for(j=0;j<pointer->floor1_partitions;j++)
			{
				clas = pointer->floor1_partition_class[j];
				cdim = pointer->floor1_class_dimensions[clas];
				cbits= pointer->floor1_class_subclasses[clas];
				csub = (1<<cbits)-1;
				cval=0;
				
				if(cbits > 0)
					cval=read_scalar_context(vs,opb,pointer->floor1_class_masterbooks[clas]);

				for(k=0;k<cdim;k++)
				{
					book=pointer->floor1_subclass_books[clas][cval&csub];
					cval=cval>>cbits;
					if(book>0)
						flor[i][k+offset]=read_scalar_context(vs,opb,book);
					else
						flor[i][k+offset]=0;
				}

				offset=offset+cdim;
			}

			/* curve computation */

			for(j=2;j<pointer->floor1_values;j++)
			{
				predicted=render_point(pointer->floor1_x_list[pointer->lo_n[j-2]],
										pointer->floor1_x_list[pointer->hi_n[j-2]],
										flor[i][pointer->lo_n[j-2]],
										flor[i][pointer->hi_n[j-2]],
										pointer->floor1_x_list[j]);
				val=flor[i][j];
				highroom=range-predicted;
				lowroom=predicted;

				if(highroom<lowroom)
					room=highroom<<1;
				else
					room=lowroom<<1;

				if(val != 0)
				{
					if(val>=room)
					{
						if(highroom>lowroom)
							val=val-lowroom;
						else
						val=-val+highroom-1;
					}
					else
					{
						if(val&0x01)
							val=-((val+1)>>1);
						else
							val=(val>>1);
					}

					flor[i][j]=val+predicted;
					flor[i][pointer->lo_n[j-2]]&=0x7fff;
					flor[i][pointer->hi_n[j-2]]&=0x7fff;
				}
				else
					flor[i][j]=predicted | 0x8000;
			}		
		}
		else
		{
			/* floor not used */
			flor[i][0]=-1;
		}
	}		
} 

void residue_curve_decode(int *no_residue, vorbis_setup *vs, oggpack_buffer *opb, vorbis_info *vi, int mode_no, vorbis_decode *vd, int size)
{
	/* only residue type 2 supported */
	mapping *pointer=vs->vorbis_mapping_configs[vs->vorbis_mode_configs[mode_no]->mode_mapping];
	residue *point;
	int dont_decode_flag[5], classifications[512][2];
	int i,j,k,l,m,z,pass=8,ch,residue_number,residue_type;
	int classwords_per_codeword, n_to_read, partitions_to_read, partition_count;
	float **res=vd->res;

	for(i=0;i<vi->channels;i++)
		memset(res[i],0,sizeof(res[i])*size);

	for(i=0;i<pointer->vorbis_mapping_submaps;i++)
	{
		ch=0;
		for(j=0;j<vi->channels;j++)
		{
			if(pointer->vorbis_mapping_mux[j]==i)
				if(no_residue[j]==1)
					dont_decode_flag[ch]=1;
				else
					dont_decode_flag[ch]=0;
			ch++;
		}

		residue_number=pointer->vorbis_mapping_submap_residue[i];
		point=vs->vorbis_residue_configs[residue_number];
		residue_type=point->vorbis_residue_type;
		if(residue_type!=2)
		{
			printf("\n residue type not 2 .. decode skipped");
			return;
		}

		classwords_per_codeword=vs->vorbis_codebook_configs[point->residue_classbook]->codebook_dimensions;
		n_to_read=point->residue_end-point->residue_begin;
		partitions_to_read=n_to_read/point->residue_partition_size;

		for(j=0;j<ch;j++)
			if(dont_decode_flag[j]==0) break;
		if(j==ch) return;    /* no residue to be decoded .. all zero */

		for(j=0;j<pass;j++)
		{
			for(partition_count=0, l=0;partition_count<partitions_to_read;l++)
			{
				if(j==0) /* decode classification numbers in first pass */
				{
					int temp=read_scalar_context(vs,opb,point->residue_classbook);
					for(k=classwords_per_codeword-1;k>=0;k--)
					{
						classifications[l][k]=temp%point->residue_classifications;
						temp/=point->residue_classifications;
					}
				}
				
				for(k=0;k<classwords_per_codeword && partition_count<partitions_to_read;k++,partition_count++)
				{
					int vqbook = point->residue_books[classifications[l][k]][j];
					if(vqbook!=-1)
					{
						int offset=point->residue_begin+partition_count*point->residue_partition_size;

						for(m=offset/ch;m<(offset+point->residue_partition_size)/ch;)
						{
							float entry[8];
							int chptr=0;
							int dim=vs->vorbis_codebook_configs[vqbook]->codebook_dimensions;

							read_vector_context(vs,opb,vqbook,entry);
							
							for(z=0;z<dim;z++)
							{
								res[chptr++][m]+=entry[z];
								if(chptr==ch)
								{
									chptr=0;
									m++;
								}
							}
						}
					}
				}
			}
		}
	}
}

void vorbis_decode_init(vorbis_info *vi, vorbis_decode *vd)
{

	int i;

	vd->flor = (int **) malloc(sizeof(int *)*vi->channels);
	for(i=0;i<vi->channels;i++)
	{
		vd->flor[i]=(int *) malloc(sizeof(int)*100);
		memset(vd->flor[i],0,sizeof(int)*100);
	}

	vd->res = (float **) malloc(sizeof(float *)*vi->channels);
	for(i=0;i<vi->channels;i++)
	{
		vd->res[i]=(float *) malloc(sizeof(float)*vi->blocksize[1]);
		memset(vd->res[i],0,sizeof(float)*vi->blocksize[1]);
	}

	vd->data=(int *)malloc(sizeof(int)*vi->blocksize[1]*vi->channels);
	memset(vd->data,0,sizeof(int)*vi->blocksize[1]*vi->channels);

	vd->right_hand_data=(float **)malloc(sizeof(float *)*vi->channels);
	for(i=0;i<vi->channels;i++)
	{
		vd->right_hand_data[i]=(float *)malloc(sizeof(float)*vi->blocksize[1]/2);
		memset(vd->right_hand_data[i],0,sizeof(float)*vi->blocksize[1]/2);
	}

	/* compute trig lookups for imdct */
	vd->mdct=(mdct_lookup **)malloc(sizeof(mdct_lookup *)*2);
	for(i=0;i<2;i++)
		vd->mdct[i] = (mdct_lookup *) malloc(sizeof(mdct_lookup));
	
	mdct_init(vd->mdct[0],vi->blocksize[0]);
	mdct_init(vd->mdct[1],vi->blocksize[1]);

}

void get_spectrum(vorbis_info *vi, vorbis_setup *vs, vorbis_decode *vd, int mode_no, int n)
{
	int i,j;
	int **flor=vd->flor;
	float **res=vd->res;

	for(i=0;i<vi->channels;i++)
	{
		int submap_number=vs->vorbis_mapping_configs[vs->vorbis_mode_configs[mode_no]->mode_mapping]->vorbis_mapping_mux[i];
		int floor_number=vs->vorbis_mapping_configs[vs->vorbis_mode_configs[mode_no]->mode_mapping]->vorbis_mapping_submap_floor[submap_number];

		floor1 *pointer=vs->vorbis_floor1_configs[floor_number];
		int floor1_vals=pointer->floor1_values;

		if(flor[i][0]!=-1)
		{
			int hx=0,lx=0;
			int ly=flor[i][0]*pointer->floor1_mult;

			for(j=1;j<floor1_vals;j++)
			{
				int current=pointer->sorted_index[j];
				int hy=flor[i][current]&0x7fff;
				if(hy==flor[i][current])
				{
					hy*=pointer->floor1_mult;
					hx=pointer->floor1_x_list[current];

					render_line(lx,hx,ly,hy,res[i]);

					lx=hx;ly=hy;
				}
			}

			for(j=hx;j<n;j++)
			   res[i][j]*=FLOOR1_fromdB_LOOKUP[ly];
		}
	}
}

int audio_decode(vorbis_setup *vs, vorbis_info *vi, oggpack_buffer *opb, vorbis_decode *vd)
{
	int n, mode_number, i, j, k, no_residue[2];
	mapping *pointer;
	
	int **flor=vd->flor;
	float **res=vd->res;
	//short *data=vd->data;

	if(oggpack_read(opb, 1)!=0)
	{
		printf("\n Not audio packet");
		return 0;
	}

	mode_number = oggpack_read(opb, ilog(vs->vorbis_mode_count-1));

	if(vs->vorbis_mode_configs[mode_number]->mode_blockflag==0)
		n = vi->blocksize[0];
	else
		n = vi->blocksize[1];

	/* window decode  */
	window_setup(vs, vi, opb, mode_number, vd, n);

	/* floor curve decode */
	floor_curve_decode(vs, vi, opb, mode_number, vd, n/2);

	for(i=0;i<vi->channels;i++)
		if(flor[i][0]==-1)
			no_residue[i]=1;
		else
			no_residue[i]=0;

	/* non-zero vector propagate */
	pointer=vs->vorbis_mapping_configs[vs->vorbis_mode_configs[mode_number]->mode_mapping];

	for(i=0;i<pointer->vorbis_mapping_coupling_steps;i++)
	{
		if(no_residue[pointer->vorbis_mapping_magnitude[i]]==0 || no_residue[pointer->vorbis_mapping_angle[i]]==0)
		{
			no_residue[pointer->vorbis_mapping_magnitude[i]]=0;
			no_residue[pointer->vorbis_mapping_angle[i]]=0;
		}
	}

	/* residue curve decode */
	residue_curve_decode(no_residue,vs,opb,vi,mode_number,vd,n/2);

	/* inverse channel coupling */
	for(i=pointer->vorbis_mapping_coupling_steps-1;i>=0;i--)
	{
		float *m=res[pointer->vorbis_mapping_magnitude[i]];
		float *a=res[pointer->vorbis_mapping_angle[i]];

		for(j=0;j<n/2;j++)
		{
			float nm,na;
			if(m[j]>0)
			{
				if(a[j]>0)
				{
					nm=m[j];
					na=m[j]-a[j];
				}
				else
				{
					nm=m[j]+a[j];
					na=m[j];
				}
			}
			else
			{
				if(a[j]>0)
				{
					nm=m[j];
					na=m[j]+a[j];
				}
				else
				{
					na=m[j];
					nm=m[j]-a[j];
				}
			}
			m[j]=nm;
			a[j]=na;
		}
	}

	get_spectrum(vi,vs,vd,mode_number,n/2);

	/* put imdct here.. pass in array 'flor' , the function does in-place computation 
		flor is expanded twice its size; final float pcm values are written to flor itself */

	for(i=0;i<vi->channels;i++)
	{
		mdct_backward(vd->mdct[n==vi->blocksize[0]?0:1],res[i],res[i]);
	}

	/* post windowing, overlap adding, right hand data caching 
	also converts short data to float */
	
 	post_window(vd,vi,n);
 
	return 1; 

}

