
#if !defined(CHANSRV_H)
#define CHANSRV_H

#include "arch.h"
#include "parse.h"
#include "log.h"


struct chan_item
{
  int id;
  int flags;
  char name[16];
};

int APP_CC
send_channel_data(int chan_id, char* data, int size);
int APP_CC
main_cleanup(void);

int APP_CC
in_unistr(struct stream* s, char *string, int str_size, int in_len);

#define CHAN_CFG_LOGGING			"Logging"
#define CHAN_CFG_LOG_FILE			"LogDir"
#define CHAN_CFG_LOG_LEVEL			"LogLevel"
#define CHAN_CFG_LOG_ENABLE_SYSLOG	"EnableSyslog"
#define CHAN_CFG_LOG_SYSLOG_LEVEL	"SyslogLevel"

#define CHAN_SPOOL_DIR					"/var/spool/xrdp"


#define LOG_LEVEL 5

#define LOG(_a, _params) \
{ \
  if (_a < LOG_LEVEL) \
  { \
    g_write("xrdp-chansrv [%10.10d]: ", g_time3()); \
    g_writeln _params ; \
  } \
}

#endif
