#include "Image.h"
#include "quakedef.h"

void ResampleLerpLine(const byte *in, byte *out, int inwidth, int outwidth) {
	int j, xi, oldx = 0, f, fstep, endx, lerp;
	fstep = (int) (inwidth * 65536.0f / outwidth);
	endx = (inwidth - 1);
	for (j = 0, f = 0; j < outwidth; j++, f += fstep) {
		xi = f >> 16;
		if (xi != oldx) {
			in += (xi - oldx) * 4;
			oldx = xi;
		}
		if (xi < endx) {
			lerp = f & 0xFFFF;
			*out++ = (byte) ((((in[4] - in[0]) * lerp) >> 16) + in[0]);
			*out++ = (byte) ((((in[5] - in[1]) * lerp) >> 16) + in[1]);
			*out++ = (byte) ((((in[6] - in[2]) * lerp) >> 16) + in[2]);
			*out++ = (byte) ((((in[7] - in[3]) * lerp) >> 16) + in[3]);
		} else // last pixel of the line has no pixel to lerp to
		{
			*out++ = in[0];
			*out++ = in[1];
			*out++ = in[2];
			*out++ = in[3];
		}
	}
}

int resamplerowsize = 0;
byte *resamplerow1 = NULL;
byte *resamplerow2 = NULL;

#define LERPBYTE(i) r = resamplerow1[i];out[i] = (byte) ((((resamplerow2[i] - r) * lerp) >> 16) + r)

void ResampleLerp(const void *indata, int inwidth, int inheight, void *outdata, int outwidth, int outheight) {
	int i, j, r, yi, oldy, f, fstep, lerp, endy = (inheight - 1), inwidth4 = inwidth * 4, outwidth4 = outwidth * 4;
	byte *out;
	const byte *inrow;
	out = (byte *) outdata;
	fstep = (int) (inheight * 65536.0f / outheight);

	inrow = (const byte *) indata;
	oldy = 0;
	ResampleLerpLine(inrow, resamplerow1, inwidth, outwidth);
	ResampleLerpLine(inrow + inwidth4, resamplerow2, inwidth, outwidth);
	for (i = 0, f = 0; i < outheight; i++, f += fstep) {
		yi = f >> 16;
		if (yi < endy) {
			lerp = f & 0xFFFF;
			if (yi != oldy) {
				inrow = (byte *) indata + inwidth4*yi;
				if (yi == oldy + 1)
					memcpy(resamplerow1, resamplerow2, outwidth4);
				else
					ResampleLerpLine(inrow, resamplerow1, inwidth, outwidth);
				ResampleLerpLine(inrow + inwidth4, resamplerow2, inwidth, outwidth);
				oldy = yi;
			}
			j = outwidth - 4;
			while (j >= 0) {
				LERPBYTE(0);
				LERPBYTE(1);
				LERPBYTE(2);
				LERPBYTE(3);
				LERPBYTE(4);
				LERPBYTE(5);
				LERPBYTE(6);
				LERPBYTE(7);
				LERPBYTE(8);
				LERPBYTE(9);
				LERPBYTE(10);
				LERPBYTE(11);
				LERPBYTE(12);
				LERPBYTE(13);
				LERPBYTE(14);
				LERPBYTE(15);
				out += 16;
				resamplerow1 += 16;
				resamplerow2 += 16;
				j -= 4;
			}
			if (j & 2) {
				LERPBYTE(0);
				LERPBYTE(1);
				LERPBYTE(2);
				LERPBYTE(3);
				LERPBYTE(4);
				LERPBYTE(5);
				LERPBYTE(6);
				LERPBYTE(7);
				out += 8;
				resamplerow1 += 8;
				resamplerow2 += 8;
			}
			if (j & 1) {
				LERPBYTE(0);
				LERPBYTE(1);
				LERPBYTE(2);
				LERPBYTE(3);
				out += 4;
				resamplerow1 += 4;
				resamplerow2 += 4;
			}
			resamplerow1 -= outwidth4;
			resamplerow2 -= outwidth4;
		} else {
			if (yi != oldy) {
				inrow = (byte *) indata + inwidth4*yi;
				if (yi == oldy + 1)
					memcpy(resamplerow1, resamplerow2, outwidth4);
				else
					ResampleLerpLine(inrow, resamplerow1, inwidth, outwidth);
				oldy = yi;
			}
			memcpy(out, resamplerow1, outwidth4);
		}
	}
}

void Resample32Nearest(const void *indata, int inwidth, int inheight, void *outdata, int outwidth, int outheight) {
	int i, j;
	unsigned frac, fracstep;
	// relies on int being 4 bytes
	int *inrow, *out;
	out = (int *) outdata;

	fracstep = inwidth * 0x10000 / outwidth;
	for (i = 0; i < outheight; i++) {
		inrow = (int *) indata + inwidth * (i * inheight / outheight);
		frac = fracstep >> 1;
		j = outwidth - 4;
		while (j >= 0) {
			out[0] = inrow[frac >> 16];
			frac += fracstep;
			out[1] = inrow[frac >> 16];
			frac += fracstep;
			out[2] = inrow[frac >> 16];
			frac += fracstep;
			out[3] = inrow[frac >> 16];
			frac += fracstep;
			out += 4;
			j -= 4;
		}
		if (j & 2) {
			out[0] = inrow[frac >> 16];
			frac += fracstep;
			out[1] = inrow[frac >> 16];
			frac += fracstep;
			out += 2;
		}
		if (j & 1) {
			out[0] = inrow[frac >> 16];
			frac += fracstep;
			out += 1;
		}
	}
}

