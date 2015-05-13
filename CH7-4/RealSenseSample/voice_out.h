// voice_synthesisサンプルより

// winmm.libをリンクする
#pragma comment(lib, "winmm.lib")

/*******************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2013-2014 Intel Corporation. All Rights Reserved.

*******************************************************************************/
#pragma once
#include <Windows.h>
#include <vector>
#include "pxcspeechsynthesis.h"

class VoiceOut {
protected:

	enum { buffering=3 };

	WAVEHDR				m_headers[buffering];
	PXCAudio::AudioData m_data[buffering];
	PXCAudio*			m_samples[buffering];

    // poor man's autogrowing bytestream
    std::vector<byte>   m_wavefile;

	int         m_nsamples;
	HWAVEOUT	m_hwo;

    WAVEFORMATEX m_wfx;

public:

	VoiceOut(PXCSpeechSynthesis::ProfileInfo *pinfo) {
		m_nsamples=0;
		m_hwo=0;
		
		memset(&m_wfx,0,sizeof(m_wfx));
		m_wfx.wFormatTag=WAVE_FORMAT_PCM;
		m_wfx.nSamplesPerSec=pinfo->outputs.sampleRate;
		m_wfx.wBitsPerSample=16;
		m_wfx.nChannels=pinfo->outputs.nchannels;
		m_wfx.nBlockAlign=(m_wfx.wBitsPerSample/8)*m_wfx.nChannels;
		m_wfx.nAvgBytesPerSec=m_wfx.nBlockAlign*m_wfx.nSamplesPerSec;

		waveOutOpen(&m_hwo,WAVE_MAPPER,&m_wfx,0,0,CALLBACK_NULL);
	}

	void RenderAudio(PXCAudio *audio) {
		int k=(m_nsamples%buffering);
		if (m_nsamples++>=buffering) {
			while (waveOutUnprepareHeader(m_hwo,&m_headers[k],sizeof(WAVEHDR))==WAVERR_STILLPLAYING)
				Sleep(10);
			m_samples[k]->ReleaseAccess(&m_data[k]);
			m_samples[k]->Release();
		}
		audio->AddRef();
		m_samples[k]=audio;
		if (m_samples[k]->AcquireAccess(PXCAudio::ACCESS_READ,PXCAudio::AUDIO_FORMAT_PCM,&m_data[k])>=PXC_STATUS_NO_ERROR) {
			memset(&m_headers[k],0,sizeof(WAVEHDR));
			m_headers[k].dwBufferLength=m_data[k].dataSize*2;
			m_headers[k].lpData=(LPSTR)m_data[k].dataPtr;
			waveOutPrepareHeader(m_hwo,&m_headers[k],sizeof(WAVEHDR));
			waveOutWrite(m_hwo, &m_headers[k], sizeof(WAVEHDR));
		}
	}

    void WriteAudio(PXCAudio *audio) {
        PXCAudio::AudioData data;
        pxcStatus sts = audio->AcquireAccess (PXCAudio::ACCESS_READ, PXCAudio::AUDIO_FORMAT_PCM, &data);
        if (sts < PXC_STATUS_NO_ERROR) return;
        pxcI32 dataSizeInBytes = data.dataSize * 2; // 2 bytes in each sample (16 bit samples)
        for(pxcI32 i = 0; i < dataSizeInBytes; i++)
        {
            m_wavefile.push_back( data.dataPtr[i] );
        }
        audio->ReleaseAccess( &data );
    }

    void SaveFile(const wchar_t* fname)
    {
        FILE* file = NULL;
        _wfopen_s(&file,fname,L"wb");
        
        if( !file ) return;

        int len = (int) m_wavefile.size();
        // These first 44 bytes are the header of a regular old
        // RIFF WAV PCM audio file
        fwrite("RIFF", 1, 4, file);                                       // [0,3] - Chunk ID 
        int chunkSize=len+36; fwrite(&chunkSize,4,1,file);                // [4, 7] - Chunk size:  = 36 + Subchunk2size
        fwrite("WAVE", 1, 4, file);                                       // [8,11] - Format 
        fwrite("fmt ", 1, 4, file);                                       // [12,15] - Subchunk1 ID 
        int subchunk1=16; fwrite(&subchunk1,4,1,file);                    // [16, 19] - Subchunk1 size:  = 16 for PCM
        short audioFormat=1; fwrite(&audioFormat,2,1,file);               // [20, 21] - AudioFormat: PCM = 1
        fwrite(&m_wfx.nChannels,2,1,file);                                // [22, 23] - Number of channels
        fwrite(&m_wfx.nSamplesPerSec,4,1,file);                           // [24, 27] - SampeRate
        fwrite(&m_wfx.nAvgBytesPerSec,4,1,file);                          // [28, 31] - ByteRate: 16 is the numbers of bits per sample
        fwrite(&m_wfx.nBlockAlign,2,1,file);                              // [32, 33] - BlockAlign: 16 is the numbers of bits per sample
        short bitsPerSample=16; fwrite(&bitsPerSample,2,1,file);          // [34, 35] - BitsPerSample
        fwrite("data", sizeof(unsigned char), 4, file);                   // [36,39] - Subchunk2 (data chunk) ID
        fwrite(&len,4,1,file);                                            // [40, 43]:  number of bytes of the data
    
        // The actual audio data goes next
        fwrite( &m_wavefile[0], 1, len, file);

        fclose( file );
    }

	~VoiceOut(void) {
		if (!m_hwo || m_nsamples<=0) return;
		for (int i=m_nsamples-buffering+1;i<m_nsamples;i++) {
			if( i<0 ) i++; //If he have 1 AudioObject in m_samples 'k' will be -1. It is exeption for the alghorithm. Bug 57423
			int k=(i%buffering); 
			while (waveOutUnprepareHeader(m_hwo,&m_headers[k],sizeof(WAVEHDR))==WAVERR_STILLPLAYING)
				Sleep(10);
			m_samples[k]->ReleaseAccess(&m_data[k]);
			m_samples[k]->Release();
		}
		waveOutClose(m_hwo);
	}
};
