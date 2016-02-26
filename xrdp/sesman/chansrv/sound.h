
#if !defined(SOUND_H)
#define SOUND_H

#include "arch.h"
#include "parse.h"


#define SOUND_FORMAT_NUMBER		3
/* Msg type */
#define SNDC_CLOSE						0x01
#define SNDC_WAVE							0x02
#define SNDC_SETVOLUME				0x03
#define SNDC_SETPITCH					0x04
#define SNDC_WAVECONFIRM			0x05
#define SNDC_TRAINING					0x06
#define SNDC_FORMATS					0x07
#define SNDC_CRYPTKEY					0x08
#define SNDC_WAVEENCRYPT			0x09
#define SNDC_UDPWAVE					0x0A
#define SNDC_UDPWAVELAST			0x0B
#define SNDC_QUALITYMODE			0x0C


/* sound version */
#define	RDP5						0x02
#define	RDP51						0x05
#define	RDP7						0x06

/* sound format */
#define WAVE_FORMAT_PCM	0x01


/* sound possible control */
#define TSSNDCAPS_ALIVE		0x00000001
#define TSSNDCAPS_VOLUME	0x00000002
#define TSSNDCAPS_PITCH		0x00000004



#define TRAINING_VALUE		0x99




#define MAX_FORMATS			10
#define MAX_CBSIZE 256


typedef struct _RD_WAVEFORMATEX
{
	uint16 wFormatTag;
	uint16 nChannels;
	uint32 nSamplesPerSec;
	uint32 nAvgBytesPerSec;
	uint16 nBlockAlign;
	uint16 wBitsPerSample;
	uint16 cbSize;
	uint8 cb[MAX_CBSIZE];
} RD_WAVEFORMATEX;




int APP_CC
sound_init(void);
int APP_CC
sound_deinit(void);
int APP_CC
sound_data_in(struct stream* s, int chan_id, int chan_flags, int length,
              int total_length);
int APP_CC
sound_get_wait_objs(tbus* objs, int* count, int* timeout);
int APP_CC
sound_check_wait_objs(void);

#endif
