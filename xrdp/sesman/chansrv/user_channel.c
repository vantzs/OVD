/**
 * Copyright (C) 2010 Ulteo SAS
 * http://www.ulteo.com
 * Author David Lechevalier <david@ulteo.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 **/

#include "user_channel.h"
#include "chansrv.h"
#include <os_calls.h>
#include <list.h>
#include <errno.h>

static int g_user_channel_up = 0;
extern char* username;
extern struct log_config log_conf;
extern int g_display_num;
static struct user_channel user_channels[15];
static int channel_count = 0;


/*****************************************************************************/
int APP_CC
user_channel_send( int chan_id, char* mess, int size){
  int rv;

  rv = send_channel_data(chan_id, mess, size);
  log_message(&log_conf, LOG_LEVEL_DEBUG_PLUS, "chansrv[user_channel_send]: "
  		"message sended: ");
  log_hexdump(&log_conf, LOG_LEVEL_DEBUG_PLUS, mess, size);
  return rv;
}

/*****************************************************************************/
int APP_CC
user_channel_cleanup(){
	return 0;
}

/*****************************************************************************/
char* APP_CC
user_channel_get_channel_app_property(const char* channel_file_conf, const char* property)
{
  char* name;
  char* value;
  struct list* names;
  struct list* values;
  int index;

  names = list_create();
  names->auto_free = 1;
  values = list_create();
  values->auto_free = 1;
  if (file_by_name_read_section(channel_file_conf, CHANNEL_GLOBAL_CONF, names, values) == 0)
  {
    for (index = 0; index < names->count; index++)
    {
      name = (char*)list_get_item(names, index);
      value = (char*)list_get_item(values, index);
      if (0 == g_strcasecmp(name, property))
      {
        if( g_strlen(value) > 1)
        {
        	return value;
        }
      }
    }
  }
  return "";
}

/*****************************************************************************/
int APP_CC
user_channel_transmit(int socket, int type, char* mess, int length, int total_length )
{
  struct stream* header;
  int rv;
  make_stream(header);
  init_stream(header, 9);
  out_uint8(header, type);
  out_uint32_be(header, length);
  out_uint32_be(header, total_length);
  s_mark_end(header);
  rv = g_tcp_send(socket, header->data, 9, 0);
  log_message(&log_conf, LOG_LEVEL_DEBUG_PLUS, "chansrv[user_channel_transmit]: "
  		"Header sended:");
  log_hexdump(&log_conf, LOG_LEVEL_DEBUG_PLUS, header->data, 9);
  if (rv != 9)
  {
    log_message(&log_conf, LOG_LEVEL_ERROR, "chansrv[user_channel_transmit]: "
    		"Error while sending the header");
    free_stream(header);
    return rv;
  }
  free_stream(header);
  rv = g_tcp_send(socket, mess, length, 0);
  log_message(&log_conf, LOG_LEVEL_DEBUG_PLUS, "chansrv[user_channel_transmit]: "
  		"Message sended:");
  log_hexdump(&log_conf, LOG_LEVEL_DEBUG_PLUS, mess, length);
  if (rv != length)
  {
    log_message(&log_conf, LOG_LEVEL_ERROR, "chansrv[user_channel_transmit]: "
    		"Error while sending the message: %s", mess);
  }
	return rv;
}

