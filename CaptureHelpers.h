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

// These are the helper functions for audio/video capture
// that a typical Quake1 engine mod needs to be wired to.
//
// I provide code snips showing where to insert into the id1
// codebase. In each case you'll want to
//  #include "CaptureHelpers.h"
// at the top of the touched file, and there's a single line to insert
// that is indicated by a "CAPTURE" marker comment:

/* in existing_id1_file.c:
void ExampleId1Function(void)
{
    ...
    ...
    existing_id1_code_fragment();

    // CAPTURE <mailto:anthony@planetquake.com>
    Single_line_to_insert_involving_a_(CaptureHelper_Method());

    more_existing_id1_code();
    ...
    ...
}
 */

// Avoid having to pull in common.h just to get qboolean:
typedef int qboolean_not_yet_declared;


// Hook to initialise console vars and commands
void CaptureHelper_Init(void);
/* in gl_screen.c:
void SCR_Init (void)
{
    ...
    ...
	scr_ram = Draw_PicFromWad ("ram");
	scr_net = Draw_PicFromWad ("net");
	scr_turtle = Draw_PicFromWad ("turtle");

    // CAPTURE <mailto:anthony@planetquake.com>
    CaptureHelper_Init();

	scr_initialized = true;
}
 */

// Hook to allow batch capture to exit on finishing demo playback
void CaptureHelper_OnStopPlayback(void);
/* in cl_demo.c:
void CL_StopPlayback (void)
{
    ...
    ...

	if (cls.timedemo)
		CL_FinishTimeDemo ();

    // CAPTURE <anthony@planetquake.com>
    CaptureHelper_OnStopPlayback();
}
 */

// Hook to allow capture of video
void CaptureHelper_OnUpdateScreen(void);
/* in gl_screen.c:
void SCR_UpdateScreen (void)
{
    ...
    ...

	V_UpdatePalette ();

    // CAPTURE <anthony@planetquake.com>
    CaptureHelper_OnUpdateScreen();

	GL_EndRendering ();
}
 */

// Hook to allow capture of audio
void CaptureHelper_OnTransferStereo16(void);
/* in snd_mix.c:
void S_TransferStereo16 (int endtime)
{
    ...
    ...

	while (lpaintedtime < endtime)
	{
        ...
        ...

		snd_p += snd_linear_count;
		lpaintedtime += (snd_linear_count>>1);

        // CAPTURE <anthony@planetquake.com>
        CaptureHelper_OnTransferStereo16();
	}

#ifdef _WIN32
	if (pDSBuf)
		pDSBuf->lpVtbl->Unlock(pDSBuf, pbuf, dwSize, NULL, 0);
#endif
}
 */

// Use an alternative calculation for GetSoundtime
qboolean CaptureHelper_GetSoundtime(void);
/* in snd_dma.c:
void GetSoundtime(void)
{
	int		samplepos;
	static	int		buffers;
	static	int		oldsamplepos;
	int		fullsamples;

    // CAPTURE <anthony@planetquake.com>
    if (CaptureHelper_GetSoundtime()) return;

	fullsamples = shm->samples / shm->channels;

    ...
    ...
}
 */

// Check if we are capturing this frame
qboolean CaptureHelper_IsActive(void);
/* in snd_dma.c:
void S_ExtraUpdate (void)
{
    // CAPTURE <anthony@planetquake.com>
    if (CaptureHelper_IsActive()) return;

#ifdef _WIN32
	IN_Accumulate ();
#endif

	if (snd_noextraupdate.value)
		return;		// don't pollute timings
	S_Update_();
}
 */

// To ensure good audio sync at _all_ framerates, you can also ensure
// we do not drop very short frames:
/* in host.c:
qboolean Host_FilterTime (float time)
{
	realtime += time;

    // CAPTURE <anthony@planetquake.com>
    if (!cls.capturedemo) // only allow the following early return if not capturing:
	if (!cls.timedemo && realtime - oldrealtime < 1.0/72.0)
		return false;		// framerate is too high

    ...
    ...
}
 */

// The only other thing you'll need to do is to add a new member
// to the client_static_t struct declaration
/* in client.h:
typedef struct
{
    ...
    ...
	struct qsocket_s	*netcon;
	sizebuf_t	message;		// writing buffer to send to server

    // CAPTURE <mailto:anthony@planetquake.com>
    qboolean    capturedemo;

} client_static_t;
 */