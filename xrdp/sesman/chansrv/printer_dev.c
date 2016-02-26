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

#include "printer_dev.h"
#include <cups/cups.h>
//#include <cups/i18n.h> /* hardy problems */
#include <errno.h>
#include <zlib.h>
#include <sys/inotify.h>
#include <dirent.h>

extern struct log_config log_conf;
extern char* username;
static struct printer_device printer_devices[128];
static int printer_devices_count = 0;
static char user_spool_dir[256];
static int printer_sock;

/************************************************************************/
int				/* O - 0 if name is no good, 1 if name is good */
printer_dev_validate_name(const char *name)		/* I - Name to check */
{
  const char	*ptr;			/* Pointer into name */
  for (ptr = name; *ptr; ptr ++)
  {
    if (*ptr == '@')
    {
      break;
    }
    else
    {
    	if ((*ptr >= 0 && *ptr <= ' ') || *ptr == 127 ||
    			*ptr == '/' || *ptr == '#')
    	{
    		return 0;
    	}
    }
  }
  return ((ptr - name) < 128);
}


/************************************************************************/
int				/* O - 0 if name is no good, 1 if name is good */
printer_dev_convert_name(char *name)		/* I - Name to check */
{
  char	*ptr;			/* Pointer into name */
  for (ptr = name; *ptr; ptr ++)
  {
    if (*ptr == '@')
    {
      *ptr = '_';
    }
    else
    {
    	if ((*ptr >= 0 && *ptr <= ' ') || *ptr == 127 ||
    			*ptr == '/' || *ptr == '#' || *ptr == '!')
    	{
    		*ptr = '_';
    	}
    }
  }
  return 0;
}


/************************************************************************/
int
printer_dev_server_connect(http_t **http )
{
	if (! *http)
	{
		*http = httpConnectEncrypt(cupsServer(), ippPort(), cupsEncryption());
		if (*http == NULL)
	  {
			return 1;
	  }
	}
	return 0;
}

/************************************************************************/
int
printer_dev_server_disconnect(http_t *http )
{
	if (! http)
	{
		httpClose(http);
	}
	return 0;
}

/************************************************************************/
int
printer_dev_add_printer(http_t* http, char* lp_name, char* device_uri)
{
  ipp_t		*request = NULL,		/* IPP Request */
					*response =NULL;		/* IPP Response */
  char		uri[HTTP_MAX_URI] = {0};	/* URI for printer/class */

  log_message(&log_conf, LOG_LEVEL_DEBUG, "chansrv[printer_dev_add_printer]: "
  		"add_printer %s of type %s ", lp_name,device_uri);
  request = ippNewRequest(CUPS_ADD_MODIFY_PRINTER);

  httpAssembleURIf(HTTP_URI_CODING_ALL, uri, sizeof(uri), "ipp", NULL,
                   "localhost", 0, "/printers/%s", lp_name);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI,
               "printer-uri", NULL, uri);

  if (device_uri[0] == '/')
  {
    snprintf(uri, sizeof(uri), "file://%s", device_uri);
    ippAddString(request, IPP_TAG_PRINTER, IPP_TAG_URI, "device-uri", NULL, uri);
  }
  else
  {
    ippAddString(request, IPP_TAG_PRINTER, IPP_TAG_URI, "device-uri", NULL, device_uri);
  }

  if ((response = cupsDoRequest(http, request, "/admin/")) == NULL)
  {
    log_message(&log_conf, LOG_LEVEL_WARNING, "chansrv[printer_dev_add_printer]: "
    		" %s", cupsLastErrorString());
    return 1;
  }
  else if (response->request.status.status_code > IPP_OK_CONFLICT)
  {
    log_message(&log_conf, LOG_LEVEL_WARNING, "chansrv[printer_dev_add_printer]: "
    		" %s", cupsLastErrorString());
    ippDelete(response);
    return 1;
  }
  else
  {
    ippDelete(response);
    return (0);
  }
}


