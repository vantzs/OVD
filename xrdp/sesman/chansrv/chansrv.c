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

#include "log.h"
#include "arch.h"
#include "os_calls.h"
#include "thread_calls.h"
#include "trans.h"
#include "chansrv.h"
#include "defines.h"
//#include "sound.h"
//#include "clipboard.h"
//#include "devredir.h"
//#include "seamrdp.h"
#include "user_channel.h"
#include "list.h"
#include "file.h"
#include "file_loc.h"

static struct trans* g_lis_trans = 0;
static struct trans* g_con_trans = 0;
static struct chan_item g_chan_items[32];
static int g_num_chan_items = 0;
static int g_cliprdr_index = -1;
static int g_rdpsnd_index = -1;
static int g_rdpdr_index = -1;
//static int g_seamrdp_index = -1;

static tbus g_term_event = 0;
static tbus g_thread_done_event = 0;
char* username = 0;
static int g_use_unix_socket = 0;
struct log_config log_conf;
int g_display_num = 0;
int g_cliprdr_chan_id = -1; /* cliprdr */
int g_rdpsnd_chan_id = -1; /* rdpsnd */
int g_rdpdr_chan_id = -1; /* rdpdr */
int g_seamrdp_chan_id = -1; /* seamrdp */

/*****************************************************************************/
/* returns error */
int APP_CC
send_channel_data(int chan_id, char* data, int size)
{
  struct stream* s;
  int chan_flags;
  int total_size;
  int sent;
  int rv;

  s = trans_get_out_s(g_con_trans, 8192);
  if (s == 0)
  {
    log_message(&log_conf, LOG_LEVEL_DEBUG, "chansrv[send_channel_data]: "
    		"No client RDP client");
    return 1;
  }
  rv = 0;
  sent = 0;
  total_size = size;
  while (sent < total_size)
  {
    size = MIN(1600, total_size - sent);
    chan_flags = 0;
    if (sent == 0)
    {
      chan_flags |= 1; /* first */
    }
    if (size + sent == total_size)
    {
      chan_flags |= 2; /* last */
    }
    out_uint32_le(s, 0); /* version */
    out_uint32_le(s, 8 + 8 + 2 + 2 + 2 + 4 + size); /* size */
    out_uint32_le(s, 8); /* msg id */
    out_uint32_le(s, 8 + 2 + 2 + 2 + 4 + size); /* size */
    out_uint16_le(s, chan_id);
    out_uint16_le(s, chan_flags);
    out_uint16_le(s, size);
    out_uint32_le(s, total_size);
    out_uint8a(s, data + sent, size);
    s_mark_end(s);
    rv = trans_force_write(g_con_trans);
    if (rv != 0)
    {
      break;
    }

    sent += size;
    s = trans_get_out_s(g_con_trans, 8192);
  }
  return rv;
}

/*****************************************************************************/
/* returns error */
static int APP_CC
send_init_response_message(void)
{
  struct stream* s;

  log_message(&log_conf, LOG_LEVEL_DEBUG, "chansrv[send_init_response_message]: ");
  s = trans_get_out_s(g_con_trans, 8192);
  if (s == 0)
  {
    return 1;
  }
  out_uint32_le(s, 0); /* version */
  out_uint32_le(s, 8 + 8); /* size */
  out_uint32_le(s, 2); /* msg id */
  out_uint32_le(s, 8); /* size */
  s_mark_end(s);
  return trans_force_write(g_con_trans);
}

/*****************************************************************************/
/* returns error */
static int APP_CC
send_channel_setup_response_message(void)
{
  struct stream* s;

  log_message(&log_conf, LOG_LEVEL_DEBUG, "chansrv[send_channel_setup_response_message]: ");
  s = trans_get_out_s(g_con_trans, 8192);
  if (s == 0)
  {
    return 1;
  }
  out_uint32_le(s, 0); /* version */
  out_uint32_le(s, 8 + 8); /* size */
  out_uint32_le(s, 4); /* msg id */
  out_uint32_le(s, 8); /* size */
  s_mark_end(s);
  return trans_force_write(g_con_trans);
}

