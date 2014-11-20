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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "ogg.h"
#include "vorbis.h"


static void _v_readstring(oggpack_buffer *o,char *buf,int bytes){
	
  while(bytes--)
  {
    *buf++=oggpack_read(o,8);
  }
}


static int _vorbis_unpack_info(vorbis_info *vi,oggpack_buffer *opb)
{
  
  vi->version=oggpack_read(opb,32);
  if(vi->version!=0)return(OV_EVERSION);

  vi->channels=oggpack_read(opb,8);
  vi->rate=oggpack_read(opb,32);

  vi->bitrate_upper=oggpack_read(opb,32);
  vi->bitrate_nominal=oggpack_read(opb,32);
  vi->bitrate_lower=oggpack_read(opb,32);

  vi->blocksize[0]=1<<oggpack_read(opb,4);
  vi->blocksize[1]=1<<oggpack_read(opb,4);
  
  if(vi->rate<1)goto err_out;
  if(vi->channels<1)goto err_out;
  if(vi->blocksize[0]<8)goto err_out; 
  if(vi->blocksize[1]<vi->blocksize[0])goto err_out;
  
  if(oggpack_read(opb,1)!=1)goto err_out; /* EOP check */

  printf("\nchannels = %d",vi->channels);
  printf("\nrate = %d",vi->rate);
  printf("\nsmall blk = %d",vi->blocksize[0]);
  printf("\nbig blk = %d",vi->blocksize[1]);
  printf("\nbitrate u = %d",vi->bitrate_upper);
  printf("\nbitrate n = %d",vi->bitrate_nominal);
  printf("\nbitrate l = %d",vi->bitrate_lower);


  return(0);
 err_out:
  //vorbis_info_clear(vi);
  return(OV_EBADHEADER);
}

static int _vorbis_unpack_comment(vorbis_comment *vc,oggpack_buffer *opb)
{
	int i;
	int vendorlen=oggpack_read(opb,32);

	if(vendorlen<0)goto err_out;
	vc->vendor=calloc(vendorlen+1,1);
	_v_readstring(opb,vc->vendor,vendorlen);
	vc->comments=oggpack_read(opb,32);
	if(vc->comments<0)goto err_out;
	vc->user_comments=calloc(vc->comments+1,sizeof(*vc->user_comments));
	vc->comment_lengths=calloc(vc->comments+1, sizeof(*vc->comment_lengths));
	    
	for(i=0;i<vc->comments;i++)
	{
		int len=oggpack_read(opb,32);
		if(len<0)goto err_out;
		vc->comment_lengths[i]=len;
		vc->user_comments[i]=calloc(len+1,1);
		_v_readstring(opb,vc->user_comments[i],len);

		printf("\n %s",vc->user_comments[i]);
	}	  
	if(oggpack_read(opb,1)!=1)goto err_out; 

	printf("\n %s",vc->vendor);

	return(0);

 err_out:

  printf("\n Error in comment header decoding");
  return(OV_EBADHEADER);
} 

