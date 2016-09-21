/*
 *The MIT License (MIT)
 *
 * Copyright (c) <2016> <Stephan Gatzka>
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

#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MAIN
#define BOOST_TEST_MODULE websocket_frame_tests

#include <boost/test/unit_test.hpp>
#include <endian.h>
#include <stdint.h>

#include "buffered_reader.h"
#include "http_connection.h"
#include "websocket.h"

#ifndef ARRAY_SIZE
 #define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#endif

static const uint8_t WS_HEADER_FIN = 0x80;
static const uint8_t WS_HEADER_MASK = 0x80;

static const uint8_t WS_OPCODE_TEXT = 0x01;
static const uint8_t WS_OPCODE_CLOSE = 0x08;
static const uint8_t WS_OPCODE_PING = 0x09;
static const uint8_t WS_OPCODE_PONG = 0x0a;

static uint8_t write_buffer[5000];
static uint8_t *write_buffer_ptr;

static uint8_t read_buffer[5000];
static uint8_t *read_buffer_ptr;
static size_t read_buffer_length;
static uint8_t readback_buffer[5000];
static size_t readback_buffer_length;

static bool close_called;
static bool text_message_received_called;
static bool ping_received_called;

static void mask_payload(uint8_t *ptr, size_t length, uint8_t mask[4])
{
	for (unsigned int i = 0; i < length; i++) {
		uint8_t byte = ptr[i] ^ mask[i % 4];
		ptr[i] = byte;
	}
}

static void ws_on_error(struct websocket *ws)
{
	(void)ws;
}

static int writev(void *this_ptr, struct buffered_socket_io_vector *io_vec, unsigned int count)
{
	(void)this_ptr;
	size_t complete_length = 0;

	for (unsigned int i = 0; i < count; i++) {
		::memcpy(write_buffer_ptr, io_vec[i].iov_base, io_vec[i].iov_len);
		write_buffer_ptr += io_vec[i].iov_len;
		complete_length += io_vec[i].iov_len;
	}
	return complete_length;
}

static int read_exactly(void *this_ptr, size_t num, read_handler handler, void *handler_context)
{
	(void)this_ptr;
	uint8_t *ptr = read_buffer_ptr;
	read_buffer_ptr += num;
	handler(handler_context, ptr, num);
	return 0;
}

static bool is_pong_frame(const char *payload)
{
	size_t payload_length = ::strlen(payload);

	uint8_t *ptr = write_buffer;
	uint8_t header;
	::memcpy(&header, ptr, sizeof(header));
	if ((header & WS_HEADER_FIN) != WS_HEADER_FIN) {
		return false;
	}
	if ((header & WS_OPCODE_PONG) != WS_OPCODE_PONG) {
		return false;
	}
	ptr += sizeof(header);

	uint8_t length;
	::memcpy(&length, ptr, sizeof(length));
	bool is_masked = false;
	if ((length & WS_HEADER_MASK) == WS_HEADER_MASK) {
		is_masked = true;
	}
	length = length & ~WS_HEADER_MASK;
	if (length != payload_length) {
		return false;
	}
	ptr += sizeof(length);

	if (is_masked) {
		uint8_t mask[4];
		memcpy(mask, ptr, sizeof(mask));
		ptr += sizeof(mask);

		mask_payload(ptr, length, mask);
	}
	

	if (::memcmp(ptr, payload, payload_length) != 0) {
		return false;
	}
	return true;
}

static bool is_close_frame()
{
	const uint8_t *ptr = write_buffer;
	uint8_t header;
	::memcpy(&header, ptr, sizeof(header));
	if ((header & WS_HEADER_FIN) != WS_HEADER_FIN) {
		return false;
	}
	if ((header & WS_OPCODE_CLOSE) != WS_OPCODE_CLOSE) {
		return false;
	}
	ptr += sizeof(header);

	uint8_t length;
	::memcpy(&length, ptr, sizeof(length));
	if ((length & WS_HEADER_MASK) == WS_HEADER_MASK) {
		return false;
	}
	if (length != 2) {
		return false;
	}
	ptr += sizeof(length);

	uint16_t status_code;
	::memcpy(&status_code, ptr, sizeof(status_code));
	status_code = be16toh(status_code);
	if (status_code != 1001) {
		return false;
	}
	return true;
}

static int close(void *this_ptr)
{
	(void)this_ptr;
	close_called = true;
	return 0;
}

static enum websocket_callback_return text_message_received(struct websocket *s, char *msg, size_t length)
{
	(void)s;
	(void)msg;
	(void)length;
	::memcpy(readback_buffer, msg, length);
	readback_buffer_length += length;
	text_message_received_called = true;
	return WS_OK;
}

static enum websocket_callback_return ping_received(struct websocket *s, uint8_t *msg, size_t length)
{
	(void)s;
	(void)msg;
	(void)length;
	::memcpy(readback_buffer, msg, length);
	readback_buffer_length += length;
	ping_received_called = true;
	return WS_OK;
}

static void fill_payload(uint8_t *ptr, uint8_t *payload, uint64_t length, bool shall_mask, uint8_t mask[4])
{
	::memcpy(ptr, payload, length);
	if (shall_mask) {
		mask_payload(ptr, length, mask);
	}
}

static void prepare_message(uint8_t type, char *message, bool shall_mask, uint8_t mask[4])
{
	uint8_t *ptr = read_buffer;
	read_buffer_length = 0;
	uint8_t header = 0x00;
	header |= WS_HEADER_FIN;
	header |= type;
	::memcpy(ptr, &header, sizeof(header));
	ptr += sizeof(header);
	read_buffer_length += sizeof(header);

	uint8_t first_length = 0x00;
	if (shall_mask) {
		first_length |= WS_HEADER_MASK;
	}
	uint64_t length = ::strlen(message);
	if (length < 126) {
		first_length = first_length | (uint8_t)length;
		::memcpy(ptr, &first_length, sizeof(first_length));
		ptr += sizeof(first_length);
		read_buffer_length += sizeof(first_length);
	} else if (length <= sizeof(uint16_t)) {
		first_length = first_length | 126;
		::memcpy(ptr, &first_length, sizeof(first_length));
		ptr += sizeof(first_length);
		read_buffer_length += sizeof(first_length);
		uint16_t len = (uint16_t)length;
		len = htobe16(len);
		::memcpy(ptr, &len, sizeof(len));
		ptr += sizeof(len);
		read_buffer_length += sizeof(len);
	} else {
		first_length = first_length | 127;
		::memcpy(ptr, &first_length, sizeof(first_length));
		ptr += sizeof(first_length);
		read_buffer_length += sizeof(first_length);
		length = htobe64(length);
		::memcpy(ptr, &length, sizeof(length));
		ptr += sizeof(length);
		read_buffer_length += sizeof(length);
	}

	if (shall_mask) {
		::memcpy(ptr, mask, 4);
		ptr += 4;
	}

	fill_payload(ptr, (uint8_t *)message, length, shall_mask, mask);
	read_buffer_length += length;
}

struct F {

	F(bool is_server)
	{
		websocket_init_random();

		close_called = false;
		text_message_received_called = false;
		ping_received_called = false;
		write_buffer_ptr = write_buffer;
		read_buffer_ptr = read_buffer;
		readback_buffer_length = 0;

		struct http_connection *connection = alloc_http_connection();
		connection->br.writev = writev;
		connection->br.read_exactly = read_exactly;
		connection->br.close = close;
		websocket_init(&ws, connection, is_server, ws_on_error, "jet");
		ws.upgrade_complete = true;
		ws.text_message_received = text_message_received;
		ws.ping_received = ping_received;
	}

	~F()
	{
	}

	struct websocket ws;
};

BOOST_AUTO_TEST_CASE(test_close_frame_on_websocket_free)
{
	bool is_server = true;
	F f(is_server);
	
	websocket_free(&f.ws);
	BOOST_CHECK_MESSAGE(is_close_frame(), "No close frame sent when freeing the websocket!");
}

BOOST_AUTO_TEST_CASE(test_receive_text_frame_on_server)
{
	bool is_server = true;
	F f(is_server);

	char message[] = "Hello World!";
	uint8_t mask[4] = {0xaa, 0x55, 0xcc, 0x11};
	prepare_message(WS_OPCODE_TEXT, message, is_server, mask);
	ws_get_header(&f.ws, read_buffer_ptr++, read_buffer_length);
	websocket_free(&f.ws);
	BOOST_CHECK_MESSAGE(text_message_received_called, "Callback for text messages was not called!");
	BOOST_CHECK_MESSAGE(::strncmp(message, (char *)readback_buffer, readback_buffer_length) == 0, "Did not received the same message as sent!");
}

BOOST_AUTO_TEST_CASE(test_receive_text_frame_on_client)
{
	bool is_server = false;
	F f(is_server);

	char message[] = "Tell me why!";
	prepare_message(WS_OPCODE_TEXT, message, is_server, NULL);
	ws_get_header(&f.ws, read_buffer_ptr++, read_buffer_length);
	websocket_free(&f.ws);
	BOOST_CHECK_MESSAGE(text_message_received_called, "Callback for text messages was not called!");
	BOOST_CHECK_MESSAGE(::strncmp(message, (char *)readback_buffer, readback_buffer_length) == 0, "Did not received the same message as sent!");
}

BOOST_AUTO_TEST_CASE(test_receive_ping_frame_on_server)
{
	bool is_server = true;
	F f(is_server);

	char message[] = "Playing ping pong!";
	uint8_t mask[4] = {0xaa, 0x55, 0xcc, 0x11};
	prepare_message(WS_OPCODE_PING, message, is_server, mask);
	ws_get_header(&f.ws, read_buffer_ptr++, read_buffer_length);
	websocket_free(&f.ws);
	BOOST_CHECK_MESSAGE(ping_received_called, "Callback for ping messages was not called!");
	BOOST_CHECK_MESSAGE(is_pong_frame(message), "No pong frame sent when ping received!");
}

BOOST_AUTO_TEST_CASE(test_receive_ping_frame_on_client)
{
	bool is_server = false;
	F f(is_server);

	char message[] = "Playing ping pong!";
	uint8_t mask[4] = {0xaa, 0x55, 0xcc, 0x11};
	prepare_message(WS_OPCODE_PING, message, is_server, mask);
	ws_get_header(&f.ws, read_buffer_ptr++, read_buffer_length);
	websocket_free(&f.ws);
	BOOST_CHECK_MESSAGE(ping_received_called, "Callback for ping messages was not called!");
	BOOST_CHECK_MESSAGE(is_pong_frame(message), "No pong frame sent when ping received!");
}