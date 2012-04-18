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
#include <jpeglib.h>
#include <math.h>

extern int image_width;
extern int image_height;

//jpg loading
byte *LoadJPG (FILE *f)
{
    struct jpeg_decompress_struct cinfo;
    JDIMENSION num_scanlines;
    JSAMPARRAY in;
    struct jpeg_error_mgr jerr;
    int numPixels;
    int row_stride;
    byte *out;
    int count;
    int i;
    //int r, g, b;
    byte *image_rgba;

    // set up the decompression.
    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_decompress (&cinfo);

    // inititalize the source
    jpeg_stdio_src (&cinfo, f);

    // initialize decompression
    (void) jpeg_read_header (&cinfo, TRUE);
    (void) jpeg_start_decompress (&cinfo);

    numPixels = cinfo.image_width * cinfo.image_height;

    // initialize the input buffer - we'll use the in-built memory management routines in the
    // JPEG library because it will automatically free the used memory for us when we destroy
    // the decompression structure.  cool.
    row_stride = cinfo.output_width * cinfo.output_components;
    in = (*cinfo.mem->alloc_sarray) ((j_common_ptr) &cinfo, JPOOL_IMAGE, row_stride, 1);

    // bit of error checking
    if (cinfo.output_components != 3)
        goto error;


    // initialize the return data
    image_rgba = (byte *)malloc ((numPixels * 4));

    // read the jpeg
    count = 0;

    while (cinfo.output_scanline < cinfo.output_height)
    {
        num_scanlines = jpeg_read_scanlines(&cinfo, in, 1);
        out = in[0];

        for (i = 0; i < row_stride;)
        {
            image_rgba[count++] = out[i++];//r
            image_rgba[count++] = out[i++];//g
            image_rgba[count++] = out[i++];//b
            image_rgba[count++] = 255;
        }
    }

    // finish decompression and destroy the jpeg
    image_width = cinfo.image_width;
    image_height = cinfo.image_height;

    (void) jpeg_finish_decompress (&cinfo);
    jpeg_destroy_decompress (&cinfo);

    fclose (f);
    return image_rgba;

error:
    // this should rarely (if ever) happen, but just in case...
    Con_DPrintf ("Invalid JPEG Format\n");
    jpeg_destroy_decompress (&cinfo);

    fclose (f);
    return NULL;
}

