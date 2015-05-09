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

#include <signal.h>
#include <stdlib.h>

#include "config/io.h"
#include "config/log.h"
#include "method.h"
#include "state.h"

int main(void)
{
	signal(SIGPIPE, SIG_IGN);

	if ((create_state_hashtable()) == -1) {
		log_err("Cannot allocate hashtable for states!\n");
		return EXIT_FAILURE;
	}

	if ((create_method_hashtable()) == -1) {
		log_err("Cannot allocate hashtable for states!\n");
		goto create_method_table_failed;
	}

	if (run_io() < 0) {
		goto run_io_failed;
	}

	delete_method_hashtable();
	delete_state_hashtable();
	return EXIT_SUCCESS;

run_io_failed:
	delete_method_hashtable();
create_method_table_failed:
	delete_state_hashtable();
	return EXIT_FAILURE;
}