/*****************************************************************************/
/* returns error */
static int APP_CC
send_channel_data_response_message(void)
{
  struct stream* s;

  log_message(&log_conf, LOG_LEVEL_DEBUG, "chansrv[send_channel_data_response_message]: ");
  s = trans_get_out_s(g_con_trans, 8192);
  if (s == 0)
  {
    return 1;
  }
  out_uint32_le(s, 0); /* version */
  out_uint32_le(s, 8 + 8); /* size */
  out_uint32_le(s, 6); /* msg id */
  out_uint32_le(s, 8); /* size */
  s_mark_end(s);
  return trans_force_write(g_con_trans);
}

/*****************************************************************************/
/* returns error */
static int APP_CC
process_message_init(struct stream* s)
{
	log_message(&log_conf, LOG_LEVEL_DEBUG, "chansrv[process_message_init]: ");
  return send_init_response_message();
}

/*****************************************************************************/
/* returns error */
static int APP_CC
process_message_channel_setup(struct stream* s)
{
  int num_chans;
  int index;
  int rv;
  struct chan_item* ci;

  g_num_chan_items = 0;
  g_cliprdr_index = -1;
  g_rdpsnd_index = -1;
  g_rdpdr_index = -1;
  g_cliprdr_chan_id = -1;
  g_rdpsnd_chan_id = -1;
  g_rdpdr_chan_id = -1;
  in_uint16_le(s, num_chans);
  log_message(&log_conf, LOG_LEVEL_DEBUG, "chansrv[process_message_channel_setup]: "
  		"num_chans %d", num_chans);
  for (index = 0; index < num_chans; index++)
  {
    ci = &(g_chan_items[g_num_chan_items]);
    g_memset(ci->name, 0, sizeof(ci->name));
    in_uint8a(s, ci->name, 8);
    in_uint16_le(s, ci->id);
    in_uint16_le(s, ci->flags);
    log_message(&log_conf, LOG_LEVEL_DEBUG, "chansrv[process_message_channel_setup]: "
    		"chan name '%s' id %d flags %8.8x", ci->name, ci->id, ci->flags);
/*    if (g_strcasecmp(ci->name, "cliprdr") == 0)
    {
      g_cliprdr_index = g_num_chan_items;
      g_cliprdr_chan_id = ci->id;
    }
*/
/*    else if (g_strcasecmp(ci->name, "rdpsnd") == 0)
    {
      g_rdpsnd_index = g_num_chan_items;
      g_rdpsnd_chan_id = ci->id;
    }
    else if (g_strcasecmp(ci->name, "rdpdr") == 0)
    {
      g_rdpdr_index = g_num_chan_items;
      g_rdpdr_chan_id = ci->id;
    }
*/
/*    else if (g_strcasecmp(ci->name, "seamrdp") == 0)
    {
      g_seamrdp_index = g_num_chan_items;
      g_seamrdp_chan_id = ci->id;
    }
*/
//    else
//    {
    	user_channel_init(ci->name, ci->id);
//    }
    g_num_chan_items++;
  }
  rv = send_channel_setup_response_message();
/*  if (g_cliprdr_index >= 0)
  {
    clipboard_init();
  }
  if (g_rdpsnd_index >= 0)
  {
    sound_init();
  }
  if (g_rdpdr_index >= 0)
  {
    dev_redir_init();
  }
*/
/*  if (g_seamrdp_index >= 0)
  {
    seamrdp_init();
  }
*/
  return rv;
}

/*****************************************************************************/
/* returns error */
static int APP_CC
process_message_channel_data(struct stream* s)
{
  int chan_id;
  int chan_flags;
  int rv;
  int length;
  int total_length;

  in_uint16_le(s, chan_id);
  in_uint16_le(s, chan_flags);
  in_uint16_le(s, length);
  in_uint32_le(s, total_length);
  log_message(&log_conf, LOG_LEVEL_DEBUG, "chansrv[process_message_channel_data]: "
  		"chan_id %d chan_flags %d", chan_id, chan_flags);
  rv = send_channel_data_response_message();
  if (rv == 0)
  {
/*    if (chan_id == g_cliprdr_chan_id)
    {
      rv = clipboard_data_in(s, chan_id, chan_flags, length, total_length);
    }
    else if (chan_id == g_rdpsnd_chan_id)
    {
      rv = sound_data_in(s, chan_id, chan_flags, length, total_length);
    }
    else if (chan_id == g_rdpdr_chan_id)
    {
      rv = dev_redir_data_in(s, chan_id, chan_flags, length, total_length);
    }*/
/*    else if (chan_id == g_seamrdp_chan_id)
    {
      rv = seamrdp_data_in(s, chan_id, chan_flags, length, total_length);
    }
*/
//    else
//    {
      rv = user_channel_data_in(s, chan_id, chan_flags, length, total_length);
//    }
  }
  return rv;
}

