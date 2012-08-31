/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

 */
//code written by Dr Labman (drlabman@flyingsaucepan.com)

//world brush related drawing code
#include "quakedef.h"
#include "Texture.h"

typedef struct glRect_s {
    unsigned char l, t, w, h;
} glRect_t;

//world texture chains
extern msurface_t *skychain;
extern msurface_t *waterchain;
extern msurface_t *extrachain;
extern msurface_t *outlinechain;

#define	BLOCK_WIDTH		128
#define	BLOCK_HEIGHT	128

#define	MAX_LIGHTMAPS	1024

extern bool lightmap_modified[MAX_LIGHTMAPS];
extern glRect_t lightmap_rectchange[MAX_LIGHTMAPS];
extern byte lightmaps[4 * MAX_LIGHTMAPS*BLOCK_WIDTH*BLOCK_HEIGHT];

extern int lightmap_textures[];

void R_RenderDynamicLightmaps(msurface_t *fa);
void R_DrawExtraChains(msurface_t *extrachain);
void Surf_DrawExtraChainsFour(msurface_t *extrachain);

/*
Thoughts about code

  Textures to draw on a surface:
Normal, Lightmap, Detail (semi optional)
  Optional textures:
Caustic, Shiny, Fullbright

  Other surfaces:
Water, Sky

4 TMU's
2 pass system

    first pass:
        Normal, Lightmap, Fullbright, Detail
    second pass:
        caustics 1, caustics 2, Shiny Glass, Shiny Metal

Needs to be replaced with a system to compress passes together
IE: shiny glass and shiny metal currently wont be used together
    so if there is no fullbright or detail is turned off they could be added into that stage

To make changing order easier each texture type should have a enable and disable function
also because caustics need a diffrent texture coord system (ie scrolling) there should be some option for that
 */

__inline void Surf_EnableNormal() {
    //just uses normal modulate
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
}

__inline void Surf_EnableLightmap() {
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_EXT);
    glTexEnvf(GL_TEXTURE_ENV, GL_COMBINE_RGB_EXT, GL_MODULATE);
    glTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE_EXT, 4.0);
}

__inline void Surf_EnableFullbright() {
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_ADD);
}

__inline void Surf_EnableDetail() {
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_EXT);
    glTexEnvf(GL_TEXTURE_ENV, GL_COMBINE_RGB_EXT, GL_MODULATE);
    glTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE_EXT, 2.0);
}

__inline void Surf_EnableShiny() {
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
}

__inline void Surf_EnableExtra() {
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
}

__inline void Surf_EnableCaustic() {
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
}

__inline void Surf_Reset() {
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
}

void Surf_Outline(msurface_t *surfchain) {
    glColor4f(0, 0, 0, 1);

    Surf_EnableNormal();
    glBindTexture(GL_TEXTURE_2D, 0);
    glCullFace(GL_BACK);
    glPolygonMode(GL_FRONT, GL_LINE);

    glLineWidth(5.5f);
    glEnable(GL_LINE_SMOOTH);

    for (msurface_t *s = surfchain; s;) {
        // Outline the polys
        glBegin(GL_POLYGON);
        float *v = s->polys->verts[0];
        for (int k = 0; k < s->polys->numverts; k++, v += VERTEXSIZE) {
            glVertex3fv(v);
        }
        glEnd();

        msurface_t *removelink = s;
        s = s->outline;
        removelink->outline = NULL;
    }

    glColor4f(1, 1, 1, 1);
    glCullFace(GL_FRONT);
    glPolygonMode(GL_FRONT, GL_FILL);

    Surf_Reset();
}

extern void UpdateLightmap(int texNum);

void PreUpdateLightmaps(model_t *model) {
    // Ok now we are ready to draw the texture chains
    glActiveTexture(GL_TEXTURE0_ARB);
    for (int i = 0; i < model->numtextures; i++) {
        // if there's no chain go to next.
        // saves binding the textures if they aren't used.
        if (!model->textures[i] || !model->textures[i]->texturechain)
            continue;

        for (msurface_t *s = model->textures[i]->texturechain; s; s = s->texturechain) {
            // Update lightmap now
            R_RenderDynamicLightmaps(s);

            glBindTexture(GL_TEXTURE_2D, lightmap_textures[s->lightmaptexturenum]);
            // Upload changes
            UpdateLightmap(s->lightmaptexturenum);
        }
    }
}

