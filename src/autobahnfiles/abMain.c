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

#include <getopt.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "abLog.h"
#include "alloc.h"
#include "authenticate.h"
#include "cmdline_config.h"
#include "generated/version.h"
#include "jet_random.h"
#include "linux/eventloop_epoll.h"
#include "linux/linux_io.h"
#include "log.h"
#include "parse.h"
#include "table.h"

int main(int argc, char **argv)
{
	struct cmdline_config config = {
		.run_foreground = true,
		.bind_local_only = false,
		.user_name = NULL,
		.passwd_file = NULL,
		.request_target = "/",
		.compression_level = 2,
	};

	if (init_random() < 0) {
		log_err("Could not initialize random seed!\n");
		return EXIT_FAILURE;
	}

	init_parser();

	int ret = EXIT_SUCCESS;

	int c;

	while ((c = getopt(argc, argv, "flp:r:u:w:c:")) != -1) {
		switch (c) {
		case 'f':
			config.run_foreground = true;
			break;
		case 'l':
			config.bind_local_only = false;
			break;
		case 'p':
			config.passwd_file = optarg;
			break;
		case 'r':
			config.request_target = optarg;
			break;
		case 'u':
			config.user_name = optarg;
			break;
		case 'w':
			switch ((unsigned int) *optarg - '0') {
			case 0:
				log_info("Nothing will be displayed.\n");
				set_warning_level(0);
				break;
			case 1:
				log_info("Only errors will be displayed.\n");
				set_warning_level(1);
				break;
			case 2:
				log_info("Warnings and errors will be displayed.\n");
				set_warning_level(2);
				break;
			case 3:
				log_info("All messages will be displayed.\n");
				set_warning_level(3);
				break;
			default:
				log_warn("Invalid warning level. Warning level is set to default (all)\n");
				set_warning_level(3);
				break;
			}
			break;
		case 'c':
			switch ((unsigned int) *optarg - '0') {
			case 0:
				log_info("No compression will be used and accepted.\n");
				config.compression_level = 0;
				break;
			case 1:
				log_info("Use compression with min. memory usage. if possible\n");
				config.compression_level = 1;
				break;
			case 2:
				log_info("Use default compression if possible.\n");
				config.compression_level = 2;
				break;
			case 3:
				log_info("Use best compression with max. memory usage if possible\n");
				config.compression_level = 3;
				break;
			default:
				log_warn("Invalid compression level. Compression level is set to default (2)\n");
				config.compression_level = 2;
				break;
			}
			break;
		case '?':
			fprintf(stderr, "Usage: %s [-l] [-f] [-r <request target>] [-u <username>] [-p <password file>] "
                            "[-w <0,1,2 or 3: warning level nothing to all>] [-c <0, 1, 2 or 3: compression level no, little memory, default, good compression\n", argv[0]);
			ret = EXIT_FAILURE;
			goto getopt_failed;
			break;
		}
	}

	if (load_passwd_data(config.passwd_file) < 0) {
		log_err("Cannot load password file!\n");
		ret = EXIT_FAILURE;
		goto load_passwd_data_failed;
	}

	signal(SIGPIPE, SIG_IGN);

	if ((element_hashtable_create()) == -1) {
		log_err("Cannot allocate hashtable for states!\n");
		ret = EXIT_FAILURE;
		goto element_hashtable_create_failed;
	}

	struct eventloop_epoll eloop = {
		.epoll_fd = 0,
		.loop = {
			.this_ptr = &eloop,
			.init = eventloop_epoll_init,
			.destroy = eventloop_epoll_destroy,
			.run = eventloop_epoll_run,
			.add = eventloop_epoll_add,
			.remove = eventloop_epoll_remove,
		},
	};

	log_info("%s version %s started", CJET_NAME, CJET_VERSION);
	if (run_io(&eloop.loop, &config) < 0) {
		ret = EXIT_FAILURE;
		goto run_io_failed;
	}

	log_info("%s stopped", CJET_NAME);

run_io_failed:
	element_hashtable_delete();
element_hashtable_create_failed:
	free_passwd_data();
load_passwd_data_failed:
getopt_failed:
	close_random();
	return ret;
}
