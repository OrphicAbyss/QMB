#===============================================================================
# Makefile for QMB
#===============================================================================

ifdef WINDIR
# Win32
    TARGET_PLATFORM = windows
    EXTENSION = .exe
else
# UNIXes
    ARCH:=$(shell uname)

ifneq ($(filter %BSD,$(ARCH)),)
    TARGET_PLATFORM=bsd
    $(warning BSD Platform Not Tested)
else
ifeq ($(ARCH), Darwin)
    TARGET_PLATFORM=macosx
    $(warning macosx Platform Not Tested)
else
ifeq ($(ARCH), SunOS)
    TARGET_PLATFORM=sunos
    $(warning sunos Platform Not Tested)
else
    TARGET_PLATFORM=linux
endif  # ifeq ($(ARCH), SunOS)
endif  # ifeq ($(ARCH), Darwin)
endif  # ifneq ($(filter %BSD,$(ARCH)),)
    EXTENSION =
    PLATFORM_ARCH = $(shell uname -m)

ifeq ($(PLATFORM_ARCH),x86_64)
# On a 64bit platform, warning about being a 32bit build.
    $(warning QMB only builds as a 32bit executable, ensure 32bit libs\
     are installed)
endif # ifeq (PLATFORM_ARCH,x86_64)
endif # ifdef windir

#===============================================================================
# Variables to configure
OUTPUT_DIR = ./Quake/
OUTPUT_BIN  = QMB$(EXTENSION)
#===============================================================================

#===============================================================================
# Regular variables
CPP  = g++
CC   = g++
WINDRES = windres
RES  = WinQuake_private.res
OBJ  =	bot.o bot_misc.o bot_setup.o  \
	cl_demo.o cl_input.o cl_main.o cl_parse.o cl_tent.o \
	cmd.o console.o cvar.o host.o host_cmd.o \
	chase.o common.o mathlib.o menu.o sbar.o view.o wad.o world.o zone.o \
	CRC.o FileManager.o \
	in_keys.o in_sdl.o \
	gl_draw.o gl_hud.o \
	gl_md3.o gl_mesh.o gl_model.o gl_sprite.o \
	ModelAlias.o \
	Image.o ImageJPG.o ImagePCX.o ImagePNG.o ImageTGA.o \
	Texture.o TextureManager.o TextureFile.o \
	gl_refrag.o gl_rlight.o gl_rmain.o gl_rmisc.o \
	gl_rpart.o gl_rsurf.o gl_renderworld.o gl_screen.o gl_warp.o \
	net_loop.o net_main.o net_vcr.o net_none.o \
	pr_cmds.o pr_edict.o pr_exec.o nn_main.o \
	cd_sdl.o snd_dma.o snd_mem.o snd_mix.o snd_sdl.o \
	sv_main.o sv_move.o sv_phys.o sv_user.o \
	CaptureAvi.o CaptureHelpers.o \
	sys_sdl.o gl_vid_sdl.o
#net_udp.o net_bsd.o net_dgrm.o

LINKOBJ  = $(OBJ)

ifeq ($(TARGET_PLATFORM),windows)
    LIBS = -L"C:/msys/1.0/local/lib" -L"C:/MinGW/lib" -lGLee -lopengl32 -lglu32 -ljpeg -lpng -lSDL
	#-lGLee
	#-mwindows -luser32 -lgdi32  -lwsock32 -lwinmm -lcomctl32 -ldxguid
    INCS = -I"C:/msys/1.0/local/include" -I"C:/MinGW/include"
else
    LIBS = -lc -lGL -lGLU -ljpeg -lpng -lSDL $(shell if test -e /usr/lib/libGLee.so; then echo -lGLee; else echo -lglee; fi)
    INCS =
endif #ifeq ($(TARGET_PLATFORM),windows)

CXXINCS =  $(INCS)
FLAGS = -W -DSDL -DGLQUAKE -m32
#-DNDEBUG
CXXFLAGS =  $(CXXINCS)
CFLAGS =  $(INCS)
#===============================================================================

#===============================================================================
# TARGETS
.PHONY: all all-before all-after clean clean-custom

all: all-before $(OUTPUT_BIN) all-after

profile: FLAGS += -pg -g -O0
profile: all

debug: FLAGS += -g -O0
debug: all

clean: clean-custom
	rm -f $(OBJ) $(OUTPUT_BIN)

$(OUTPUT_BIN): $(LINKOBJ)
	$(CPP) $(FLAGS) $(LINKOBJ) -o "$(OUTPUT_BIN)" $(LIBS)

all-after: $(OUTPUT_BIN)
	cp $(OUTPUT_BIN) $(OUTPUT_DIR)

all-before: $(OUTPUT_BIN)
	@test -d $(OUTPUT_DIR) || mkdir $(OUTPUT_DIR)

%.o : %.c
	$(CC) -c $(CFLAGS) $(FLAGS) -o $@ $<

%.o : %.cpp
	$(CPP) -c $(CXXFLAGS) $(FLAGS) -o $@ $<

WinQuake_private.res: WinQuake_private.rc winquake.rc
	$(WINDRES) -i WinQuake_private.rc -I rc -o WinQuake_private.res -O coff
#===============================================================================