/*****************************************************************************/
int APP_CC
user_channel_launch_server_channel(char* channel_name)
{
	char channel_file_conf[256];
	char* channel_program_name;
	char channel_program_path[256];
	char* channel_program_arguments;
	char* channel_type;
	char* display_string;
	struct list* channel_params;
	int pid = 0;

	g_sprintf(channel_file_conf, "%s/%s.conf", XRDP_CFG_PATH, channel_name);
	if (!g_file_exist(channel_file_conf))
	{
		log_message(&log_conf, LOG_LEVEL_DEBUG, "chansrv[user_channel_launch_server_channel]: "
				"Channel %s is not registered ", channel_name);
		return 0;
	}
	channel_program_name = user_channel_get_channel_app_property(channel_file_conf, CHANNEL_APP_NAME_PROP);
	channel_type = user_channel_get_channel_app_property(channel_file_conf, CHANNEL_TYPE_PROP);
	channel_program_arguments = user_channel_get_channel_app_property(channel_file_conf, CHANNEL_APP_ARGS_PROP);


	if (g_strcmp(channel_program_name, "") == 0 || g_strcmp(channel_type, "") == 0)
	{
		log_message(&log_conf, LOG_LEVEL_WARNING, "chansrv[user_channel_launch_server_channel]: "
				"Channel conf file for %s is not correct", channel_name);
		return 1;
	}

	if (g_strcmp(channel_type, CHANNEL_TYPE_CUSTOM) == 0)
	{
		log_message(&log_conf, LOG_LEVEL_DEBUG, "chansrv[user_channel_launch_server_channel]: "
				"The custom channel %s must be start by the user", channel_name);
		return 1;
	}

	g_snprintf(channel_program_path, 256, "%s/%s", XRDP_SBIN_PATH, channel_program_name);
	log_message(&log_conf, LOG_LEVEL_DEBUG, "chansrv[user_channel_launch_server_channel]: "
			"Channel app name for %s: %s", channel_name, channel_program_name);
	log_message(&log_conf, LOG_LEVEL_DEBUG, "chansrv[user_channel_launch_server_channel]: "
			"Channel app path for %s: %s", channel_name, channel_program_path);
	log_message(&log_conf, LOG_LEVEL_DEBUG, "chansrv[user_channel_launch_server_channel]: "
				"Channel type for %s: %s", channel_name, channel_type);

	channel_params = list_create();
	channel_params->auto_free = 1;

	/* building parameters */
	list_add_item(channel_params, (long)g_strdup(channel_program_path));
	list_add_item(channel_params, (long)g_strdup(username));
	list_add_item(channel_params, 0);

	if( g_strcmp(channel_type, CHANNEL_TYPE_ROOT) == 0)
	{
		pid = g_launch_process(g_display_num, channel_params, 0);
	}
	else
	{
		pid = g_su(username, g_display_num, channel_params, 0);
	}
	if (pid == 0)
	{
		log_message(&log_conf, LOG_LEVEL_WARNING, "chansrv[user_channel_launch_server_channel]: "
					"Unable to launch the channel application %s ", channel_program_path);
		return 1;
	}
	return 0;
}

/*****************************************************************************/
int APP_CC
user_channel_do_up(char* chan_name)
{
  log_message(&log_conf, LOG_LEVEL_DEBUG, "chansrv[user_channel_do_up]: "
  		"Activate the channel '%s'", chan_name);
	char socket_filename[256];
	int sock = 0;
	g_sprintf(socket_filename, "/var/spool/xrdp/%i/vchannel_%s", g_display_num, chan_name);
	sock = g_create_unix_socket(socket_filename);
	g_chown(socket_filename, username);
  log_message(&log_conf, LOG_LEVEL_DEBUG, "chansrv[user_channel_do_up]: "
  		"Channel socket '%s' is created", socket_filename);
  user_channel_launch_server_channel(chan_name);

	return sock;
}

/*****************************************************************************/
int APP_CC
user_channel_get_channel_from_name(char* channel_name)
{
	int i;
	int sock;
	for(i=0 ; i<channel_count ; i++)
	{
		if(strcmp(user_channels[i].channel_name, channel_name) == 0)
		{
			return i;
		}
	}
	return -1;
}

/*****************************************************************************/
int APP_CC
user_channel_init(char* channel_name, int channel_id)
{
	int sock = 0;
	int i;
	int channel_index = -1;
	char status[1];

	if (g_user_channel_up == 0)
	{
		g_user_channel_up = 1;
	}
	channel_index = user_channel_get_channel_from_name(channel_name);
	if (channel_index != -1 )
	{
		sock = user_channels[channel_index].client_channel_socket[0];
	}
	if (sock != 0 )
	{
		status[0] = STATUS_CONNECTED;
		user_channel_transmit(sock, SETUP_MESSAGE, status, 1, 1);
		return 0;
	}
	log_message(&log_conf, LOG_LEVEL_DEBUG, "chansrv[user_channel_init]: "
			"new client connection for channel '%s' with id= %i ", channel_name, channel_id);
	sock = user_channel_do_up(channel_name);
	if (sock < 0)
	{
		log_message(&log_conf, LOG_LEVEL_ERROR, "chansrv[user_channel_init]: "
				"Unable to open channel %s", channel_name);
		return 0;
	}
	if (channel_index == -1)
	{
		channel_index = channel_count;
		channel_count++;
	}
	user_channels[channel_index].channel_id = channel_id;
	g_strcpy(user_channels[channel_index].channel_name, channel_name);
	user_channels[channel_index].server_channel_socket = sock;
	return 0;
}

