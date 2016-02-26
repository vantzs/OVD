/*
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

   xrdp: A Remote Desktop Protocol server.
   Copyright (C) Jay Sorg 2009
*/

#include "os_calls.h"
#include "sound.h"
#include "chansrv.h"


extern int g_rdpsnd_chan_id; /* in chansrv.c */
extern struct log_config log_conf;
static int is_fragmented_packet = 0;
static int fragment_size;
static struct stream* splitted_packet;
static RD_WAVEFORMATEX formats[MAX_FORMATS];
static int format_index = 0;

/*****************************************************************************/
int APP_CC
sound_send(struct stream* s){
  int rv;
  int size = (int)(s->end - s->data);
  /* set the body size */
  *(s->data+2) = (uint16)size-4;

  rv = send_channel_data(g_rdpsnd_chan_id, s->data, size);
  if (rv != 0)
  {
    log_message(&log_conf, LOG_LEVEL_ERROR, "chansrv[sound_send]: "
    		"enable to send message");
  }
  log_message(&log_conf, LOG_LEVEL_DEBUG, "chansrv[sound_send]: "
  		"send message");
  log_hexdump(&log_conf, LOG_LEVEL_DEBUG, (unsigned char*)s->data, size );

  return rv;
}

/*****************************************************************************/
int APP_CC
sound_init(void)
{
	log_message(&log_conf, LOG_LEVEL_DEBUG, "chansrv[sound_init]: "
			"init sound channel");
	g_rdpsnd_chan_id = 1;
	sound_send_format_and_version();
  return 0;
}

/*****************************************************************************/
int APP_CC
sound_deinit(void)
{
	log_message(&log_conf, LOG_LEVEL_DEBUG, "chansrv[sound_deinit]: "
			"deinit sound channel");
  return 0;
}

/*****************************************************************************/
int APP_CC
sound_data_in(struct stream* s, int chan_id, int chan_flags, int length,
              int total_length)
{
  int msg_type;
  int result;
  struct stream* packet;
  int unused;
  int msg_size;

  if(length != total_length)
  {
  	log_message(&log_conf, LOG_LEVEL_DEBUG, "chansrv[sound_data_in]: "
  			"packet is fragmented");
  	if(is_fragmented_packet == 0)
  	{
    	log_message(&log_conf, LOG_LEVEL_DEBUG, "chansrv[sound_data_in]: "
  				"packet is fragmented : first part");
  		is_fragmented_packet = 1;
  		fragment_size = length;
  		make_stream(splitted_packet);
  		init_stream(splitted_packet, total_length);
  		g_memcpy(splitted_packet->p,s->p, length );
  		log_hexdump(&log_conf, LOG_LEVEL_DEBUG, (unsigned char*)s->p, length);
  		return 0;
  	}
  	else
  	{
  		g_memcpy(splitted_packet->p+fragment_size, s->p, length );
  		fragment_size += length;
  		if (fragment_size == total_length )
  		{
  	  	log_message(&log_conf, LOG_LEVEL_DEBUG, "chansrv[sound_data_in]: "
    				"packet is fragmented : last part");
  			packet = splitted_packet;
  		}
  		else
  		{
  	  	log_message(&log_conf, LOG_LEVEL_DEBUG, "chansrv[sound_data_in]: "
    				"packet is fragmented : next part");
  			return 0;
  		}
  	}
  }
  else
  {
  	packet = s;
  }
	log_message(&log_conf, LOG_LEVEL_DEBUG, "chansrv[sound_data_in]: ");
  log_hexdump(&log_conf, LOG_LEVEL_DEBUG, (unsigned char*)packet->p, total_length);
  in_uint8(packet, msg_type);
  in_uint8(packet, unused);
  in_uint16_le(s, msg_size);

	log_message(&log_conf, LOG_LEVEL_DEBUG, "chansrv[sound_data_in]: "
  		"msg_type=0x%01x", msg_type);
	log_message(&log_conf, LOG_LEVEL_DEBUG, "chansrv[sound_data_in]: "
	  		"msg_size=%i", msg_size);

	switch (msg_type)
	{
	case SNDC_FORMATS:
		log_message(&log_conf, LOG_LEVEL_DEBUG, "chansrv[sound_data_in]: "
	  		"SNDC_FORMATS message");
		result = sound_process_client_format(s);
		break;

	case SNDC_TRAINING:
		log_message(&log_conf, LOG_LEVEL_DEBUG, "chansrv[sound_data_in]: "
	  		"SNDC_TRANING message");
		result = sound_process_training_message(s);
		break;

	default:
		log_message(&log_conf, LOG_LEVEL_DEBUG, "chansrv[sound_data_in]: "
				"unknown message %02x", msg_type);
		result = 1;
	}
	if(is_fragmented_packet == 1)
	{
		is_fragmented_packet = 0;
		fragment_size = 0;
		free_stream(packet);
	}
  return result;
}

