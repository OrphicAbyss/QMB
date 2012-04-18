/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "quakedef.h"

extern int image_width;
extern int image_height;

/*
=========================================================

TARGA LOADING

=========================================================
*/
/*
typedef struct _TargaHeader
{
	unsigned char 	id_length, colormap_type, image_type;
	unsigned short	colormap_index, colormap_length;
	unsigned char	colormap_size;
	unsigned short	x_origin, y_origin, width, height;
	unsigned char	pixel_size, attributes;
}
TargaHeader;

void PrintTargaHeader(TargaHeader *t)
{
	Con_Printf("TargaHeader:\n");
	Con_Printf("uint8 id_length = %i;\n", t->id_length);
	Con_Printf("uint8 colormap_type = %i;\n", t->colormap_type);
	Con_Printf("uint8 image_type = %i;\n", t->image_type);
	Con_Printf("uint16 colormap_index = %i;\n", t->colormap_index);
	Con_Printf("uint16 colormap_length = %i;\n", t->colormap_length);
	Con_Printf("uint8 colormap_size = %i;\n", t->colormap_size);
	Con_Printf("uint16 x_origin = %i;\n", t->x_origin);
	Con_Printf("uint16 y_origin = %i;\n", t->y_origin);
	Con_Printf("uint16 width = %i;\n", t->width);
	Con_Printf("uint16 height = %i;\n", t->height);
	Con_Printf("uint8 pixel_size = %i;\n", t->pixel_size);
	Con_Printf("uint8 attributes = %i;\n", t->attributes);
}

/*
=============
LoadTGA
=============
* /
byte *LoadTGA (char *file, int matchwidth, int matchheight)
{
	FILE *files;
	int x, y, row_inc, compressed, readpixelcount, red, green, blue, alpha, runlen;
	byte *pixbuf, *image_rgba, *f;
	byte *fin, *enddata;
	TargaHeader targa_header;
	unsigned char palette[256*4], *p;
	int fs_filesize;

	f = COM_LoadTempFile(file);
	fs_filesize = COM_FOpenFile(file,&files);

	if (fs_filesize < 19)
		return NULL;

	enddata = f + fs_filesize;

	targa_header.id_length = f[0];
	targa_header.colormap_type = f[1];
	targa_header.image_type = f[2];

	targa_header.colormap_index = f[3] + f[4] * 256;
	targa_header.colormap_length = f[5] + f[6] * 256;
	targa_header.colormap_size = f[7];
	targa_header.x_origin = f[8] + f[9] * 256;
	targa_header.y_origin = f[10] + f[11] * 256;
	targa_header.width = image_width = f[12] + f[13] * 256;
	targa_header.height = image_height = f[14] + f[15] * 256;
	if (image_width > 4096 || image_height > 4096 || image_width <= 0 || image_height <= 0)
	{
		Con_Printf("LoadTGA: invalid size\n");
		PrintTargaHeader(&targa_header);
		return NULL;
	}
	if ((matchwidth && image_width != matchwidth) || (matchheight && image_height != matchheight))
		return NULL;
	targa_header.pixel_size = f[16];
	targa_header.attributes = f[17];

	fin = f + 18;
	if (targa_header.id_length != 0)
		fin += targa_header.id_length;  // skip TARGA image comment
	if (targa_header.image_type == 2 || targa_header.image_type == 10)
	{
		if (targa_header.pixel_size != 24 && targa_header.pixel_size != 32)
		{
			Con_Printf ("LoadTGA: only 24bit and 32bit pixel sizes supported for type 2 and type 10 images\n");
			PrintTargaHeader(&targa_header);
			return NULL;
		}
	}
	else if (targa_header.image_type == 1 || targa_header.image_type == 9)
	{
		if (targa_header.pixel_size != 8)
		{
			Con_Printf ("LoadTGA: only 8bit pixel size for type 1, 3, 9, and 11 images supported\n");
			PrintTargaHeader(&targa_header);
			return NULL;
		}
		if (targa_header.colormap_length != 256)
		{
			Con_Printf ("LoadTGA: only 256 colormap_length supported\n");
			PrintTargaHeader(&targa_header);
			return NULL;
		}
		if (targa_header.colormap_index)
		{
			Con_Printf ("LoadTGA: colormap_index not supported\n");
			PrintTargaHeader(&targa_header);
			return NULL;
		}
		if (targa_header.colormap_size == 24)
		{
			for (x = 0;x < targa_header.colormap_length;x++)
			{
				palette[x*4+2] = *fin++;
				palette[x*4+1] = *fin++;
				palette[x*4+0] = *fin++;
				palette[x*4+3] = 255;
			}
		}
		else if (targa_header.colormap_size == 32)
		{
			for (x = 0;x < targa_header.colormap_length;x++)
			{
				palette[x*4+2] = *fin++;
				palette[x*4+1] = *fin++;
				palette[x*4+0] = *fin++;
				palette[x*4+3] = *fin++;
			}
		}
		else
		{
			Con_Printf ("LoadTGA: Only 32 and 24 bit colormap_size supported\n");
			PrintTargaHeader(&targa_header);
			return NULL;
		}
	}
	else if (targa_header.image_type == 3 || targa_header.image_type == 11)
	{
		if (targa_header.pixel_size != 8)
		{
			Con_Printf ("LoadTGA: only 8bit pixel size for type 1, 3, 9, and 11 images supported\n");
			PrintTargaHeader(&targa_header);
			return NULL;
		}
	}
	else
	{
		Con_Printf ("LoadTGA: Only type 1, 2, 3, 9, 10, and 11 targa RGB images supported, image_type = %i\n", targa_header.image_type);
		PrintTargaHeader(&targa_header);
		return NULL;
	}

	if (targa_header.attributes & 0x10)
	{
		Con_Printf ("LoadTGA: origin must be in top left or bottom left, top right and bottom right are not supported\n");
		return NULL;
	}

	image_rgba = malloc(image_width * image_height * 4);
	if (!image_rgba)
	{
		Con_Printf ("LoadTGA: not enough memory for %i by %i image\n", image_width, image_height);
		return NULL;
	}

	// If bit 5 of attributes isn't set, the image has been stored from bottom to top
	if ((targa_header.attributes & 0x20) == 0)
	{
		pixbuf = image_rgba + (image_height - 1)*image_width*4;
		row_inc = -image_width*4*2;
	}
	else
	{
		pixbuf = image_rgba;
		row_inc = 0;
	}

	compressed = targa_header.image_type == 9 || targa_header.image_type == 10 || targa_header.image_type == 11;
	x = 0;
	y = 0;
	red = green = blue = alpha = 255;
	while (y < image_height)
	{
		// decoder is mostly the same whether it's compressed or not
		readpixelcount = 1000000;
		runlen = 1000000;
		if (compressed && fin < enddata)
		{
			runlen = *fin++;
			// high bit indicates this is an RLE compressed run
			if (runlen & 0x80)
				readpixelcount = 1;
			runlen = 1 + (runlen & 0x7f);
		}

		while((runlen--) && y < image_height)
		{
			if (readpixelcount > 0)
			{
				readpixelcount--;
				red = green = blue = alpha = 255;
				if (fin < enddata)
				{
					switch(targa_header.image_type)
					{
					case 1:
					case 9:
						// colormapped
						p = palette + (*fin++) * 4;
						red = p[0];
						green = p[1];
						blue = p[2];
						alpha = p[3];
						break;
					case 2:
					case 10:
						// BGR or BGRA
						blue = *fin++;
						if (fin < enddata)
							green = *fin++;
						if (fin < enddata)
							red = *fin++;
						if (targa_header.pixel_size == 32 && fin < enddata)
							alpha = *fin++;
						break;
					case 3:
					case 11:
						// greyscale
						red = green = blue = *fin++;
						break;
					}
				}
			}
			*pixbuf++ = red;
			*pixbuf++ = green;
			*pixbuf++ = blue;
			*pixbuf++ = alpha;
			x++;
			if (x == image_width)
			{
				// end of line, advance to next
				x = 0;
				y++;
				pixbuf += row_inc;
			}
		}
	}

	return image_rgba;
}/**/

