// The November netlists contain no audio DAC, codec, I2S bus or speaker.
// Keep the guest audio interface deterministic without initializing unrelated pins.
#include "sysdeps.h"
#include "audio.h"
#include "audio_defs.h"

void AudioInit(void)
{
    AudioStatus.sample_rate = 22050u << 16;
    AudioStatus.sample_size = 16;
    AudioStatus.channels = 2;
    AudioStatus.mixer = 0;
    AudioStatus.num_sources = 0;
    audio_sample_rates.push_back(22050u << 16);
    audio_sample_sizes.push_back(16);
    audio_channel_counts.push_back(2);
    audio_component_flags = cmpWantsRegisterMessage | kStereoOut | k16BitOut;
    audio_open = false;
}
void AudioExit(void) {}
void AudioInterrupt(void) {}
void audio_enter_stream(void) {}
void audio_exit_stream(void) {}
bool audio_set_sample_rate(int index) { return index == 0; }
bool audio_set_sample_size(int index) { return index == 0; }
bool audio_set_channels(int index) { return index == 0; }
bool audio_get_main_mute(void) { return true; }
bool audio_get_speaker_mute(void) { return true; }
uint32 audio_get_main_volume(void) { return 0; }
uint32 audio_get_speaker_volume(void) { return 0; }
void audio_set_main_mute(bool) {}
void audio_set_speaker_mute(bool) {}
void audio_set_main_volume(uint32) {}
void audio_set_speaker_volume(uint32) {}
