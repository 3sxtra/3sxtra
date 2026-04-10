#ifndef _PS3_AUDIO_H_
#define _PS3_AUDIO_H_

#ifdef __cplusplus
extern "C" {
#endif

void ps3_audio_init(void);
void ps3_audio_quit(void);
void ps3_audio_signal_ready(void);

#ifdef __cplusplus
}
#endif

#endif // _PS3_AUDIO_H_
