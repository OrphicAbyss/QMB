// vid_sdl.h -- sdl video driver

#include <GL/GLee.h>
#include <SDL/SDL.h>
#include "quakedef.h"
#include "input.h"

viddef_t vid; // global video state
unsigned d_8to24table[256];

// Much better for high resolution displays
#define    BASEWIDTH    (800)
#define    BASEHEIGHT   (600)

static SDL_Surface *screen = NULL;

// No support for option menus
void (*vid_menudrawfn)(void) = NULL;
void (*vid_menukeyfn)(int key) = NULL;

/*-----------------------------------------------------------------------*/

//int		texture_mode = GL_NEAREST;
int texture_mode = GL_NEAREST_MIPMAP_NEAREST;
//int		texture_mode = GL_NEAREST_MIPMAP_LINEAR;
//int		texture_mode = GL_LINEAR;
//int		texture_mode = GL_LINEAR_MIPMAP_NEAREST;
//int		texture_mode = GL_LINEAR_MIPMAP_LINEAR;

//qmb :extra stuff
bool gl_combine = false;
bool gl_point_sprite = false;
bool gl_sgis_mipmap = false;
bool gl_texture_non_power_of_two = false;
bool gl_n_patches = false;
bool gl_stencil = true;
bool gl_shader = false;
bool gl_filtering_anisotropic = false;
float gl_maximumAnisotropy;
//====================================

const char *gl_vendor;
const char *gl_renderer;
const char *gl_version;
const char *gl_extensions;

void CheckMultiTextureExtensions(void) {
	if (strstr(gl_extensions, "GL_ARB_multitexture ")) {
		glGetIntegerv(GL_MAX_TEXTURE_UNITS_ARB, &gl_textureunits);
		Con_Printf("&c840Multitexture extensions found&r.\n");
		Con_Printf("&c840Found %i texture unit support&r.\n", gl_textureunits);
		if (gl_textureunits < 4)
			Sys_Error("QMB only supports a minimum of 4 texture units.");
	}
}

void CheckCombineExtension(void) {
	if (strstr(gl_extensions, "GL_ARB_texture_env_combine ")) {
		Con_Printf("&c840Combine extension found&r.\n");
		gl_combine = true;
	}
	if (strstr(gl_extensions, "GL_NV_texture_shader ")) {
		Con_Printf("&c840Nvidia texture shader extension found&r.\n");
		gl_shader = true;
	}
	if (strstr(gl_extensions, "GL_NV_point_sprite ")) {
		Con_Printf("&c840Nvidia point sprite extension found&r.\n");
		gl_point_sprite = true;
	}
	if (strstr(gl_extensions, "GL_SGIS_generate_mipmap ")) {
		Con_Printf("&c840SGIS generate mipmap extension found&r.\n");
		gl_sgis_mipmap = true;
	}
	if (strstr(gl_extensions, "GL_ARB_texture_non_power_of_two ")) {
		Con_Printf("&c840ARB texture non power of two extension found&r.\n");
		gl_texture_non_power_of_two = true;
	}

	if (GLEE_EXT_texture_filter_anisotropic) {
		//glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &gl_maximumAnisotropy);
		Con_Printf("&c840ARB anisotropic extension found&r.\n");
		gl_filtering_anisotropic = true;
	}

	if (GLEE_ATI_pn_triangles) {
		Con_Printf("&c840TruForm (n-patches) extension found&r.\n");
		gl_n_patches = true;
	}
}

/**
 * Checks for a GL error and prints the error to the console
 */
void checkGLError(const char *text) {
	const char *errorStr;

	do {
		errorStr = GLeeGetErrorString();
		if (strcmp(errorStr, "") == 0)
			break;

		Con_DPrintf("&c900Error:&c0092D&r %s %s\n", text, errorStr);
	} while (1);
}

