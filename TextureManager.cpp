#include "Texture.h"
#include <list>

static std::list<Texture *> Textures;

void TextureManager::resetTextureModes() {
	extern int gl_filter_min, gl_filter_max;
	std::list<Texture *>::iterator i;
	
	for (i = Textures.begin(); i != Textures.end(); i++) {	
		Texture *t = *i;
		
		if (t->mipmap) {
			glBindTexture(GL_TEXTURE_2D, t->textureId);
			if (t->mipmap)
				glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
			else
				glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_max);

			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
		}
	}
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