void Surf_DrawTextureChainsFour(model_t *model) {
    int detail;
    extern int detailtexture;
    float *v;
    texture_t *t;

    //Draw the sky chains first so they can have depth on...
    //draw the sky
    R_DrawSkyChain(skychain);
    skychain = NULL;

    //always a normal texture, so enable tmu
    glActiveTexture(GL_TEXTURE0_ARB);
    glEnable(GL_TEXTURE_2D);
    Surf_EnableNormal();

    if (!r_fullbright.getBool()) {
        //always a lightmap, so enable tmu
        glActiveTexture(GL_TEXTURE1_ARB);
        glEnable(GL_TEXTURE_2D);
        Surf_EnableLightmap();
    }

    //could be a fullbright, just setup for them
    glActiveTexture(GL_TEXTURE2_ARB);
    Surf_EnableFullbright();

    if (gl_detail.getBool()) {
        GL_EnableTMU(GL_TEXTURE3_ARB);
        glBindTexture(GL_TEXTURE_2D, detailtexture);
        Surf_EnableDetail();
        detail = true;
    } else {
        detail = false;
    }

    PreUpdateLightmaps(model);

    //ok now we are ready to draw the texture chains
    for (int i = 0; i < model->numtextures; i++) {
        //if theres no chain go to next.
        //saves binding the textures if they arnt used.
        if (!model->textures[i] || !model->textures[i]->texturechain)
            continue;

        //work out what texture we need if its animating texture
        t = R_TextureAnimation(model->textures[i]->texturechain->texinfo->texture);
        // Binds world to texture unit 0
        glActiveTexture(GL_TEXTURE0_ARB);
        glBindTexture(GL_TEXTURE_2D, t->gl_texturenum);

        if (t->gl_fullbright != 0) {
            //if there is a fullbright texture then bind it to TMU2
            glActiveTexture(GL_TEXTURE2_ARB);
            glEnable(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, t->gl_fullbright);
        }

        glActiveTexture(GL_TEXTURE1_ARB);
        for (msurface_t *s = model->textures[i]->texturechain; s;) {
            // Select the right lightmap
            glBindTexture(GL_TEXTURE_2D, lightmap_textures[s->lightmaptexturenum]);

            // Draw the polys
            glBegin(GL_POLYGON);
            v = s->polys->verts[0];
            for (int k = 0; k < s->polys->numverts; k++, v += VERTEXSIZE) {
                glMultiTexCoord2f(GL_TEXTURE0_ARB, v[3], v[4]);
                glMultiTexCoord2f(GL_TEXTURE1_ARB, v[5], v[6]);
                if (t->gl_fullbright)
                    glMultiTexCoord2f(GL_TEXTURE2_ARB, v[3], v[4]);
                if (detail)
                    glMultiTexCoord2f(GL_TEXTURE3_ARB, v[7]*18, v[8]*18);

                glVertex3fv(v);
            }
            glEnd();

            msurface_t *removelink = s;
            s = s->texturechain;
            removelink->texturechain = NULL;
        }

        if (t->gl_fullbright != 0) {
            //if there was a fullbright disable the TMU now
            GL_DisableTMU(GL_TEXTURE2_ARB);
        }

        model->textures[i]->texturechain = NULL;
    }

    // Disable detail texture
    if (detail) {
        GL_DisableTMU(GL_TEXTURE3_ARB);
        Surf_Reset();
    }

    // Disable fullbright texture
    GL_DisableTMU(GL_TEXTURE2_ARB);
    Surf_Reset();

    // Disable lightmaps
    GL_DisableTMU(GL_TEXTURE1_ARB);
    Surf_Reset();

    glActiveTexture(GL_TEXTURE0_ARB);
    Surf_Reset();

    if (r_outline.getBool()) {
        Surf_Outline(outlinechain);
        outlinechain = NULL;
    }

    //draw the extra polys (caustics, metal, glass)
    Surf_DrawExtraChainsFour(extrachain);
    extrachain = NULL;

    //draw the water
    R_DrawWaterChain(waterchain);
    waterchain = NULL;
}

