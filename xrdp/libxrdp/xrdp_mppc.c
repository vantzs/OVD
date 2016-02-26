/**
 * Copyright (C) 2010 Ulteo SAS
 * http://www.ulteo.com
 * Author David Lechevalier <david@ulteo.com> 2010
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

#include "xrdp_mppc.h"
#include "defines.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <list.h>

static int duration = 0;


void
hexdump(char* p, int len)
{
	printf("len : %i\n", len);
  unsigned char* line;
  int i;
  int thisline;
  int offset;
  line = (unsigned char*)p;
  offset = 0;
  while (offset < len)
  {
    printf("%04x ", offset);
    thisline = len - offset;
    if (thisline > 16)
    {
      thisline = 16;
    }
    for (i = 0; i < thisline; i++)
    {
      printf("%02x ", line[i]);
    }
    for (; i < 16; i++)
    {
      printf("   ");
    }
    for (i = 0; i < thisline; i++)
    {
      printf("%c", (line[i] >= 0x20 && line[i] < 0x7f) ? line[i] : '.');
    }
    printf("\n");
    offset += thisline;
    line += thisline;
  }
}

void
dumpWalker(struct xrdp_compressor* compressor)
{
	int i;
	char* b = malloc(4);
	int tmp = compressor->walker;
	for (i = 0; i < 4; i++)
		b[i] = (char) ((tmp & 0xff000000) >> (INT_SIZE - BYTE_SIZE));
	{
		tmp <<= BYTE_SIZE;
	}
	printf("#############################################\n");
	printf("dumpWalker (length: %i : \n", compressor->walker_len);
	hexdump(b, 4);
	printf("#############################################\n");
}

void updateHistory(struct list** histTab, unsigned char c, int offset)
{
	if (histTab[c] == 0)
	{
		//une nouvelle valeur
		histTab[c] = list_create();
		histTab[c]->auto_free = 0;
	}
	list_add_item(histTab[c], (long)offset);
}

void historyFlush(struct xrdp_compressor* compressor)
{
	int i;
	for (i = 0 ; i<256 ; i++)
	{
		if (compressor->histTab[i] != NULL)
		{
			list_delete(compressor->histTab[i]);
		}
		compressor->histTab[i] = 0;
	}
}

struct xrdp_compressor*
mppc_init(int new_type)
{
	int i = 0;
	int dictSize;
	struct xrdp_compressor* compressor;
	compressor = malloc(sizeof(struct xrdp_compressor));
	compressor->type = new_type;

	switch (new_type)
	{
	case TYPE_8K:
		dictSize = MPPC_8K_DICT_SIZE;
		break;
	case TYPE_64K:
		dictSize = MPPC_64K_DICT_SIZE;
			break;
		default:
			//TODO add log message
			return 0;
	}
	compressor->histTab = malloc(256 * sizeof(struct list));
	for (i = 0; i < 256 ; i++)
	{
		compressor->histTab[i] = NULL;
	}

	compressor->historyBuffer_size = dictSize;
	compressor->historyOffset = 0;
	compressor->historyBuffer = malloc(dictSize);
	return compressor;
}

int
mppc_dinit(struct xrdp_compressor* compressor)
{
	historyFlush(compressor);
	free(compressor->histTab);
	if (compressor != 0)
	{
		if (compressor->historyBuffer != 0)
		{
			free(compressor->historyBuffer);
		}
		free(compressor);
	}
}

void
compressInit(struct xrdp_compressor* compressor, struct stream* datas)
{
	compressor->srcLength = datas->size;
	compressor->walker = 0;
	compressor->walker_len = 0;
	compressor->outputBuffer.size = 0;
	make_stream(compressor->outputBuffer.packet);
	init_stream(compressor->outputBuffer.packet, datas->size);
}

int
outByte(struct xrdp_compressor* compressor, int  fill)
{
	if ((1 + compressor->outputBuffer.size) > compressor->srcLength)
		return 1;

	while (compressor->walker_len >= BYTE_SIZE)
	{
		out_uint8_c(compressor->outputBuffer, (unsigned char)(compressor->walker >> (INT_SIZE - BYTE_SIZE)));
		compressor->walker <<= BYTE_SIZE;
		compressor->walker_len -= BYTE_SIZE;
	}
	if (fill && compressor->walker_len > 0)
	{
		out_uint8_c(compressor->outputBuffer, (unsigned char)(compressor->walker >> (INT_SIZE - BYTE_SIZE)));
		compressor->walker <<= BYTE_SIZE;
		compressor->walker_len = 0;
	}
	return 0;
}


int
createCopyTuple(struct xrdp_compressor* compressor, int copy_offset, int copy_length)
{
	int walker = compressor->walker;
	int walker_len = compressor->walker_len;
	// copy offset
	if (compressor->type == TYPE_8K)
	{
		if (copy_offset < 64)
		{
			walker_len += 4;
			walker |= 0xF << (INT_SIZE - walker_len);
			walker_len += 6;
			walker |= (copy_offset & 0x0000003F) << (INT_SIZE - walker_len);
		}
		else if ((copy_offset >= 64) && (copy_offset < 320))
		{
			walker_len += 4;
			walker |= 0xE << (INT_SIZE - walker_len);
			walker_len += BYTE_SIZE;
			walker |= ((copy_offset - 64) & 0x000000FF) << (INT_SIZE - walker_len);
		}
		else if ((copy_offset >= 320) && (copy_offset < MPPC_8K_DICT_SIZE))
		{
			walker_len += 3;
			walker |= 0x6 << (INT_SIZE - walker_len);
			walker_len += 13;
			walker |= ((copy_offset - 320) & 0x00001FFF) << (INT_SIZE - walker_len);
		}
		else
		{
			return 1;
		}
	}
	else if (compressor->type == TYPE_64K)
	{
		if (copy_offset < 64)
		{
			walker_len += 5;
			walker |= 0x1F << (INT_SIZE - walker_len);
			walker_len += 6;
			walker |= (copy_offset & 0x0000003F) << (INT_SIZE - walker_len);
		}
		else if ((copy_offset >= 64) && (copy_offset < 320))
		{
			walker_len += 5;
			walker |= 0x1E << (INT_SIZE - walker_len);
			walker_len += BYTE_SIZE;
			walker |= ((copy_offset - 64) & 0x000000FF) << (INT_SIZE - walker_len);
		}
		else if ((copy_offset >= 320) && (copy_offset < 2368))
		{
			walker_len += 4;
			walker |= 0xE << (INT_SIZE - walker_len);
			walker_len += 11;
			walker |= ((copy_offset - 320) & 0x000007FF) << (INT_SIZE - walker_len);
		}
		else if ((copy_offset >= 2368) && (copy_offset < MPPC_64K_DICT_SIZE))
		{
			walker_len += 3;
			walker |= 0x6 << (INT_SIZE - walker_len);
			walker_len += 16;
			walker |= ((copy_offset - 2368) & 0x0000FFFF) << (INT_SIZE - walker_len);
		}
		else
		{
			return 1;
		}
	}
	compressor->walker = walker;
	compressor->walker_len = walker_len;

	outByte(compressor, 0);
	walker = compressor->walker;
	walker_len = compressor->walker_len;

	// copy length
	if (copy_length == 3)
	{
		walker_len++;
		walker &= 0xfffffffe << (INT_SIZE - walker_len);
	}
	else if ((copy_length >= 4) && (copy_length < 8))
	{
		walker_len += 2;
		walker |= 0x2 << (INT_SIZE - walker_len);
		walker_len += 2;
		walker |= (copy_length & 0x00000003) << (INT_SIZE - walker_len);
	}
	else if ((copy_length >= 8) && (copy_length < 16))
	{
			walker_len += 3;
			walker |= 0x6 << (INT_SIZE - walker_len);
			walker_len += 3;
			walker |= (copy_length & 0x00000007) << (INT_SIZE - walker_len);
	}
	else if ((copy_length >= 16) && (copy_length < 32))
	{
		walker_len += 4;
		walker |= 0xE << (INT_SIZE - walker_len);
		walker_len += 4;
		walker |= (copy_length & 0x0000000F) << (INT_SIZE - walker_len);
	}
	else if ((copy_length >= 32) && (copy_length < 64))
	{
		walker_len += 5;
		walker |= 0x1E << (INT_SIZE - walker_len);
		walker_len += 5;
		walker |= (copy_length & 0x0000001F) << (INT_SIZE - walker_len);
	}
	else if ((copy_length >= 64) && (copy_length < 128))
	{
		walker_len += 6;
		walker |= 0x3E << (INT_SIZE - walker_len);
		walker_len += 6;
		walker |= (copy_length & 0x0000003F) << (INT_SIZE - walker_len);
	}
	else if ((copy_length >= 128) && (copy_length < 256))
	{
		walker_len += 7;
		walker |= 0x7E << (INT_SIZE - walker_len);
		walker_len += 7;
		walker |= (copy_length & 0x0000007F) << (INT_SIZE - walker_len);
	}
	else if ((copy_length >= 256) && (copy_length < 512))
	{
		walker_len += 8;
		walker |= 0xFE << (INT_SIZE - walker_len);
		walker_len += 8;
		walker |= (copy_length & 0x000000FF) << (INT_SIZE - walker_len);
	}
	else if ((copy_length >= 512) && (copy_length < 1024))
	{
		walker_len += 9;
		walker |= 0x1FE << (INT_SIZE - walker_len);
		walker_len += 9;
		walker |= (copy_length & 0x000001FF) << (INT_SIZE - walker_len);
	}
	else if ((copy_length >= 1024) && (copy_length < 2048))
	{
		walker_len += 10;
		walker |= 0x3FE << (INT_SIZE - walker_len);
		walker_len += 10;
		walker |= (copy_length & 0x000003FF) << (INT_SIZE - walker_len);
	}
	else if ((copy_length >= 2048) && (copy_length < 4096))
	{
		walker_len += 11;
		walker |= 0x7FE << (INT_SIZE - walker_len);
		walker_len += 11;
		walker |= (copy_length & 0x000007FF) << (INT_SIZE - walker_len);
	}
	else if ((copy_length >= 4096) && (copy_length < MPPC_8K_DICT_SIZE))
	{
		walker_len += 12;
		walker |= 0xFFE << (INT_SIZE - walker_len);
		walker_len += 12;
		walker |= (copy_length & 0x00000FFF) << (INT_SIZE - walker_len);
	}
	else if (compressor->type == TYPE_64K)
	{
		if ((copy_length >= MPPC_8K_DICT_SIZE) && (copy_length < 16384))
		{
			walker_len += 13;
			walker |= 0x1FFE << (INT_SIZE - walker_len);
			walker_len += 13;
			walker |= (copy_length & 0x00001FFF) << (INT_SIZE - walker_len);
		}
		else if ((copy_length >= 16384) && (copy_length < 32768))
		{
			walker_len += 14;
			walker |= 0x3FFE << (INT_SIZE - walker_len);
			walker_len += 14;
			walker |= (copy_length & 0x00003FFF) << (INT_SIZE - walker_len);
		}
		else if ((copy_length >= 32768) && (copy_length < MPPC_64K_DICT_SIZE))
		{
			walker_len += 15;
			walker |= 0x7FFE << (INT_SIZE - walker_len);
			walker_len += 15;
			walker |= (copy_length & 0x00007FFF) << (INT_SIZE - walker_len);
		}
		else
		{
			return 1;
		}
	}
	else
	{
		return 1;
	}
	compressor->walker = walker;
	compressor->walker_len = walker_len;

	return 0;
}


void
searchMatchBytes(struct xrdp_compressor* compressor, int* historyPtr)
{
	int i = 0;
	int j=0;
	int literal;
	int best_copy_offset = -1;
	int best_copy_length = -1;
	int copy_length;
	int copy_offset;

	unsigned char data = compressor->historyBuffer[*historyPtr];
	struct list* list = compressor->histTab[data];
	int offset;
	if (list != NULL)
	{
		i = 0;
		while( i < list->count)
		{
			offset = (int)list_get_item(list, i++);
			copy_length = 1;
			copy_offset = *historyPtr - offset;
			while ((*historyPtr + copy_length) < compressor->historyOffset &&
					compressor->historyBuffer[offset + copy_length] == compressor->historyBuffer[*historyPtr + copy_length])
			{
				copy_length++;
			}
			if (copy_length >= best_copy_length)
			{
				best_copy_length = copy_length;
				best_copy_offset = copy_offset;
			}
		}
	}

	if (best_copy_length < 3)
	{
		literal = (compressor->historyBuffer[*historyPtr] & 0x000000ff);
		if (literal > 0x7F)
		{
			compressor->walker_len++;
			compressor->walker |= 0x1 << (INT_SIZE - compressor->walker_len);
			compressor->walker_len += BYTE_SIZE;
			compressor->walker |= (literal & 0x0000007F) << (INT_SIZE - compressor->walker_len);
		}
		else
		{
			compressor->walker_len += BYTE_SIZE;
			compressor->walker |= (literal) << (INT_SIZE - compressor->walker_len);
		}
		updateHistory(compressor->histTab, compressor->historyBuffer[*historyPtr], *historyPtr);
		(*historyPtr)++;
		return;
	}
	createCopyTuple(compressor, best_copy_offset, best_copy_length);
	for (i=0; i < best_copy_length; i++)
	{
		updateHistory(compressor->histTab, compressor->historyBuffer[*historyPtr + i], *historyPtr + i);
	}
	*historyPtr += best_copy_length;
	return;
}

struct stream *
compressMain(struct xrdp_compressor* compressor, struct stream* uncompressedDatas, int* flags)
{
	int historyPtr;
	int flushed = 0;
	if (uncompressedDatas->size > compressor->historyBuffer_size - compressor->historyOffset)
	{
		compressor->historyOffset = 0;
		*flags |= MPPC_RESET;
		historyFlush(compressor);
	}

	memcpy(compressor->historyBuffer + compressor->historyOffset, uncompressedDatas->data, uncompressedDatas->size);
	historyPtr = compressor->historyOffset;
	compressor->historyOffset += uncompressedDatas->size;

  while (historyPtr < compressor->historyOffset)
  {
  	searchMatchBytes(compressor, &historyPtr);
  	if (outByte(compressor, 0) != 0)
  	{
  		flushed = 1;
  		break;
  	}
  }
  if (outByte(compressor, 1) != 0)
  {
  	flushed = 1;
  }
  if (flushed)
  {
  	historyFlush(compressor);
  	flags[0] |= MPPC_FLUSH;
  }
  flags[0] |= MPPC_COMPRESSED;
  compressor->outputBuffer.packet->size = compressor->outputBuffer.size;
  return compressor->outputBuffer.packet;
}


struct stream*
mppc_compress(struct xrdp_compressor* compressor, struct stream* uncompressedPacket, int* flags)
{
	struct stream* uncompressedDatas;
	make_stream(uncompressedDatas);
	init_stream(uncompressedDatas, uncompressedPacket->size);

	struct stream* compressedDatas;
	char* datasToSend = NULL;

	*flags |= compressor->type;

	out_uint8p(uncompressedDatas, uncompressedPacket->p, uncompressedPacket->size);
	uncompressedDatas->size = uncompressedPacket->size;
	uncompressedDatas->p = uncompressedDatas->data;

	//int time = g_time2();
	compressInit(compressor, uncompressedPacket);
	compressedDatas = compressMain(compressor, uncompressedDatas, flags);
	//time = g_time2()-time;
	//printf("time passed in searching match byte %i\n", time);
	if (compressedDatas->size > uncompressedDatas->size)
	{
		return uncompressedDatas;
	}
	return compressedDatas;
}




