/*
 * The MIT License (MIT)
 *
 * Copyright (c) <2017> <Felix Retsch>
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
#define BOOST_TEST_MODULE utf8_checker_tests

#include <boost/test/unit_test.hpp>

#include "utf8_checker.h"

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#endif

static const int invalid_message_size = 9;
static const uint8_t invalid_message[invalid_message_size][4] = {{0xC0,0x00,0x00,0x00},	//invalid start
                                                                 {0xF6,0x00,0x00,0x00},	//invalid start
                                                                 {0x80,0x00,0x00,0x00},	//invalid start
                                                                 {0xC2,0x80,0x80,0x00},	//invalid continuation
                                                                 {0xE0,0x60,0x80,0x00},	//invalid continuation
                                                                 {0xE0,0x9F,0x80,0x00},	//reserved zone
                                                                 {0xED,0xA0,0x80,0x00},	//reserved zone
                                                                 {0xF0,0x8F,0x80,0x80},	//reserved zone
                                                                 {0xF4,0x90,0x80,0x80}};	//reserved zone
unsigned int invalid_message_word[invalid_message_size][4] = {{0x000000C0,0x0,0x0,0x0},	//invalid start
                                                              {0x000000F6,0x0,0x0,0x0},	//invalid start
                                                              {0x00000080,0x0,0x0,0x0},	//invalid start
                                                              {0x000000C2,0x80800000,0x0,0x0},	//invalid continuation
                                                              {0x000000E0,0x60800000,0x0,0x0},	//invalid continuation
                                                              {0x000000E0,0x9F800000,0x0,0x0},	//reserved zone
                                                              {0x000000ED,0xA0800000,0x0,0x0},	//reserved zone
                                                              {0x000000F0,0x8F808000,0x0,0x0},	//reserved zone
                                                              {0x000000F4,0x90808000,0x0,0x0}};	//reserved zone
uint64_t invalid_message_word64[invalid_message_size][4] = {{0xC000000000000000,0x0,0x0,0x0},	//invalid start
                                                            {0xF600000000000000,0x0,0x0,0x0},	//invalid start
                                                            {0x8000000000000000,0x0,0x0,0x0},	//invalid start
                                                            {0x00000000000000C2,0x8080000000000000,0x0,0x0},	//invalid continuation
                                                            {0x00000000000000E0,0x6080000000000000,0x0,0x0},	//invalid continuation
                                                            {0x00000000000000E0,0x9F80000000000000,0x0,0x0},	//reserved zone
                                                            {0x00000000000000ED,0xA080000000000000,0x0,0x0},	//reserved zone
                                                            {0x00000000000000F0,0x8F80800000000000,0x0,0x0},	//reserved zone
                                                            {0x00000000000000F4,0x9080800000000000,0x0,0x0}};	//reserved zone

static const char valid_message[] = "Hello-µ@ßöäüàá-UTF-8!!";
static const uint8_t valid_message_long[] = {0xF1,0x80,0x80,0x80,0xF2,0xA0,0xA0,0xA0};
const unsigned int valid_message_long_word[4] = {0xF1808080,0x00000F2A0,0xA0A00000,0xF2A0A0A0};
const uint64_t valid_message_long_word64[4] = {0xF1808080F2A0A0A0,0xF18080800000F2A0,0xA0A0F18080800000,0xF1808080F2A0A0A0};

struct F {
	F ()
	{
		cjet_init_checker(&c);
	}

	~F()
	{
	}

	struct cjet_utf8_checker c;
};

/**
 * @brief tests the utf8 checker with a message of length 0
 */
BOOST_AUTO_TEST_CASE(test_message_zero_length)
{
	F f;
	char zero = '\0';
	int ret = cjet_is_text_valid(&f.c, &zero, 0, true);
	BOOST_CHECK_MESSAGE(ret == true, "Message should be valid!");
	cjet_init_checker(&f.c);
	ret = cjet_is_word_sequence_valid(&f.c,(unsigned int* )&zero, 0, true);
	BOOST_CHECK_MESSAGE(ret == true, "Message should be valid!");
	cjet_init_checker(&f.c);
	ret = cjet_is_word64_sequence_valid(&f.c,(uint64_t*) &zero, 0, true);
	BOOST_CHECK_MESSAGE(ret == true, "Message should be valid!");
}

/**
 * @brief tests the utf8 checker with a valid message
 */
BOOST_AUTO_TEST_CASE(test_valid_message)
{
	F f;
	int ret = cjet_is_text_valid(&f.c, valid_message, sizeof(valid_message), true);
	BOOST_CHECK_MESSAGE(ret == true, "Message should be valid!");
}

/**
 * @brief tests the utf8 checker with a fragmented valid message
 *
 * The message is fragmented after an utf8 character, considering that
 * an utf8 character may consists of more than one byte.
 */
