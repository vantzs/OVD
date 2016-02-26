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

   for help see
   http://tronche.com/gui/x/icccm/sec-2.html#s-2
   .../kde/kdebase/workspace/klipper/clipboardpoll.cpp

*/

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include "arch.h"
#include "parse.h"
#include "os_calls.h"
#include "chansrv.h"


static Display* g_display = 0;

extern int g_cliprdr_chan_id; /* in chansrv.c */
extern struct log_config log_conf;

/*****************************************************************************/
int DEFAULT_CC
clipboard_error_handler(Display* dis, XErrorEvent* xer)
{
  char text[256];

  XGetErrorText(dis, xer->error_code, text, 255);
  log_message(&log_conf, LOG_LEVEL_ERROR, "chansrv[clipboard_error_handler]:"
  		"error : %s ",text);
  return 0;
}

/*****************************************************************************/
/* The X server had an internal error.  This is the last function called.
   Do any cleanup that needs to be done on exit, like removing temporary files.
   Don't worry about memory leaks */
int DEFAULT_CC
clipboard_fatal_handler(Display* dis)
{
	log_message(&log_conf, LOG_LEVEL_DEBUG, "chansrv[clipboard_fatal_handler]:"
			"fatal error");
	return 0;
}


/*****************************************************************************/
/* returns error */
int APP_CC
clipboard_init(void)
{
	log_message(&log_conf, LOG_LEVEL_DEBUG, "chansrv[clipboard_init]:"
			"clipboard init");
  return 0;
}

/*****************************************************************************/
int APP_CC
clipboard_deinit(void)
{
	log_message(&log_conf, LOG_LEVEL_DEBUG, "clipboard_init:"
			"clipboard deinit");
  return 0;
}


/*****************************************************************************/
int APP_CC
clipboard_data_in(struct stream* s, int chan_id, int chan_flags, int length,
                  int total_length)
{
	log_message(&log_conf, LOG_LEVEL_DEBUG, "chansrv[clipboard_data_in]:"
			"clipboard init");
	return 0;
}

/*****************************************************************************/
/* returns error
   this is called to get any wait objects for the main loop
   timeout can be nil */
int APP_CC
clipboard_get_wait_objs(tbus* objs, int* count, int* timeout)
{
	log_message(&log_conf, LOG_LEVEL_DEBUG, "chansrv[clipboard_get_wait_objs]:"
			"clipboard_get_wait_objs");
	return 0;
}

/*****************************************************************************/
int APP_CC
clipboard_check_wait_objs(void)
{
	log_message(&log_conf, LOG_LEVEL_DEBUG, "chansrv[clipboard_check_wait_objs]:"
			"clipboard_check_wait_objs");
  return 0;
}
