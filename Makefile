# Makefile for QMB

CPP  = g++
CC   = gcc
WINDRES = windres.exe
RES  = WinQuake_private.res
OBJ  = nn_main.o cl_demo.o cl_input.o cl_main.o cl_parse.o cl_tent.o sv_main.o sv_move.o sv_phys.o sv_user.o net_dgrm.o net_loop.o net_main.o net_win.o net_wins.o net_wipx.o host.o host_cmd.o bot.o bot_misc.o bot_setup.o pr_cmds.o pr_edict.o pr_exec.o gl_draw.o gl_hud.o gl_jpg.o gl_md3.o gl_mesh.o gl_model.o gl_pcx.o gl_png.o gl_noise.o renderque.o gl_hc_texes.o gl_refrag.o gl_rlight.o gl_rmain.o gl_rmisc.o gl_rpart_veng.o gl_rsurf.o gl_renderworld.o gl_screen.o gl_sprite.o gl_tga.o gl_vidnt.o gl_warp.o cd_win.o snd_dma.o snd_mem.o snd_mix.o snd_win.o chase.o cmd.o common.o conproc.o console.o crc.o cvar.o in_win.o keys.o mathlib.o menu.o sbar.o sys_win.o view.o wad.o world.o zone.o CaptureAvi.o CaptureHelpers.o $(RES)
LINKOBJ  = nn_main.o cl_demo.o cl_input.o cl_main.o cl_parse.o cl_tent.o sv_main.o sv_move.o sv_phys.o sv_user.o net_dgrm.o net_loop.o net_main.o net_win.o net_wins.o net_wipx.o host.o host_cmd.o bot.o bot_misc.o bot_setup.o pr_cmds.o pr_edict.o pr_exec.o gl_draw.o gl_hud.o gl_jpg.o gl_md3.o gl_mesh.o gl_model.o gl_pcx.o gl_png.o gl_noise.o renderque.o gl_hc_texes.o gl_refrag.o gl_rlight.o gl_rmain.o gl_rmisc.o gl_rpart_veng.o gl_rsurf.o gl_renderworld.o gl_screen.o gl_sprite.o gl_tga.o gl_vidnt.o gl_warp.o cd_win.o snd_dma.o snd_mem.o snd_mix.o snd_win.o chase.o cmd.o common.o conproc.o console.o crc.o cvar.o in_win.o keys.o mathlib.o menu.o sbar.o sys_win.o view.o wad.o world.o zone.o CaptureAvi.o CaptureHelpers.o $(RES)
LIBS =  -L"C:/msys/1.0/local/lib" -L"C:/MinGW/lib" -L"./dxsdk/sdk/lib/" -mwindows -luser32 -lgdi32 -lopengl32 -lglu32 -lwsock32 -lwinmm -lcomctl32 -ldxguid -ljpeg -llibpng
INCS =  -I"C:/msys/1.0/local/include" -I"C:/MinGW/include"  -I"./dxsdk/sdk/inc" 
CXXINCS =  -I"C:/msys/1.0/local/include" -I"C:/MinGW/include"  -I"./dxsdk/sdk/inc" 
OUTPUT_FOLDER = ./Quake/
BIN  = QMB.exe
CXXFLAGS =  $(CXXINCS)-D__GNUWIN32__ -W -DWIN32 -DNDEBUG -D_WINDOWS -DGLQUAKE 
CFLAGS =  $(INCS)-D__GNUWIN32__ -W -DWIN32 -DNDEBUG -D_WINDOWS -DGLQUAKE 

.PHONY: all all-before all-after clean clean-custom

all: all-before $(BIN) all-after

clean: clean-custom
	rm -f $(OBJ) $(BIN)

$(BIN): $(OBJ)
	$(CPP) -pg $(LINKOBJ) -o "$(BIN)" $(LIBS)

all-after: $(BIN)
	cp $(BIN) $(OUTPUT_FOLDER)

%.o : %.c
	$(CC) -pg -c $(CFLAGS) -o $@ $<

%.o : %.cpp
	$(CPP) -pg -c $(CXXFLAGS) -o $@ $<

WinQuake_private.res: WinQuake_private.rc winquake.rc 
	$(WINDRES) -i WinQuake_private.rc -I rc -o WinQuake_private.res -O coff 