void decode_codebooks(vorbis_setup *vs, oggpack_buffer *opb, int codebook_no)
{
	int codebook_dimensions, codebook_entries, *codebook_lengths;
	int sparse, flag, length, i, j;
	int current_entry, current_length, number;

	int codebook_lookup_type,codebook_value_bits, *codebook_multiplicands;
	int codebook_sequence_p, codebook_lookup_values;
	unsigned int codebook_min_value, codebook_delta_value;

	int cnt=0;

	int *used_entries,*all_used;

	codebook_multiplicands = NULL;

	vs->vorbis_codebook_configs[codebook_no]=(codebook *)malloc(sizeof(codebook));	

	if(oggpack_read(opb,24)!=0x564342) goto error;
	
	codebook_dimensions=oggpack_read(opb,16);
	codebook_entries=oggpack_read(opb,24);
	all_used = (int *) malloc(codebook_entries*sizeof(int));

	if(oggpack_read(opb,1))
	{
		/* length ordered*/
		current_entry=0;
		current_length=oggpack_read(opb,5)+1;

		codebook_lengths=(int *) malloc(sizeof(int)*codebook_entries);
		used_entries=NULL;
			
	label :
		number=oggpack_read(opb,ilog(codebook_entries-current_entry));

		for(j=current_entry;j<=current_entry+number-1;j++)
		{
			codebook_lengths[j]=current_length;
			printf(" %d[%d]",*(codebook_lengths+cnt),j);
			cnt++;
		}

		for(i=0;i<codebook_entries;i++)
			all_used[i]=i;

		current_entry+=number;
		current_length++;

		if(current_entry>codebook_entries) goto error;
		if(current_entry<codebook_entries) goto label;
	}
	else
	{
		/* length unordered */
		sparse=oggpack_read(opb,1);
		
		if(sparse==0)
			codebook_lengths=(int *) malloc(sizeof(int)*codebook_entries);
		else
			codebook_lengths=NULL;

		used_entries=NULL;
		
		for(i=0;i<codebook_entries;i++)
		{
			if(sparse)
			{
				flag=oggpack_read(opb,1);
				
				if(flag)
				{
					length=oggpack_read(opb,5);
					codebook_lengths=(int *) realloc(codebook_lengths, sizeof(int)*(cnt+1));
					used_entries=(int *) realloc(used_entries, sizeof(int)*(cnt+1));
					*(codebook_lengths+cnt)=length+1;
					used_entries[cnt]=i;
					cnt++;
				}
			}
			else
			{				
				length=oggpack_read(opb,5);
				*(codebook_lengths+cnt)=length+1;
				cnt++;
			}
			all_used[i]=i;
		}
	}

	/* codebook lengths decoded. Proceed with vector lookup table 
		Vorbis supports three lookups */
	codebook_lookup_type=oggpack_read(opb,4);

	switch(codebook_lookup_type)
	{
		case 0:
			/* no lookup to be read. Proceed past lookup*/
			break;
		
		case 1:
		case 2:
			codebook_min_value=oggpack_read(opb,32);
			codebook_delta_value=oggpack_read(opb,32);
			codebook_value_bits=oggpack_read(opb,4)+1;
			codebook_sequence_p=oggpack_read(opb,1);

			if(codebook_lookup_type)
				codebook_lookup_values=lookup1_values(codebook_entries, codebook_dimensions);
			else
				codebook_lookup_values=codebook_entries * codebook_dimensions;

			codebook_multiplicands=(int *) malloc(sizeof(int)*codebook_lookup_values);

			for(j=0;j<codebook_lookup_values;j++)
				codebook_multiplicands[j]=oggpack_read(opb,codebook_value_bits);
			
			break;
		
		default:
			printf("\n Codebook lookup type is greater than 2");
			goto error;
	}
	
	vs->vorbis_codebook_configs[codebook_no]->codebook_delta_value = codebook_delta_value;
	vs->vorbis_codebook_configs[codebook_no]->codebook_min_value = codebook_min_value;
	vs->vorbis_codebook_configs[codebook_no]->codebook_entries=codebook_entries;
	vs->vorbis_codebook_configs[codebook_no]->codebook_lookup_type=codebook_lookup_type;
	vs->vorbis_codebook_configs[codebook_no]->codebook_multiplicands=codebook_multiplicands;
	//vs->vorbis_codebook_configs[codebook_no]->codebook_lengths=&codebook_lengths[0];
	//vs->vorbis_codebook_configs[codebook_no]->used_entries=used_entries;
	//vs->vorbis_codebook_configs[codebook_no]->used_entries_count=cnt;
	vs->vorbis_codebook_configs[codebook_no]->codebook_dimensions=codebook_dimensions;
	vs->vorbis_codebook_configs[codebook_no]->codebook_lookup_values=codebook_lookup_values;
	vs->vorbis_codebook_configs[codebook_no]->codebook_sequence_p=codebook_sequence_p;
	if(codebook_lengths!=NULL)
	{
		if(used_entries==NULL)
			vs->vorbis_codebook_configs[codebook_no]->huffman=huffman_tree_build(codebook_lengths,all_used,codebook_entries);
		else
			vs->vorbis_codebook_configs[codebook_no]->huffman=huffman_tree_build(codebook_lengths,used_entries,cnt);
	}

	//printf("\ncodebook no [%d] ---- no of entries used= %d ----- total no of entries = %d-------\n",codebook_no,cnt,codebook_entries);

	free(all_used);
	free(codebook_lengths);
	free(used_entries);

	return;

error:

	printf("\n This stream is not decodable");
	exit(1);

}

static int icomp(const void *a,const void *b){

  return(**(int **)a-**(int **)b);
}

