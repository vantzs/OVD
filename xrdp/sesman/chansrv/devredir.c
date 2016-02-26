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

#include "arch.h"
#include "parse.h"
#include "os_calls.h"
#include "chansrv.h"
#include "devredir.h"
#include "printer_dev.h"
#include "xrdp_constants.h"

extern int g_rdpdr_chan_id; /* in chansrv.c */
extern struct log_config log_conf;
static int g_devredir_up = 0;
static char hostname[256];
static int use_unicode;
static int vers_major;
static int vers_minor;
static int client_id;
static int supported_operation[6] = {0};
struct device device_list[128];
int device_count = 0;
static int is_fragmented_packet = 0;
static int fragment_size;
static struct stream* splitted_packet;
static Action actions[128];
static int action_index=1;


/*****************************************************************************/
int APP_CC
dev_redir_send(struct stream* s){
  int rv;
  int size = (int)(s->end - s->data);

  rv = send_channel_data(g_rdpdr_chan_id, s->data, size);
  if (rv != 0)
  {
    log_message(&log_conf, LOG_LEVEL_ERROR, "rdpdr channel: enable to send message");
  }
  rv = log_message(&log_conf, LOG_LEVEL_DEBUG, "xrdp-devredir: send message: ");
  log_hexdump(&log_conf, LOG_LEVEL_DEBUG, (unsigned char*)s->data, size );

  return rv;
}




/* Converts a windows path to a unix path */
/*void
convert_to_unix_filename(char *filename)
{
	char *p;

	while ((p = strchr(filename, '\\')))
	{
		*p = '/';
	}
}
*/

/*****************************************************************************/
int
dev_redir_get_device_index(int device_id)
{
	int i;
	for (i=0 ; i< device_count ; i++)
	{
		if(device_list[i].device_id == device_id)
		{
			return i;
		}
	}
	return -1;
}



/*****************************************************************************/
int
dev_redir_begin_io_request(char* job,int device)
{
	struct stream* s;
	int index;
	make_stream(s);
	init_stream(s,1024);

	actions[action_index].device = device;
	actions[action_index].file_id = action_index;
	actions[action_index].last_req = IRP_MJ_CREATE;
	actions[action_index].message_id = 0;
	g_strcpy(actions[action_index].path, job);

	log_message(&log_conf, LOG_LEVEL_DEBUG, "chansrv[dev_redir_send_file]:"
  		"process job[%s]",job);
	out_uint16_le(s, RDPDR_CTYP_CORE);
	out_uint16_le(s, PAKID_CORE_DEVICE_IOREQUEST);
	out_uint32_le(s, actions[action_index].device);
	out_uint32_le(s, actions[action_index].file_id);
	out_uint32_le(s, actions[action_index].file_id);
	out_uint32_le(s, IRP_MJ_CREATE);   	/* major version */
	out_uint32_le(s, 0);								/* minor version */
	index = dev_redir_get_device_index(device);
	switch (device_list[index].device_type) {
		case RDPDR_DTYP_PRINT:
			out_uint32_le(s, 0);								/* desired access(unused) */
			out_uint64_le(s, 0);								/* size (unused) */
			out_uint32_le(s, 0);								/* file attribute (unused) */
			out_uint32_le(s, 0);								/* shared access (unused) */
			out_uint32_le(s, 0);								/* disposition (unused) */
			out_uint32_le(s, 0);								/* create option (unused) */
			out_uint32_le(s, 0);								/* path length (unused) */
			break;
		default:
			log_message(&log_conf, LOG_LEVEL_ERROR, "chansrv[dev_redir_send_file: "
					"the device type %04x is not yet supported",device);
			free_stream(s);
			return 0;
			break;
	}
	s_mark_end(s);
	dev_redir_send(s);
	free_stream(s);
  return 0;
}


