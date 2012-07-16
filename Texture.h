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
	GLuint textureType;
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
	
	static GLuint defaultTexture;
	static GLuint shinetex_glass;
	static GLuint shinetex_chrome;
	static GLuint underwatertexture;
	static GLuint highlighttexture;
	static GLuint celtexture;
	static GLuint vertextexture;
	static GLuint crosshair_tex[32];

	static int LoadExternTexture(const char* filename, bool complain, bool mipmap, bool grayscale);
	static int LoadInternTexture(const char *identifier, int width, int height, byte *data, bool mipmap, bool fullbright, int bytesperpixel, bool grayscale);
	static Texture *LoadTexture(Texture *texture);
	static Texture *LoadFile(const char *filename, bool complain);
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

	static void LoadDefaultTexture();
	static void LoadMiscTextures();
	static void LoadCrosshairTextures();
	static void LoadModelShadingTextures();
	
	static void Init();
	
	static GLuint getTextureId();
	static void delTextureId(GLuint);
};

#endif	/* TEXTURE_H */
