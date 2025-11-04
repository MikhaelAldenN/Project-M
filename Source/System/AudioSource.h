#pragma once

#include <memory>
#include <xaudio2.h>
#include "System/AudioResource.h"

// オーディオソース
class AudioSource
{
public:
	AudioSource(IXAudio2* xaudio, std::shared_ptr<AudioResource>& resource);
	~AudioSource();

	// 再生
	void Play(bool loop);

	// 停止
	void Stop();

	// 音量設定
	void SetVolume(float volume);

private:
	IXAudio2SourceVoice*			sourceVoice = nullptr;
	std::shared_ptr<AudioResource>	resource;
};
