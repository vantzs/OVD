/**
 * Copyright (C) 2010-2012 Ulteo SAS
 * http://www.ulteo.com
 * Author David LECHEVALIER <david@ulteo.com> 2010, 2012
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

#ifndef MPPC_H_
#define MPPC_H_


#include <parse.h>

#define MPPC_BIG            0x01
#define MPPC_COMPRESSED     0x20
#define MPPC_RESET          0x40
#define MPPC_FLUSH          0x80
#define MPPC_8K_DICT_SIZE   8192
#define MPPC_64K_DICT_SIZE  65536

#define INT_SIZE            32
#define BYTE_SIZE           8


#define TYPE_8K             0x0
#define TYPE_64K            0x1
#define TYPE_RDP6           0x2
#define TYPE_RDP61          0x3

#define FLAG_TYPE_8K        0x0
#define FLAG_TYPE_64K       0x200
#define FLAG_TYPE_RDP6      0x400
#define FLAG_TYPE_RDP61     0x600

#define DEFAULT_XRDP_CONNECTIVITY_CHECK_INTERVAL  60


/******************************************************************************/
#define out_uint8_c(s, v) \
{ \
  *((s).packet->p) = (unsigned char)(v); \
  (s).packet->p++; \
  (s).size++; \
}

struct compressed_stream
{
	struct stream* packet;
	int size;
};

struct xrdp_compressor
{
	int type;

	struct compressed_stream outputBuffer;
	int historyOffset;
	int historyBuffer_size;
	char* historyBuffer;
	int dictSize;
	int srcLength;

	struct list** histTab;
	/* for binary operation */
	int walker;
	int walker_len;
};

struct xrdp_compressor*
mppc_init(int new_type);
int
mppc_dinit(struct xrdp_compressor* compressor);

#endif /* MPPC_H_ */
