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
	
	addTexture(texture);
	return texture;
}

Texture *TextureManager::findTexture(const char *identifier) {
	std::list<Texture *>::iterator i;
	
	for (i = Textures.begin(); i != Textures.end(); i++) {
		Texture *tex = *i;
		
		if (0 == Q_strcmp(tex->identifier, identifier))
			return tex;
	}
	
	return NULL;
}

Texture *TextureManager::findTexture(Texture *find) {
	std::list<Texture *>::iterator i;
	
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

void TextureManager::delTextureId(GLuint textureId) {
	glDeleteTextures(1, &textureId);
}

GLuint TextureManager::getTextureId() {
	GLuint textureId = 0;
	glGenTextures(1, &textureId);
	return textureId;
}

void TextureManager::LoadMiscTextures() {
	
}