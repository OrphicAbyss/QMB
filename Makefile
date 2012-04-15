#===============================================================================
# Makefile for QMB
#===============================================================================

#===============================================================================
# Variables to configure
OUTPUT_DIR = ./Quake/
OUTPUT_BIN  = QMB
#===============================================================================

#===============================================================================
# Regular variables
CPP  = g++
CC   = gcc
WINDRES = windres
RES  = WinQuake_private.res
OBJ  =	bot.o bot_misc.o bot_setup.o  \
	cl_demo.o cl_input.o cl_main.o cl_parse.o cl_tent.o \
	cmd.o console.o cvar.o host.o host_cmd.o \
	chase.o common.o crc.o keys.o mathlib.o menu.o sbar.o view.o wad.o world.o zone.o \
	gl_draw.o gl_hud.o gl_jpg.o gl_md3.o gl_mesh.o gl_model.o \
	gl_pcx.o gl_png.o gl_refrag.o gl_rlight.o gl_rmain.o gl_rmisc.o \
	gl_rpart.o gl_rsurf.o gl_renderworld.o gl_screen.o gl_sprite.o \
	gl_tga.o  gl_warp.o \
	net_bsd.o net_dgrm.o net_loop.o net_main.o net_udp.o net_vcr.o \
	pr_cmds.o pr_edict.o pr_exec.o nn_main.o \
	cd_sdl.o snd_dma.o snd_mem.o snd_mix.o snd_sdl.o \
	sv_main.o sv_move.o sv_phys.o sv_user.o \
	CaptureAvi.o CaptureHelpers.o \
	sys_sdl.o gl_vid_sdl.o

LINKOBJ  = $(OBJ)

WINLIBS = -L"C:/msys/1.0/local/lib" -L"C:/MinGW/lib" -L"./dxsdk/sdk/lib/" -mwindows -luser32 -lgdi32 -lopengl32 -lglu32 -lwsock32 -lwinmm -lcomctl32 -ldxguid -ljpeg -llibpng
LIBS =  -L"/usr/local/lib" -L"./dxsdk/sdk/lib/" -lGL -lGLU -ljpeg -lpng -lSDL -lGLee
INCS =  -I"C:/msys/1.0/local/include" -I"C:/MinGW/include"  -I"./dxsdk/sdk/inc"
CXXINCS =  $(INCS)
FLAGS = -pg -g -O0 -DSDL -DGLQUAKE -m32
#-W -DNDEBUG
CXXFLAGS =  $(CXXINCS) $(FLAGS)
CFLAGS =  $(INCS) $(FLAGS)
#===============================================================================

#===============================================================================
# TARGETS
.PHONY: all all-before all-after clean clean-custom

all: all-before $(OUTPUT_BIN) all-after

clean: clean-custom
	rm -f $(OBJ) $(OUTPUT_BIN)

$(OUTPUT_BIN): $(LINKOBJ)
	$(CPP) $(FLAGS) $(LINKOBJ) -o "$(OUTPUT_BIN)" $(LIBS)

all-after: $(OUTPUT_BIN)
	cp $(OUTPUT_BIN) $(OUTPUT_DIR)

all-before: $(OUTPUT_BIN)
	test -d $(OUTPUT_DIR) || mkdir $(OUTPUT_DIR)

%.o : %.c
	$(CC) -c $(CFLAGS) -o $@ $<

%.o : %.cpp
	$(CPP) -c $(CXXFLAGS) -o $@ $<

WinQuake_private.res: WinQuake_private.rc winquake.rc
	$(WINDRES) -i WinQuake_private.rc -I rc -o WinQuake_private.res -O coff
#===============================================================================