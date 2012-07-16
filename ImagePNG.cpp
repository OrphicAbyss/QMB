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
#include "Image.h"
#include <png.h>

typedef struct png_s { // TPng = class(TGraphic)
	char *tmpBuf;
	int tmpi;
	long FBgColor; // DL Background color Added 30/05/2000
	int FTransparent; // DL Is this Image Transparent   Added 30/05/2000
	long FRowBytes; //DL Added 30/05/2000
	double FGamma; //DL Added 07/06/2000
	double FScreenGamma; //DL Added 07/06/2000
	char *FRowPtrs; // DL Changed for consistancy 30/05/2000
	char* Data; //property Data: pByte read fData;
	char* Title;
	char* Author;
	char* Description;
	int BitDepth;
	int BytesPerPixel;
	int ColorType;
	png_uint_32 Height;
	png_uint_32 Width;
	int Interlace;
	int Compression;
	int Filter;
	double LastModified;
	int Transparent;
} png_t;
png_t *my_png = 0;
unsigned char* pngbytes;

void InitializeDemData() {
	long* cvaluep; //ub

	// Initialize Data and RowPtrs
	if (my_png->Data) {
		//MemoryObj::ZFree(my_png->Data);
		free(my_png->Data);
		my_png->Data = 0;
	}
	if (my_png->FRowPtrs) {
		//MemoryObj::ZFree(my_png->FRowPtrs);
		free(my_png->FRowPtrs);
		my_png->FRowPtrs = 0;
	}
	//my_png->Data = MemoryObj::ZAlloc(my_png->Height * my_png->FRowBytes ); // DL Added 30/5/2000
	//my_png->FRowPtrs = MemoryObj::ZAlloc(sizeof(void*) * my_png->Height);
	my_png->Data = (char *) malloc(my_png->Height * my_png->FRowBytes); // DL Added 30/5/2000
	my_png->FRowPtrs = (char *) malloc(sizeof (void*) * my_png->Height);

	if ((my_png->Data) && (my_png->FRowPtrs)) {
		cvaluep = (long*) my_png->FRowPtrs;
		for (unsigned int y = 0; y < my_png->Height; y++) {
			cvaluep[y] = (long) my_png->Data + (y * (long) my_png->FRowBytes); //DL Added 08/07/2000
		}
	}
}

void mypng_struct_create() {
	if (my_png) return;
	//my_png = MemoryObj::ZAlloc(sizeof(png_t));
	my_png = (png_t *) malloc(sizeof (png_t));
	my_png->Data = 0;
	my_png->FRowPtrs = 0;
	my_png->Height = 0;
	my_png->Width = 0;
	my_png->ColorType = PNG_COLOR_TYPE_RGB;
	my_png->Interlace = PNG_INTERLACE_NONE;
	my_png->Compression = PNG_COMPRESSION_TYPE_DEFAULT;
	my_png->Filter = PNG_FILTER_TYPE_DEFAULT;
}

void mypng_struct_destroy(bool keepData) {
	if (!my_png)
		return;
	if (my_png->Data && !keepData)
		//MemoryObj::ZFree(my_png->Data);
		free(my_png->Data);
	if (my_png->FRowPtrs)
		//MemoryObj::ZFree(my_png->FRowPtrs);
		free(my_png->FRowPtrs);
	//MemoryObj::ZFree(my_png);
	free(my_png);
	my_png = 0;
}

void PNGAPI fReadData(png_structp png, png_bytep data, png_size_t length) { // called by pnglib
	unsigned int i;
	for (i = 0; i < length; i++)
		data[i] = my_png->tmpBuf[my_png->tmpi++]; // give pnglib a some more bytes
}

void Image::LoadPNG(FILE *f, char *filename, Texture *tex) {
	png_structp png;
	png_infop pnginfo;
	byte ioBuffer[8192];
	byte *raw;

	int filesize = Sys_FileLength(f);
	raw = (byte *) malloc(filesize + 1);

	fread(raw, 1, filesize, f);
	fclose(f);

	if (!raw) {
		Con_Printf("Bad png file %s\n", filename);
		return;
	}

	if (png_sig_cmp(raw, 0, 4))
		return;

	png = png_create_read_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0);
	if (!png)
		return;

	pnginfo = png_create_info_struct(png);

	if (!pnginfo) {
		png_destroy_read_struct(&png, &pnginfo, 0);
		return;
	}
	png_set_sig_bytes(png, 0/*sizeof( sig )*/);

	mypng_struct_create(); // creates the my_png struct

	my_png->tmpBuf = (char *) raw; //buf = whole file content
	my_png->tmpi = 0;

	png_set_read_fn(png, ioBuffer, fReadData);

	png_read_info(png, pnginfo);

	png_get_IHDR(png, pnginfo, &my_png->Width, &my_png->Height, &my_png->BitDepth, &my_png->ColorType, &my_png->Interlace, &my_png->Compression, &my_png->Filter);
	// ...removed bgColor code here...

	if (my_png->ColorType == PNG_COLOR_TYPE_PALETTE)
		png_set_palette_to_rgb(png);

	if (my_png->ColorType == PNG_COLOR_TYPE_GRAY && my_png->BitDepth < 8)
		png_set_expand_gray_1_2_4_to_8(png);

	// Add alpha channel if present
	if (png_get_valid(png, pnginfo, PNG_INFO_tRNS))
		png_set_tRNS_to_alpha(png);

	// hax: expand 24bit to 32bit
	if (my_png->BitDepth == 8 && my_png->ColorType == PNG_COLOR_TYPE_RGB)
		png_set_filler(png, 255, PNG_FILLER_AFTER);

	if ((my_png->ColorType == PNG_COLOR_TYPE_GRAY) || (my_png->ColorType == PNG_COLOR_TYPE_GRAY_ALPHA)) {
		png_set_gray_to_rgb(png);
		png_set_add_alpha(png, 255, PNG_FILLER_AFTER);
	}

	if (my_png->BitDepth < 8)
		png_set_expand(png);

	// update the info structure
	png_read_update_info(png, pnginfo);

	my_png->FRowBytes = png_get_rowbytes(png, pnginfo);
	my_png->BytesPerPixel = png_get_channels(png, pnginfo); // DL Added 30/08/2000

	InitializeDemData();
	if ((my_png->Data) && (my_png->FRowPtrs))
		png_read_image(png, (png_bytepp) my_png->FRowPtrs);

	png_read_end(png, pnginfo); // read last information chunks

	png_destroy_read_struct(&png, &pnginfo, 0);

	if (my_png->BitDepth == 8) {
		tex->width = my_png->Width;
		tex->height = my_png->Height;
		tex->data = (byte *) my_png->Data;
	} else {
		Con_Printf("Bad png color depth: %s\n", filename);
		free(my_png->Data);
	}
	
	free(raw);
	mypng_struct_destroy(true);

	return;
}