void Image::resample(const void *indata, int inwidth, int inheight, void *outdata, int outwidth, int outheight, int bytesperpixel, int quality) {
	if (resamplerowsize < outwidth * 4) {
		if (resamplerow1)
			free(resamplerow1);
		resamplerowsize = outwidth * 4;
		resamplerow1 = (byte *) malloc(resamplerowsize * 2);
		resamplerow2 = resamplerow1 + resamplerowsize;
	}
	if (bytesperpixel == 4) {
		if (quality)
			ResampleLerp(indata, inwidth, inheight, outdata, outwidth, outheight);
		else
			Resample32Nearest(indata, inwidth, inheight, outdata, outwidth, outheight);
	} else
		Sys_Error("Image_Resample: unsupported bytesperpixel %i\n", bytesperpixel);
}

// in can be the same as out
void Image::mipReduce(const void *inData, void *outData, int *width, int *height, int destwidth, int destheight, int bytesperpixel) {
	int x, y;
	byte *in = (byte *)inData;
	byte *out = (byte *)outData;
	
	int nextrow = *width * bytesperpixel;
	if (*width > destwidth) {
		*width >>= 1;
		if (*height > destheight) {
			// reduce both
			*height >>= 1;
			if (bytesperpixel == 4) {
				for (y = 0; y < *height; y++) {
					for (x = 0; x < *width; x++) {
						out[0] = (byte) ((in[0] + in[4] + in[nextrow ] + in[nextrow + 4]) >> 2);
						out[1] = (byte) ((in[1] + in[5] + in[nextrow + 1] + in[nextrow + 5]) >> 2);
						out[2] = (byte) ((in[2] + in[6] + in[nextrow + 2] + in[nextrow + 6]) >> 2);
						out[3] = (byte) ((in[3] + in[7] + in[nextrow + 3] + in[nextrow + 7]) >> 2);
						out += 4;
						in += 8;
					}
					in += nextrow; // skip a line
				}
			} else if (bytesperpixel == 3) {
				for (y = 0; y < *height; y++) {
					for (x = 0; x < *width; x++) {
						out[0] = (byte) ((in[0] + in[3] + in[nextrow ] + in[nextrow + 3]) >> 2);
						out[1] = (byte) ((in[1] + in[4] + in[nextrow + 1] + in[nextrow + 4]) >> 2);
						out[2] = (byte) ((in[2] + in[5] + in[nextrow + 2] + in[nextrow + 5]) >> 2);
						out += 3;
						in += 6;
					}
					in += nextrow; // skip a line
				}
			} else
				Sys_Error("Image_MipReduce: unsupported bytesperpixel %i\n", bytesperpixel);
		} else {
			// reduce width
			if (bytesperpixel == 4) {
				for (y = 0; y < *height; y++) {
					for (x = 0; x < *width; x++) {
						out[0] = (byte) ((in[0] + in[4]) >> 1);
						out[1] = (byte) ((in[1] + in[5]) >> 1);
						out[2] = (byte) ((in[2] + in[6]) >> 1);
						out[3] = (byte) ((in[3] + in[7]) >> 1);
						out += 4;
						in += 8;
					}
				}
			} else if (bytesperpixel == 3) {
				for (y = 0; y < *height; y++) {
					for (x = 0; x < *width; x++) {
						out[0] = (byte) ((in[0] + in[3]) >> 1);
						out[1] = (byte) ((in[1] + in[4]) >> 1);
						out[2] = (byte) ((in[2] + in[5]) >> 1);
						out += 3;
						in += 6;
					}
				}
			} else
				Sys_Error("Image_MipReduce: unsupported bytesperpixel %i\n", bytesperpixel);
		}
	} else {
		if (*height > destheight) {
			// reduce height
			*height >>= 1;
			if (bytesperpixel == 4) {
				for (y = 0; y < *height; y++) {
					for (x = 0; x < *width; x++) {
						out[0] = (byte) ((in[0] + in[nextrow ]) >> 1);
						out[1] = (byte) ((in[1] + in[nextrow + 1]) >> 1);
						out[2] = (byte) ((in[2] + in[nextrow + 2]) >> 1);
						out[3] = (byte) ((in[3] + in[nextrow + 3]) >> 1);
						out += 4;
						in += 4;
					}
					in += nextrow; // skip a line
				}
			} else if (bytesperpixel == 3) {
				for (y = 0; y < *height; y++) {
					for (x = 0; x < *width; x++) {
						out[0] = (byte) ((in[0] + in[nextrow ]) >> 1);
						out[1] = (byte) ((in[1] + in[nextrow + 1]) >> 1);
						out[2] = (byte) ((in[2] + in[nextrow + 2]) >> 1);
						out += 3;
						in += 3;
					}
					in += nextrow; // skip a line
				}
			} else
				Sys_Error("Image_MipReduce: unsupported bytesperpixel %i\n", bytesperpixel);
		} else
			Sys_Error("Image_MipReduce: desired size already achieved\n");
	}
}

