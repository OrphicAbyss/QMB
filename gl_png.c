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
#include "png.h"

extern int image_width;
extern int image_height;

typedef struct png_s
{ // TPng = class(TGraphic)
   char *tmpBuf;
   int tmpi;
   long FBgColor; // DL Background color Added 30/05/2000
   int FTransparent; // DL Is this Image Transparent   Added 30/05/2000
   long FRowBytes;   //DL Added 30/05/2000
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
   int Height;
   int Width;
   int Interlace;
   int Compression;
   int Filter;
   double LastModified;
   int Transparent;
} png_t;
png_t *my_png=0;
unsigned char* pngbytes;
//_____________________________________________________________________________
void InitializeDemData()
{
   long* cvaluep; //ub
   long y;

   // Initialize Data and RowPtrs
   if(my_png->Data)
   {
      //Z_Free(my_png->Data);      
      free(my_png->Data);      
      my_png->Data = 0;
   }
   if(my_png->FRowPtrs)
   {
      //Z_Free(my_png->FRowPtrs);
      free(my_png->FRowPtrs);
      my_png->FRowPtrs = 0;
   }
   //my_png->Data = Z_Malloc(my_png->Height * my_png->FRowBytes ); // DL Added 30/5/2000
   //my_png->FRowPtrs = Z_Malloc(sizeof(void*) * my_png->Height);
   my_png->Data = malloc(my_png->Height * my_png->FRowBytes ); // DL Added 30/5/2000
   my_png->FRowPtrs = malloc(sizeof(void*) * my_png->Height);


   if((my_png->Data)&&(my_png->FRowPtrs))
   {
      cvaluep = (long*)my_png->FRowPtrs;   
      for(y=0;y<my_png->Height;y++)
      {
         cvaluep[y] = (long)my_png->Data + ( y * (long)my_png->FRowBytes ); //DL Added 08/07/2000     
      }
   }
}
//_____________________________________________________________________________
void mypng_struct_create()
{
   if(my_png) return;
   //my_png = Z_Malloc(sizeof(png_t));
   my_png = malloc(sizeof(png_t));
   my_png->Data    = 0;
   my_png->FRowPtrs = 0;
   my_png->Height  = 0;
   my_png->Width   = 0;
   my_png->ColorType = PNG_COLOR_TYPE_RGB;
   my_png->Interlace = PNG_INTERLACE_NONE;
   my_png->Compression = PNG_COMPRESSION_TYPE_DEFAULT;
   my_png->Filter = PNG_FILTER_TYPE_DEFAULT;
}
//_____________________________________________________________________________
void mypng_struct_destroy(qboolean keepData) { 
   if(!my_png)
      return;
   if(my_png->Data && !keepData)
      //Z_Free(my_png->Data);
      free(my_png->Data);
   if(my_png->FRowPtrs)
      //Z_Free(my_png->FRowPtrs);
      free(my_png->FRowPtrs);
   //Z_Free(my_png);
   free(my_png);
   my_png = 0;
}
//_____________________________________________________________________________
void PNGAPI fReadData(png_structp png,png_bytep data,png_size_t length) { // called by pnglib
   unsigned int i; 
   for(i=0;i<length;i++)
      data[i] = my_png->tmpBuf[my_png->tmpi++];    // give pnglib a some more bytes 
}
//_____________________________________________________________________________

extern int filelength (FILE *f);

//Tei png version, ripped and adapted from sul_png.c from Quake2max
byte * LoadPNG (FILE *f,char * name)
{
   png_structp png;
   png_infop pnginfo;
   byte ioBuffer[8192];
   int len;
   byte *raw;
   byte *imagedata;

   len = filelength(f);

   raw = malloc(len+1);//Z_Malloc(len + 1);

   fread (raw, 1, len, f);
   fclose(f);

   if (!raw)
   {
      Con_Printf ("Bad png file %s\n", name);
      return 0;
   }

   if( png_sig_cmp(raw,0,4))
      return 0; 

   png = png_create_read_struct(PNG_LIBPNG_VER_STRING,0,0,0);
   if(!png)
      return 0;

   pnginfo = png_create_info_struct( png );

   if(!pnginfo)
   {
      png_destroy_read_struct(&png,&pnginfo,0);
      return 0;
   }
   png_set_sig_bytes(png,0/*sizeof( sig )*/);

   mypng_struct_create(); // creates the my_png struct

   my_png->tmpBuf = raw; //buf = whole file content
   my_png->tmpi=0;

   png_set_read_fn(png,ioBuffer,fReadData );

   png_read_info( png, pnginfo );

   png_get_IHDR(png, pnginfo, &my_png->Width, &my_png->Height,&my_png->BitDepth, &my_png->ColorType, &my_png->Interlace, &my_png->Compression, &my_png->Filter );
   // ...removed bgColor code here...

   if (my_png->ColorType == PNG_COLOR_TYPE_PALETTE) 
      png_set_palette_to_rgb(png);
   if (my_png->ColorType == PNG_COLOR_TYPE_GRAY && my_png->BitDepth < 8)
      png_set_gray_1_2_4_to_8(png);

   // Add alpha channel if present
   if(png_get_valid( png, pnginfo, PNG_INFO_tRNS ))
      png_set_tRNS_to_alpha(png);

   // hax: expand 24bit to 32bit
   if (my_png->BitDepth == 8 && my_png->ColorType == PNG_COLOR_TYPE_RGB)
      png_set_filler(png,255, PNG_FILLER_AFTER);

   if (( my_png->ColorType == PNG_COLOR_TYPE_GRAY) || (my_png->ColorType == PNG_COLOR_TYPE_GRAY_ALPHA ))
      png_set_gray_to_rgb( png );


   if (my_png->BitDepth < 8)
      png_set_expand (png);

   // update the info structure
   png_read_update_info( png, pnginfo );

   my_png->FRowBytes = png_get_rowbytes( png, pnginfo );
   my_png->BytesPerPixel = png_get_channels( png, pnginfo );  // DL Added 30/08/2000

   InitializeDemData();
   if((my_png->Data)&&(my_png->FRowPtrs))
      png_read_image(png, (png_bytepp)my_png->FRowPtrs);

   png_read_end(png, pnginfo); // read last information chunks

   png_destroy_read_struct(&png, &pnginfo, 0);


   //only load 32 bit by now...


   //Fix code BorisU
   //if (my_png->BitDepth == 8 &&  !(my_png->ColorType  & PNG_COLOR_MASK_ALPHA) )
     // png_set_filler(png, 255, PNG_FILLER_AFTER);
   //Fix code BorisU

   if (my_png->BitDepth == 8)
   {
      image_width      = my_png->Width;
      image_height   = my_png->Height;
      imagedata      = my_png->Data;
   }
   else
   {
		Con_Printf ("Bad png color depth: %s\n", name);
		//*pic = NULL;
		imagedata = 0;
		free( my_png->Data );      
		free(raw);
   }

   mypng_struct_destroy(true);
   
   return imagedata;
}
//Tei png version, ripped and adapted from sul_png.c from Quake2max

//_____________________________________________________________________________