int floor1_decode(vorbis_setup *vs, oggpack_buffer *opb, int floor_no)
{
	int maxclass=-1,i,rangebits,j,current_class_number;
	int count=0, *index[100];
	floor1 *pointer;

	vs->vorbis_floor1_configs[floor_no]=(floor1 *)malloc(sizeof(floor1));
	pointer=vs->vorbis_floor1_configs[floor_no];

	pointer->floor1_partitions=oggpack_read(opb,5);
	pointer->floor1_partition_class=(int *)malloc(pointer->floor1_partitions*sizeof(int));

	for(i=0;i<pointer->floor1_partitions;i++)
	{
		pointer->floor1_partition_class[i]=oggpack_read(opb,4);
		
		if(maxclass < pointer->floor1_partition_class[i])
			maxclass=pointer->floor1_partition_class[i];
	}
	pointer->maxclass=maxclass;i=maxclass+1;
	pointer->floor1_class_dimensions=(int *)malloc(i*sizeof(int));
	pointer->floor1_class_subclasses=(int *)malloc(i*sizeof(int));
	pointer->floor1_class_masterbooks=(int *)malloc(i*sizeof(int));
	pointer->floor1_subclass_books=(int **)malloc(i*sizeof(int *));


	for(i=0;i<maxclass+1;i++)
	{
		pointer->floor1_class_dimensions[i]=oggpack_read(opb,3)+1;
		pointer->floor1_class_subclasses[i]=oggpack_read(opb,2);

		if(pointer->floor1_class_subclasses[i]!=0)
			pointer->floor1_class_masterbooks[i]=oggpack_read(opb,8);
		
		*(pointer->floor1_subclass_books+i)=(int *)malloc(sizeof(int)*(1<<pointer->floor1_class_subclasses[i]));

		for(j=0;j<(1<<pointer->floor1_class_subclasses[i]);j++)
			*(*(pointer->floor1_subclass_books+i)+j)=oggpack_read(opb,8)-1;
	}

	pointer->floor1_mult=oggpack_read(opb,2)+1;
	rangebits=oggpack_read(opb,4);
	
	pointer->floor1_x_list[0]=0;
	pointer->floor1_x_list[1]=1<<rangebits;
	pointer->floor1_values=2;

	for(i=0,j=0;i<pointer->floor1_partitions;i++)
	{
		current_class_number = pointer->floor1_partition_class[i];
		count+=pointer->floor1_class_dimensions[current_class_number];
		for(;j<count;j++)
		{
			pointer->floor1_x_list[j+2]=oggpack_read(opb,rangebits);
			pointer->floor1_values++;
		}
	}

	for(i=0;i<pointer->floor1_values;i++)
		index[i]=pointer->floor1_x_list+i;

	qsort(index,pointer->floor1_values,sizeof(*index),icomp);

	for(i=0;i<pointer->floor1_values;i++)
		pointer->sorted_index[i]=index[i]-pointer->floor1_x_list;

	for(i=0;i<pointer->floor1_values-2;i++)
	{
		int lo=0;
		int hi=1;
		int lx=0;
		int hx=pointer->floor1_x_list[1];
		int currentx=pointer->floor1_x_list[i+2];
		for(j=0;j<i+2;j++)
		{
			int x=pointer->floor1_x_list[j];
			if(x>lx && x<currentx)
			{
				lo=j;
				lx=x;
			}
			if(x<hx && x>currentx)
			{
				hi=j;
				hx=x;
			}
		}
		pointer->lo_n[i]=lo;
		pointer->hi_n[i]=hi;
  }

	return 0;

}

static int icount(unsigned int v)
{
  int ret=0;
  while(v){
    ret+=v&1;
    v>>=1;
  }
  return(ret);
}

int residue_decode(vorbis_setup *vs, oggpack_buffer *opb, int type, int residue_cnt)
{
	residue *pointer;
	int *residue_cascade, highbits, lowbits, bitflag, i,j;

	vs->vorbis_residue_configs[residue_cnt]=(residue *)malloc(sizeof(residue));

	pointer=vs->vorbis_residue_configs[residue_cnt];
	
	pointer->residue_begin=oggpack_read(opb,24);
	pointer->residue_end=oggpack_read(opb,24);
	pointer->residue_partition_size=oggpack_read(opb,24)+1;
	pointer->residue_classifications=oggpack_read(opb,6)+1;
	pointer->residue_classbook=oggpack_read(opb,8);

	residue_cascade=(int *) malloc(sizeof(int)*pointer->residue_classifications);

	for(i=0;i<pointer->residue_classifications;i++)
	{
		highbits=0;
		lowbits=oggpack_read(opb,3);
		bitflag=oggpack_read(opb,1);

		if(bitflag)
			highbits=oggpack_read(opb,5);
		
		residue_cascade[i]=(highbits<<3)+lowbits;
	}

	pointer->residue_books=(int **)malloc(pointer->residue_classifications*sizeof(int *));

	for(i=0;i<pointer->residue_classifications;i++)
	{
		pointer->residue_books[i]=(int *)malloc(sizeof(int)*8);

		for(j=0;j<8;j++)
		{
			
			if(((residue_cascade[i]&(1<<j))>>j)==1)
				pointer->residue_books[i][j]=oggpack_read(opb,8);
			else
				pointer->residue_books[i][j]=-1;
		}
	}
	free(residue_cascade);
	return 0;
}