/*****************************************************************************/
int APP_CC
dev_redir_process_write_io_request(int completion_id, int offset)
{
	struct stream* s;
	int fd;
	char buffer[1024];
	int size;

	make_stream(s);
	init_stream(s,1100);
	actions[completion_id].last_req = IRP_MJ_WRITE;
	log_message(&log_conf, LOG_LEVEL_DEBUG, "chansrv[dev_redir_process_write_io_request]:"
  		"process next io request[%s]",actions[completion_id].path);
	out_uint16_le(s, RDPDR_CTYP_CORE);
  out_uint16_le(s, PAKID_CORE_DEVICE_IOREQUEST);
	out_uint32_le(s, actions[completion_id].device);
	out_uint32_le(s, actions[completion_id].file_id);
	out_uint32_le(s, completion_id);
	out_uint32_le(s, IRP_MJ_WRITE);   	/* major version */
	out_uint32_le(s, 0);								/* minor version */
	if(g_file_exist(actions[completion_id].path)){
		fd = g_file_open(actions[completion_id].path);
		g_file_seek(fd, offset);
		size = g_file_read(fd, buffer, 1024);
		out_uint32_le(s,size);
		out_uint64_le(s,offset);
		out_uint8s(s,20);
		out_uint8p(s,buffer,size);
		s_mark_end(s);
		dev_redir_send(s);
		actions[completion_id].message_id++;
		free_stream(s);
		return 0;
	}
	log_message(&log_conf, LOG_LEVEL_DEBUG, "chansrv[dev_redir_process_write_io_request]:"
  		"the file %s did not exists",actions[completion_id].path);
	free_stream(s);
	return 1;
}

/*****************************************************************************/
int APP_CC
dev_redir_process_close_io_request(int completion_id)
{
	struct stream* s;

	log_message(&log_conf, LOG_LEVEL_DEBUG, "chansrv[dev_redir_process_close_io_request]:"
	  		"close file : %s",actions[completion_id].path);
	make_stream(s);
	init_stream(s,1100);
	actions[completion_id].last_req = IRP_MJ_CLOSE;
	out_uint16_le(s, RDPDR_CTYP_CORE);
	out_uint16_le(s, PAKID_CORE_DEVICE_IOREQUEST);
	out_uint32_le(s, actions[completion_id].device);
	out_uint32_le(s, actions[completion_id].file_id);
	out_uint32_le(s, completion_id);
	out_uint32_le(s, IRP_MJ_CLOSE);   	/* major version */
	out_uint32_le(s, 0);								/* minor version */
	out_uint8s(s,32);
	s_mark_end(s);
	dev_redir_send(s);
	actions[completion_id].message_id++;
	free_stream(s);
	return 0;
	free_stream(s);
}

/*****************************************************************************/
int APP_CC
dev_redir_iocompletion(struct stream* s)
{
	int device;
	int completion_id;
	int io_status;
	int result;
	int offset;
	int size;

	log_message(&log_conf, LOG_LEVEL_DEBUG, "rdpdr channel[dev_redir_iocompletion]: "
			"device reply");
	in_uint32_le(s, device);
	log_message(&log_conf, LOG_LEVEL_DEBUG, "rdpdr channel[dev_redir_iocompletion]: "
			"device : %i",device);
	in_uint32_le(s, completion_id);
	log_message(&log_conf, LOG_LEVEL_DEBUG, "rdpdr channel[dev_redir_iocompletion]: "
			"completion id : %i", completion_id);
	in_uint32_le(s, io_status);
	log_message(&log_conf, LOG_LEVEL_DEBUG, "rdpdr channel[dev_redir_iocompletion]: "
			"io_statio : %08x", io_status);
	if( io_status != STATUS_SUCCESS)
	{
		log_message(&log_conf, LOG_LEVEL_DEBUG, "rdpdr channel[dev_redir_iocompletion]: "
  			"the action failed with the status : %08x",io_status);
  	result = -1;
	}

	switch(actions[completion_id].last_req)
	{
	case IRP_MJ_CREATE:
		in_uint32_le(s, actions[completion_id].file_id);
		log_message(&log_conf, LOG_LEVEL_DEBUG, "rdpdr channel[dev_redir_iocompletion]: "
				"file %s created",actions[completion_id].last_req);
		size = g_file_size(actions[completion_id].path);
		offset = 0;
		log_message(&log_conf, LOG_LEVEL_DEBUG, "chansrv[dev_redir_next_io]: "
				"the file size to transfert: %i",size);
		result = dev_redir_process_write_io_request(completion_id, offset);
		break;

	case IRP_MJ_WRITE:
		in_uint32_le(s, size);
		log_message(&log_conf, LOG_LEVEL_DEBUG, "rdpdr channel[dev_redir_iocompletion]: "
				"%i octect written for the jobs %s",size, actions[completion_id].path);
		offset = 1024* actions[completion_id].message_id;
		size = g_file_size(actions[completion_id].path);
		if(offset > size)
		{
			result = dev_redir_process_close_io_request(completion_id);
			break;
		}
		result = dev_redir_process_write_io_request(completion_id, offset);
		break;

	case IRP_MJ_CLOSE:
		log_message(&log_conf, LOG_LEVEL_DEBUG, "rdpdr channel[dev_redir_iocompletion]: "
				"file %s closed",actions[completion_id].path);
		result = printer_dev_delete_job(actions[completion_id].path);
		break;
	default:
		log_message(&log_conf, LOG_LEVEL_DEBUG, "rdpdr channel[dev_redir_iocompletion]: "
				"last request %08x is invalid",actions[completion_id].last_req);
		result=-1;
		break;

	}
	return result;
}