/************************************************************************/
int
printer_dev_del_printer(http_t* http, char* lp_name)
{
	ipp_t		*request,		/* IPP Request */
					*response;		/* IPP Response */
	char		uri[HTTP_MAX_URI];	/* URI for printer/class */

  log_message(&log_conf, LOG_LEVEL_DEBUG, "chansrv[printer_dev_del_printer]: "
			"delete_printer %s", lp_name);
	request = ippNewRequest(CUPS_DELETE_PRINTER);
	httpAssembleURIf(HTTP_URI_CODING_ALL, uri, sizeof(uri), "ipp", NULL,
                 "localhost", 0, "/printers/%s", lp_name);
	ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI,
             "printer-uri", NULL, uri);

	if ((response = cupsDoRequest(http, request, "/admin/")) == NULL)
	{
	  log_message(&log_conf, LOG_LEVEL_WARNING, "chansrv[printer_dev_del_printer]: "
    		" %s", cupsLastErrorString());
		return 1;
	}
	else if (response->request.status.status_code > IPP_OK_CONFLICT)
	{
	  log_message(&log_conf, LOG_LEVEL_WARNING, "chansrv[printer_dev_del_printer]: "
    		" %s", cupsLastErrorString());
    ippDelete(response);
    return 1;
	}
	else
	{
		ippDelete(response);
		return 0;
	}
}



/************************************************************************/
int
printer_dev_set_ppd(http_t *http, char* lp_name, char* ppd_file)
{
  ipp_t		*request,		/* IPP Request */
					*response;		/* IPP Response */
  char		uri[HTTP_MAX_URI];	/* URI for printer/class */
  char		tempfile[1024];		/* Temporary filename */
  int		fd;			/* Temporary file */
  gzFile	*gz;			/* GZIP'd file */
  char		buffer[8192];		/* Copy buffer */
  int		bytes;			/* Bytes in buffer */
  int size;


  log_message(&log_conf, LOG_LEVEL_DEBUG, "chansrv[printer_dev_set_ppd]: "
  		"set ppd file %s",ppd_file);

 /*
  * See if the file is gzip'd; if so, unzip it to a temporary file and
  * send the uncompressed file.
  */

  if (!strcmp(ppd_file + strlen(ppd_file) - 3, ".gz"))
  {
   /*
    * Yes, the file is compressed; uncompress to a temp file...
    */

    if ((fd = cupsTempFd(tempfile, sizeof(tempfile))) < 0)
    {
      log_message(&log_conf, LOG_LEVEL_WARNING, "chansrv[printer_dev_set_ppd]: "
      		"unable to create temporary file");
      return (1);
    }

    if ((gz = gzopen(ppd_file, "rb")) == NULL)
    {
      log_message(&log_conf, LOG_LEVEL_WARNING, "chansrv[printer_dev_set_ppd]: "
      		"unable to open file \"%s\": %s",ppd_file, strerror(errno));
      close(fd);
      unlink(tempfile);
      return (1);
    }

    while ((bytes = gzread(gz, buffer, sizeof(buffer))) > 0)
    {
      size = write(fd, buffer, bytes);
    }

    close(fd);
    gzclose(gz);

    ppd_file = tempfile;
  }

 /*
  * Build a CUPS_ADD_PRINTER request, which requires the following
  * attributes:
  *
  *    attributes-charset
  *    attributes-natural-language
  *    printer-uri
  */

  request = ippNewRequest(CUPS_ADD_PRINTER);

  httpAssembleURIf(HTTP_URI_CODING_ALL, uri, sizeof(uri), "ipp", NULL,
                   "localhost", 0, "/printers/%s", lp_name);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI,
               "printer-uri", NULL, uri);

 /*
  * Do the request and get back a response...
  */

  response = cupsDoFileRequest(http, request, "/admin/", ppd_file);
  ippDelete(response);

 /*
  * Remove the temporary file as needed...
  */

  if (ppd_file == tempfile)
    unlink(tempfile);

  if (cupsLastError() > IPP_OK_CONFLICT)
  {
    log_message(&log_conf, LOG_LEVEL_WARNING, "chansrv[printer_dev_set_ppd]: "
					"%s", cupsLastErrorString());

    return (1);
  }
  else
    return (0);
}