void Surf_DrawExtraChainsFour(msurface_t *extrachain) {
    bool caustic = false, shiny_metal = false, shiny_glass = false;

    //the first water caustic
    GL_DisableTMU(GL_TEXTURE0_ARB);
    Surf_EnableCaustic();
    glBindTexture(GL_TEXTURE_2D, TextureManager::underwatertexture);

    //the second water caustic
    glActiveTexture(GL_TEXTURE1_ARB);
    Surf_EnableCaustic();
    glBindTexture(GL_TEXTURE_2D, TextureManager::underwatertexture);

    //the glass shiny texture
    glActiveTexture(GL_TEXTURE2_ARB);
    Surf_EnableShiny();
    glBindTexture(GL_TEXTURE_2D, TextureManager::shinetex_glass);

    //the metal shiny texture
    glActiveTexture(GL_TEXTURE3_ARB);
    Surf_EnableShiny();
    glBindTexture(GL_TEXTURE_2D, TextureManager::shinetex_chrome);

    glBlendFunc(GL_DST_COLOR, GL_SRC_COLOR);
    glEnable(GL_BLEND);
    glColor4f(1, 1, 1, 1);

    for (msurface_t *surf = extrachain; surf;) {
        if (!caustic && surf->flags & SURF_UNDERWATER && gl_caustics.getBool()) {
            GL_EnableTMU(GL_TEXTURE0_ARB);
            GL_EnableTMU(GL_TEXTURE1_ARB);

            caustic = true;
        }

        if (!shiny_glass && surf->flags & SURF_SHINY_GLASS && gl_shiny.getBool()) {
            GL_EnableTMU(GL_TEXTURE2_ARB);

            glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_SPHERE_MAP);
            glTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_SPHERE_MAP);
            glEnable(GL_TEXTURE_GEN_S);
            glEnable(GL_TEXTURE_GEN_T);

            shiny_glass = true;
        }

        if (!shiny_metal && surf->flags & SURF_SHINY_METAL && gl_shiny.getBool()) {
            GL_EnableTMU(GL_TEXTURE3_ARB);

            glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_SPHERE_MAP);
            glTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_SPHERE_MAP);
            glEnable(GL_TEXTURE_GEN_S);
            glEnable(GL_TEXTURE_GEN_T);

            shiny_metal = true;
        }

        glBegin(GL_POLYGON);
        glNormal3fv(surf->plane->normal);
        float *v = surf->polys->verts[0];
        for (int k = 0; k < surf->polys->numverts; k++, v += VERTEXSIZE) {
            if (caustic) {
                //work out tex coords
                float os = v[7];
                float ot = v[8];

                float s = os / 4 + (realtime * 0.05);
                float t = ot / 4 + (realtime * 0.05);
                float ss = os / 5 - (realtime * 0.05);
                float tt = ot / 5 + (realtime * 0.05);

                glMultiTexCoord2f(GL_TEXTURE0_ARB, s, t);
                glMultiTexCoord2f(GL_TEXTURE1_ARB, ss, tt);
            }

            if (shiny_glass)
                glMultiTexCoord2f(GL_TEXTURE2_ARB, v[7], v[8]);
            if (shiny_metal)
                glMultiTexCoord2f(GL_TEXTURE3_ARB, v[7], v[8]);

            glVertex3fv(v);
        }
        glEnd();

        if (surf->extra) {
            if (!surf->extra->flags & SURF_UNDERWATER && caustic && gl_caustics.getBool()) {
                GL_DisableTMU(GL_TEXTURE0_ARB);
                GL_DisableTMU(GL_TEXTURE1_ARB);

                caustic = false;
            }

            if (!(surf->extra->flags & SURF_SHINY_GLASS) && shiny_glass && gl_shiny.getBool()) {
                GL_DisableTMU(GL_TEXTURE2_ARB);

                glDisable(GL_TEXTURE_GEN_S);
                glDisable(GL_TEXTURE_GEN_T);

                shiny_glass = false;
            }

            if (!surf->extra->flags & SURF_SHINY_METAL && shiny_metal && gl_shiny.getBool()) {
                GL_DisableTMU(GL_TEXTURE3_ARB);

                glDisable(GL_TEXTURE_GEN_S);
                glDisable(GL_TEXTURE_GEN_T);

                shiny_metal = false;
            }
        }

        msurface_t *removelink = surf;
        surf = surf->extra;
        removelink->extra = NULL;
    }

    // Disable shiny metal
    GL_DisableTMU(GL_TEXTURE3_ARB);
    glDisable(GL_TEXTURE_GEN_S);
    glDisable(GL_TEXTURE_GEN_T);
    Surf_Reset();

    // Disable shiny glass
    GL_DisableTMU(GL_TEXTURE2_ARB);
    glDisable(GL_TEXTURE_GEN_S);
    glDisable(GL_TEXTURE_GEN_T);
    Surf_Reset();

    // Disable caustic
    GL_DisableTMU(GL_TEXTURE1_ARB);
    Surf_Reset();

    // Disable caustic
    glActiveTexture(GL_TEXTURE0_ARB);
    Surf_Reset();

    GL_EnableTMU(GL_TEXTURE0_ARB);
    glDisable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(1, 1, 1, 1);

    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
}

