#include "byte_op.h"

extern int heap_count;

char getbyte(DATA_PTR *data_ptr)
{
	int temp;

	temp=*(data_ptr->data);

	if(data_ptr->mod==0)
		return (unsigned int)(temp & 0xFF000000)>>24;
	else if(data_ptr->mod==1)
		return (temp & 0x00FF0000)>>16;
	else if(data_ptr->mod==2)
		return (temp & 0x0000FF00)>>8;
	else 
		return (temp & 0x000000FF);
}

void putbyte(DATA_PTR *data_ptr, char val)
{
	int temp;
	unsigned int temp1=val & 0x000000FF;

	temp=*(data_ptr->data);

	if(data_ptr->mod==0)
	{
		temp=temp & 0x00FFFFFF;
		temp=temp | temp1<<24;
	}
	else if(data_ptr->mod==1)
	{
		temp=temp & 0xFF00FFFF;
		temp=temp | temp1<<16;
	}
	else if(data_ptr->mod==2)
	{
		temp=temp & 0xFFFF00FF;
		temp=temp | temp1<<8;
	}
	else
	{
		temp=temp & 0xFFFFFF00;
		temp=temp | temp1;
	}

	*(data_ptr->data) = temp;
}

void incr_ptr(DATA_PTR *data_ptr, int byte_incr)
{
		data_ptr->data += byte_incr/4;

		if(data_ptr->mod+byte_incr%4 > 3)
		{
			(data_ptr->data)++;
			data_ptr->mod+=(byte_incr%4)-4;
		}
		else
			data_ptr->mod+=byte_incr%4;

}

DATA_PTR incr_ptr_nochange(DATA_PTR data_ptr, int byte_incr)
{
	DATA_PTR temp;
	
	temp.data=data_ptr.data;
	temp.mod =data_ptr.mod;

	temp.data += byte_incr/4;

		if(temp.mod+byte_incr%4 > 3)
		{
			(temp.data)++;
			temp.mod+=(byte_incr%4)-4;
		}
		else
			temp.mod+=byte_incr%4;

		return (temp);
}

void incr_ptr_putbyte(DATA_PTR data_ptr, int byte_incr, char val)
{
	DATA_PTR temp=data_ptr;
	
	incr_ptr(&temp, byte_incr);

	putbyte(&temp, val);
}

char incr_ptr_getbyte(DATA_PTR data_ptr, int byte_incr)
{
	DATA_PTR temp;
	unsigned int answer;
	
	temp.data=data_ptr.data;
	temp.mod = data_ptr.mod;

	incr_ptr(&temp, byte_incr);

	answer = *(temp.data);

	if(temp.mod==0)
		return (unsigned int)(answer & 0xFF000000)>>24;
	else if(temp.mod==1)
		return (answer & 0x00FF0000)>>16;
	else if(temp.mod==2)
		return (answer & 0x0000FF00)>>8;
	else 
		return (answer & 0x000000FF);
}