void GL_Init(void) {
	extern char *ENGINE_EXTENSIONS;

	gl_vendor = (char *) glGetString(GL_VENDOR);
	Con_Printf("GL_VENDOR: %s\n", gl_vendor);
	gl_renderer = (char *) glGetString(GL_RENDERER);
	Con_Printf("GL_RENDERER: %s\n", gl_renderer);

	gl_version = (char *) glGetString(GL_VERSION);
	Con_Printf("GL_VERSION: %s\n", gl_version);
	gl_extensions = (char *) glGetString(GL_EXTENSIONS);
	//Con_Printf ("GL_EXTENSIONS: %s\n", gl_extensions);

	CheckMultiTextureExtensions();
	CheckCombineExtension();

	// LordHavoc: report supported extensions
	Con_Printf("\n&c05cEngine extensions:&r &c505 %s &r\n", ENGINE_EXTENSIONS);

	glClearColor(0, 0, 0, 0);
	glCullFace(GL_FRONT);
	glEnable(GL_TEXTURE_2D);

	glEnable(GL_ALPHA_TEST);
	glAlphaFunc(GL_GREATER, 0.666f);

	//glPolygonMode (GL_FRONT_AND_BACK, GL_FILL);
	glPolygonMode(GL_FRONT, GL_FILL);

	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
}

void GL_BeginRendering(int *x, int *y, int *width, int *height) {
	*x = 0;
	*y = 0;
	*width = vid.width;
	*height = vid.height;
}

void GL_EndRendering(void) {
	SDL_GL_SwapBuffers();
}

void VID_SetPalette(unsigned char *palette) {
	unsigned r, g, b;
	unsigned *table;

	// Save palette table for lookups
	table = d_8to24table;

	for (int i = 0; i < 256; ++i) {
		r = *palette++;
		g = *palette++;
		b = *palette++;

		*table++ = (255 << 24) + (r << 0) + (g << 8) + (b << 16);
	}
	d_8to24table[255] &= 0x00000000; // 255 is transparent
}

void VID_ShiftPalette(unsigned char *palette) {
	VID_SetPalette(palette);
}

void VID_Init(unsigned char *palette) {
	int pnum;
	Uint32 flags;
	char gldir[MAX_OSPATH];

	// Load the SDL library
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_CDROM) < 0)
		Sys_Error("VID: Couldn't load SDL: %s", SDL_GetError());

	// Set up display mode (width and height)
	vid.width = BASEWIDTH;
	vid.height = BASEHEIGHT;
	if ((pnum = COM_CheckParm("-winsize"))) {
		if (pnum >= com_argc - 2)
			Sys_Error("VID: -winsize <width> <height>\n");
		vid.width = atoi(com_argv[pnum + 1]);
		vid.height = atoi(com_argv[pnum + 2]);
		if (!vid.width || !vid.height)
			Sys_Error("VID: Bad window width/height\n");
	}

	if ((pnum = COM_CheckParm("-width")) != 0)
		vid.width = atoi(com_argv[pnum + 1]);
	if ((pnum = COM_CheckParm("-height")) != 0)
		vid.height = atoi(com_argv[pnum + 1]);

	// Set video width, height and flags
	flags = (SDL_OPENGL);
	if (COM_CheckParm("-fullscreen"))
		flags |= SDL_FULLSCREEN;

	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

	// Initialise display
	if (!(screen = SDL_SetVideoMode(vid.width, vid.height, 32, flags)))
		Sys_Error("VID: Couldn't set video mode: %s\n", SDL_GetError());

	VID_SetPalette(palette);
	SDL_WM_SetCaption("Quake: QMB", "Quake: QMB");
	// now know everything we need to know about the buffer
	vid.aspect = ((float) vid.height / (float) vid.width);
	vid.conwidth = 640;
	vid.conheight = 480 * vid.aspect;
	vid.numpages = 2;

	vid.colormap = host_colormap;

	// Initialise the mouse
	IN_HideMouse();

	GL_Init();

	sprintf(gldir, "%s/glquake", com_gamedir);
	Sys_mkdir(gldir);
}

void VID_Shutdown(void) {
	SDL_Quit();
}