/*****************************************************************************/
/* returns error */
static int APP_CC
process_message_channel_data_response(struct stream* s)
{
	log_message(&log_conf, LOG_LEVEL_DEBUG, "chansrv[process_message_channel_data_response]: "
				"process_message_channel_data_response:");
  return 0;
}

/*****************************************************************************/
/* returns error */
static int APP_CC
process_message(void)
{
  struct stream* s;
  int size;
  int id;
  int rv;
  char* next_msg;

  if (g_con_trans == 0)
  {
    return 1;
  }
  s = trans_get_in_s(g_con_trans);
  if (s == 0)
  {
    return 1;
  }
  rv = 0;
  while (s_check_rem(s, 8))
  {
    next_msg = s->p;
    in_uint32_le(s, id);
    in_uint32_le(s, size);
    next_msg += size;
    switch (id)
    {
      case 1: /* init */
        rv = process_message_init(s);
        break;
      case 3: /* channel setup */
        rv = process_message_channel_setup(s);
        break;
      case 5: /* channel data */
        rv = process_message_channel_data(s);
        break;
      case 7: /* channel data response */
        rv = process_message_channel_data_response(s);
        break;
      default:
      	log_message(&log_conf, LOG_LEVEL_DEBUG, "chansrv[process_message]: "
                "unknown msg %d", id);
        break;
    }
    if (rv != 0)
    {
      break;
    }
    s->p = next_msg;
  }
  return rv;
}

/*****************************************************************************/
/* returns error */
int DEFAULT_CC
my_trans_data_in(struct trans* trans)
{
  struct stream* s;
  int id;
  int size;
  int error;

  if (trans == 0)
  {
    return 0;
  }
  if (trans != g_con_trans)
  {
    return 1;
  }
  log_message(&log_conf, LOG_LEVEL_DEBUG, "chansrv[my_trans_data_in]: "
				"my_trans_data_in:");
  s = trans_get_in_s(trans);
  in_uint32_le(s, id);
  in_uint32_le(s, size);
  error = trans_force_read(trans, size - 8);
  if (error == 0)
  {
    /* here, the entire message block is read in, process it */
    error = process_message();
  }
  return error;
}

/*****************************************************************************/
int DEFAULT_CC
my_trans_conn_in(struct trans* trans, struct trans* new_trans)
{
  if (trans == 0)
  {
    return 1;
  }
  if (trans != g_lis_trans)
  {
    return 1;
  }
  if (g_con_trans != 0) /* if already set, error */
  {
    return 1;
  }
  if (new_trans == 0)
  {
    return 1;
  }
  LOG(10, ("my_trans_conn_in:"));
  g_con_trans = new_trans;
  g_con_trans->trans_data_in = my_trans_data_in;
  g_con_trans->header_size = 8;
  /* stop listening */
  trans_delete(g_lis_trans);
  g_lis_trans = 0;
  return 0;
}

/*****************************************************************************/
static int APP_CC
setup_listen(void)
{
  char port[256];
  int error;

  if (g_lis_trans != 0)
  {
    trans_delete(g_lis_trans);
  }
  if (g_use_unix_socket)
  {
    g_lis_trans = trans_create(2, 8192, 8192);
    g_snprintf(port, 255, "/var/spool/xrdp/xrdp_chansrv_socket_%d", 7200 + g_display_num);
  }
  else
  {
    g_lis_trans = trans_create(1, 8192, 8192);
    g_snprintf(port, 255, "%d", 7200 + g_display_num);
  }
  g_lis_trans->trans_conn_in = my_trans_conn_in;
  error = trans_listen(g_lis_trans, port);
  if (error != 0)
  {
  	log_message(&log_conf, LOG_LEVEL_DEBUG, "chansrv[setup_listen]: "
					"setup_listen: trans_listen failed for port %s", port);
    return 1;
  }
  return 0;
}

