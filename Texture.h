#ifndef TEXTURE_H
#define	TEXTURE_H

#include "quakedef.h"

class Texture {
public:	
	Texture(const char *ident);
	virtual ~Texture();
	void calculateHash();
	bool convert8To32Fullbright();
	void convertToGrayscale();
	void convert8To32();
	void convert24To32();
	void fixSize();
	void resample(const int scaledHeight, const int scaledWidth);
	void upload();
	bool isMissMatch(const Texture *other);

	bool operator==(const Texture &other) const;
	bool operator!=(const Texture &other) const;
	
	char *identifier;
	int width;
	int height;
	int bytesPerPixel;
	int dataHash;
	unsigned char *data;
	GLuint textureId;
	GLuint fullbrightTextureId;
	bool mipmap;
	bool grayscale;
};

typedef struct {
	char *name;
	GLuint minimize;
	GLuint maximize;
} glFilterMode;

class TextureManager {
public:
	static GLuint glFilterMax;
	static GLuint glFilterMin;
	static glFilterMode glFilterModes[];
	
	static Texture *LoadTexture(Texture *texture);
	static Texture *LoadFile(char *filename, bool complain);
	static Texture *LoadFilePNG(FILE *f, char *name);
	static Texture *LoadFileJPG(FILE *f, char *name);
	static Texture *LoadFileTGA(FILE *f, char *name);
	static Texture *LoadFilePCX(FILE *f, char *name);

	static void setTextureModeCMD();
	static void resetTextureModes();
	static Texture *findTexture(const char *identifier);
	static Texture *findTexture(Texture *find);
	static void addTexture(Texture *add);
	static void removeTexture(Texture *remove);
};
#endif	/* TEXTURE_H */