/************************************************************************/
int
printer_dev_do_operation(http_t * http, int operation, char* lp_name)
{
	ipp_t		*request;		/* IPP Request */
	char		uri[HTTP_MAX_URI];	/* URI for printer/class */
	char* reason = NULL;

  log_message(&log_conf, LOG_LEVEL_DEBUG, "chansrv[printer_dev_do_operation]: "
				"do operation (%i => %p, \"%s\")", operation, http, lp_name);
  request = ippNewRequest(operation);

  httpAssembleURIf(HTTP_URI_CODING_ALL, uri, sizeof(uri), "ipp", NULL,
                   "localhost", 0, "/printers/%s", lp_name);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI,
               "printer-uri", NULL, uri);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME,
               "requesting-user-name", NULL, cupsUser());


 	ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_TEXT,
                 "printer-state-message", NULL, reason);

  ippDelete(cupsDoRequest(http, request, "/admin/"));

  if (cupsLastError() > IPP_OK_CONFLICT)
  {
    log_message(&log_conf, LOG_LEVEL_WARNING, "chansrv[printer_dev_do_operation]: "
					"operation failed: %s", ippErrorString(cupsLastError()));
  	return 1;
  }

  request = ippNewRequest(IPP_PURGE_JOBS);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI,
                 "printer-uri", NULL, uri);

  ippDelete(cupsDoRequest(http, request, "/admin/"));

  if (cupsLastError() > IPP_OK_CONFLICT)
	{
    log_message(&log_conf, LOG_LEVEL_WARNING, "chansrv[printer_dev_do_operation]: "
    		"%s", cupsLastErrorString());
  	return 1;
	}
  return 0;
}

/************************************************************************/
int
printer_dev_get_restricted_user_list(http_t* http, char* lp_name, char* user_list)
{
  ipp_t	*request;		/* IPP Request */
	ipp_t	*response;
  char uri[HTTP_MAX_URI];	/* URI for printer/class */
  ipp_attribute_t *attr;
  int i=0;
  char* user_p = user_list;
  int size = 0;

  httpAssembleURIf(HTTP_URI_CODING_ALL, uri, sizeof(uri), "ipp", NULL,
                   "localhost", 0, "/printers/%s", lp_name);
  request = ippNewRequest(IPP_GET_PRINTER_ATTRIBUTES);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri", NULL, uri);
  response = cupsDoRequest(http, request, "/admin/");
  if (cupsLastError() > IPP_OK_CONFLICT)
  {
    log_message(&log_conf, LOG_LEVEL_WARNING, "chansrv[printer_dev_get_restricted_user_list]: "
					"operation failed: %s", ippErrorString(cupsLastError()));
  	ippDelete(response);
  	return 1;
  }
  attr = ippFindAttribute(response, "requesting-user-name-allowed", IPP_TAG_NAME);
  if (attr == 0 || attr->num_values == 0)
  {
  	ippDelete(response);
  	return 0;
  }
  for(i=0 ; i < attr->num_values ; i++)
  {
  	size = sprintf(user_p, "%s,",attr->values[i].string.text);
  	user_p+= size;
  }
  log_message(&log_conf, LOG_LEVEL_DEBUG, "chansrv[printer_dev_get_restricted_user_list]: "
				"requesting-user-name-allowed : %s", user_list);
  ippDelete(response);
  return attr->num_values;
}

/************************************************************************/
int
printer_dev_set_restrict_user_list(http_t* http, char* lp_name, char* user_list)
{
	int num_options = 0;
  cups_option_t	*options;
  ipp_t	*request;		/* IPP Request */
  char uri[HTTP_MAX_URI];	/* URI for printer/class */

  /*
  * Add the options...
  */

  httpAssembleURIf(HTTP_URI_CODING_ALL, uri, sizeof(uri), "ipp", NULL,
                  "localhost", 0, "/printers/%s", lp_name);

  request = ippNewRequest(CUPS_ADD_MODIFY_PRINTER);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri", NULL, uri);
  num_options = cupsAddOption("requesting-user-name-denied",
	                          "all", num_options, &options);
  num_options = cupsAddOption("requesting-user-name-allowed",
                          user_list, num_options,&options);
  cupsEncodeOptions2(request, num_options, options, IPP_TAG_PRINTER);
  ippDelete(cupsDoRequest(http, request, "/admin/"));

  if (cupsLastError() > IPP_OK_CONFLICT)
  {
    log_message(&log_conf, LOG_LEVEL_WARNING, "chansrv[printer_dev_set_restrict_user_list]: "
						"%s", cupsLastErrorString());
    return 1;
  }
  else
  {
    return 0;
  }
}

