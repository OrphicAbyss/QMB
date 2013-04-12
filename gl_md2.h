/* gl_md2.h
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef GL_MD2_H
#define	GL_MD2_H

#include "quakedef.h"

void Mod_LoadQ2AliasModel(model_t *mod, void *buffer);
void R_SetupQ2AliasFrame(entity_t *e, md2_t *pheader);
void GL_DrawQ2AliasFrame(entity_t *e, md2_t *pheader, int lastpose, int pose, float lerp);

#endif	/* GL_MD2_H */