void Sys_SendKeyEvents(void) {
	SDL_Event event;
	int sym, state;
	int modstate;

	while (SDL_PollEvent(&event)) {
		switch (event.type) {

			case SDL_KEYDOWN:
			case SDL_KEYUP:
				sym = event.key.keysym.sym;
				state = event.key.state;
				modstate = SDL_GetModState();
				switch (sym) {
						//case SDLK_BACKQUOTE: sym = '`'; break;
					case SDLK_DELETE: sym = K_DEL;
						break;
					case SDLK_BACKSPACE: sym = K_BACKSPACE;
						break;
					case SDLK_F1: sym = K_F1;
						break;
					case SDLK_F2: sym = K_F2;
						break;
					case SDLK_F3: sym = K_F3;
						break;
					case SDLK_F4: sym = K_F4;
						break;
					case SDLK_F5: sym = K_F5;
						break;
					case SDLK_F6: sym = K_F6;
						break;
					case SDLK_F7: sym = K_F7;
						break;
					case SDLK_F8: sym = K_F8;
						break;
					case SDLK_F9: sym = K_F9;
						break;
					case SDLK_F10: sym = K_F10;
						break;
					case SDLK_F11: sym = K_F11;
						break;
					case SDLK_F12: sym = K_F12;
						break;
					case SDLK_BREAK:
					case SDLK_PAUSE: sym = K_PAUSE;
						break;
					case SDLK_UP: sym = K_UPARROW;
						break;
					case SDLK_DOWN: sym = K_DOWNARROW;
						break;
					case SDLK_RIGHT: sym = K_RIGHTARROW;
						break;
					case SDLK_LEFT: sym = K_LEFTARROW;
						break;
					case SDLK_INSERT: sym = K_INS;
						break;
					case SDLK_HOME: sym = K_HOME;
						break;
					case SDLK_END: sym = K_END;
						break;
					case SDLK_PAGEUP: sym = K_PGUP;
						break;
					case SDLK_PAGEDOWN: sym = K_PGDN;
						break;
					case SDLK_RSHIFT:
					case SDLK_LSHIFT: sym = K_SHIFT;
						break;
					case SDLK_RCTRL:
					case SDLK_LCTRL: sym = K_CTRL;
						break;
					case SDLK_RALT:
					case SDLK_LALT: sym = K_ALT;
						break;
					case SDLK_KP0:
						if (modstate & KMOD_NUM) sym = K_INS;
						else sym = SDLK_0;
						break;
					case SDLK_KP1:
						if (modstate & KMOD_NUM) sym = K_END;
						else sym = SDLK_1;
						break;
					case SDLK_KP2:
						if (modstate & KMOD_NUM) sym = K_DOWNARROW;
						else sym = SDLK_2;
						break;
					case SDLK_KP3:
						if (modstate & KMOD_NUM) sym = K_PGDN;
						else sym = SDLK_3;
						break;
					case SDLK_KP4:
						if (modstate & KMOD_NUM) sym = K_LEFTARROW;
						else sym = SDLK_4;
						break;
					case SDLK_KP5: sym = SDLK_5;
						break;
					case SDLK_KP6:
						if (modstate & KMOD_NUM) sym = K_RIGHTARROW;
						else sym = SDLK_6;
						break;
					case SDLK_KP7:
						if (modstate & KMOD_NUM) sym = K_HOME;
						else sym = SDLK_7;
						break;
					case SDLK_KP8:
						if (modstate & KMOD_NUM) sym = K_UPARROW;
						else sym = SDLK_8;
						break;
					case SDLK_KP9:
						if (modstate & KMOD_NUM) sym = K_PGUP;
						else sym = SDLK_9;
						break;
					case SDLK_KP_PERIOD:
						if (modstate & KMOD_NUM) sym = K_DEL;
						else sym = SDLK_PERIOD;
						break;
					case SDLK_KP_DIVIDE: sym = SDLK_SLASH;
						break;
					case SDLK_KP_MULTIPLY: sym = SDLK_ASTERISK;
						break;
					case SDLK_KP_MINUS: sym = SDLK_MINUS;
						break;
					case SDLK_KP_PLUS: sym = SDLK_PLUS;
						break;
					case SDLK_KP_ENTER: sym = SDLK_RETURN;
						break;
					case SDLK_KP_EQUALS: sym = SDLK_EQUALS;
						break;
				}
				// If we're not directly handled and still above 255
				// just force it to 0
				if (sym > 255) sym = 0;
				Key_Event(sym, state);
				break;

			case SDL_MOUSEMOTION:
				IN_MouseEvent(event);
				break;

			case SDL_MOUSEBUTTONDOWN:
			case SDL_MOUSEBUTTONUP:
				{
					Uint8 button = event.button.button;
					Uint8 state = event.button.state;

					Key_Event(K_MOUSE1 + button - 1, state);
				}
				break;

			case SDL_QUIT:
				CL_Disconnect();
				Host_ShutdownServer(false);
				Sys_Quit();
				break;
			default:
				break;
		}
	}
}