/*****************************************************************************/
THREAD_RV THREAD_CC
channel_thread_loop(void* in_val)
{
  tbus objs[32];
  int num_objs;
  int timeout;
  int error;
  THREAD_RV rv;
  log_message(&log_conf, LOG_LEVEL_DEBUG, "chansrv[channel_thread_loop]: "
  		"channel_thread_loop: thread start");
  rv = 0;
  error = setup_listen();
  if (error == 0)
  {
    timeout = 0;
    num_objs = 0;
    objs[num_objs] = g_term_event;
    num_objs++;
    trans_get_wait_objs(g_lis_trans, objs, &num_objs, &timeout);
    while (g_obj_wait(objs, num_objs, 0, 0, timeout) == 0)
    {
      if (g_is_wait_obj_set(g_term_event))
      {
      	log_message(&log_conf, LOG_LEVEL_DEBUG, "chansrv[channel_thread_loop]: "
							"channel_thread_loop: g_term_event set");
        //clipboard_deinit();
        //sound_deinit();
        //dev_redir_deinit();
        //seamrdp_deinit();
        user_channel_deinit();
        break;
      }
      if (g_lis_trans != 0)
      {
        if (trans_check_wait_objs(g_lis_trans) != 0)
        {
        	log_message(&log_conf, LOG_LEVEL_WARNING, "chansrv[channel_thread_loop]: "
								"trans_check_wait_objs error");
        }
      }
      if (g_con_trans != 0)
      {
        if (trans_check_wait_objs(g_con_trans) != 0)
        {
        	log_message(&log_conf, LOG_LEVEL_WARNING, "chansrv[channel_thread_loop]: "
                  "trans_check_wait_objs error resetting");
          //clipboard_deinit();
          //sound_deinit();
          //dev_redir_deinit();
          //seamrdp_deinit();
          user_channel_deinit();

          trans_delete(g_con_trans);
          g_con_trans = 0;
          error = setup_listen();
          if (error != 0)
          {
            break;
          }
        }
      }
      //clipboard_check_wait_objs();
      //sound_check_wait_objs();
      //dev_redir_check_wait_objs();
      //seamrdp_check_wait_objs();
      user_channel_check_wait_objs();
      timeout = 0;
      num_objs = 0;
      objs[num_objs] = g_term_event;
      num_objs++;
      trans_get_wait_objs(g_lis_trans, objs, &num_objs, &timeout);
      trans_get_wait_objs(g_con_trans, objs, &num_objs, &timeout);
      //clipboard_get_wait_objs(objs, &num_objs, &timeout);
      //sound_get_wait_objs(objs, &num_objs, &timeout);
      //dev_redir_get_wait_objs(objs, &num_objs, &timeout);
      //seamrdp_get_wait_objs(objs, &num_objs, &timeout);
      user_channel_get_wait_objs(objs, &num_objs, &timeout);

    }
  }
  trans_delete(g_lis_trans);
  g_lis_trans = 0;
  trans_delete(g_con_trans);
  g_con_trans = 0;
  log_message(&log_conf, LOG_LEVEL_DEBUG, "chansrv[channel_thread_loop]: "
				"channel_thread_loop: thread stop");
  g_set_wait_obj(g_thread_done_event);
  return rv;
}

/*****************************************************************************/
void DEFAULT_CC
term_signal_handler(int sig)
{
	log_message(&log_conf, LOG_LEVEL_DEBUG, "chansrv[term_signal_handler]: "
				"term_signal_handler: got signal %d", sig);
  g_set_wait_obj(g_term_event);
}

/*****************************************************************************/
void DEFAULT_CC
nil_signal_handler(int sig)
{
	log_message(&log_conf, LOG_LEVEL_DEBUG, "chansrv[nil_signal_handler]: "
				"nil_signal_handler: got signal %d", sig);
}

/*****************************************************************************/
void DEFAULT_CC
stop_signal_handler(int sig)
{
	log_message(&log_conf, LOG_LEVEL_DEBUG, "chansrv[stop_signal_handler]: "
				"signal_handler: got signal %d", sig);
	g_waitchild();
}

