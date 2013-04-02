#include "Texture.h"
#include <list>

static std::list<Texture *> Textures;

GLuint TextureManager::glFilterMin = GL_LINEAR_MIPMAP_LINEAR;
GLuint TextureManager::glFilterMax = GL_LINEAR;
glFilterMode TextureManager::glFilterModes[] = {
	{"GL_NEAREST", GL_NEAREST, GL_NEAREST},
	{"GL_LINEAR", GL_LINEAR, GL_LINEAR},
	{"GL_NEAREST_MIPMAP_NEAREST", GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST},
	{"GL_NEAREST_MIPMAP_LINEAR", GL_NEAREST_MIPMAP_LINEAR, GL_NEAREST},
	{"GL_LINEAR_MIPMAP_NEAREST", GL_LINEAR_MIPMAP_NEAREST, GL_LINEAR},
	{"GL_LINEAR_MIPMAP_LINEAR", GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR}
};

GLuint TextureManager::defaultTexture = 0;
GLuint TextureManager::shinetex_glass = 0;
GLuint TextureManager::shinetex_chrome = 0;
GLuint TextureManager::underwatertexture = 0;
GLuint TextureManager::highlighttexture = 0;
GLuint TextureManager::celtexture = 0;
GLuint TextureManager::vertextexture = 0;
GLuint TextureManager::crosshair_tex[32];

void TextureManager::setTextureModeCMD() {
		int i;

	if (CmdArgs::getArgCount() == 1) {
		for (int i = 0; i < 6; i++) {
			if (glFilterMin == glFilterModes[i].minimize) {
				Con_Printf("%s\n", glFilterModes[i].name);
				return;
			}
		}
		Con_Printf("current filter is unknown???\n");
	} else {
		int i=0;
		for ( ; i < 6; i++) {
			if (!Q_strcasecmp(glFilterModes[i].name, CmdArgs::getArg(1)))
				break;
		}

		if (i == 6) {
			Con_Printf("bad filter name\n");
			return;
		}

		glFilterMin = glFilterModes[i].minimize;
		glFilterMax = glFilterModes[i].maximize;

		// change all the existing mipmap texture objects
		resetTextureModes();
	}
}

void TextureManager::resetTextureModes() {
	std::list<Texture *>::iterator i;

	for (i = Textures.begin(); i != Textures.end(); i++) {
		Texture *t = *i;

		if (t->mipmap) {
			glBindTexture(GL_TEXTURE_2D, t->textureId);
			if (t->mipmap)
				glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, glFilterMin);
			else
				glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, glFilterMax);

			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, glFilterMax);
		}
	}
}

int TextureManager::LoadExternTexture(const char* filename, bool complain, bool mipmap, bool grayscale) {
	Texture *t = NULL;

	if (gl_quick_texture_reload.getBool()) {
		t = findTexture(filename);
		if (t != NULL)
			return t->textureId;
	}


	t = LoadFile(filename, complain);
	if (!t)
		return 0;

	if (!t->hasData()) {
		delete t;
		return 0;
	}

	t->mipmap = mipmap;
	t->bytesPerPixel = 4;
	t->grayscale = grayscale;

	t = LoadTexture(t);
	return t->textureId;
}

int TextureManager::LoadInternTexture(const char *identifier, int width, int height, byte *data, bool mipmap, bool fullbright, int bytesperpixel, bool grayscale) {
	Texture *t = new Texture(identifier);
	t->height = height;
	t->width = width;
	t->bytesPerPixel = bytesperpixel;
	t->setData(data, true);
	t->mipmap = mipmap;
	t->grayscale = grayscale;

	if (fullbright)
		if (!t->convert8To32Fullbright()) {
			delete t;
			return 0;
		}

	t = LoadTexture(t);

	return t->textureId;
}

Texture *TextureManager::LoadTexture(Texture *texture) {
	// See if the texture has already been loaded
	texture->calculateHash();
	Texture *found = findTexture(texture);
	if (found != NULL) {
		if (found->isMissMatch(texture)) {
			Con_DPrintf("GL_LoadTexture: cache mismatch\n");
			removeTexture(found);
			delete found;
		} else {
			delete texture;
			return found;
		}
	}

	if (!isDedicated) {
		texture->upload();
	}

	checkGLError("Loading texture");

	addTexture(texture);
	return texture;
}

Texture *TextureManager::findTexture(const char *identifier) {
	std::list<Texture *>::iterator i;

	for (i = Textures.begin(); i != Textures.end(); i++) {
		Texture *tex = *i;

		if (tex->identifier != NULL && 0 == Q_strcmp(tex->identifier, identifier))
			return tex;
	}

	return NULL;
}