/*****************************************************************************/
int APP_CC
dev_redir_init(void)
{
  struct stream* s;

  log_message(&log_conf, LOG_LEVEL_INFO, "init");
  g_devredir_up = 1;
  /*server announce*/
  make_stream(s);
  init_stream(s, 256);
  out_uint16_le(s, RDPDR_CTYP_CORE);
  out_uint16_le(s, PAKID_CORE_SERVER_ANNOUNCE);
  out_uint16_le(s, 0x1);
  out_uint16_le(s, 0x0c);
  out_uint32_le(s, 0x1);
  s_mark_end(s);
  dev_redir_send(s);
  free_stream(s);
  return 0;
}

/*****************************************************************************/
int APP_CC
dev_redir_deinit(void)
{
	int i;
	if (g_devredir_up == 0)
	{
		return 0;
	}
	log_message(&log_conf, LOG_LEVEL_DEBUG, "rdpdr channel[dev_redir_deinit]:"
			" deinit all active channels ");
  for(i=0 ; i<device_count ; i++)
  {
  	log_message(&log_conf, LOG_LEVEL_DEBUG, "rdpdr channel[dev_redir_deinit]:"
  			"device type : %i\n",device_list[i].device_type);
  	switch(device_list[i].device_type)
  	{
			case RDPDR_DTYP_PRINT:
				log_message(&log_conf, LOG_LEVEL_DEBUG, "rdpdr channel[dev_redir_deinit]:"
						" remove printer with the id %i", device_list[i].device_type);
				printer_dev_del(device_list[i].device_id);
				break;
			default:
				log_message(&log_conf, LOG_LEVEL_WARNING, "rdpdr channel[dev_redir_deinit]:"
						" unknow device type: %i", device_list[i].device_type);
				break;
  	}
  }
  g_devredir_up = 0;
  return 0;
}


/*****************************************************************************/
int APP_CC
dev_redir_data_in(struct stream* s, int chan_id, int chan_flags, int length,
                  int total_length)
{
  int component;
  int packetId;
  int result;
  struct stream* packet;

  if(length != total_length)
  {
  	log_message(&log_conf, LOG_LEVEL_DEBUG, "rdpdr channel[dev_redir_data_in]: "
  			"packet is fragmented");
  	if(is_fragmented_packet == 0)
  	{
  		log_message(&log_conf, LOG_LEVEL_DEBUG, "rdpdr channel[dev_redir_data_in]: "
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
    		log_message(&log_conf, LOG_LEVEL_DEBUG, "rdpdr channel[dev_redir_data_in]: "
    				"packet is fragmented : last part");
  			packet = splitted_packet;
  		}
  		else
  		{
    		log_message(&log_conf, LOG_LEVEL_DEBUG, "rdpdr channel[dev_redir_data_in]: "
    				"packet is fragmented : next part");
  			return 0;
  		}
  	}
  }
  else
  {
  	packet = s;
  }
  log_message(&log_conf, LOG_LEVEL_DEBUG, "rdpdr channel[dev_redir_data_in]: data received:");
  log_hexdump(&log_conf, LOG_LEVEL_DEBUG, (unsigned char*)packet->p, total_length);
  in_uint16_le(packet, component);
  in_uint16_le(packet, packetId);
  log_message(&log_conf, LOG_LEVEL_DEBUG, "rdpdr channel[dev_redir_data_in]: component=0x%04x packetId=0x%04x", component, packetId);
  if ( component == RDPDR_CTYP_CORE )
  {
    switch (packetId)
    {
    case PAKID_CORE_CLIENTID_CONFIRM :
      result = dev_redir_clientID_confirm(packet);
      break;

    case PAKID_CORE_CLIENT_NAME :
    	result =  dev_redir_client_name(packet);
    	break;

    case PAKID_CORE_CLIENT_CAPABILITY:
    	result = dev_redir_client_capability(packet);
    	break;

    case PAKID_CORE_DEVICELIST_ANNOUNCE:
    	result = dev_redir_devicelist_announce(packet);
      break;
    case PAKID_CORE_DEVICE_IOCOMPLETION:
    	result = dev_redir_iocompletion(packet);
    	break;
    default:
      log_message(&log_conf, LOG_LEVEL_WARNING, "rdpdr channel[dev_redir_data_in]: "
      		"unknown message %02x",packetId);
      result = 1;
    }
    if(is_fragmented_packet == 1)
    {
    	is_fragmented_packet = 0;
    	fragment_size = 0;
    	free_stream(packet);
    }

  }
  return result;
}

