/*
 *The MIT License (MIT)
 *
 * Copyright (c) <2014> <Stephan Gatzka>
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef CJET_BUFFERED_SOCKET_H
#define CJET_BUFFERED_SOCKET_H

#include <stddef.h>
#include <sys/types.h>

#include "eventloop.h"
#include "generated/cjet_config.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BS_PEER_CLOSED 0
#define BS_IO_WOULD_BLOCK -1
#define BS_IO_ERROR -2
#define BS_IO_TOOMUCHDATA -3

enum bs_read_callback_return {BS_OK, BS_CLOSED};

union buffered_socket_reader_context {
	const char *ptr;
	size_t num;
};

struct buffered_socket_io_vector {
	const void *iov_base;
	size_t iov_len;
};

struct buffered_socket {
	struct io_event ev;
	unsigned int to_write;
	char *read_ptr;
	char *write_ptr;
	char *write_buffer_ptr;
	char read_buffer[CONFIG_MAX_MESSAGE_SIZE];
	char write_buffer[CONFIG_MAX_WRITE_BUFFER_SIZE];
	ssize_t (*reader)(struct buffered_socket *bs, union buffered_socket_reader_context reader_context, char **read_ptr);
	union buffered_socket_reader_context reader_context;
	enum bs_read_callback_return (*read_callback)(void *context, char *buf, size_t len);
	void *read_callback_context;
	void (*error)(void *error_context);
	void *error_context;
};

void buffered_socket_init(struct buffered_socket *bs, socket_type sock, const struct eventloop *loop, void (*error)(void *error_context), void *error_context);
int buffered_socket_close(struct buffered_socket *bs);
int buffered_socket_writev(struct buffered_socket *bs, struct buffered_socket_io_vector *io_vec, unsigned int count);
void buffered_socket_set_error(struct buffered_socket *bs, void (*error)(void *error_context), void *error_context);

int buffered_socket_read_exactly(struct buffered_socket *bs, size_t num, enum bs_read_callback_return (*read_callback)(void *context, char *buf, size_t len), void *context);
int buffered_socket_read_until(struct buffered_socket *bs, const char *delim, enum bs_read_callback_return (*read_callback)(void *context, char *buf, size_t len), void *context);

#ifdef __cplusplus
}
#endif

#endif