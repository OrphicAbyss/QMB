#ifndef IMAGE_H
#define	IMAGE_H

class Image {
public:
	static void resample(const void *indata, int inwidth, int inheight, void *outdata, int outwidth, int outheight, int bytesperpixel, int quality);
	static void mipReduce(const void *in, void *out, int *width, int *height, int destwidth, int destheight, int bytesperpixel);
};

#endif	/* IMAGE_H */