/*****************************************************************************/
int APP_CC
dev_redir_get_wait_objs(tbus* objs, int* count, int* timeout)
{
  int lcount;
  if ((!g_devredir_up) || (objs == 0) || (count == 0))
  {
    return 0;
  }
  lcount = *count;
  objs[lcount] = printer_dev_get_printer_socket();
  lcount++;
  *count = lcount;
  return 0;
}

/*****************************************************************************/
int APP_CC
dev_redir_check_wait_objs(void)
{
	char buffer[1024];
	int len;
	int device_id;

  if (!g_devredir_up)
  {
    return 0;
  }
	log_message(&log_conf, LOG_LEVEL_INFO, "rdpdr channel[dev_redir_check_wait_objs]: check wait object");
	/* check change */
	if( printer_dev_get_next_job(buffer, &device_id) == 0)
	{
		dev_redir_begin_io_request(buffer, device_id);
	}
  return 0;
}


/*****************************************************************************/
int APP_CC
dev_redir_clientID_confirm(struct stream* s)
{
    log_message(&log_conf, LOG_LEVEL_DEBUG, "rdpdr channel[dev_redir_data_in]: new message: PAKID_CORE_CLIENTID_CONFIRM");
    in_uint16_le(s, vers_major);
    in_uint32_le(s, vers_minor);
    in_uint32_le(s, client_id);
    log_message(&log_conf, LOG_LEVEL_DEBUG, "rdpdr channel[dev_redir_data_in]: version : %i:%i, client_id : %i", vers_major, vers_minor, client_id);
    return 0;
}

/*****************************************************************************/
int APP_CC
dev_redir_client_name(struct stream* s)
{
  log_message(&log_conf, LOG_LEVEL_DEBUG, "rdpdr channel[dev_redir_data_in]: new message: PAKID_CORE_CLIENT_NAME");
  int hostname_size;
  in_uint32_le(s, use_unicode);
  in_uint32_le(s, hostname_size);   /* flag not use */
  in_uint32_le(s, hostname_size);
  if (hostname_size < 1)
  {
    log_message(&log_conf, LOG_LEVEL_ERROR, "rdpdr channel[dev_redir_data_in]: no hostname specified");
    return 1;
  }
  if (use_unicode == 1)
  {
    log_message(&log_conf, LOG_LEVEL_DEBUG, "rdpdr channel[dev_redir_data_in]: unicode is used");
    in_unistr(s, hostname, sizeof(hostname), hostname_size);
  }
  else
  {
    in_uint8a(s, hostname, hostname_size);
  }
  log_message(&log_conf, LOG_LEVEL_DEBUG, "rdpdr channel[dev_redir_data_in]: hostname : '%s'",hostname);
  if (g_strlen(hostname) >0)
  {
    dev_redir_confirm_clientID_request();
  }
  return 0;
}

