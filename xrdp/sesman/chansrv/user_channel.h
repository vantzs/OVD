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

#ifndef USER_CHANNEL_H
#define USER_CHANNEL_H

#include "arch.h"
#include "parse.h"


#define SETUP_MESSAGE			0x01
#define DATA_MESSAGE			0x02
#define CHANNEL_OPEN			0x03

#define STATUS_DISCONNECTED			0x01
#define STATUS_CONNECTED				0x02


/* configuration file section and param */
#define CHANNEL_GLOBAL_CONF			"Globals"
#define CHANNEL_APP_NAME_PROP		"ApplicationName"
#define CHANNEL_APP_PATH_PROP		"ApplicationPath"
#define CHANNEL_APP_ARGS_PROP		"ApplicationArguments"
#define CHANNEL_TYPE_PROP				"ChannelType"
#define CHANNEL_TYPE_ROOT				"RootChannel"
#define CHANNEL_TYPE_USER				"UserChannel"
#define CHANNEL_TYPE_CUSTOM				"CustomChannel"

struct user_channel
{
	int channel_id;
	char channel_name[9];
	int client_channel_socket[5];
	int client_channel_count;
	int server_channel_socket;
};

int APP_CC
user_channel_do_up();

int APP_CC
user_channel_init(char* channel_name, int channel_id);
int APP_CC
user_channel_deinit(void);
int APP_CC
user_channel_data_in(struct stream* s, int chan_id, int chan_flags, int length,
                  int total_length);
int APP_CC
user_channel_get_wait_objs(tbus* objs, int* count, int* timeout);
int APP_CC
user_channel_check_wait_objs(void);

#endif