int _vorbis_unpack_books(vorbis_info *vi, vorbis_setup *vs, oggpack_buffer *opb)
{
	int i, j, vorbis_time_count;
	int vorbis_mapping_submaps;
	int temp;

	/******************* codebooks decode ***************/

	vs->vorbis_codebook_count=oggpack_read(opb,8)+1;

	vs->vorbis_codebook_configs = (codebook **) malloc(vs->vorbis_codebook_count*sizeof(codebook *));

	for(i=0;i<vs->vorbis_codebook_count;i++)
	{
		decode_codebooks(vs, opb, i);
	}

	/****************** time domain transforms ******************/

	vorbis_time_count=oggpack_read(opb,6)+1;

	for(i=0;i<vorbis_time_count;i++)
		if(oggpack_read(opb,16) != 0)
			goto error;

	/****************** floor decode ******************/

	vs->vorbis_floor_count=oggpack_read(opb,6)+1;

	vs->vorbis_floor1_configs =(floor1 **) malloc(vs->vorbis_floor_count*sizeof(floor1 *));

	for(i=0;i<vs->vorbis_floor_count;i++)
	{
		int vorbis_floor_type=oggpack_read(opb,16);

		if(!vorbis_floor_type)
		{
			printf("\n Floor type 0 is not supported");
			exit(1);
		}
		else
			floor1_decode(vs, opb, i);
	}

	/****************** residue decode ******************/

	vs->vorbis_residue_count = oggpack_read(opb,6)+1;

	vs->vorbis_residue_configs= (residue **) malloc(vs->vorbis_residue_count*sizeof(residue *));
	
	for(i=0;i<vs->vorbis_residue_count;i++)
	{
		int vorbis_residue_type=oggpack_read(opb,16);

		if(vorbis_residue_type>2 || vorbis_residue_type<0)
		{
			printf("\n Residue type not 0,1 or 2");
			goto error;
		}
		else
			residue_decode(vs,opb,vorbis_residue_type,i);

		vs->vorbis_residue_configs[i]->vorbis_residue_type=vorbis_residue_type;
	}

	/******************** mappings ********************/

	vs->vorbis_mapping_count = oggpack_read(opb,6)+1;

	vs->vorbis_mapping_configs = (mapping **)malloc(sizeof(mapping *)*vs->vorbis_mapping_count);
	
	for(i=0;i<vs->vorbis_mapping_count;i++)
	{
		int map_type=oggpack_read(opb,16);
		mapping *pointer;

		vs->vorbis_mapping_configs[i]=(mapping *)malloc(sizeof(mapping));
		memset(vs->vorbis_mapping_configs[i],0,sizeof(mapping));

		pointer = vs->vorbis_mapping_configs[i];

		if(map_type !=0)
		{
			printf("\n Maptype is non zero");
			goto error;
		}
		
		if(oggpack_read(opb,1))
			vorbis_mapping_submaps=oggpack_read(opb,4)+1;
		else
			vorbis_mapping_submaps=1;

		if(oggpack_read(opb,1))
		{
			/*square polar channel mapping in use*/
			pointer->vorbis_mapping_coupling_steps = oggpack_read(opb,8)+1;

			pointer->vorbis_mapping_magnitude = malloc(sizeof(int)*pointer->vorbis_mapping_coupling_steps);
			pointer->vorbis_mapping_angle = malloc(sizeof(int)*pointer->vorbis_mapping_coupling_steps);

			temp=ilog(vi->channels-1);

			for(j=0;j<pointer->vorbis_mapping_coupling_steps;j++)
			{
				pointer->vorbis_mapping_magnitude[j]=oggpack_read(opb,temp);
				pointer->vorbis_mapping_angle[j]=oggpack_read(opb,temp);

				if(pointer->vorbis_mapping_magnitude[j] == pointer->vorbis_mapping_angle[j] ||
					pointer->vorbis_mapping_angle[j] >= vi->channels ||
					pointer->vorbis_mapping_magnitude[j] >=vi->channels)
				{
					printf("\n Mapping magnitude or angle invalid");
					goto error;
				}
			}
		}
		
		if(oggpack_read(opb,2) != 0)
		{
			printf("\n Invalid value in reserve field");
			goto error;
		}

		if(vorbis_mapping_submaps>1)
		{
			for(j=0;j<vi->channels;j++)
				pointer->vorbis_mapping_mux[j]=oggpack_read(opb,4);

			if(pointer->vorbis_mapping_mux[j]>vorbis_mapping_submaps-1)
			{
				printf("\n vorbis_mapping_mux[j]>vorbis_mapping_submaps-1");
				goto error;
			}
		}

		pointer->vorbis_mapping_submap_floor=malloc(sizeof(int)*vorbis_mapping_submaps);
		pointer->vorbis_mapping_submap_residue=malloc(sizeof(int)*vorbis_mapping_submaps);

		for(j=0;j<vorbis_mapping_submaps;j++)
		{
			oggpack_read(opb,8);

			if((pointer->vorbis_mapping_submap_floor[j]=oggpack_read(opb,8)) > vs->vorbis_floor_count)
			{
				printf("\n vorbis_mapping_submap_floor > vorbis_floor_count");
				goto error;
			}
			if((pointer->vorbis_mapping_submap_residue[j]=oggpack_read(opb,8)) > vs->vorbis_residue_count)
			{
				printf("\n vorbis_mapping_submap_residue > vorbis_residue_count");
				goto error;
			}
		}

		pointer->vorbis_mapping_submaps=vorbis_mapping_submaps;

	}
	
	/**************** modes *************/

	vs->vorbis_mode_count = oggpack_read(opb,6)+1;

	vs->vorbis_mode_configs = (mode **)malloc(sizeof(mode *)*vs->vorbis_mode_count);

	for(i=0;i<vs->vorbis_mode_count;i++)
	{
		mode * pointer;
		int mode_windowtype, mode_transformtype;

		vs->vorbis_mode_configs[i] = (mode *) malloc(sizeof(mode));
		pointer=vs->vorbis_mode_configs[i];

		pointer->mode_blockflag=oggpack_read(opb,1);
		mode_windowtype=oggpack_read(opb,16);
		mode_transformtype=oggpack_read(opb,16);
		pointer->mode_mapping=oggpack_read(opb,8);

		if(mode_windowtype != 0 || mode_transformtype != 0)
		{
			printf("\n mode_windowtype != 0 || mode_transformtype != 0");
			goto error;
		}
		if(pointer->mode_mapping > vs->vorbis_mapping_count)
		{
			printf("\n mode_mapping > vorbis_mapping_count");
			goto error;
		}

	}

	if(oggpack_read(opb,1) !=1)
	{
		printf("\n Framing error");
		goto error;
	}

	/****************** done ******************/


	return (0);

error:
	printf("\n Stream is undecodable");
	return -1;
}


