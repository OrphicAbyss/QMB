/*
Copyright (C) 2001 Quake done Quick

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

// This is a Win32 only implementation of ICapture for
// audio/video capture of AVI using a specified codec.
//  --Anthony.

#ifdef _WIN32

#include <windows.h>
#include <vfw.h>

class CaptureAviWriter
{
public:
    CaptureAviWriter(char* filename, char* fourcc, float fps, int audio_hz);
    virtual ~CaptureAviWriter();

    BOOL WriteVideo(int width, int height, unsigned char* rgba_pixel_buffer);
    BOOL WriteAudio(int samples, unsigned char* stereo_16bit_sample_buffer);

protected:
    typedef void (CALLBACK* LPFNAVIFILEINIT)(void);
    typedef HRESULT (CALLBACK* LPFNAVIFILEOPEN)(PAVIFILE*, LPCTSTR, UINT, LPCLSID);
    typedef HRESULT (CALLBACK* LPFNAVIFILECREATESTREAM)(PAVIFILE, PAVISTREAM*, AVISTREAMINFO*);
    typedef HRESULT (CALLBACK* LPFNAVIMAKECOMPRESSEDSTREAM)(PAVISTREAM*, PAVISTREAM, AVICOMPRESSOPTIONS*, CLSID*);
    typedef HRESULT (CALLBACK* LPFNAVISTREAMSETFORMAT)(PAVISTREAM, LONG, LPVOID, LONG);
    typedef HRESULT (CALLBACK* LPFNAVISTREAMWRITE)(PAVISTREAM, LONG, LONG, LPVOID, LONG, DWORD, LONG*, LONG*);
    typedef ULONG (CALLBACK* LPFNAVISTREAMRELEASE)(PAVISTREAM);
    typedef ULONG (CALLBACK* LPFNAVIFILERELEASE)(PAVIFILE);
    typedef void (CALLBACK* LPFNAVIFILEEXIT)(void);

    HINSTANCE m_avidll_handle;
    LPFNAVIFILEINIT m_lpfnAVIFileInit;
    LPFNAVIFILEOPEN m_lpfnAVIFileOpen;
    LPFNAVIFILECREATESTREAM m_lpfnAVIFileCreateStream;
    LPFNAVIMAKECOMPRESSEDSTREAM m_lpfnAVIMakeCompressedStream;
    LPFNAVISTREAMSETFORMAT m_lpfnAVIStreamSetFormat;
    LPFNAVISTREAMWRITE m_lpfnAVIStreamWrite;
    LPFNAVISTREAMRELEASE m_lpfnAVIStreamRelease;
    LPFNAVIFILERELEASE m_lpfnAVIFileRelease;
    LPFNAVIFILEEXIT m_lpfnAVIFileExit;

    PAVIFILE m_file;
    PAVISTREAM m_uncompressed_video_stream;
    PAVISTREAM m_compressed_video_stream;
    PAVISTREAM VideoStream() const;
    PAVISTREAM m_uncompressed_audio_stream;
    PAVISTREAM AudioStream() const;

    unsigned long m_codec_fourcc;
    int m_video_frame_counter;
    int m_video_frame_size;
    float m_fps;
    int m_audio_hz;

    int m_audio_frame_counter;
    WAVEFORMATEX m_wave_format;
};

CaptureAviWriter::CaptureAviWriter(char* filename, char* fourcc, float fps, int audio_hz)
:   m_fps(fps),
    m_audio_hz(audio_hz),
    m_video_frame_counter(0),
    m_audio_frame_counter(0),
    m_file(NULL),
    m_codec_fourcc(0),
    m_compressed_video_stream(NULL),
    m_uncompressed_video_stream(NULL),
    m_uncompressed_audio_stream(NULL),
    m_avidll_handle(NULL)
{
    m_avidll_handle = LoadLibrary("avifil32.dll");

    if (fourcc != NULL) { // codec fourcc supplied
        m_codec_fourcc = mmioFOURCC(*(fourcc+0), *(fourcc+1), *(fourcc+2), *(fourcc+3));
    }

    if (m_avidll_handle != NULL) {

        m_lpfnAVIFileInit = (LPFNAVIFILEINIT)GetProcAddress(m_avidll_handle, "AVIFileInit");
        m_lpfnAVIFileOpen = (LPFNAVIFILEOPEN)GetProcAddress(m_avidll_handle, "AVIFileOpen");
        m_lpfnAVIFileCreateStream = (LPFNAVIFILECREATESTREAM)GetProcAddress(m_avidll_handle, "AVIFileCreateStream");
        m_lpfnAVIMakeCompressedStream = (LPFNAVIMAKECOMPRESSEDSTREAM)GetProcAddress(m_avidll_handle, "AVIMakeCompressedStream");
        m_lpfnAVIStreamSetFormat = (LPFNAVISTREAMSETFORMAT)GetProcAddress(m_avidll_handle, "AVIStreamSetFormat");
        m_lpfnAVIStreamWrite = (LPFNAVISTREAMWRITE)GetProcAddress(m_avidll_handle, "AVIStreamWrite");
        m_lpfnAVIStreamRelease = (LPFNAVISTREAMRELEASE)GetProcAddress(m_avidll_handle, "AVIStreamRelease");
        m_lpfnAVIFileRelease = (LPFNAVIFILERELEASE)GetProcAddress(m_avidll_handle, "AVIFileRelease");
        m_lpfnAVIFileExit = (LPFNAVIFILEEXIT)GetProcAddress(m_avidll_handle, "AVIFileExit");

        m_lpfnAVIFileInit();
        HRESULT hr = m_lpfnAVIFileOpen(&m_file, filename, OF_WRITE | OF_CREATE, NULL);
    }
}

CaptureAviWriter::~CaptureAviWriter()
{

    if (m_uncompressed_video_stream) m_lpfnAVIStreamRelease(m_uncompressed_video_stream);
    if (m_compressed_video_stream) m_lpfnAVIStreamRelease(m_compressed_video_stream);
    if (m_uncompressed_audio_stream) m_lpfnAVIStreamRelease(m_uncompressed_audio_stream);
    if (m_file) m_lpfnAVIFileRelease(m_file);
    m_lpfnAVIFileExit();
}

PAVISTREAM CaptureAviWriter::VideoStream() const
{
    return m_codec_fourcc ? m_compressed_video_stream : m_uncompressed_video_stream;
}

PAVISTREAM CaptureAviWriter::AudioStream() const
{
    return m_uncompressed_audio_stream;
}

BOOL CaptureAviWriter::WriteVideo(int width, int height, unsigned char* pixel_buffer)
{
    if (m_avidll_handle == NULL) return FALSE;
    if (m_file == NULL) return FALSE;

    HRESULT hr;

    if (!m_video_frame_counter) {
        // write header etc based on first_frame
        m_video_frame_size = width * height * 3;

        BITMAPINFOHEADER bitmap_info_header;
        Q_memset(&bitmap_info_header, 0, sizeof(BITMAPINFOHEADER));
        bitmap_info_header.biSize = 40;
        bitmap_info_header.biWidth = width;
        bitmap_info_header.biHeight = height;
        bitmap_info_header.biPlanes = 1;
        bitmap_info_header.biBitCount = 24;
        bitmap_info_header.biCompression = BI_RGB;
        bitmap_info_header.biSizeImage = m_video_frame_size * 3;

        AVISTREAMINFO stream_header;
        Q_memset(&stream_header, 0, sizeof(stream_header));
        stream_header.fccType = streamtypeVIDEO;
        stream_header.fccHandler = m_codec_fourcc;
        stream_header.dwScale = 100;
        stream_header.dwRate = (unsigned long)(0.5 + m_fps * 100.0);
        SetRect(&stream_header.rcFrame, 0, 0, width, height);

        hr = m_lpfnAVIFileCreateStream(m_file, &m_uncompressed_video_stream, &stream_header);
        if (FAILED(hr)) return FALSE;

        if (m_codec_fourcc) {
            AVICOMPRESSOPTIONS opts;
            Q_memset(&opts, 0, sizeof(opts));
            AVICOMPRESSOPTIONS* aopts[1] = { &opts };
            opts.fccType = stream_header.fccType;
            opts.fccHandler = m_codec_fourcc;
            // Make the stream according to compression
            hr = m_lpfnAVIMakeCompressedStream(&m_compressed_video_stream, m_uncompressed_video_stream, &opts, NULL);
            if (FAILED(hr)) return FALSE;
        }

        hr = m_lpfnAVIStreamSetFormat(VideoStream(), 0, &bitmap_info_header, sizeof(BITMAPINFOHEADER));
        if (FAILED(hr)) return FALSE;
    } else {
        // check frame size (TODO: other things too?) hasn't changed
        if (m_video_frame_size != width * height * 3) return FALSE;
    }

    if (VideoStream() == NULL) return FALSE;

    hr = m_lpfnAVIStreamWrite(VideoStream(), m_video_frame_counter++, 1,
								pixel_buffer, m_video_frame_size,
								AVIIF_KEYFRAME, NULL, NULL);
    if (FAILED(hr)) return FALSE;

    return TRUE;
}

BOOL CaptureAviWriter::WriteAudio(int samples, unsigned char* sample_buffer)
{
    if (m_avidll_handle == NULL) return FALSE;
    if (m_file == NULL) return FALSE;

    HRESULT hr;

    if (!m_audio_frame_counter) {
        // write header etc based on first_frame
        Q_memset(&m_wave_format, 0, sizeof(WAVEFORMATEX));
        m_wave_format.wFormatTag = WAVE_FORMAT_PCM;
        m_wave_format.nChannels = 2; // always stereo in Quake sound engine
        m_wave_format.nSamplesPerSec = m_audio_hz;
        m_wave_format.wBitsPerSample = 16; // always 16bit in Quake sound engine
        m_wave_format.nBlockAlign = (unsigned short)(m_wave_format.wBitsPerSample/8 * m_wave_format.nChannels);
        m_wave_format.nAvgBytesPerSec = m_wave_format.nSamplesPerSec * m_wave_format.nBlockAlign;
        m_wave_format.cbSize = 0;

        AVISTREAMINFO stream_header;
        Q_memset(&stream_header, 0, sizeof(stream_header));
        stream_header.fccType = streamtypeAUDIO;
        stream_header.dwScale = m_wave_format.nBlockAlign;
        stream_header.dwRate = stream_header.dwScale * (unsigned long)m_wave_format.nSamplesPerSec;
        stream_header.dwSampleSize = m_wave_format.nBlockAlign;

        hr = m_lpfnAVIFileCreateStream(m_file, &m_uncompressed_audio_stream, &stream_header);
        if (FAILED(hr)) return FALSE;

        hr = m_lpfnAVIStreamSetFormat(AudioStream(), 0, &m_wave_format, sizeof(WAVEFORMATEX));
        if (FAILED(hr)) return FALSE;
    }

    if (AudioStream() == NULL) return FALSE;

    hr = m_lpfnAVIStreamWrite(
	    AudioStream(), m_audio_frame_counter++, 1,
        sample_buffer, samples * m_wave_format.nBlockAlign,
        AVIIF_KEYFRAME, NULL, NULL
    );
    if (FAILED(hr)) return FALSE;

    return TRUE;
}

CaptureAviWriter* g_avi_writer = NULL;

char Capture_DOTEXTENSION[5] = ".avi";

void Capture_Open(char* filename, char* fourcc, float fps, int audio_hz)
{
    g_avi_writer = new CaptureAviWriter(filename, fourcc, fps, audio_hz);
}

int Capture_WriteVideo(int width, int height, unsigned char* rgb_pixel_buffer)
{
    return Capture_IsRecording() &&
        g_avi_writer->WriteVideo(width, height, rgb_pixel_buffer);
}

int Capture_WriteAudio(int samples, unsigned char* stereo_16bit_sample_buffer)
{
    return Capture_IsRecording() &&
        g_avi_writer->WriteAudio(samples, stereo_16bit_sample_buffer);
}

void Capture_Close()
{
    delete g_avi_writer;
    g_avi_writer = NULL;
}

int Capture_IsRecording()
{
    return (g_avi_writer == NULL) ? 0 : 1;
}

#else // NULL implementation for non-Win32 platforms

char Capture_DOTEXTENSION[5] = "";

void Capture_Open(char*, char*, float, int)
{
}

void Capture_Close()
{
}

int Capture_WriteVideo(int, int, unsigned char*)
{
    return 0;
}

int Capture_WriteAudio(int, unsigned char*)
{
    return 0;
}

int Capture_IsRecording()
{
    return 0;
}

#endif