/*****************************************************************************/
int APP_CC
dev_redir_client_capability(struct stream* s)
{
  log_message(&log_conf, LOG_LEVEL_DEBUG, "rdpdr channel[dev_redir_client_capability]: new message: PAKID_CORE_CLIENT_CAPABILITY");
  int capability_count, ignored, temp, general_capability_version, rdp_version, i;

  in_uint16_le(s, capability_count);
  log_message(&log_conf, LOG_LEVEL_DEBUG, "rdpdr channel[dev_redir_client_capability]: capability number : %i", capability_count);
  if (capability_count == 0 )
  {
    log_message(&log_conf, LOG_LEVEL_ERROR, "rdpdr channel[dev_redir_client_capability]: No capability ");
    return 1;
  }
  in_uint16_le(s, ignored);
  /* GENERAL_CAPS_SET */
  in_uint16_le(s, temp);		/* capability type */
  if (temp != CAP_GENERAL_TYPE )
  {
    log_message(&log_conf, LOG_LEVEL_ERROR,
    		"rdpdr channel[dev_redir_client_capability]: malformed message (normaly  CAP_GENERAL_TYPE)");
    return 1;
  }
  in_uint16_le(s, ignored);		/* capability length */
  in_uint32_le(s, general_capability_version);		/* general capability version */
  log_message(&log_conf, LOG_LEVEL_DEBUG, "rdpdr channel[dev_redir_client_capability]: "
		  "general capability version = %i ",general_capability_version);
  if (general_capability_version != GENERAL_CAPABILITY_VERSION_01 &&
		  general_capability_version != GENERAL_CAPABILITY_VERSION_02  )
  {
    log_message(&log_conf, LOG_LEVEL_ERROR,
    		"rdpdr channel[dev_redir_client_capability]: malformed message "
    		"(normaly general_capability_version = [GENERAL_CAPABILITY_VERSION_01|GENERAL_CAPABILITY_VERSION_02])");
    return 1;
  }
  /* Capability message */
  in_uint32_le(s, ignored);		/* OS type */
  in_uint32_le(s, ignored);		/* OS version */
  in_uint16_le(s, ignored);		/* major version */
  in_uint16_le(s, rdp_version);	/* minor version */
  log_message(&log_conf, LOG_LEVEL_DEBUG,
      		"rdpdr channel[dev_redir_client_capability]: RDP version = %i ",rdp_version);
  if(rdp_version == RDP6)
  {
    log_message(&log_conf, LOG_LEVEL_WARNING, "rdpdr channel[dev_redir_client_capability]: "
    		"only RDP5 is supported");
  }
  in_uint16_le(s, temp);	/* oiCode1 */
  if (temp != RDPDR_IRP_MJ_ALL)
  {
    log_message(&log_conf, LOG_LEVEL_ERROR, "rdpdr channel[dev_redir_client_capability]: "
	      		"client did not support all the IRP operation");
    return 1;
  }
  in_uint16_le(s, ignored);			/* oiCode2(unused) */
  in_uint32_le(s, ignored);			/* extendedPDU(unused) */
  in_uint32_le(s, ignored);			/* extraFlags1 */
  in_uint32_le(s, ignored);			/* extraFlags2 */
  in_uint32_le(s, ignored);			/* SpecialTypeDeviceCap (device redirected before logon (smartcard/com port */

  for( i=1 ; i<capability_count ; i++ )
  {
    in_uint16_le(s, temp);	/* capability type */
    switch (temp)
    {
    case CAP_DRIVE_TYPE:
      log_message(&log_conf, LOG_LEVEL_DEBUG, "rdpdr channel[dev_redir_client_capability]: "
    		  "CAP_DRIVE_TYPE supported by the client");
      supported_operation[temp]=1;
      break;
    case CAP_PORT_TYPE:
      log_message(&log_conf, LOG_LEVEL_DEBUG, "rdpdr channel[dev_redir_client_capability]: "
    		  "CAP_PORT_TYPE supported by the client but not by the server");
      supported_operation[temp]=1;
      break;
    case CAP_PRINTER_TYPE:
      log_message(&log_conf, LOG_LEVEL_DEBUG, "rdpdr channel[dev_redir_client_capability]: "
    		  "CAP_PRINTER_TYPE supported by the client");
      supported_operation[temp]=1;
      break;
    case CAP_SMARTCARD_TYPE:
      log_message(&log_conf, LOG_LEVEL_DEBUG, "rdpdr channel[dev_redir_client_capability]: "
    		  "CAP_SMARTCARD_TYPE supported by the client but not by the server");
      supported_operation[temp]=1;
      break;
    default:
      log_message(&log_conf, LOG_LEVEL_ERROR, "rdpdr channel[dev_redir_client_capability]: "
      		  "invalid capability type : %i", temp);
      break;
    }
    in_uint16_le(s, ignored);	/* capability length */
    in_uint32_le(s, ignored);	/* capability version */
  }
  return 0;
}