/*****************************************************************************/
int APP_CC
sound_get_wait_objs(tbus* objs, int* count, int* timeout)
{
  return 0;
}

/*****************************************************************************/
int APP_CC
sound_check_wait_objs(void)
{
  return 0;
}

/*****************************************************************************/
int APP_CC
sound_send_format_and_version(void)
{
	log_message(&log_conf, LOG_LEVEL_DEBUG, "chansrv[sound_send_format_and_version]: "
			"send available server format and version");
	struct stream* s;
	make_stream(s);
	init_stream(s, 1024);
	/* RDPSND PDU Header */
	out_uint8(s, SNDC_FORMATS);
	out_uint8(s, 0);												/* unused */
	out_uint16_le(s, 0);										/* data size: unused */
	/* Body */
	out_uint32_le(s, 0); 										/* dwFlags: unused */
	out_uint32_le(s, 0); 										/* dwVolume: unused */
	out_uint32_le(s, 0); 										/* dwPitch: unused */
	out_uint16_le(s, 0); 										/* wDGramPort: unused */
	out_uint16_le(s, 1);									 	/* wNumberOfFormats */
	out_uint8(s, 0);												/* cLastBlockConfirmed */
	out_uint16_le(s, RDP51); 								/* wVersion */
	out_uint8(s, 0);												/* bPad: unused */
	/* sound formats table */
	/* format 1 */
	out_uint16_le(s, WAVE_FORMAT_PCM); 			/* wFormatTag */
	out_uint16_le(s, 2); 										/* nChannels */
	out_uint32_le(s, 5622); 								/* nSamplesPerSec */
	out_uint32_le(s, 15888); 								/* nAvgBytesPerSec */
	out_uint16_le(s, 4);		 								/* nBlockAlign */
	out_uint16_le(s, 10);		 								/* wBitsPerSample */
	out_uint16_le(s, 0);		 								/* cbSize */
	//out_uint16_le(s, RDP51); 								/* data */
	s_mark_end(s);
	sound_send(s);
	free_stream(s);
}




/*****************************************************************************/
int APP_CC
sound_send_training()
{
	log_message(&log_conf, LOG_LEVEL_DEBUG, "chansrv[sound_send_training]: "
			"send sound training");
	struct stream* s;
	make_stream(s);
	init_stream(s, 1024);
	/* RDPSND PDU Header */
	out_uint8(s, SNDC_TRAINING);
	out_uint8(s, 0);												/* unused */
	out_uint16_le(s, 0);										/* data size: unused */
	/* Body */
	out_uint16_le(s, TRAINING_VALUE);				/* wTimeStamp: arbitrary */
	out_uint16_le(s, 0); 										/* wPackSize */
	s_mark_end(s);
	sound_send(s);
	free_stream(s);
}

