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
#include "Texture.h"

/*
=============
TARGA LOADING
=============
 */
typedef struct _TargaHeader {
	unsigned char id_length;
	unsigned char colormap_type;
	unsigned char image_type;
	unsigned short colormap_index;
	unsigned short colormap_length;
	unsigned char colormap_size;
	unsigned short x_origin;
	unsigned short y_origin;
	unsigned short width;
	unsigned short height;
	unsigned char pixel_size;
	unsigned char attributes;
} TargaHeader;


TargaHeader targa_header;

int fgetLittleShort(FILE *f) {
	byte b1, b2;

	b1 = fgetc(f);
	b2 = fgetc(f);

	return (short) (b1 + b2 * 256);
}

int fgetLittleLong(FILE *f) {
	byte b1, b2, b3, b4;

	b1 = fgetc(f);
	b2 = fgetc(f);
	b3 = fgetc(f);
	b4 = fgetc(f);

	return b1 + (b2 << 8) + (b3 << 16) + (b4 << 24);
}

/**
 * Load TGA image file
 */
void LoadTGA(FILE *f, char *filename, Texture *tex) {
	int columns, rows, numPixels;
	byte *pixbuf;
	int row, column;
	byte *image_rgba, *fin, *datafile;

	int filesize = Sys_FileLength(f);
	
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

	if (targa_header.image_type != 2 && targa_header.image_type != 10) {
		Con_Printf("&c900LoadTGA:&r Only type 2 and 10 targa RGB images supported &c090[%s]&r\n", filename);
		return;
	}

	if (targa_header.colormap_type != 0 || (targa_header.pixel_size != 32 && targa_header.pixel_size != 24)) {
		Con_Printf("&c900LoadTGA:&r Only 32 or 24 bit images supported (no colormaps) &c090[%s]&r\n", filename);
		return;
	}

	columns = targa_header.width;
	rows = targa_header.height;
	numPixels = columns * rows;

	image_rgba = (byte *) malloc(numPixels * 4);
	if (!image_rgba) {
		Con_Printf("&c900LoadTGA:&r not enough memory for %i by %i image: %s\n", columns, rows, filename);
		return;
	}

	if (targa_header.id_length != 0)
		fseek(f, targa_header.id_length, SEEK_CUR); // skip TARGA image comment

	//file to read = total size - current position
	filesize = filesize - ftell(f);

	//read it whole image at once
	datafile = (byte *) malloc(filesize);
	if (!datafile) {
		Con_Printf("&c900LoadTGA:&r not enough memory for %i by %i image: %s\n", columns, rows, filename);
		return;
	}

	fread(datafile, 1, filesize, f);
	fin = datafile;

	if (targa_header.image_type == 2) { // Uncompressed, RGB images
		for (row = rows - 1; row >= 0; row--) {
			pixbuf = image_rgba + row * columns * 4;
			for (column = 0; column < columns; column++) {
				unsigned char red = 0, green = 0, blue = 0, alphabyte = 0;
				switch (targa_header.pixel_size) {
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
	} else if (targa_header.image_type == 10) { // Runlength encoded RGB images
		unsigned char red = 0, green = 0, blue = 0, alphabyte = 0, packetHeader, packetSize, j;
		for (row = rows - 1; row >= 0; row--) {
			pixbuf = image_rgba + row * columns * 4;
			for (column = 0; column < columns;) {
				packetHeader = *fin++;
				packetSize = 1 + (packetHeader & 0x7f);
				if (packetHeader & 0x80) { // run-length packet
					switch (targa_header.pixel_size) {
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
					for (j = 0; j < packetSize; j++) {
						*pixbuf++ = red;
						*pixbuf++ = green;
						*pixbuf++ = blue;
						*pixbuf++ = alphabyte;
						column++;
						if (column == columns) { // run spans across rows
							column = 0;
							if (row > 0)
								row--;
							else
								goto breakOut;
							pixbuf = image_rgba + row * columns * 4;
						}
					}
				} else { // non run-length packet
					for (j = 0; j < packetSize; j++) {
						switch (targa_header.pixel_size) {
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
						if (column == columns) { // pixel packet run spans across rows
							column = 0;
							if (row > 0)
								row--;
							else
								goto breakOut;
							pixbuf = image_rgba + row * columns * 4;
						}
					}
				}
			}
breakOut:
			;
		}
	}
	free(datafile);
	
	tex->width = columns;
	tex->height = rows;
	tex->data = image_rgba;
}
