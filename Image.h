#ifndef IMAGE_H
#define	IMAGE_H

#include <stdio.h>

class Texture;

class Image {
public:
	static void resample(const void *indata, int inwidth, int inheight, void *outdata, int outwidth, int outheight, int bytesperpixel, int quality);
	static void mipReduce(const void *in, void *out, int *width, int *height, int destwidth, int destheight, int bytesperpixel);
	
	static void LoadJPG(FILE *f, char *filename, Texture *tex);
	static void LoadPCX(FILE *f, char *filename, Texture *tex);
	static void LoadPNG(FILE *f, char *filename, Texture *tex);
	static void LoadTGA(FILE *f, char *filename, Texture *tex);
};

#endif	/* IMAGE_H */