/*
=============
TARGA LOADING
=============
*/
typedef struct _TargaHeader
{
	unsigned char 	id_length;
	unsigned char	colormap_type;
	unsigned char	image_type;
	unsigned short	colormap_index;
	unsigned short	colormap_length;
	unsigned char	colormap_size;
	unsigned short	x_origin;
	unsigned short	y_origin;
	unsigned short	width;
	unsigned short	height;
	unsigned char	pixel_size;
	unsigned char	attributes;
} TargaHeader;


TargaHeader		targa_header;

int fgetLittleShort (FILE *f)
{
	byte	b1, b2;

	b1 = fgetc(f);
	b2 = fgetc(f);

	return (short)(b1 + b2*256);
}

int fgetLittleLong (FILE *f)
{
	byte	b1, b2, b3, b4;

	b1 = fgetc(f);
	b2 = fgetc(f);
	b3 = fgetc(f);
	b4 = fgetc(f);

	return b1 + (b2<<8) + (b3<<16) + (b4<<24);
}

/*
========
LoadTGA
========
*/
byte* LoadTGA (FILE *f, char *name, int filesize)
{
	int	columns, rows, numPixels;
	byte	*pixbuf;
	int		row, column;
	byte	*image_rgba, *fin, *datafile;

	targa_header.id_length = fgetc(f);
	targa_header.colormap_type = fgetc(f);
	targa_header.image_type = fgetc(f);

	targa_header.colormap_index = fgetLittleShort(f);
	targa_header.colormap_length = fgetLittleShort(f);
	targa_header.colormap_size = fgetc(f);
	targa_header.x_origin = fgetLittleShort(f);
	targa_header.y_origin = fgetLittleShort(f);
	targa_header.width = fgetLittleShort(f);
	targa_header.height = fgetLittleShort(f);
	targa_header.pixel_size = fgetc(f);
	targa_header.attributes = fgetc(f);

	if (targa_header.image_type!=2 && targa_header.image_type!=10){
		Con_DPrintf ("&c900LoadTGA:&r Only type 2 and 10 targa RGB images supported &c090[%s]&r\n",name);
		return NULL;
	}

	if (targa_header.colormap_type !=0 || (targa_header.pixel_size!=32 && targa_header.pixel_size!=24)){
		Con_DPrintf ("&c900LoadTGA:&r Only 32 or 24 bit images supported (no colormaps) &c090[%s]&r\n",name);
		return NULL;
	}

	columns = targa_header.width;
	rows = targa_header.height;
	numPixels = columns * rows;

	image_rgba = (byte *)malloc (numPixels*4);
	if (!image_rgba)
	{
		Con_DPrintf ("&c900LoadTGA:&r not enough memory for %i by %i image\n", image_width, image_height);
		return NULL;
	}


	if (targa_header.id_length != 0)
		fseek(f, targa_header.id_length, SEEK_CUR);  // skip TARGA image comment

	//file to read = total size - current position
	filesize = filesize - ftell(f);

	//read it whole image at once
	datafile = (byte *)malloc (filesize);
	if (!datafile)
	{
		Con_DPrintf ("&c900LoadTGA:&r not enough memory for %i by %i image\n", image_width, image_height);
		return NULL;
	}

	fread(datafile, 1, filesize,f);
	fin = datafile;

	if (targa_header.image_type==2)
	{  // Uncompressed, RGB images
		for(row=rows-1; row>=0; row--)
		{
			pixbuf = image_rgba + row*columns*4;
			for(column=0; column<columns; column++)
			{
				unsigned char red = 0,green = 0,blue = 0,alphabyte = 0;
				switch (targa_header.pixel_size)
				{
					case 24:

							blue = *fin++;
							green = *fin++;
							red = *fin++;
							*pixbuf++ = red;
							*pixbuf++ = green;
							*pixbuf++ = blue;
							*pixbuf++ = 255;
							break;
					case 32:
							blue = *fin++;
							green = *fin++;
							red = *fin++;
							alphabyte = *fin++;
							*pixbuf++ = red;
							*pixbuf++ = green;
							*pixbuf++ = blue;
							*pixbuf++ = alphabyte;
							break;
				}
			}
		}
	}
	else if (targa_header.image_type==10)
	{   // Runlength encoded RGB images
		unsigned char red = 0,green = 0,blue = 0,alphabyte = 0,packetHeader,packetSize,j;
		for(row=rows-1; row>=0; row--)
		{
			pixbuf = image_rgba + row*columns*4;
			for(column=0; column<columns; )
			{
				packetHeader=*fin++;
				packetSize = 1 + (packetHeader & 0x7f);
				if (packetHeader & 0x80)
				{        // run-length packet
					switch (targa_header.pixel_size)
					{
						case 24:
								blue = *fin++;
								green = *fin++;
								red = *fin++;
								alphabyte = 255;
								break;
						case 32:
								blue = *fin++;
								green = *fin++;
								red = *fin++;
								alphabyte = *fin++;
								break;
					}
					for(j=0;j<packetSize;j++)
					{
						*pixbuf++=red;
						*pixbuf++=green;
						*pixbuf++=blue;
						*pixbuf++=alphabyte;
						column++;
						if (column==columns)
						{ // run spans across rows
							column=0;
							if (row>0)
								row--;
							else
								goto breakOut;
							pixbuf = image_rgba + row*columns*4;
						}
					}
				}
				else
				{	// non run-length packet
					for(j=0;j<packetSize;j++)
					{
						switch (targa_header.pixel_size)
						{
							case 24:
									blue = *fin++;
									green = *fin++;
									red = *fin++;
									*pixbuf++ = red;
									*pixbuf++ = green;
									*pixbuf++ = blue;
									*pixbuf++ = 255;
									break;
							case 32:
									blue = *fin++;
									green = *fin++;
									red = *fin++;
									alphabyte = *fin++;
									*pixbuf++ = red;
									*pixbuf++ = green;
									*pixbuf++ = blue;
									*pixbuf++ = alphabyte;
									break;
						}
						column++;
						if (column==columns)
						{ // pixel packet run spans across rows
							column=0;
							if (row>0)
								row--;
							else
								goto breakOut;
							pixbuf = image_rgba + row*columns*4;
						}
					}
				}
			}
			breakOut:;
		}
	}

	image_width = columns;
	image_height = rows;

	free(datafile);

	return image_rgba;
}