/*****************************************************************************/
int APP_CC
dev_redir_devicelist_announce(struct stream* s)
{
  int device_list_count, device_id, device_type, device_data_length;
  int i;
  char dos_name[9] = {0};
  int handle;

  log_message(&log_conf, LOG_LEVEL_DEBUG, "rdpdr channel[dev_redir_devicelist_announce]: "
  		"	new message: PAKID_CORE_DEVICELIST_ANNOUNCE");
  in_uint32_le(s, device_list_count);	/* DeviceCount */
  log_message(&log_conf, LOG_LEVEL_DEBUG, "rdpdr channel[dev_redir_devicelist_announce]: "
		  "%i device(s) declared", device_list_count);
  /* device list */
  for( i=0 ; i<device_list_count ; i++)
  {
    in_uint32_le(s, device_type);
    log_message(&log_conf, LOG_LEVEL_DEBUG, "rdpdr channel[dev_redir_devicelist_announce]: "
    		"device type: %i", device_type);
    in_uint32_le(s, device_id);
    log_message(&log_conf, LOG_LEVEL_DEBUG, "rdpdr channel[dev_redir_devicelist_announce]: "
  		  "device id: %i", device_id);

    in_uint8a(s,dos_name,8)
    log_message(&log_conf, LOG_LEVEL_DEBUG, "rdpdr channel[dev_redir_devicelist_announce]: "
  		  "dos name: '%s'", dos_name);
    in_uint32_le(s, device_data_length);
    log_message(&log_conf, LOG_LEVEL_DEBUG, "rdpdr channel[dev_redir_devicelist_announce]: "
  		  "data length: %i", device_data_length);

    switch(device_type)
    {
    case RDPDR_DTYP_PRINT :
      log_message(&log_conf, LOG_LEVEL_DEBUG, "rdpdr channel[dev_redir_devicelist_announce]: "
      		  "Add printer device");
      handle = printer_dev_add(s, device_data_length, device_id, dos_name);
      if (handle != 1)
      {
      	device_list[device_count].device_id = device_id;
      	device_list[device_count].device_type = RDPDR_DTYP_PRINT;
      	device_count++;
      	dev_redir_device_list_reply(handle);
      	break;
      }
      log_message(&log_conf, LOG_LEVEL_DEBUG, "rdpdr channel[dev_redir_devicelist_announce]: "
      		  "Enable to add printer device");
      break;

    case RDPDR_DTYP_FILESYSTEM :
      log_message(&log_conf, LOG_LEVEL_DEBUG, "rdpdr channel[dev_redir_devicelist_announce]: "
        	  "Add filesystem device");
      break;
    }
  }
  return 0;
}


/*****************************************************************************/
int APP_CC
dev_redir_device_list_reply(int handle)
{
  struct stream* s;
  log_message(&log_conf, LOG_LEVEL_DEBUG, "rdpdr channel[dev_redir_device_list_reply]:"
  		" reply to the device add");
  make_stream(s);
  init_stream(s, 256);
  out_uint16_le(s, RDPDR_CTYP_CORE);
  out_uint16_le(s, PAKID_CORE_DEVICE_REPLY);
  out_uint16_le(s, 0x1);  							/* major version */
  out_uint16_le(s, RDP5);							/* minor version */
  out_uint32_le(s, client_id);							/* client ID */
  s_mark_end(s);
  dev_redir_send(s);
  free_stream(s);
  return 0;

}

/*****************************************************************************/
int APP_CC
dev_redir_confirm_clientID_request()
{
  struct stream* s;
  log_message(&log_conf, LOG_LEVEL_DEBUG, "rdpdr channel[dev_redir_send_capability_request]: Send Server Client ID Confirm Request");
  make_stream(s);
  init_stream(s, 256);
  out_uint16_le(s, RDPDR_CTYP_CORE);
  out_uint16_le(s, PAKID_CORE_CLIENTID_CONFIRM);
  out_uint16_le(s, 0x1);  							/* major version */
  out_uint16_le(s, RDP5);							/* minor version */

  s_mark_end(s);
  dev_redir_send(s);
  free_stream(s);
  return 0;
}

