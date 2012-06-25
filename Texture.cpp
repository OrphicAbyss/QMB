#include "Texture.h"

#include <GL/glu.h>

Texture::Texture(const char *ident) {
	this->identifier = (char *)MemoryObj::ZAlloc(Q_strlen(ident));
	Q_strcpy(this->identifier, ident);
	this->data = NULL;
	this->dataHash = 0;
	this->width = 0;
	this->height = 0;
	this->bytesPerPixel = 0;
	this->textureId = 0;
	this->mipmap = true;
	this->grayscale = false;
}

Texture::~Texture() {
	if (this->data)
		free(this->data);
	if (this->identifier)
		MemoryObj::ZFree(this->identifier);
}

bool Texture::operator ==(const Texture& other) const {
	return Q_strcmp(this->identifier, other.identifier) == 0;
}

bool Texture::operator !=(const Texture& other) const {
	return !(*this == other);
}

bool Texture::isMissMatch(const Texture *other) {
	return (this->dataHash != other->dataHash || this->width != other->width || this->height != other->height);
}

void Texture::calculateHash() {
	static int lhcsumtable[256];
	dataHash = 0;

	if (!this->data) {
		// Setup table
		for (int i = 0; i < 256; i++) 
			lhcsumtable[i] = i + 1;
		// Calculate hash
		int size = width * height * bytesPerPixel;
		for (int i = 0; i < size; i++) 
			dataHash += (lhcsumtable[data[i] & 255]++);
	}
}

void Texture::convertToGrayscale() {
	extern unsigned RGBAtoGrayscale(unsigned rgba);
	
	//go through data and convert
	int size = width * height;
	for (int i = 0; i < size; i++) {
		data[i] = RGBAtoGrayscale(data[i]);
	}
}

void Texture::convert8To32() {
	unsigned *newData = (unsigned *)malloc(width * height * 4);
	
	int size = width * height;
	for (int i=0; i<size; i++) {
		newData[i] = d_8to24table[data[i]];
	}
	
	free(this->data);
	this->data = (unsigned char *)newData;
}

void Texture::convert24To32() {
	unsigned char *newData = (unsigned char *)malloc(width * height * 4);
	
	int size = width * height;
	int input = 0;
	int output = 0;
	for (int i=0; i<size; i++) {
		newData[output++] = data[input++];
		newData[output++] = data[input++];
		newData[output++] = data[input++];
		newData[output++] = 0xff;
	}
	
	free(this->data);
	this->data = newData;
}

void Texture::fixSize() {
	int scaled_width, scaled_height;
	
	if (gl_texture_non_power_of_two) {
		scaled_width = width;
		scaled_height = height;

		//this seems buggered (will squash really large textures, but then again, not may huge textures around)
		if (scaled_width > gl_max_size.getInt()) scaled_width = gl_max_size.getInt(); //make sure its not bigger than the max size
		if (scaled_height > gl_max_size.getInt()) scaled_height = gl_max_size.getInt();//make sure its not bigger than the max size
	} else {
		scaled_width = 1 << (int) ceil(log(width) / log(2.0));
		scaled_height = 1 << (int) ceil(log(height) / log(2.0));

		//this seems buggered (will squash really large textures, but then again, not may huge textures around)
		scaled_width = min(scaled_width, gl_max_size.getInt()); //make sure its not bigger than the max size
		scaled_height = min(scaled_height, gl_max_size.getInt()); //make sure its not bigger than the max size
	}
	
	if (scaled_width != width || scaled_height != height) {
		resample(scaled_height, scaled_width);
	}
}

void Texture::resample(const int scaledHeight, const int scaledWidth) {
	extern void Image_Resample(const void *indata, int inwidth, int inheight, int indepth, void *outdata, int outwidth, int outheight, int outdepth, int bytesperpixel, int quality);
	unsigned char *scaled = (unsigned char *)malloc(scaledHeight * scaledWidth * this->bytesPerPixel);
	
	Image_Resample(this->data, this->width, this->height, 1, scaled, scaledWidth, scaledHeight, 1, this->bytesPerPixel, 1);
	
	free(this->data);
	this->data = scaled;
}

void Texture::upload() {
	extern int gl_filter_min;
	extern int gl_filter_max;
	
	// Ensure we have a 32bit image to upload
	if (bytesPerPixel == 1)
		this->convert8To32();
	else if (bytesPerPixel == 3)
		this->convert24To32();
	
	// If we are grayscaling things, do it now
	if (grayscale && gl_sincity.getBool())
		this->convertToGrayscale();
	
	// Resize the image if we need to (non-power of 2 or over max size)
	fixSize();
	
	if (!mipmap) {
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
	} else { //else build mipmaps for it
		if (gl_sgis_mipmap) {
			glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP_SGIS, GL_TRUE);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
			glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP_SGIS, GL_FALSE);
		} else {
			gluBuild2DMipmaps(GL_TEXTURE_2D, GL_RGBA, width, height, GL_RGBA, GL_UNSIGNED_BYTE, data);
		}
	}
	
	if (mipmap)
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
	else
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_max);
	
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, gl_anisotropic.getInt());
}