int vorbis_synthesis_headerin(vorbis_info *vi, vorbis_setup *vs, vorbis_comment *vc, ogg_packet *op,  oggpack_buffer *opb)
{
 

	if(op)
	{
		oggpack_readinit(opb,op->packet,op->bytes);

		/* Which of the three types of header is this? */
		/* Also verify header-ness, vorbis */
		{
			char buffer[6];
			int packtype=oggpack_read(opb,8);
			memset(buffer,0,6);
			_v_readstring(opb,buffer,6);
			if(memcmp(buffer,"vorbis",6))
			{
				/* not a vorbis header */
				return(OV_ENOTVORBIS);
			}
			switch(packtype)
			{
				case 0x01: /* least significant *bit* is read first */
						if(!op->b_o_s)
						{
							/* Not the initial packet */
							return(OV_EBADHEADER);
						}
						if(vi->rate!=0)
						{
							/* previously initialized info header */
							return(OV_EBADHEADER);
						}

						return(_vorbis_unpack_info(vi,opb));
				

				case 0x03: 
						if(vi->rate==0)
						{
							
							return(OV_EBADHEADER);
						}

						return(_vorbis_unpack_comment(vc,opb));

						
				case 0x05: 
						if(vi->rate==0)
						{
							
							return(OV_EBADHEADER);
						}

						return(_vorbis_unpack_books(vi,vs,opb));

				default:
						/* Not a valid vorbis header type */
						return(OV_EBADHEADER);
						//break;
			}
		}
	}
	return(OV_EBADHEADER);
}