/*****************************************************************************/
int APP_CC
user_channel_deinit(void)
{
	char status[1];
	int i;
	int j;
	int socket;
	status[0] = STATUS_DISCONNECTED;

	for (i=0 ; i<channel_count ; i++ )
	{
		for (j=0 ; j<user_channels[i].client_channel_count ; j++ )
		{
			socket = user_channels[i].client_channel_socket[j];
			user_channel_transmit(socket, SETUP_MESSAGE, status, 1, 1 );
		}
	}
	return 0;
}

/*****************************************************************************/
int APP_CC
user_channel_data_in(struct stream* s, int chan_id, int chan_flags, int length,
                  int total_length)
{
	int i;
	int size;

	for(i=0; i<channel_count; i++)
	{
		if( user_channels[i].channel_id == chan_id )
		{
		  log_message(&log_conf, LOG_LEVEL_DEBUG_PLUS, "chansrv[user_channel_data_in]: "
		  		"new client message for channel %s ",user_channels[i].channel_name);
			log_hexdump(&log_conf, LOG_LEVEL_DEBUG_PLUS, (unsigned char*)s->p, length);
			//user_channels[i].client_channel_socket[0] -> the main client socket
			if(user_channels[i].client_channel_socket[0] == 0 )
			{
			  log_message(&log_conf, LOG_LEVEL_DEBUG, "chansrv[user_channel_data_in]: "
			  		"server side channel is not opened");
			  s->p = s->end;
				return 0;
			}
			user_channel_transmit(user_channels[i].client_channel_socket[0], DATA_MESSAGE, s->p, length, total_length);
			return 0;
		}
	}

  log_message(&log_conf, LOG_LEVEL_DEBUG, "chansrv[user_channel_data_in]: "
  		"the channel id %i is invalid",chan_id);

	return 0;
}

/*****************************************************************************/
int APP_CC
user_channel_get_wait_objs(tbus* objs, int* count, int* timeout)
{
  int lcount;
  int i;
  int j;

  if ((!g_user_channel_up) || (objs == 0) || (count == 0))
  {
    return 0;
  }
  lcount = *count;
  for (i=0 ; i<channel_count ; i++)
  {
  	if( user_channels[i].server_channel_socket != 0)
  	{
  		objs[lcount] = user_channels[i].server_channel_socket;
  		lcount++;
  	}
		for (j=0 ; j<user_channels[i].client_channel_count ; j++ )
		{
			if( user_channels[i].client_channel_socket[j] != 0)
			{
				objs[lcount] = user_channels[i].client_channel_socket[j];
				lcount++;
			}
		}
  }
  *count = lcount;
  return 0;
}

/************************************************************************/
int DEFAULT_CC
user_channel_process_channel_opening(int channel_index, int client)
{
	struct stream* s;
  int data_length;
  int size;
  int type;
  int count;

	make_stream(s);
	init_stream(s, 1024);

	size = g_tcp_recv(client, s->data, sizeof(int)+1, 0);

	if ( size < 1)
	{
		log_message(&log_conf, LOG_LEVEL_DEBUG, "chansrv[user_channel_process_channel_opening]: "
				"Unable to get information on the opening channel");
		g_tcp_close(client);
		free_stream(s);
		return 0;
	}
	in_uint8(s, type);
	if(type != CHANNEL_OPEN)
	{
		log_message(&log_conf, LOG_LEVEL_DEBUG, "chansrv[user_channel_process_channel_opening]: "
				"Invalid operation type");
		free_stream(s);
		return 0;
	}
	in_uint32_be(s, data_length);
	log_message(&log_conf, LOG_LEVEL_DEBUG, "chansrv[user_channel_process_channel_opening]: "
			"Data_length : %i", data_length);
	size = g_tcp_recv(client, s->data, data_length, 0);
	s->data[data_length] = 0;
	log_message(&log_conf, LOG_LEVEL_DEBUG, "chansrv[user_channel_process_channel_opening]: "
			"Channel name : %s",s->data);

	if(g_strcmp(user_channels[channel_index].channel_name, s->data) == 0)
	{
		log_message(&log_conf, LOG_LEVEL_DEBUG, "chansrv[user_channel_process_channel_opening]: "
				"New server connection for channel %s ", user_channels[channel_index].channel_name);
		count = user_channels[channel_index].client_channel_count;
		user_channels[channel_index].client_channel_socket[count] = client;
		log_message(&log_conf, LOG_LEVEL_DEBUG, "chansrv[user_channel_process_channel_opening]: "
				"Socket : %i", client);
		user_channels[channel_index].client_channel_count++;
	}
	else
	{
		log_message(&log_conf, LOG_LEVEL_DEBUG, "chansrv[user_channel_process_channel_opening]: "
				"Unable to open a channel without client");
	}
	free_stream(s);
	return 0;
}