/*****************************************************************************/
int APP_CC
main_cleanup(void)
{
  g_delete_wait_obj(g_term_event);
  g_delete_wait_obj(g_thread_done_event);
  g_deinit(); /* os_calls */
  //printf("CLEANUP\n");
  user_channel_cleanup();
  return 0;
}

/*****************************************************************************/
static int APP_CC
read_ini(void)
{
  char filename[256];
  struct list* names;
  struct list* values;
  char* name;
  char* value;
  int index;

  names = list_create();
  names->auto_free = 1;
  values = list_create();
  values->auto_free = 1;
  g_use_unix_socket = 0;
  g_snprintf(filename, 255, "%s/vchannel.ini", XRDP_CFG_PATH);
  if (file_by_name_read_section(filename, "Globals", names, values) == 0)
  {
    for (index = 0; index < names->count; index++)
    {
      name = (char*)list_get_item(names, index);
      value = (char*)list_get_item(values, index);
      if (g_strcasecmp(name, "ListenAddress") == 0)
      {
        if (g_strcasecmp(value, "127.0.0.1") == 0)
        {
          g_use_unix_socket = 1;
        }
      }
    }
  }
  list_delete(names);
  list_delete(values);
  return 0;
}


/*****************************************************************************/
static int APP_CC
read_logging_conf(void)
{
  char filename[256];
  struct list* names;
  struct list* values;
  char* name;
  char* value;
  int index;

  log_conf.program_name = g_strdup("chansrv");
  log_conf.log_file = 0;
  log_conf.fd = 0;
  log_conf.log_level = LOG_LEVEL_DEBUG;
  log_conf.enable_syslog = 0;
  log_conf.syslog_level = LOG_LEVEL_DEBUG;

  names = list_create();
  names->auto_free = 1;
  values = list_create();
  values->auto_free = 1;
  g_snprintf(filename, 255, "%s/vchannel.ini", XRDP_CFG_PATH);
  if (file_by_name_read_section(filename, CHAN_CFG_LOGGING, names, values) == 0)
  {
    for (index = 0; index < names->count; index++)
    {
      name = (char*)list_get_item(names, index);
      value = (char*)list_get_item(values, index);
      if (0 == g_strcasecmp(name, CHAN_CFG_LOG_FILE))
      {
        log_conf.log_file = g_strdup(value);
      }
      if (0 == g_strcasecmp(name, CHAN_CFG_LOG_LEVEL))
      {
        log_conf.log_level = log_text2level(value);
      }
      if (0 == g_strcasecmp(name, CHAN_CFG_LOG_ENABLE_SYSLOG))
      {
        log_conf.enable_syslog = log_text2bool(value);
      }
      if (0 == g_strcasecmp(name, CHAN_CFG_LOG_SYSLOG_LEVEL))
      {
        log_conf.syslog_level = log_text2level(value);
      }
    }
  }
  list_delete(names);
  list_delete(values);
  return 0;
}

/*****************************************************************************/
int DEFAULT_CC
chan_init()
{
  char* display_text;
  char file_string[256];
  display_text = g_getenv("DISPLAY");
  g_display_num = g_get_display_num_from_display(display_text);
  if (g_display_num == 0)
  {
  	g_printf("chansrv[chan_init]: Error, display is zero\n");
    g_exit(1);
  }

  /* spool directory */
  g_sprintf(file_string, "%s/%i", CHAN_SPOOL_DIR, g_display_num);
  g_mkdir(file_string);
  g_chown(file_string, username);
  if (g_directory_exist(file_string) == 0)
  {
  	g_printf("chansrv[chan_init]: Unable to create %s\n", file_string);
  	g_exit(1);
  }
  return 0;
}

