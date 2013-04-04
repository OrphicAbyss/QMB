#ifndef MODEL_H
#define	MODEL_H

#include "quakedef.h"

class AliasShader {
public:
	char	name[64];
	int		index;		//not used
	int		texnum;
};

class AliasSurfaceFrame {
public:
	vec3_t *vertices;
	vec3_t *normals;
};

class AliasTextureCoord {
public:
	float s;
	float t;
};

class AliasSurface {
public:
	char	name[64];
	int		flags;		//not used yet
	int		numFrames;
	int		numShaders;
	int		numVerts;
	int		numIndices;
	AliasShader *shaders;
	vec3_t *indices;
	AliasSurfaceFrame *vertexValues;
	AliasTextureCoord *texCoords;
};

class AliasTag {
public:
    char	name[64];
    vec3_t	position;
    vec3_t	rotate[3];
};

class AliasFrame {
public:
    vec3_t		mins;
	vec3_t		maxs;
    vec3_t		position;
    float		radius;
    char		name[16];
};

class AliasHeader {
public:
	char	filename[64];
	int		flags;			//not used (will be for particle trails etc)
	int		numFrames;
	int		numTags;
	int		numSurfaces;
	int		numSkins;		//old model format leftovers, not used
	AliasFrame *frames;
	AliasTag *tags;
	AliasSurface *surfaces;
};


typedef enum {
	UNKNOWN, ALIAS, SPRITE, BRUSH
} ModelType;

class Model {
public:
	ModelType type;
};

class AliasModel : public Model {
	AliasHeader *data;

	AliasModel();
	static AliasModel *LoadMD3(void *buffer);
};

#endif	/* MODEL_H */