BOOST_AUTO_TEST_CASE(test_valid_message_fragmented_on_codepoints)
{
	F f;
	int ret = cjet_is_text_valid(&f.c, valid_message, 15, false);
	BOOST_CHECK_MESSAGE(ret == true, "Message should be valid!");
	ret = cjet_is_text_valid(&f.c, valid_message + 15, sizeof(valid_message) - 15, true);
	BOOST_CHECK_MESSAGE(ret == true, "Message should be valid!");
}

/**
 * @brief tests the utf8 checker with a fragmented valid message
 *
 * The message is fragmented after each byte. Hence some utf8 characters
 * are fragmented, too.
 */
BOOST_AUTO_TEST_CASE(test_valid_message_fragmented_between_letters)
{
	F f;
	int ret;
	for (unsigned int i = 0; i < sizeof(valid_message) - 1; i++) {
		ret = cjet_is_text_valid(&f.c, valid_message + i, 1, false);
		BOOST_CHECK_MESSAGE(ret == true, "Message should be valid!");
	}
	ret = cjet_is_text_valid(&f.c, valid_message + sizeof(valid_message) - 1, 1, true);
	BOOST_CHECK_MESSAGE(ret == true, "Message should be valid!");
}

/**
 * @brief tests the utf8 checker with a valid message
 *
 * The message consists of two 4 byte utf8 characters
 * In the word tests one character is distributed over two bytes
 */
BOOST_AUTO_TEST_CASE(test_valid_message_long)
{
	F f;
	int ret = cjet_is_byte_sequence_valid(&f.c, valid_message_long, sizeof(valid_message_long), true);
	BOOST_CHECK_MESSAGE(ret == true, "Message should be valid!");
	cjet_init_checker(&f.c);
	ret = cjet_is_word_sequence_valid(&f.c, valid_message_long_word, ARRAY_SIZE(valid_message_long_word), true);
	BOOST_CHECK_MESSAGE(ret == true, "Message should be valid!");
	cjet_init_checker(&f.c);
	ret = cjet_is_word64_sequence_valid(&f.c, valid_message_long_word64, ARRAY_SIZE(valid_message_long_word64), true);
	BOOST_CHECK_MESSAGE(ret == true, "Message should be valid!");
}

/**
 * @brief tests the utf8 checker with a fragmented valid message
 *
 * The message consists of two 4 byte utf8 characters and is fragmented
 * between them.
 */
BOOST_AUTO_TEST_CASE(test_valid_message_fragmented_on_codepoints_long)
{
	F f;
	int ret = cjet_is_byte_sequence_valid(&f.c, valid_message_long, 4, false);
	BOOST_CHECK_MESSAGE(ret == true, "Message should be valid!");
	ret = cjet_is_byte_sequence_valid(&f.c, valid_message_long + 4, sizeof(valid_message_long) - 4, true);
	BOOST_CHECK_MESSAGE(ret == true, "Message should be valid!");
}

/**
 * @brief tests the utf8 checker with a fragmented valid message
 *
 * The message consists of two 4 byte utf8 characters. The message
 * is fragmented after each byte.
 * In the word tests one character is distributed over two bytes.
 */
BOOST_AUTO_TEST_CASE(test_valid_message_fragmented_between_letters_long)
{
	F f;
	int ret;
	for (unsigned int i = 0; i < sizeof(valid_message_long) - 1; i++) {
		ret = cjet_is_byte_sequence_valid(&f.c, valid_message_long + i, 1, false);
		BOOST_CHECK_MESSAGE(ret == true, "Message should be valid!");
	}
	ret = cjet_is_byte_sequence_valid(&f.c, valid_message_long + sizeof(valid_message_long) - 1, 1, true);
	BOOST_CHECK_MESSAGE(ret == true, "Message should be valid!");
	cjet_init_checker(&f.c);
	for (unsigned int i = 0; i < ARRAY_SIZE(valid_message_long_word) - 1; i++) {
		ret = cjet_is_word_sequence_valid(&f.c, valid_message_long_word + i, 1, false);
		BOOST_CHECK_MESSAGE(ret == true, "Message should be valid!");
	}
	ret = cjet_is_word_sequence_valid(&f.c, valid_message_long_word + ARRAY_SIZE(valid_message_long_word) - 1, 1, true);
	BOOST_CHECK_MESSAGE(ret == true, "Message should be valid!");
	cjet_init_checker(&f.c);
	for (unsigned int i = 0; i < ARRAY_SIZE(valid_message_long_word64) - 1; i++) {
		ret = cjet_is_word64_sequence_valid(&f.c, valid_message_long_word64 + i, 1, false);
		BOOST_CHECK_MESSAGE(ret == true, "Message should be valid!");
	}
	ret = cjet_is_word64_sequence_valid(&f.c, valid_message_long_word64 + ARRAY_SIZE(valid_message_long_word64) - 1, 1, true);
	BOOST_CHECK_MESSAGE(ret == true, "Message should be valid!");

}

