
#if !defined(DEVREDIR_H)
#define DEVREDIR_H

#include "arch.h"
#include "parse.h"

/* protocol message */
/* RDPDR_HEADER */
/* Component */
#define RDPDR_CTYP_CORE 	0x4472
#define RDPDR_CTYP_PRN		0x5052
/* PacketId */
#define PAKID_CORE_SERVER_ANNOUNCE		0x496E
#define PAKID_CORE_CLIENTID_CONFIRM		0x4343
#define PAKID_CORE_CLIENT_NAME			0x434E
#define PAKID_CORE_DEVICELIST_ANNOUNCE 	0x4441
#define PAKID_CORE_DEVICE_REPLY			0x6472
#define PAKID_CORE_DEVICE_IOREQUEST		0x4952
#define PAKID_CORE_DEVICE_IOCOMPLETION	0x4943
#define PAKID_CORE_SERVER_CAPABILITY	0x5350
#define PAKID_CORE_CLIENT_CAPABILITY	0x4350
#define PAKID_CORE_DEVICELIST_REMOVE	0x444D
#define PAKID_PRN_CACHE_DATA			0x5043
#define PAKID_CORE_USER_LOGGEDON		0x554C
#define PAKID_PRN_USING_XPS				0x5543


/* CAPABILITY HEADER */
/* Capability type */
#define CAP_GENERAL_TYPE		0x0001
#define CAP_PRINTER_TYPE		0x0002
#define CAP_PORT_TYPE			0x0003
#define CAP_DRIVE_TYPE			0x0004
#define CAP_SMARTCARD_TYPE		0x0005

/* Version */
#define GENERAL_CAPABILITY_VERSION_01	0x00000001
#define GENERAL_CAPABILITY_VERSION_02	0x00000002
#define PRINT_CAPABILITY_VERSION_01		0x00000001
#define PORT_CAPABILITY_VERSION_01		0x00000001
#define DRIVE_CAPABILITY_VERSION_01		0x00000001
#define DRIVE_CAPABILITY_VERSION_02		0x00000002
#define SMARTCARD_CAPABILITY_VERSION_0	0x00000001


#define OS_TYPE_WINNT		0x00000002

/* VersionMinor */
#define RDP6	0x000C
#define RDP52	0x000A
#define RDP51	0x0005
#define RDP5	0x0002

/* ioCode1 */
#define RDPDR_IRP_MJ_CREATE						0x00000001
#define RDPDR_IRP_MJ_CLEANUP					0x00000002
#define RDPDR_IRP_MJ_CLOSE						0x00000004
#define RDPDR_IRP_MJ_READ						0x00000008
#define RDPDR_IRP_MJ_WRITE						0x00000010
#define RDPDR_IRP_MJ_FLUSH_BUFFERS				0x00000020
#define RDPDR_IRP_MJ_SHUTDOWN					0x00000040
#define RDPDR_IRP_MJ_DEVICE_CONTROL				0x00000080
#define RDPDR_IRP_MJ_QUERY_VOLUME_INFORMATION	0x00000100
#define RDPDR_IRP_MJ_SET_VOLUME_INFORMATION		0x00000200
#define RDPDR_IRP_MJ_QUERY_INFORMATION			0x00000400
#define RDPDR_IRP_MJ_SET_INFORMATION			0x00000800
#define RDPDR_IRP_MJ_DIRECTORY_CONTROL			0x00001000
#define RDPDR_IRP_MJ_LOCK_CONTROL				0x00002000
#define RDPDR_IRP_MJ_QUERY_SECURITY				0x00004000
#define RDPDR_IRP_MJ_SET_SECURITY				0x00008000
#define RDPDR_IRP_MJ_ALL						0x0000FFFF

/* extraFlag1 */
#define ENABLE_ASYNCIO							0x00000001

/* device Type */
#define RDPDR_DTYP_SERIAL				0x00000001
#define RDPDR_DTYP_PARALLEL				0x00000002
#define RDPDR_DTYP_PRINT				0x00000004
#define RDPDR_DTYP_FILESYSTEM			0x00000008
#define RDPDR_DTYP_SMARTCARD			0x00000020



/* io operation */
#define IRP_MJ_CREATE											0x00000000
#define IRP_MJ_CLOSE											0x00000002
#define IRP_MJ_READ												0x00000003
#define IRP_MJ_WRITE											0x00000004
#define IRP_MJ_DEVICE_CONTROL							0x0000000E
#define IRP_MJ_QUERY_VOLUME_INFORMATION		0x0000000A
#define IRP_MJ_SET_VOLUME_INFORMATION			0x0000000B
#define IRP_MJ_QUERY_INFORMATION					0x00000005
#define	IRP_MJ_SET_INFORMATION						0x00000006
#define IRP_MJ_DIRECTORY_CONTROL					0x0000000C
#define IRP_MJ_LOCK_CONTROL								0x00000011



struct device
{
	int device_id;
	int device_type;
};


typedef struct {
	int device;
	char path[256];
	int last_req;
	int file_id ;
	int message_id;
} Action;





int APP_CC
dev_redir_init(void);
int APP_CC
dev_redir_deinit(void);
int APP_CC
dev_redir_data_in(struct stream* s, int chan_id, int chan_flags, int length,
                  int total_length);
int APP_CC
dev_redir_get_wait_objs(tbus* objs, int* count, int* timeout);
int APP_CC
dev_redir_check_wait_objs(void);
int APP_CC
dev_redir_clientID_confirm(struct stream* s);
int APP_CC
dev_redir_client_name(struct stream* s);
int APP_CC
dev_redir_client_capability(struct stream* s);
int APP_CC
dev_redir_devicelist_announce(struct stream* s);
int APP_CC
dev_redir_confirm_clientID_request();
int APP_CC
in_unistr(struct stream* s, char *string, int str_size, int in_len);
int APP_CC
dev_redir_device_list_reply(int handle);


#endif
