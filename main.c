#define COBJMACROS
#define VC_EXTRALEAN
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <xaudio2.h>
#include <mmsystem.h>
#include <stdbool.h>
#include <assert.h>
#include <stdio.h>

#pragma comment(lib, "ole32")
#pragma comment(lib, "winmm")

typedef struct
{
    IXAudio2 *handle;
    IXAudio2MasteringVoice *masterVoice;
    
} XAudio2Context;

bool
xaudio2_init(XAudio2Context *xaudio2)
{
    HRESULT hr;
    
    hr = CoInitializeEx(0, COINIT_MULTITHREADED);
    if (SUCCEEDED(hr))
    {
        hr = XAudio2Create(&xaudio2->handle, 0, XAUDIO2_DEFAULT_PROCESSOR);
        if (SUCCEEDED(hr))
        {
            hr = IXAudio2_CreateMasteringVoice(xaudio2->handle,
                                               &xaudio2->masterVoice,
                                               0, 0, 0, 0, 0, 0);
            if (SUCCEEDED(hr))
            {
                return true;
            }
            else
            {
                printf("CreateMasteringVoice failed: 0x%08x\n", hr);
                IXAudio2_Release(xaudio2->handle);
            }
        }
        else
        {
            printf("XAudio2Create failed: 0x%08x\n", hr);
        }
    }
    else
    {
        printf("CoInitializeEx failed: 0x%08x\n", hr);
    }
    
    CoUninitialize();
    return false;
}

IXAudio2SourceVoice *
xaudio2_create_source_voice(XAudio2Context *xaudio2, WAVEFORMATEX *waveFormat)
{
    IXAudio2SourceVoice *result = 0;
    
    HRESULT hr = IXAudio2_CreateSourceVoice(xaudio2->handle,
                                            &result,
                                            waveFormat,
                                            0,
                                            XAUDIO2_DEFAULT_FREQ_RATIO,
                                            0, 0, 0);
    if (FAILED(hr))
    {
        printf("CreateSourceVoice failed: 0x%08x\n", hr);
        assert(result == 0);
    }
    
    return result;
}

bool
xaudio2_submit_buffer(IXAudio2SourceVoice *sourceVoice,
                      BYTE *audioData, UINT32 audioBytes)
{
    XAUDIO2_BUFFER buffer = {0};
    buffer.AudioBytes = audioBytes;
    buffer.pAudioData = audioData;
    buffer.Flags = XAUDIO2_END_OF_STREAM;
    
    HRESULT hr = IXAudio2SourceVoice_SubmitSourceBuffer(sourceVoice, &buffer, 0);
    if (FAILED(hr))
    {
        printf("SubmitSourceBuffer failed: 0x%08x\n", hr);
        return false;
    }
    
    return true;
}

bool
xaudio2_play(IXAudio2SourceVoice *sourceVoice)
{
    HRESULT hr = IXAudio2SourceVoice_Start(sourceVoice, 0, 0);
    if (FAILED(hr))
    {
        printf("Start failed: 0x%08x\n", hr);
        return false;
    }
    
    return true;
}

void
xaudio2_cleanup(XAudio2Context *xaudio2)
{
    if (xaudio2->masterVoice)
    {
        IXAudio2SourceVoice_DestroyVoice(xaudio2->masterVoice);
        xaudio2->masterVoice = 0;
    }
    
    if (xaudio2)
    {
        IXAudio2_Release(xaudio2->handle);
        xaudio2->handle = 0;
    }
    
    CoUninitialize();
}

//
// WAV
//

typedef struct
{
    WAVEFORMATEX format;
    BYTE *data;
    DWORD dataSize;
    
} LoadedWav;

void
wav_free(LoadedWav *wav)
{
    free(wav->data);
    wav->data = 0;
    wav->dataSize = 0;
}

bool
wav_load(char *filename, LoadedWav *out)
{
    HMMIO file = mmioOpenA((LPSTR)filename, NULL, MMIO_READ);
    if (file)
    {
        MMCKINFO riff, fmt, data;
        riff.fccType = mmioFOURCC('W', 'A', 'V', 'E');
        if (mmioDescend(file, &riff, NULL, MMIO_FINDRIFF) == MMSYSERR_NOERROR)
        {
            fmt.ckid = mmioFOURCC('f', 'm', 't', ' ');
            if (mmioDescend(file, &fmt, &riff, MMIO_FINDCHUNK) == MMSYSERR_NOERROR)
            {
                mmioRead(file, (HPSTR)&out->format, sizeof(WAVEFORMATEX));
                mmioAscend(file, &fmt, 0);
                
                data.ckid = mmioFOURCC('d', 'a', 't', 'a');
                if (mmioDescend(file, &data, &riff, MMIO_FINDCHUNK) == MMSYSERR_NOERROR)
                {
                    out->dataSize = data.cksize;
                    out->data = malloc(data.cksize);
                    mmioRead(file, (HPSTR)out->data, data.cksize);
                    
                    mmioClose(file, 0);
                    
                    return true;
                }
                else
                {
                    printf("DATA chunk not found");
                }
            }
            else
            {
                printf("FMT chunk not found");
            }
        }
        else
        {
            printf("WAVE chunk not found");
        }
        
        mmioClose(file, 0);
    }
    else
    {
        printf("mmioOpenA Failed");
    }
    
    return false;
}

//
//
//

int main()
{
    XAudio2Context xaudio2;
    
    if (xaudio2_init(&xaudio2))
    {
        WAVEFORMATEX waveFormat = {0};
        waveFormat.wFormatTag = WAVE_FORMAT_PCM;
        waveFormat.nChannels = 2;
        waveFormat.nSamplesPerSec = 44100;
        waveFormat.wBitsPerSample = 16;
        waveFormat.nBlockAlign = (waveFormat.nChannels * waveFormat.wBitsPerSample) / 8;
        waveFormat.nAvgBytesPerSec = waveFormat.nSamplesPerSec * waveFormat.nBlockAlign;
        
        IXAudio2SourceVoice *sourceVoice =
            xaudio2_create_source_voice(&xaudio2, &waveFormat);
        
        if (sourceVoice)
        {
            LoadedWav test;
            // Test sfx from: https://pixabay.com/sound-effects/piano-logo-reveal-201060/
            if (wav_load("test.wav", &test))
            {
                if (xaudio2_submit_buffer(sourceVoice, test.data, test.dataSize))
                {
                    xaudio2_play(sourceVoice);
                }
                
                printf("Playing sfx for 7 seconds...\n");
                Sleep(7000);
                
                wav_free(&test);
            }
        }
        
        IXAudio2SourceVoice_DestroyVoice(sourceVoice);
        sourceVoice = 0;
        
        xaudio2_cleanup(&xaudio2);
    }
    
    return 0;
}