/**
 * @brief tests the utf8 checker with a invalid message
 *
 * In the word tests the invalid character is distributed over the first two bytes,
 * so it is similar to fragmentation. The function of the is_complete flag is checked
 * seperately, hence a fragmentation check for word validation is not necessary.
 */
BOOST_AUTO_TEST_CASE(test_invalid_message)
{
	int ret;
	for (unsigned int i = 0; i < invalid_message_size; i++) {
		F f;
		ret = cjet_is_byte_sequence_valid(&f.c, invalid_message[i], sizeof(invalid_message[i]), true);
		BOOST_CHECK_MESSAGE(ret == false, "Message should be invalid!");
	}
	for (unsigned int i = 0; i < invalid_message_size; i++) {
		F f;
		ret = cjet_is_word_sequence_valid(&f.c, invalid_message_word[i], 4, true);
		BOOST_CHECK_MESSAGE(ret == false, "Message should be invalid!");
	}
	for (unsigned int i = 0; i < invalid_message_size; i++) {
		F f;
		ret = cjet_is_word64_sequence_valid(&f.c, invalid_message_word64[i], 4, true);
		BOOST_CHECK_MESSAGE(ret == false, "Message should be invalid!");
	}
}

/**
 * @brief tests the utf8 checker with a fragmented invalid message
 *
 * The message is fragmented after an utf8 character, considering that
 * an utf8 character may consists of more than one byte.
 */
BOOST_AUTO_TEST_CASE(test_invalid_message_fragmented_on_codepoints)
{
	F f;
	int ret = cjet_is_byte_sequence_valid(&f.c, valid_message_long, sizeof(valid_message_long), false);
	BOOST_CHECK_MESSAGE(ret == 1, "Message should be valid!");
	ret = cjet_is_byte_sequence_valid(&f.c, invalid_message[1], sizeof(invalid_message[1]), true);
	BOOST_CHECK_MESSAGE(ret == false, "Message should be invalid!");
}

/**
 * @brief tests the utf8 checker with a fragmented invalid message
 *
 * The message is fragmented after each byte. Hence some utf8 characters
 * are fragmented, too.
 */
BOOST_AUTO_TEST_CASE(test_invalid_message_fragmented_between_letters)
{
	int ret;
	bool invalid = false;
	for (unsigned int i = 0; i < invalid_message_size; i++) {
		F f;
		for (unsigned int j = 0; j < sizeof(invalid_message[i]) - 1; j++) {
			ret = cjet_is_byte_sequence_valid(&f.c, &invalid_message[i][j], 1, false);
			if (ret < 1) {
				invalid = true;
				break;
			}
		}
		if (!invalid) {
			ret = cjet_is_byte_sequence_valid(&f.c, &invalid_message[i][sizeof(invalid_message[i]) - 1], 1, true);
		}
		BOOST_CHECK_MESSAGE(ret == false, "Message should be invalid!");
	}
}

/**
 * @brief tests the utf8 checkers is_complete flag
 *
 * An incomplete valid utf8 byte / word is checked with and without is_complete set.
 */
BOOST_AUTO_TEST_CASE(test_is_complete_flag)
{
	F f;
	uint8_t test_msg = 0xC2;
	unsigned int test_msg_word = 0xC2;
	uint64_t test_msg_word64 = 0xC2;
	int ret = cjet_is_byte_sequence_valid(&f.c, &test_msg, 1, false);
	BOOST_CHECK_MESSAGE(ret == true, "Message should be valid!");
	cjet_init_checker(&f.c);
	ret = cjet_is_byte_sequence_valid(&f.c, &test_msg, 1, true);
	BOOST_CHECK_MESSAGE(ret == false, "Message should be invalid!");
	cjet_init_checker(&f.c);
	ret = cjet_is_word_sequence_valid(&f.c, &test_msg_word, 1, false);
	BOOST_CHECK_MESSAGE(ret == true, "Message should be valid!");
	cjet_init_checker(&f.c);
	ret = cjet_is_word_sequence_valid(&f.c, &test_msg_word, 1, true);
	BOOST_CHECK_MESSAGE(ret == false, "Message should be invalid!");
	cjet_init_checker(&f.c);
	ret = cjet_is_word64_sequence_valid(&f.c, &test_msg_word64, 1, false);
	BOOST_CHECK_MESSAGE(ret == true, "Message should be valid!");
	cjet_init_checker(&f.c);
	ret = cjet_is_word64_sequence_valid(&f.c, &test_msg_word64, 1, true);
	BOOST_CHECK_MESSAGE(ret == false, "Message should be invalid!");
}