/************************************************************************/
int APP_CC
printer_dev_add(struct stream* s, int device_data_length,
								int device_id, char* dos_name)
{
  int flags;
  int ignored;
  int pnp_name_len;
  int printer_name_len;
  int driver_name_len;
  int cached_field_name;
  char pnp_name[256] = {0};			/* ignored */
  char driver_name[256] = {0};
  char printer_name[256] = {0};
  char user_list[1024] = {0};
	http_t *http = NULL;
	int watch;

  in_uint32_le(s, flags);
  log_message(&log_conf, LOG_LEVEL_DEBUG, "chansrv[printer_dev_add]: "
		  "flags = %i", flags);
  in_uint32_le(s, ignored);
  in_uint32_le(s, pnp_name_len);
  log_message(&log_conf, LOG_LEVEL_DEBUG, "chansrv[printer_dev_add]: "
		  "pnp_name_len = %i", pnp_name_len);
  in_uint32_le(s, driver_name_len);
  log_message(&log_conf, LOG_LEVEL_DEBUG, "chansrv[printer_dev_add]: "
		  "driver_name_len = %i", driver_name_len);
  in_uint32_le(s, printer_name_len);
  log_message(&log_conf, LOG_LEVEL_DEBUG, "chansrv[printer_dev_add]: "
		  "print_name_len = %i", printer_name_len);
  in_uint32_le(s, cached_field_name);
  if(pnp_name_len != 0)
  {
    in_unistr(s, pnp_name, sizeof(pnp_name), pnp_name_len);
    log_message(&log_conf, LOG_LEVEL_DEBUG, "chansrv[printer_dev_add]: "
  		  "pnp_name = %s", pnp_name);
  }
  if(driver_name_len != 0)
  {
    in_unistr(s, driver_name, sizeof(driver_name), driver_name_len);
    log_message(&log_conf, LOG_LEVEL_DEBUG, "chansrv[printer_dev_add]: "
		  "driver_name = %s", driver_name);
  }
  if(printer_name_len != 0)
  {
    in_unistr(s, printer_name, sizeof(printer_name), printer_name_len);
    log_message(&log_conf, LOG_LEVEL_DEBUG, "chansrv[printer_dev_add]: "
		  "printer name = %s", printer_name);
  }
  printer_dev_convert_name(printer_name);

  log_message(&log_conf, LOG_LEVEL_DEBUG, "chansrv[printer_dev_add]: "
				"try to connect to cups server");
	if (printer_dev_server_connect(&http) == 1)
	{
	  log_message(&log_conf, LOG_LEVEL_WARNING, "chansrv[printer_dev_add]: "
					"enable to connect to printer server\n");
		return 1;
	}
	if( printer_dev_add_printer(http, printer_name, DEVICE_URI) !=0)
	{
		log_message(&log_conf, LOG_LEVEL_ERROR, "failed to add printer\n");
		printer_dev_server_disconnect(http);
		return 1;
	}
	if(printer_dev_set_ppd(http, printer_name, PPD_FILE) !=0)
	{
		log_message(&log_conf, LOG_LEVEL_ERROR, "failed to set ppd file\n");
		printer_dev_server_disconnect(http);
		return 1;
	}
	log_message(&log_conf, LOG_LEVEL_DEBUG, "Succed to add printer\n");
	printer_dev_do_operation(http, IPP_RESUME_PRINTER, printer_name);
	printer_dev_do_operation(http, CUPS_ACCEPT_JOBS, printer_name);
	if( printer_dev_get_restricted_user_list(http, printer_name, user_list) == 0)
	{
		g_strncpy(user_list, username, sizeof(username));
	}
	else
	{
		sprintf(user_list, "%s%s", user_list, username);
	}
	printer_dev_set_restrict_user_list(http, printer_name, user_list);
	printer_dev_server_disconnect(http);
	watch = printer_dev_init_printer_socket(printer_name);
	printer_devices[printer_devices_count].watch = watch;
	printer_devices[printer_devices_count].device_id = device_id;
	g_strcpy(printer_devices[printer_devices_count].printer_name, printer_name);
	printer_devices_count++;
  return g_time1();
}

