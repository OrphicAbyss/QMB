#include "Texture.h"
#include "Image.h"

#include <GL/glu.h>

Texture::Texture(const char *ident) {
	this->identifier = (char *)MemoryObj::ZAlloc(strlen(ident) + 1);
	strcpy(this->identifier, ident);
	this->textureType = GL_TEXTURE_2D;
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
	if (textureId != 0)
		TextureManager::delTextureId(textureId);
	if (data)
		free(data);
	if (identifier)
		MemoryObj::ZFree(identifier);
}

bool Texture::operator ==(const Texture& other) const {
	return strcmp(this->identifier, other.identifier) == 0;
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

__inline unsigned RGBAtoGrayscale(unsigned rgba) {
	unsigned char *rgb, value;
	unsigned output;
	double shift;

	output = rgba;
	rgb = ((unsigned char *) &output);

	value = min(255, rgb[0] * 0.2125 + rgb[1] * 0.7154 + rgb[2] * 0.0721);
	//shift = sqrt(max(0,(rgb[0])-(rgb[1]+rgb[2])/2))/16;
	//value = min(255,rgb[0] * 0.299 + rgb[1] * 0.587 + rgb[2] * 0.114);

#if 1
	rgb[0] = value;
	rgb[1] = value;
	rgb[2] = value;
#else
	rgb[0] = rgb[0] + (value - rgb[0]) * shift;
	rgb[1] = rgb[1] + (value - rgb[1]) * shift;
	rgb[2] = rgb[2] + (value - rgb[2]) * shift;
#endif

	return output;
}

void PrintErrorMessage(const char *function, const char *message) {
	Con_SafePrintf("&c500%s:&r %s\n");
}

void Texture::convertToGrayscale() {
	if (bytesPerPixel != 4) {
		PrintErrorMessage("convertToGrayscale", "Requires 32bit image to convert...");
		return;
	}

	//go through data and convert
	int size = width * height;
	for (int i = 0; i < size; i++) {
		data[i] = RGBAtoGrayscale(data[i]);
	}
}

bool Texture::convert8To32Fullbright() {
	if (bytesPerPixel != 1) {
		PrintErrorMessage("convert8To32Fullbright", "Requires 8bit image to convert...");
		return false;
	}

	bool hasFullbright = false;
	unsigned *newData = (unsigned *)malloc(width * height * 4);

	int size = width * height;
	for (int i=0; i<size; i++) {
		unsigned char p = data[i];
		if (p < 224)
			newData[i] = d_8to24table[255]; // transparent
		else {
			newData[i] = d_8to24table[p]; // fullbright
			hasFullbright = true;
		}
	}

	free(this->data);
	data = (unsigned char *)newData;
	bytesPerPixel = 4;
	return hasFullbright;
}

void Texture::convert8To32() {
	if (bytesPerPixel != 1) {
		PrintErrorMessage("convert8To32", "Requires 8bit image to convert...");
		return;
	}

	unsigned *newData = (unsigned *)malloc(width * height * 4);

	int size = width * height;
	for (int i=0; i<size; i++) {
		newData[i] = d_8to24table[data[i]];
	}

	free(this->data);
	this->data = (unsigned char *)newData;
	this->bytesPerPixel = 4;
}

void Texture::convert24To32() {
	if (bytesPerPixel != 3) {
		PrintErrorMessage("convert8To32", "Requires 24bit image to convert...");
		return;
	}

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
	this->bytesPerPixel = 4;
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
		width = scaled_width;
		height = scaled_height;
	}
}

void Texture::resample(const int scaledHeight, const int scaledWidth) {
	unsigned char *scaled = (unsigned char *)malloc(scaledHeight * scaledWidth * this->bytesPerPixel);

	Image::resample((void *)this->data, this->width, this->height, (void *)scaled, scaledWidth, scaledHeight, this->bytesPerPixel, 1);

	free(this->data);
	this->data = scaled;
}

void Texture::upload() {
	// Ensure we have a 32bit image to upload
	if (bytesPerPixel == 1)
		convert8To32();
	else if (bytesPerPixel == 3)
		convert24To32();

	// If we are grayscaling things, do it now
	if (grayscale && gl_sincity.getBool())
		convertToGrayscale();

	// Resize the image if we need to (non-power of 2 or over max size)
	fixSize();

	if (textureId == 0)
		textureId = TextureManager::getTextureId();

	if (this->textureType == GL_TEXTURE_2D) {
		glBindTexture(GL_TEXTURE_2D, textureId);
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
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, TextureManager::glFilterMin);
		else
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, TextureManager::glFilterMax);

		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, TextureManager::glFilterMax);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, gl_anisotropic.getInt());
	} else if (this->textureType == GL_TEXTURE_1D) {
		glBindTexture(GL_TEXTURE_1D, textureId);
		glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		// Upload
		glTexImage1D(GL_TEXTURE_1D, 0, GL_RGBA, width, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
	} else {
		Con_Printf("Unknown texture type for texture: %s", identifier);
	}
}

void Texture::setData(unsigned char *data) {
	setData(data, false);
}

void Texture::setData(unsigned char *data, bool copy) {
	if (width == 0 || height == 0 || bytesPerPixel == 0) {
		Sys_Printf("Error texture has a zero width, height or bbp: %s\n", this->identifier);
	}

	if (copy) {
		this->data = (unsigned char*)malloc(width * height * bytesPerPixel);

		memcpy(this->data, data, width * height * bytesPerPixel);
	} else {
		this->data = data;
	}
}

bool Texture::hasData() {
	return this->data != NULL;
}
