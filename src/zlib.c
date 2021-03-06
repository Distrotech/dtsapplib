/*
Copyright (C) 2012  Gregory Nietsky <gregory@distrotetch.co.za>
        http://www.distrotech.co.za

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/** @addtogroup LIB-Z
  * @{
  * @file
  * @brief Simplified implementation of zlib functions.*/

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#include "include/dtsapp.h"

static const unsigned char gzipMagicBytes[] = { 0x1f, 0x8b, 0x08, 0x00 };

static void zobj_free(void *data) {
	struct zobj *zdata = data;

	if (zdata->buff) {
		free(zdata->buff);
	}
}

/** @brief Allocate a buffer and return it with compressed data
  *
  * @param buff Buffer to compress.
  * @param len Length of the buffer.
  * @param level Compression level.
  * @return reference to zobj data structure containing compressed data or NULL on error.*/
extern struct zobj *zcompress(uint8_t *buff, uint16_t len, uint8_t level) {
	struct zobj *ret;

	if (!(ret = objalloc(sizeof(*ret), zobj_free))) {
		return (NULL);
	}

	ret->zlen = compressBound(len);
	ret->olen = len;

	if (!(ret->buff = malloc(ret->zlen))) {
		return (NULL);
	}
	compress2(ret->buff, (uLongf *)&ret->zlen, buff, len, level);

	return (ret);
}

/** @brief Uncompress zobj buffer to buffer.
  *
  * @param buff Compressed buffer to uncompress.
  * @param obuff Buffer to uncompress too.
  * @warning obuff needs to be large enough to contain the data.
  * @todo Implement this without needing original buff len using inflate*/
extern void zuncompress(struct zobj *buff, uint8_t *obuff) {
	uLongf olen = buff->olen;

	if (!obuff) {
		return;
	}

	uncompress(obuff, &olen, buff->buff, buff->zlen);
}

/** @brief check a buffer if it contains gzip magic
  * @param buf buffer to check.
  * @param buf_size buffer len it must be more than 4.
  * @return non zero value if the buffer contains gzip data*/
extern int is_gzip(uint8_t *buf, int buf_size) {
	if (buf_size < 4) {
		return 0;
	}
	if (memcmp(buf, gzipMagicBytes, 4)) {
		return 0;
	}
	return 1;
}

/** @brief Ungzip a buffer.
  *
  * @param buf_in Buffer to inflate.
  * @param buf_size Size of buf_in buffer.
  * @param len Pointer that will contain the uncompressed data length.
  * @return Uncompressed data in a buffer or NULL on error.*/
extern uint8_t *gzinflatebuf(uint8_t *buf_in, int buf_size, uint32_t *len) {
	z_stream zdat;
	uint8_t *buf = NULL, *tmp;
	int res;

	zdat.opaque = NULL;
	zdat.zalloc = NULL;
	zdat.zfree = NULL;

	zdat.next_in = buf_in;
	zdat.avail_in = buf_size;
	zdat.next_out = buf;
	zdat.avail_out = 0;
	zdat.total_out = 0;

	if (inflateInit2(&zdat, 31)) {
		return NULL;
	}

	do {
		if (!(tmp = realloc(buf,zdat.total_out + (zdat.avail_in * 5) + 1))) {
			res = Z_MEM_ERROR;
			break;
		} else {
			buf = tmp;
		}
		buf[zdat.total_out] = '\0';
		zdat.next_out = &buf[zdat.total_out];
		zdat.avail_out += zdat.avail_in * 5;
	} while ((res = inflate(&zdat, Z_NO_FLUSH)) == Z_OK);

	if (res == Z_STREAM_END) {
		buf = realloc(buf, zdat.total_out);
		*len = zdat.total_out;
	} else {
		free(buf);
		*len = 0;
		buf = NULL;
	}
	inflateEnd(&zdat);

	return buf;
}

/** @}*/