/************************************************************************/
int APP_CC
printer_dev_get_printer(int device_id)
{
	int i;
	for (i=0 ; i< printer_devices_count ; i++)
	{
		if(device_id == printer_devices[i].device_id)
		{
			return i;
		}
	}
	return -1;
}

/************************************************************************/
int APP_CC
printer_dev_get_printer_from_watch(int watch)
{
	int i;
	for (i=0 ; i< printer_devices_count ; i++)
	{
		if(watch == printer_devices[i].watch)
		{
			return i;
		}
	}
	return -1;
}

/************************************************************************/
int APP_CC
printer_dev_del(int device_id)
{
	int printer_index;
	char user_list[1024] = {0};
	char* lp_name;
	char* user_list_p;
	http_t *http = NULL;

	log_message(&log_conf, LOG_LEVEL_DEBUG, "chansrv[printer_dev_del]:"
			"printer remove : %i",device_id);
	printer_index = printer_dev_get_printer(device_id);
	if (printer_index == -1)
	{
		log_message(&log_conf, LOG_LEVEL_DEBUG, "chansrv[printer_dev_del]:"
				"the printer %i did not exist",device_id);
		return 1;
	}
	lp_name = printer_devices[printer_index].printer_name;

	log_message(&log_conf, LOG_LEVEL_DEBUG, "chansrv[printer_dev_del]:"
				"try to connect to cups server");
	if (printer_dev_server_connect(&http) == 1)
	{
		log_message(&log_conf, LOG_LEVEL_DEBUG, "chansrv[printer_dev_del]:"
				"enable to connect to printer server\n");
		return 1;
	}

	printer_dev_get_restricted_user_list(http, lp_name, user_list);
	user_list_p = &user_list;
	if (g_str_replace_first(user_list, username, "") == 1)
	{
		log_message(&log_conf, LOG_LEVEL_WARNING, "chansrv[printer_dev_del]:"
				"enable to delete user restriction\n");
	}
	else
	{
		/* cleanning */
		g_str_replace_first(user_list, ",,", "");
		if( user_list[0] == ',' )
		{
			user_list_p++;
		}
	}
	if( g_strlen(user_list_p) == 0 )
	{
		if (printer_dev_del_printer(http, lp_name) == 1 )
		{
			log_message(&log_conf, LOG_LEVEL_WARNING, "chansrv[printer_dev_del]:"
					"enable to delete printer");
			return 1;
		}
	}
	else
	{
		if ( printer_dev_set_restrict_user_list(http, lp_name, user_list_p) == 1 )
		{
			log_message(&log_conf, LOG_LEVEL_WARNING, "chansrv[printer_dev_del]:"
					"enable to restrict user");
			return 1;
		}
	}
	if(g_directory_exist(user_spool_dir))
	{
		log_message(&log_conf, LOG_LEVEL_DEBUG, "chansrv[printer_dev_del]:"
				"remove user spool directory : %s",user_spool_dir);
		if(g_remove_dirs(user_spool_dir))
		{
			log_message(&log_conf, LOG_LEVEL_WARNING, "chansrv[printer_dev_del]:"
							"enable to remove user spool directory : %s",user_spool_dir);
		}
	}

	return 0;
}

int DEFAULT_CC
printer_dev_job_pending()
{
  fd_set rfds;
  struct timeval time;
  int rv;
  time.tv_sec = 0;
  time.tv_usec = 0;
  FD_ZERO(&rfds);
  if (printer_sock > 0)
  {
    FD_SET(((unsigned int)printer_sock), &rfds);
    rv = select(printer_sock + 1, &rfds, 0, 0, &time);
    return rv;
  }
  return 0;
}