/*****************************************************************************/
int APP_CC
user_channel_check_wait_objs(void)
{
  struct stream* s;
  struct stream* header;
  int data_length;
	int size;
	int i;
	int j;
	int new_client;
	int test;
	int type;
	int sock;

	for( i=0 ; i<channel_count; i++)
	{
		test = g_is_wait_obj_set(user_channels[i].server_channel_socket);
		if (test)
	  {
  	  log_message(&log_conf, LOG_LEVEL_DEBUG, "chansrv[user_channel_check_wait_objs]: "
  	  		"New server side channel connection for channel %s : ",user_channels[i].channel_name);
			new_client = g_wait_connection(user_channels[i].server_channel_socket);
			user_channel_process_channel_opening(i, new_client);
			return 0;
		}
	}
	for( i=0 ; i<channel_count ; i++)
  {
		for( j=0 ; j<user_channels[i].client_channel_count; j++)
		{
			sock = user_channels[i].client_channel_socket[j];
			test = g_is_wait_obj_set(sock);
			if (test)
			{
				log_message(&log_conf, LOG_LEVEL_DEBUG, "chansrv[user_channel_check_wait_objs]: "
						"New data from channel '%s'",user_channels[i].channel_name);
				make_stream(header);
				init_stream(header, 5);

				size = g_tcp_recv(sock, header->data, 5, 0);
				if ( size != 5)
				{
					log_message(&log_conf, LOG_LEVEL_DEBUG, "chansrv[user_channel_check_wait_objs]: "
							"Channel %s closed : [%s]",user_channels[i].channel_name, g_get_strerror());
					g_tcp_close(sock);
					user_channels[i].client_channel_count = 0;
					user_channels[i].client_channel_socket[0] = 0;
					free_stream(header);
					continue;
				}
				in_uint8(header, type);
				if(type != DATA_MESSAGE)
				{
					log_message(&log_conf, LOG_LEVEL_DEBUG, "chansrv[user_channel_process_channel_opening]: "
							"Invalid operation type");
					free_stream(header);
					return 0;
				}
				in_uint32_be(header, data_length);
				log_message(&log_conf, LOG_LEVEL_DEBUG, "chansrv[user_channel_check_wait_objs]: "
						"Data_length : %i\n", data_length);
				free_stream(header);

				make_stream(s);
				init_stream(s, data_length);
				size = g_tcp_recv(sock, s->data, data_length, 0);
				if ( size != data_length)
				{
					log_message(&log_conf, LOG_LEVEL_DEBUG, "chansrv[user_channel_check_wait_objs]: "
							"Unable to read data message");
					free_stream(s);
					continue;
				}
				//s->data[data_length] = 0;
				log_message(&log_conf, LOG_LEVEL_DEBUG_PLUS, "chansrv[user_channel_check_wait_objs]: "
						"Data:");
				log_hexdump(&log_conf, LOG_LEVEL_DEBUG_PLUS, s->data, data_length);
				if( user_channels[i].channel_id == -1)
				{
					log_message(&log_conf, LOG_LEVEL_DEBUG, "chansrv[user_channel_check_wait_objs]: "
							"Client channel is not opened");
					free_stream(s);
					return 0 ;
				}
				user_channel_send(user_channels[i].channel_id, s->data, data_length);
				free_stream(s);
			}
		}
  }
	return 0;
}

