/*
Copyright (C) 2002 Quake done Quick

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

#include "quakedef.h"
#include "ICapture.h"
#include "CaptureHelpers.h"

extern CVar host_framerate;
extern float scr_con_current;
extern bool scr_drawloading;
extern short* snd_out;
extern int snd_linear_count;
extern int soundtime;


// Variables for buffering audio
short capture_audio_samples[44100]; // big enough buffer for 1fps at 22050KHz
int captured_audio_samples;
int frames;

// Code in engine is written relatively independent of kind of capture.
// When wired to AVI as currently, capture_codec is a fourcc.
// "0" indicates no compression codec.
CVar capture_codec("capture_codec","0",true);

bool CaptureHelper_IsActive(void)
{
    // don't output whilst console is down or 'loading' is displayed
    if (scr_con_current > 0) return false;
    if (scr_drawloading) return false;

    // otherwise output if a file is open to write to
    return Capture_IsRecording();
}


void CaptureHelper_Start_f (void)
{
    float fps;
    char filename[MAX_QPATH];

	//if aready recording ignore the request...
	if (Capture_IsRecording()) return;

	if (CmdArgs::getArgCount() != 2) {
		Con_Printf ("capture_start <filename>: Start capturing to named file.\n");
		return;
	}

    if (!host_framerate.getBool()) {
        Con_Printf("Set a non-zero host_framerate before starting capture.\n");
        return;
    }
    fps = 1/host_framerate.getFloat();

    Q_strcpy(filename, CmdArgs::getArg(1));
	COM_DefaultExtension (filename, Capture_DOTEXTENSION); // currently we capture AVI

	if (shm){
		Capture_Open(
			filename,
			(*capture_codec.getString() != '0') ? capture_codec.getString() : 0,
			fps,
			shm->speed
		);
	}else{
		Capture_Open(
			filename,
			(*capture_codec.getString() != '0') ? capture_codec.getString() : 0,
			fps,
			0
		);
	}
	frames = 0;
}

void CaptureHelper_Stop_f (void)
{
	//if not recording cant stop and ignore the request...
	if (!Capture_IsRecording()) return;

    Capture_Close();
    Con_Printf("&c900Stopped capturing.\n&c&c090Captured %i frames.&c\n",frames);
}

void CaptureHelper_CaptureDemo_f(void)
{
	if (CmdArgs::getArgCount() != 2) {
		Con_Printf ("capturedemo <demoname> : capture <demoname>.dem then exit\n");
		return;
	}

	Con_Printf ("Capturing %s.dem\n", CmdArgs::getArg(1), CmdArgs::getArg(1));

    if (!host_framerate.getBool())
		host_framerate.set(1/30.0f); // 15fps default

    CL_PlayDemo_f ();
    if (!cls.demoplayback) return;

    CaptureHelper_Start_f();

    cls.capturedemo = true;
	//Con_ToggleConsole_f();
}


void CaptureHelper_Init(void)
{
    captured_audio_samples = 0;

    Cmd::addCmd("capture_start", CaptureHelper_Start_f);
    Cmd::addCmd("capture_stop", CaptureHelper_Stop_f);
    Cmd::addCmd("capturedemo", CaptureHelper_CaptureDemo_f);
	//capture_codec = new CVar("capture_codec", "0", true);
    CVar::registerCVar(&capture_codec);
}


void CaptureHelper_OnStopPlayback(void)
{
    if (!cls.capturedemo) return;

	cls.capturedemo = false;
    CaptureHelper_Stop_f();
	CL_Disconnect();
	Host_ShutdownServer(false);
	Sys_Quit();
}

void CaptureHelper_OnUpdateScreen(void)
{
    int i;
    byte temp;
    byte* buffer;
    int size = glwidth * glheight * 3;

    if (!CaptureHelper_IsActive()) return;

    buffer = (byte  *)malloc(size);
	glReadPixels (glx, gly, glwidth, glheight, GL_RGB, GL_UNSIGNED_BYTE, buffer);
	for (i = 0; i < size; i += 3) {
		temp = buffer[i];
		buffer[i] = buffer[i+2];
		buffer[i+2] = temp;
	}
    if (!Capture_WriteVideo(glwidth, glheight, buffer)) {
        Con_Print("Problem capturing video frame!\n");
    }
    free(buffer);
	frames++;
}


void CaptureHelper_OnTransferStereo16(void)
{
    if (!CaptureHelper_IsActive()) return;

    // Copy last audio chunk written into our temporary buffer
    Q_memcpy(capture_audio_samples + 2 * captured_audio_samples, snd_out, snd_linear_count * shm->channels);
    captured_audio_samples += snd_linear_count / 2;

    if (captured_audio_samples >= (int)(0.5 + host_framerate.getFloat() * shm->speed)) {
        // We have enough audio samples to match one frame of video

        if (!Capture_WriteAudio(captured_audio_samples, (unsigned char*)capture_audio_samples)) {
            Con_Print("Problem capturing audio!\n");
        }
        captured_audio_samples = 0;

    }
}

bool CaptureHelper_GetSoundtime(void)
{
    if (CaptureHelper_IsActive()) {
        soundtime += (int)(0.5 + host_framerate.getFloat() * shm->speed);
        return true;
    } else {
        return false;
    }
}