Texture *TextureManager::findTexture(Texture *find) {
	std::list<Texture *>::iterator i;

	if (find->identifier == NULL) {
		Con_Printf("Error finding texture\n");
	}

	for (i = Textures.begin(); i != Textures.end(); i++) {
		Texture *tex = *i;

		if (find == tex)
			return tex;
	}

	return NULL;
}

void TextureManager::addTexture(Texture *add) {
	Textures.push_back(add);
}

void TextureManager::removeTexture(Texture *remove) {
	Textures.remove(remove);
}

void TextureManager::clearAllTextures() {
	std::list<Texture *>::iterator i;

	for (i = Textures.begin(); i != Textures.end(); ) {
		Texture *tex = *i;

		delete(tex);
		i = Textures.erase(i);
	}
}

void TextureManager::delTextureId(GLuint textureId) {
	glDeleteTextures(1, &textureId);
}

GLuint TextureManager::getTextureId() {
	GLuint textureId = 0;
	glGenTextures(1, &textureId);
	return textureId;
}

void TextureManager::LoadDefaultTexture() {
	int width = 32;
	int height = 32;
	unsigned char *data = (unsigned char *)malloc(sizeof(unsigned char) * width * height * 4);
	unsigned cur = 0;

	for (int i=0; i<32; i++) {
		for (int j=0; j<32; j++) {
			bool x = i < 16;
			bool y = j < 16;

			if ((x && y) || (!x && !y)) {
				data[cur++] = 255;
				data[cur++] = 255;
				data[cur++] = 255;
				data[cur++] = 255;
			} else {
				data[cur++] = 0;
				data[cur++] = 0;
				data[cur++] = 0;
				data[cur++] = 255;
			}
		}
	}

	//default texture
	Texture *defTex = new Texture("defaultTexture");
	defTex->setData(data);
	defTex->height = height;
	defTex->width = width;
	defTex->bytesPerPixel = 4;
	LoadTexture(defTex);
	defaultTexture = defTex->textureId;

	// hook into map default texture
	extern texture_t *r_notexture_mip;
	r_notexture_mip->gl_texturenum = defaultTexture;
}

void TextureManager::LoadMiscTextures() {
	shinetex_glass = LoadExternTexture("textures/shine_glass", false, true, false);
	shinetex_chrome = LoadExternTexture("textures/shine_chrome", false, true, false);
	underwatertexture = LoadExternTexture("textures/water_caustic", false, true, false);
	highlighttexture = LoadExternTexture("gfx/highlight", false, true, false);
}

void TextureManager::LoadCrosshairTextures() {
	char crosshairFname[32];

	for (int i = 0; i < 32; i++) {
		snprintf(&crosshairFname[0],32,"textures/crosshairs/crosshair%02i.tga",i);
		crosshair_tex[i] = LoadExternTexture((char *)&crosshairFname[0], false, false, false);
	}
}

void TextureManager::LoadModelShadingTextures() {
	unsigned char cellData[32] = {55, 55, 55, 55, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255};
	unsigned char *cellFull = (unsigned char *)malloc(sizeof(unsigned char) * 32 * 3);
	unsigned char *vertexFull = (unsigned char *)malloc(sizeof(unsigned char) * 32 * 3);

	for (int i = 0; i < 32; i++) {
		cellFull[i * 3 + 0] = cellFull[i * 3 + 1] = cellFull[i * 3 + 2] = cellData[i];
		vertexFull[i * 3 + 0] = vertexFull[i * 3 + 1] = vertexFull[i * 3 + 2] = (unsigned char)((i / 32.0f) * 255);
	}

	//cell shading stuff...
	Texture *cell = new Texture("celTexture");
	cell->height = 1;
	cell->width = 32;
	cell->bytesPerPixel = 3;
	cell->setData(cellFull);
	cell->mipmap = false;
	cell->textureType = GL_TEXTURE_1D;
	LoadTexture(cell);
	celtexture = cell->textureId;

	//vertex shading stuff...
	Texture *vertex = new Texture("vertexTexture");
	vertex->height = 1;
	vertex->width = 32;
	vertex->bytesPerPixel = 3;
	vertex->setData(vertexFull);
	vertex->mipmap = false;
	vertex->textureType = GL_TEXTURE_1D;
	LoadTexture(vertex);
	vertextexture = vertex->textureId;
}

void TextureManager::Init() {
	LoadDefaultTexture();
	LoadMiscTextures();
	LoadCrosshairTextures();
	LoadModelShadingTextures();
}