/*****************************************************************************/
int APP_CC
sound_send_wave_info()
{
	log_message(&log_conf, LOG_LEVEL_DEBUG, "chansrv[sound_send_wave_info]: "
			"send wave information");
	char data="data";
	struct stream* s;
	make_stream(s);
	init_stream(s, 1024);
	/* RDPSND PDU Header */
	out_uint8(s, SNDC_WAVE);
	out_uint8(s, 0);												/* unused */
	out_uint16_le(s, 0);										/* data size: unused */
	/* Body */
	out_uint16_le(s, 0);										/* wTimeStamp */
	out_uint16_le(s, 0); 										/* wFormatNo */
	out_uint8(s, 0);												/* cBlockNo */
	out_uint8s(s,3);		 										/* bPad */
	out_uint8a(s,data,4); 									/* first 4 data bytes */
	s_mark_end(s);
	sound_send(s);
	free_stream(s);
}

/*****************************************************************************/
int APP_CC
sound_process_client_format(struct stream* s)
{
	log_message(&log_conf, LOG_LEVEL_DEBUG, "chansrv[sound_process_client_format]: "
			"register client format");
	RD_WAVEFORMATEX *format;
	int dwFlags;
	int dwVolume;
	int dwPitch;
	int port;
	int format_number_announced;
	int unused;
	int version;
	int wDGramPort;
	int i;

	in_uint32_le(s, dwFlags);
	in_uint32_le(s, dwVolume);
	in_uint32_le(s, dwPitch);
	in_uint16_be(s, wDGramPort);
	in_uint16_le(s, format_number_announced);
	in_uint8(s, unused);
	in_uint16_le(s, version);
	in_uint8(s, unused);

	if (dwFlags & TSSNDCAPS_ALIVE == 0)
	{
		log_message(&log_conf, LOG_LEVEL_WARNING, "chansrv[sound_process_client_format]: "
				"the client did not to use audio channel");
	}
	if (dwFlags & TSSNDCAPS_VOLUME == 0)
	{
		log_message(&log_conf, LOG_LEVEL_WARNING, "chansrv[sound_process_client_format]: "
				"the client can not control volume");
		dwVolume = 0;
	}
	if (dwFlags & TSSNDCAPS_PITCH == 0)
	{
		log_message(&log_conf, LOG_LEVEL_WARNING, "chansrv[sound_process_client_format]: "
				"the client can not control pitch");
		/* TODO interprete the pitch value */
		dwPitch = 0;
	}
	/* sound format list */
	for ( i=0; i<format_number_announced ; i++)
	{
		format = &formats[i];
		in_uint16_le(s, format->wFormatTag); 					/* wFormatTag */
		out_uint16_le(s, format->nChannels); 					/* nChannels */
		out_uint32_le(s, format->nSamplesPerSec); 		/* nSamplesPerSec */
		out_uint32_le(s, format->nAvgBytesPerSec); 		/* nAvgBytesPerSec */
		out_uint16_le(s, format->nBlockAlign);				/* nBlockAlign */
		out_uint16_le(s, format->wBitsPerSample);			/* wBitsPerSample */
		out_uint16_le(s, format->cbSize);		 					/* cbSize */
		if(format->cbSize != 0)
		{
			out_uint8p(s, format->cb, format->cbSize);
		}
	}
	if( wDGramPort == 0)
	{
		log_message(&log_conf, LOG_LEVEL_DEBUG, "chansrv[sound_process_client_format]: "
				"client do not use UDP transport layer");
		sound_send_training();

	}
	else
	{
		/* if datagram is present, add Quality Mode PDU */
		log_message(&log_conf, LOG_LEVEL_WARNING, "chansrv[sound_process_client_format]: "
				"UDP transfert is not supported");
	}
	return 0;
}

/*****************************************************************************/
int APP_CC
sound_process_training_message(struct stream* s)
{
	log_message(&log_conf, LOG_LEVEL_DEBUG, "chansrv[sound_process_training_message]:");
	int training_value;
	in_uint16_le(s, training_value);

	if (training_value != TRAINING_VALUE)
	{
		log_message(&log_conf, LOG_LEVEL_WARNING, "chansrv[sound_process_training_message]: "
				"the training value returned is invalid : %02x",training_value);
	}
	return 0;
}

