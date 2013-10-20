#ifndef CJET_STATE_H
#define CJET_STATE_H

#include "cJSON.h"
#include "list.h"
#include "peer.h"

struct state {
	struct list_head next_state;
	const char *path;
	cJSON *value;
};

int add_state_to_peer(struct peer *p, const char *path, cJSON *value);

#endif