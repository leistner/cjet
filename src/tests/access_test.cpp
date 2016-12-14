/*
 * The MIT License (MIT)
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
#define BOOST_TEST_MODULE state/method access tests

#include <boost/test/unit_test.hpp>
#include <list>

#include <stdio.h>

#include "authenticate.h"
#include "eventloop.h"
#include "fetch.h"
#include "json/cJSON.h"
#include "parse.h"
#include "state.h"
#include "table.h"

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(*(x)))

enum event {
	UNKNOWN_EVENT,
	ADD_EVENT,
	CHANGE_EVENT,
	REMOVE_EVENT
};

static struct io_event *timer_ev;
static struct eventloop loop;

const char path[] = "/foo/bar/";

static const char users[] = "users";
static const char admins[] = "admin";

static cJSON *user_auth;

static struct peer fetch_peer;

static std::list<cJSON*> fetch_events;

extern "C" {
	const cJSON *credentials_ok(const char *user_name, char *passwd)
	{
		(void)passwd;

		if (::strcmp(user_name, "user") == 0) {
			return user_auth;
		}

		return NULL;
	}

	ssize_t socket_read(socket_type sock, void *buf, size_t count)
	{
		(void)sock;
		(void)count;
		uint64_t number_of_timeouts = 1;
		::memcpy(buf, &number_of_timeouts, sizeof(number_of_timeouts));
		return 8;
	}

	int socket_close(socket_type sock)
	{
		(void)sock;
		return 0;
	}
}

static cJSON *parse_send_buffer(const char *json)
{
	const char *end_parse;
	cJSON *root = cJSON_ParseWithOpts(json, &end_parse, 0);
	return root;
}

static int send_message(const struct peer *p, char *rendered, size_t len)
{
	(void)len;
	if (p == &fetch_peer) {
		cJSON *fetch_event = parse_send_buffer(rendered);
		fetch_events.push_back(fetch_event);
	}

	return 0;
}

static enum eventloop_return fake_add(const void *this_ptr, const struct io_event *ev)
{
	(void)this_ptr;
	timer_ev = (struct io_event *)ev;
	return EL_CONTINUE_LOOP;
}

static void fake_remove(const void *this_ptr, const struct io_event *ev)
{
	(void)this_ptr;
	(void)ev;
	timer_ev = NULL;
	return;
}

static cJSON *create_fetch_params(const char *path_equals_string)
{
	cJSON *root = cJSON_CreateObject();
	BOOST_REQUIRE(root != NULL);
	cJSON_AddStringToObject(root, "id", "fetch_id_1");
	cJSON *path = cJSON_CreateObject();
	BOOST_REQUIRE(path != NULL);
	cJSON_AddItemToObject(root, "path", path);
	if (strlen(path_equals_string)) {
		cJSON_AddStringToObject(path, "equals", path_equals_string);
	}
	return root;
}

static enum event get_event_from_json(cJSON *json)
{
	cJSON *params = cJSON_GetObjectItem(json, "params");
	if (params == NULL) return UNKNOWN_EVENT;
	cJSON *event = cJSON_GetObjectItem(params, "event");
	if (event == NULL) return UNKNOWN_EVENT;
	if (event->type != cJSON_String) return UNKNOWN_EVENT;
	if (strcmp(event->valuestring, "add") == 0) return ADD_EVENT;
	if (strcmp(event->valuestring, "change") == 0) return CHANGE_EVENT;
	if (strcmp(event->valuestring, "remove") == 0) return REMOVE_EVENT;
	return UNKNOWN_EVENT;
}

static void perform_fetch(const char *path)
{
	struct fetch *f = NULL;
	cJSON *params = create_fetch_params(path);
	cJSON *error = add_fetch_to_peer(&fetch_peer, params, &f);
	cJSON_Delete(params);
	BOOST_REQUIRE_MESSAGE(error == NULL, "add_fetch_to_peer() failed!");
	error = add_fetch_to_states(f);
	BOOST_REQUIRE_MESSAGE(error == NULL, "add_fetch_to_states() failed!");
}

struct F {
	F()
	{
		timer_ev = NULL;
		loop.this_ptr = NULL;
		loop.init = NULL;
		loop.destroy = NULL;
		loop.run = NULL;
		loop.add = fake_add;
		loop.remove = fake_remove;

		init_parser();
		state_hashtable_create();
		init_peer(&owner_peer, false, &loop);
		owner_peer.send_message = send_message;
		init_peer(&set_peer, false, &loop);
		set_peer.send_message = send_message;
		init_peer(&fetch_peer, false, &loop);
		fetch_peer.send_message = send_message;

		cJSON *groups = cJSON_CreateArray();
		cJSON_AddItemToArray(groups, cJSON_CreateString(admins));
		cJSON_AddItemToArray(groups, cJSON_CreateString(users));
		cJSON_AddItemToArray(groups, cJSON_CreateString("operators"));
		cJSON_AddItemToArray(groups, cJSON_CreateString("viewers"));
		create_groups();
		add_groups(groups);
		cJSON_Delete(groups);

		user_auth = cJSON_CreateObject();
		cJSON *fetch_groups = cJSON_CreateArray();
		cJSON_AddItemToArray(fetch_groups, cJSON_CreateString(users));
		cJSON_AddItemToObject(user_auth, "fetchGroups", fetch_groups);
		cJSON *set_groups = cJSON_CreateArray();
		cJSON_AddItemToArray(set_groups, cJSON_CreateString(users));
		cJSON_AddItemToObject(user_auth, "setGroups", set_groups);
		cJSON *call_groups = cJSON_CreateArray();
		cJSON_AddItemToArray(call_groups, cJSON_CreateString(users));
		cJSON_AddItemToObject(user_auth, "callGroups", call_groups);

		value = cJSON_CreateNumber(1234);
		password = ::strdup("password");
	}

	~F()
	{
		::free(password);
		cJSON_Delete(value);

		while (!fetch_events.empty()) {
			cJSON *ptr = fetch_events.front();
			fetch_events.pop_front();
			cJSON_Delete(ptr);
		}
		cJSON_Delete(user_auth);
		free_groups();
		free_peer_resources(&fetch_peer);
		free_peer_resources(&set_peer);
		free_peer_resources(&owner_peer);
		state_hashtable_delete();
	}

	cJSON *value;
	char *password;
	struct peer owner_peer;
	struct peer set_peer;
};

BOOST_FIXTURE_TEST_CASE(fetch_state_allowed, F)
{
	cJSON *access = cJSON_CreateObject();
	cJSON *fetch_groups = cJSON_CreateArray();
	cJSON_AddItemToArray(fetch_groups, cJSON_CreateString(users));
	cJSON_AddItemToObject(access, "fetchGroups", fetch_groups);

	cJSON *error = add_state_or_method_to_peer(&owner_peer, path, value, access, 0x00, CONFIG_ROUTED_MESSAGES_TIMEOUT);
	BOOST_REQUIRE_MESSAGE(error == NULL, "add_state_or_method_to_peer() failed!");
	cJSON_Delete(access);

	error = handle_authentication(&fetch_peer, "user", password);
	BOOST_CHECK_MESSAGE(error == NULL, "fetch peer authentication failed!");

	perform_fetch(path);
	BOOST_REQUIRE_MESSAGE(fetch_events.size() == 1, "Number of emitted events != 1!");
	cJSON *json = fetch_events.front();
	fetch_events.pop_front();
	event event = get_event_from_json(json);
	BOOST_CHECK_MESSAGE(event == ADD_EVENT, "Emitted event is not an ADD event!");
	cJSON_Delete(json);
	remove_all_fetchers_from_peer(&fetch_peer);
}

BOOST_FIXTURE_TEST_CASE(fetch_state_not_allowed, F)
{
	cJSON *access = cJSON_CreateObject();
	cJSON *fetch_groups = cJSON_CreateArray();
	cJSON_AddItemToArray(fetch_groups, cJSON_CreateString(admins));
	cJSON_AddItemToObject(access, "fetchGroups", fetch_groups);

	cJSON *error = add_state_or_method_to_peer(&owner_peer, path, value, access, 0x00, CONFIG_ROUTED_MESSAGES_TIMEOUT);
	BOOST_REQUIRE_MESSAGE(error == NULL, "add_state_or_method_to_peer() failed!");
	cJSON_Delete(access);

	error = handle_authentication(&fetch_peer, "user", password);
	BOOST_CHECK_MESSAGE(error == NULL, "fetch peer authentication failed!");

	perform_fetch(path);
	BOOST_REQUIRE_MESSAGE(fetch_events.size() == 0, "Number of emitted events != 0!");
	remove_all_fetchers_from_peer(&fetch_peer);
}


BOOST_AUTO_TEST_CASE(add_group_twice)
{
	cJSON *groups = cJSON_CreateArray();
	cJSON_AddItemToArray(groups, cJSON_CreateString("viewers"));
	cJSON_AddItemToArray(groups, cJSON_CreateString("viewers"));
	create_groups();
	int ret = add_groups(groups);
	cJSON_Delete(groups);
	BOOST_CHECK_MESSAGE(ret == 0, "adding a group twice failed!");
	free_groups();
}

BOOST_AUTO_TEST_CASE(add_too_many_groups)
{
	create_groups();

	cJSON *groups = cJSON_CreateArray();
	for (unsigned int i = 0; i <= sizeof(group_t) * 8; i++) {
		char buffer[10];
		::sprintf(buffer, "%s%d", "viewer", i);
		cJSON_AddItemToArray(groups, cJSON_CreateString(buffer));
	}

	int ret = add_groups(groups);
	cJSON_Delete(groups);
	BOOST_CHECK_MESSAGE(ret == -1, "adding lots of groups did not fail!");
	free_groups();
}

BOOST_AUTO_TEST_CASE(add_non_array_group)
{
	create_groups();

	cJSON *groups = cJSON_CreateFalse();
	int ret = add_groups(groups);
	cJSON_Delete(groups);
	BOOST_CHECK_MESSAGE(ret == -1, "adding a non-array group did not fail!");
	free_groups();
}