/*****************************************************************************/
int DEFAULT_CC
main(int argc, char** argv)
{
  int pid;
  char text[256];
  if (argc != 2)
  {
  	g_printf("Usage : xrdp-chansrv 'username'\n");
  	g_exit(1);
  }
  username = argv[1];
  g_init(); /* os_calls */
  read_ini();
  read_logging_conf();
  chan_init();
  log_start(&log_conf);
  pid = g_getpid();
  log_message(&log_conf, LOG_LEVEL_DEBUG, "chansrv[main]: "
				"app started pid %d(0x%8.8x)", pid, pid);
  g_signal_kill(term_signal_handler); /* SIGKILL */
  g_signal_terminate(term_signal_handler); /* SIGTERM */
  g_signal_user_interrupt(term_signal_handler); /* SIGINT */
  g_signal_pipe(nil_signal_handler); /* SIGPIPE */
  g_signal_child_stop(stop_signal_handler);
  g_snprintf(text, 255, "xrdp_chansrv_%8.8x_main_term", pid);
  g_term_event = g_create_wait_obj(text);
  g_snprintf(text, 255, "xrdp_chansrv_%8.8x_thread_done", pid);
  g_thread_done_event = g_create_wait_obj(text);
  tc_thread_create(channel_thread_loop, 0);
  while (!g_is_wait_obj_set(g_term_event))
  {
    if (g_obj_wait(&g_term_event, 1, 0, 0, 0) != 0)
    {
    	log_message(&log_conf, LOG_LEVEL_WARNING, "chansrv[main]: "
						"main: error, g_obj_wait failed");
      break;
    }
  }
  while (!g_is_wait_obj_set(g_thread_done_event))
  {
    /* wait for thread to exit */
    if (g_obj_wait(&g_thread_done_event, 1, 0, 0, 0) != 0)
    {
    	log_message(&log_conf, LOG_LEVEL_WARNING, "chansrv[main]: "
						"main: error, g_obj_wait failed");
      break;
    }
  }
  /* cleanup */
  main_cleanup();
  log_message(&log_conf, LOG_LEVEL_INFO, "chansrv[main]: "
				"main: app exiting pid %d(0x%8.8x)", pid, pid);
  return 0;
}




/*****************************************************************************/
int APP_CC
in_unistr(struct stream* s, char *string, int str_size, int in_len)
{
#ifdef HAVE_ICONV
  size_t ibl = in_len, obl = str_size - 1;
  char *pin = (char *) s->p, *pout = string;
  static iconv_t iconv_h = (iconv_t) - 1;

  if (g_iconv_works)
  {
    if (iconv_h == (iconv_t) - 1)
    {
      if ((iconv_h = iconv_open(g_codepage, WINDOWS_CODEPAGE)) == (iconv_t) - 1)
      {
        log_message(&log_conf, LOG_LEVEL_WARNING, "chansrv[rdp_in_unistr]: iconv_open[%s -> %s] fail %p",
					WINDOWS_CODEPAGE, g_codepage, iconv_h);
        g_iconv_works = False;
        return rdp_in_unistr(s, string, str_size, in_len);
      }
    }

    if (iconv(iconv_h, (ICONV_CONST char **) &pin, &ibl, &pout, &obl) == (size_t) - 1)
    {
      if (errno == E2BIG)
      {
        log_message(&log_conf, LOG_LEVEL_WARNING, "chansrv[rdp_in_unistr]: "
							"server sent an unexpectedly long string, truncating");
      }
      else
      {
        iconv_close(iconv_h);
        iconv_h = (iconv_t) - 1;
        log_message(&log_conf, LOG_LEVEL_WARNING, "chansrv[rdp_in_unistr]: "
							"iconv fail, errno %d\n", errno);
        g_iconv_works = False;
        return rdp_in_unistr(s, string, str_size, in_len);
      }
    }

    /* we must update the location of the current STREAM for future reads of s->p */
    s->p += in_len;
    *pout = 0;
    return pout - string;
  }
  else
#endif
  {
    int i = 0;
    int len = in_len / 2;
    int rem = 0;

    if (len > str_size - 1)
    {
      log_message(&log_conf, LOG_LEVEL_WARNING, "chansrv[rdp_in_unistr]: "
						"server sent an unexpectedly long string, truncating");
      len = str_size - 1;
      rem = in_len - 2 * len;
    }
    while (i < len)
    {
      in_uint8a(s, &string[i++], 1);
      in_uint8s(s, 1);
    }
    in_uint8s(s, rem);
    string[len] = 0;
    return len;
  }
}