/*****************************************************************************/
int DEFAULT_CC
printer_dev_get_next_job(char* jobs, int *device_id)
{
  char buf[BUF_LEN];
	struct inotify_event *event;
	int len;
	int index;

	if(printer_dev_job_pending() < 1)
	{
		return 1;
	}
	len = read(printer_sock, buf, BUF_LEN);

	if( len < 0 )
	{
		return 1;
	}
	event = (struct inotify_event *) buf;
	if (event->len < 0)
	{
		log_message(&log_conf, LOG_LEVEL_WARNING, "chansrv[printer_dev_get_next_job]:"
					"no new jobs in '%s'\n", user_spool_dir);
		return 1;
	}
	log_message(&log_conf, LOG_LEVEL_DEBUG, "chansrv[printer_dev_get_next_job]:"
			"new job : %s", event->name);
	index = printer_dev_get_printer_from_watch(event->wd);
	if(index == -1)
	{
		log_message(&log_conf, LOG_LEVEL_DEBUG, "chansrv[printer_dev_get_next_job]:"
				"enable to get printer from watch id");
	}
	*device_id = printer_devices[index].device_id;
	g_sprintf(jobs, "%s%s/%s/%s", SPOOL_DIR, username, printer_devices[index].printer_name, event->name);
	return 0;
}

/*****************************************************************************/
int DEFAULT_CC
printer_dev_delete_job(char* jobs)
{
	log_message(&log_conf, LOG_LEVEL_DEBUG, "chansrv[printer_dev_delete_job]:"
				"delete job '%s'", jobs);
	if(g_file_delete(jobs) == 0)
	{
		log_message(&log_conf, LOG_LEVEL_WARNING, "chansrv[printer_dev_delete_job]:"
					"enable to delete job '%s'", jobs);
	}

	return 0;
}


/*****************************************************************************/
int APP_CC
printer_dev_init_printer_socket( char* printer_name)
{
	char printer_spool_dir[1024];
	int watch;

	log_message(&log_conf, LOG_LEVEL_DEBUG, "chansrv[printer_dev_init_printer_socket]:"
  		" Init printer_socket");
	g_sprintf(user_spool_dir, "%s%s",SPOOL_DIR, username);
	g_mkdir(user_spool_dir);
	if( g_directory_exist(user_spool_dir) < 0)
	{
		log_message(&log_conf, LOG_LEVEL_ERROR, "chansrv[printer_dev_init_printer_socket]:"
	  		" Enable to create user spool directory : %s",user_spool_dir);
		return 1;
	}
	if( g_chown(user_spool_dir, "lp"))
	{
		log_message(&log_conf, LOG_LEVEL_ERROR, "chansrv[printer_dev_init_printer_socket]:"
	  		" Enable to change spool directory owner : %s",user_spool_dir);
		return 1;
	}
	g_sprintf(printer_spool_dir, "%s/%s",user_spool_dir, printer_name );
	g_mkdir(printer_spool_dir);
	if( g_directory_exist(printer_spool_dir) < 0)
	{
		log_message(&log_conf, LOG_LEVEL_ERROR, "chansrv[printer_dev_init_printer_socket]:"
	  		" Enable to create printer spool directory : %s",user_spool_dir);
		return 1;
	}
	if( g_chown(printer_spool_dir, "lp"))
	{
		log_message(&log_conf, LOG_LEVEL_ERROR, "chansrv[printer_dev_init_printer_socket]:"
	  		" Enable to change printer directory owner : %s",user_spool_dir);
		return 1;
	}
	if(printer_sock == 0)
	{
		printer_sock = inotify_init();
		if (printer_sock <= 0)
		{
			log_message(&log_conf, LOG_LEVEL_WARNING, "chansrv[dev_redir_init_printer_socket]:"
					"Enable to setup inotify (%s)",strerror(errno));
			return 0;
		}
	}
	log_message(&log_conf, LOG_LEVEL_DEBUG, "chansrv[dev_redir_init_printer_socket]:"
			"Adding watch to %s\n", printer_spool_dir);
  watch = inotify_add_watch(printer_sock, printer_spool_dir, IN_MOVE);
  if (watch  < 0)
  {
  	log_message(&log_conf, LOG_LEVEL_WARNING, "chansrv[dev_redir_init_printer_socket]:"
  			"Unable to add inotify watch (%s)\n", strerror(errno));
      return 0;
  }
  return watch;
}

/*****************************************************************************/
int APP_CC
printer_dev_get_printer_socket()
{
	return printer_sock;
}


/*****************************************************************************/
int APP_CC
printer_dev_deinit_printer_socket()
{
	return 0;
}
