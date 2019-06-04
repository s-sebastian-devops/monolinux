/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019, Erik Moqvist
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
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
 *
 * This file is part of the Monolinux project.
 */

#include <unicorn/unicorn.h>
#include "ml/ml.h"
#include "utils/mocks/mock_libc.h"
#include "utils/mocks/mock.h"

/* File descriptors. */
#define SOCK_FD   11
#define RENEW_FD  12
#define REBIND_FD 13
#define RESP_FD   14
#define INIT_FD   15

#define SOCK_IX   0
#define RENEW_IX  1
#define REBIND_IX 2
#define RESP_IX   3
#define INIT_IX   4

static uint8_t discover[] = {
    0x01, 0x01, 0x06, 0x00, 0x32, 0x49, 0x36, 0x78, 0x00, 0x00, 0x80, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x63, 0x82, 0x53, 0x63,
    0x35, 0x01, 0x01, 0x39, 0x02, 0x04, 0x00, 0x37, 0x03, 0x01, 0x06, 0x03,
    0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static uint8_t offer[1024] = {
    0x02, 0x01, 0x06, 0x00, 0x32, 0x49, 0x36, 0x78, 0x00, 0x00, 0x80, 0x00,
    0x00, 0x00, 0x00, 0x00, 0xc0, 0xa8, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x63, 0x82, 0x53, 0x63,
    0x35, 0x01, 0x02, 0x36, 0x04, 0xc0, 0xa8, 0x00, 0x01, 0x33, 0x04, 0x00,
    0x00, 0x00, 0x3c, 0x3a, 0x04, 0x00, 0x00, 0x00, 0x1e, 0x3b, 0x04, 0x00,
    0x00, 0x00, 0x34, 0x01, 0x04, 0xff, 0xff, 0xff, 0x00, 0x03, 0x04, 0xc0,
    0xa8, 0x00, 0x01, 0x06, 0x08, 0xc0, 0xa8, 0x00, 0x01, 0xc0, 0xa8, 0x01,
    0x01, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static uint8_t request[576] = {
    0x01, 0x01, 0x06, 0x00, 0x32, 0x49, 0x36, 0x78, 0x00, 0x00, 0x80, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x63, 0x82, 0x53, 0x63,
    0x35, 0x01, 0x03, 0x36, 0x04, 0x01, 0x02, 0x03, 0x04, 0x32, 0x04, 0xc0,
    0xa8, 0x00, 0x03, 0x33, 0x04, 0x00, 0x00, 0x00, 0x3c, 0xff, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static uint8_t ack[1024] = {
    0x02, 0x01, 0x06, 0x00, 0x32, 0x49, 0x36, 0x78, 0x00, 0x00, 0x80, 0x00,
    0x00, 0x00, 0x00, 0x00, 0xc0, 0xa8, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x63, 0x82, 0x53, 0x63,
    0x35, 0x01, 0x05, 0x36, 0x04, 0xc0, 0xa8, 0x00, 0x01, 0x33, 0x04, 0x00,
    0x00, 0x00, 0x3c, 0x3a, 0x04, 0x00, 0x00, 0x00, 0x1e, 0x3b, 0x04, 0x00,
    0x00, 0x00, 0x34, 0x0c, 0x02, 0x52, 0x30, 0x01, 0x04, 0xff, 0xff, 0xff,
    0x00, 0x03, 0x04, 0xc0, 0xa8, 0x00, 0x01, 0x06, 0x08, 0xc0, 0xa8, 0x00,
    0x01, 0xc0, 0xa8, 0x01, 0x01, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static uint8_t nak[1024] = {
    0x02, 0x01, 0x06, 0x00, 0x32, 0x49, 0x36, 0x78, 0x00, 0x00, 0x80, 0x00,
    0x00, 0x00, 0x00, 0x00, 0xc0, 0xa8, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x63, 0x82, 0x53, 0x63,
    0x35, 0x01, 0x06, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static void init_pollfds(struct pollfd *fds_p)
{
    memset(fds_p, 0, sizeof(*fds_p) * 5);
    fds_p[SOCK_IX].fd = SOCK_FD;
    fds_p[SOCK_IX].events = POLLIN;
    fds_p[RENEW_IX].fd = RENEW_FD;
    fds_p[RENEW_IX].events = POLLIN;
    fds_p[REBIND_IX].fd = REBIND_FD;
    fds_p[REBIND_IX].events = POLLIN;
    fds_p[RESP_IX].fd = RESP_FD;
    fds_p[RESP_IX].events = POLLIN;
    fds_p[INIT_IX].fd = INIT_FD;
    fds_p[INIT_IX].events = POLLIN;
}

static void mock_push_poll_fd(int index)
{
    struct pollfd fds[5];

    init_pollfds(&fds[0]);
    fds[index].revents = POLLIN;
    mock_push_poll(&fds[0], 5, -1, 1);
}

static void mock_push_dhcp_sendto(const uint8_t *buf_p, size_t size)
{
    struct sockaddr_in addr;

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_BROADCAST);
    addr.sin_port = htons(67);
    mock_push_sendto(SOCK_FD, buf_p, size, &addr, size);
}

static void mock_push_dhcp_read(uint8_t *buf_p, size_t size)
{
    mock_push_ml_read(SOCK_FD, buf_p, 1024, size);
}

static void mock_push_timer_read(int fd)
{
    uint64_t value;

    value = 0;
    mock_push_ml_read(fd, &value, sizeof(value), sizeof(value));
}

static void mock_push_ml_dhcp_client_start(void)
{
    struct itimerspec timeout;
    struct sockaddr_in addr;
    int yes;

    mock_push_socket(AF_INET, SOCK_DGRAM, 0, SOCK_FD);
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(68);
    addr.sin_addr.s_addr = INADDR_ANY;
    mock_push_bind(SOCK_FD, (struct sockaddr *)&addr, sizeof(addr), 0);
    yes = 1;
    mock_push_setsockopt(SOCK_FD,
                         SOL_SOCKET,
                         SO_BROADCAST,
                         &yes,
                         sizeof(yes),
                         0);
    mock_push_timerfd_create(CLOCK_REALTIME, 0, RENEW_FD);
    mock_push_timerfd_create(CLOCK_REALTIME, 0, REBIND_FD);
    mock_push_timerfd_create(CLOCK_REALTIME, 0, RESP_FD);
    mock_push_timerfd_create(CLOCK_REALTIME, 0, INIT_FD);
    memset(&timeout, 0, sizeof(timeout));
    timeout.it_value.tv_sec = 0;
    timeout.it_value.tv_nsec = 1;
    mock_push_timerfd_settime(INIT_FD, 0, &timeout, 0);
}

static void mock_push_init_to_selecting(void)
{
    struct itimerspec timeout;

    mock_push_poll_fd(INIT_IX);
    mock_push_timer_read(INIT_FD);
    mock_push_dhcp_sendto(&discover[0], sizeof(discover));
    memset(&timeout, 0, sizeof(timeout));
    timeout.it_value.tv_sec = 5;
    mock_push_timerfd_settime(RESP_FD, 0, &timeout, 0);
}

static void mock_push_selecting_to_requesting(void)
{
    struct itimerspec timeout;

    mock_push_poll_fd(SOCK_IX);
    mock_push_dhcp_read(&offer[0], sizeof(offer));
    mock_push_dhcp_sendto(&request[0], sizeof(request));
    memset(&timeout, 0, sizeof(timeout));
    timeout.it_value.tv_sec = 5;
    mock_push_timerfd_settime(RESP_FD, 0, &timeout, 0);
}

static void mock_push_requesting_to_bound(void)
{
    struct itimerspec timeout;

    mock_push_poll_fd(SOCK_IX);
    mock_push_dhcp_read(&ack[0], sizeof(ack));
    memset(&timeout, 0, sizeof(timeout));
    timeout.it_value.tv_sec = 0;
    mock_push_timerfd_settime(RESP_FD, 0, &timeout, 0);
    timeout.it_value.tv_sec = 50;
    mock_push_timerfd_settime(RENEW_FD, 0, &timeout, 0);
    timeout.it_value.tv_sec = 60;
    mock_push_timerfd_settime(REBIND_FD, 0, &timeout, 0);
}

static void mock_push_bound_to_renewing(void)
{
    struct itimerspec timeout;

    mock_push_poll_fd(RENEW_IX);
    mock_push_timer_read(RENEW_FD);
    mock_push_dhcp_sendto(&request[0], sizeof(request));
    memset(&timeout, 0, sizeof(timeout));
    timeout.it_value.tv_sec = 5;
    mock_push_timerfd_settime(RESP_FD, 0, &timeout, 0);
}

static void mock_push_renewing_to_bound(void)
{
    mock_push_requesting_to_bound();
}

static void mock_push_enter_init(void)
{
    struct itimerspec timeout;

    memset(&timeout, 0, sizeof(timeout));
    timeout.it_value.tv_sec = 0;
    mock_push_timerfd_settime(RESP_FD, 0, &timeout, 0);
    timeout.it_value.tv_sec = 0;
    mock_push_timerfd_settime(REBIND_FD, 0, &timeout, 0);
    timeout.it_value.tv_sec = 10;
    mock_push_timerfd_settime(INIT_FD, 0, &timeout, 0);
}

static void mock_push_poll_failure(void)
{
    struct pollfd fds[5];

    init_pollfds(&fds[0]);
    mock_push_poll(&fds[0], 5, -1, -1);
}

TEST(start_join)
{
    struct ml_dhcp_client_t client;

    mock_push_ml_dhcp_client_start();
    mock_push_poll_failure();

    ml_dhcp_client_init(&client, "eth0", ML_LOG_ALL);
    ml_dhcp_client_start(&client);
    ml_dhcp_client_join(&client);

    mock_finalize();
}

TEST(start_failure_last_init_step)
{
    struct ml_dhcp_client_t client;
    struct sockaddr_in addr;
    int yes;

    mock_push_socket(AF_INET, SOCK_DGRAM, 0, SOCK_FD);
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(68);
    addr.sin_addr.s_addr = INADDR_ANY;
    mock_push_bind(SOCK_FD, (struct sockaddr *)&addr, sizeof(addr), 0);
    yes = 1;
    mock_push_setsockopt(SOCK_FD,
                         SOL_SOCKET,
                         SO_BROADCAST,
                         &yes,
                         sizeof(yes),
                         0);
    mock_push_timerfd_create(CLOCK_REALTIME, 0, RENEW_FD);
    mock_push_timerfd_create(CLOCK_REALTIME, 0, REBIND_FD);
    mock_push_timerfd_create(CLOCK_REALTIME, 0, RESP_FD);
    mock_push_timerfd_create(CLOCK_REALTIME, 0, -1);
    mock_push_ml_close(RESP_FD, 0);
    mock_push_ml_close(REBIND_FD, 0);
    mock_push_ml_close(RENEW_FD, 0);
    mock_push_ml_close(SOCK_FD, 0);

    ml_dhcp_client_init(&client, "eth0", ML_LOG_ALL);
    ml_dhcp_client_start(&client);

    mock_finalize();
}

TEST(new)
{
    struct ml_dhcp_client_t client;

    mock_push_ml_dhcp_client_start();
    mock_push_init_to_selecting();
    mock_push_selecting_to_requesting();
    mock_push_requesting_to_bound();
    mock_push_poll_failure();

    ml_dhcp_client_init(&client, "eth0", ML_LOG_ALL);
    ml_dhcp_client_start(&client);
    ml_dhcp_client_join(&client);

    mock_finalize();
}

TEST(renew)
{
    struct ml_dhcp_client_t client;

    mock_push_ml_dhcp_client_start();
    mock_push_init_to_selecting();
    mock_push_selecting_to_requesting();
    mock_push_requesting_to_bound();
    mock_push_bound_to_renewing();
    mock_push_renewing_to_bound();
    mock_push_poll_failure();

    ml_dhcp_client_init(&client, "eth0", ML_LOG_ALL);
    ml_dhcp_client_start(&client);
    ml_dhcp_client_join(&client);
    FAIL();
    mock_finalize();
}

TEST(renew_nack)
{
    struct ml_dhcp_client_t client;

    mock_push_ml_dhcp_client_start();
    mock_push_init_to_selecting();
    mock_push_selecting_to_requesting();
    mock_push_requesting_to_bound();
    mock_push_bound_to_renewing();
    mock_push_poll_fd(SOCK_IX);
    mock_push_dhcp_read(&nak[0], sizeof(nak));
    mock_push_enter_init();
    mock_push_poll_failure();

    ml_dhcp_client_init(&client, "eth0", ML_LOG_ALL);
    ml_dhcp_client_start(&client);
    ml_dhcp_client_join(&client);

    mock_finalize();
}

TEST(renew_response_timeout)
{
    struct ml_dhcp_client_t client;

    mock_push_ml_dhcp_client_start();
    mock_push_init_to_selecting();
    mock_push_selecting_to_requesting();
    mock_push_requesting_to_bound();
    mock_push_bound_to_renewing();
    mock_push_poll_fd(RESP_IX);
    mock_push_timer_read(RESP_FD);
    mock_push_enter_init();
    mock_push_poll_failure();

    ml_dhcp_client_init(&client, "eth0", ML_LOG_ALL);
    ml_dhcp_client_start(&client);
    ml_dhcp_client_join(&client);

    mock_finalize();
}

TEST(rebind_timeout)
{
    struct ml_dhcp_client_t client;

    mock_push_ml_dhcp_client_start();
    mock_push_init_to_selecting();
    mock_push_selecting_to_requesting();
    mock_push_requesting_to_bound();
    mock_push_bound_to_renewing();
    mock_push_poll_fd(REBIND_IX);
    mock_push_timer_read(REBIND_FD);
    mock_push_enter_init();
    mock_push_poll_failure();

    ml_dhcp_client_init(&client, "eth0", ML_LOG_ALL);
    ml_dhcp_client_start(&client);
    ml_dhcp_client_join(&client);

    mock_finalize();
}

TEST(discovery_response_timeout)
{
    struct ml_dhcp_client_t client;

    mock_push_ml_dhcp_client_start();
    mock_push_init_to_selecting();
    mock_push_poll_fd(RESP_IX);
    mock_push_timer_read(RESP_FD);
    mock_push_enter_init();
    mock_push_poll_failure();

    ml_dhcp_client_init(&client, "eth0", ML_LOG_ALL);
    ml_dhcp_client_start(&client);
    ml_dhcp_client_join(&client);

    mock_finalize();
}

TEST(request_response_timeout)
{
    mock_finalize();
}

TEST(discard_offers_in_requesting)
{
    struct ml_dhcp_client_t client;

    mock_push_ml_dhcp_client_start();
    mock_push_init_to_selecting();
    mock_push_selecting_to_requesting();
    mock_push_poll_fd(SOCK_IX);
    mock_push_dhcp_read(&offer[0], sizeof(offer));
    mock_push_requesting_to_bound();
    mock_push_poll_failure();

    ml_dhcp_client_init(&client, "eth0", ML_LOG_ALL);
    ml_dhcp_client_start(&client);
    ml_dhcp_client_join(&client);

    mock_finalize();
}

TEST(request_nack)
{
    struct ml_dhcp_client_t client;

    mock_push_ml_dhcp_client_start();
    mock_push_init_to_selecting();
    mock_push_selecting_to_requesting();
    mock_push_poll_fd(SOCK_IX);
    mock_push_dhcp_read(&nak[0], sizeof(nak));
    mock_push_enter_init();
    mock_push_poll_failure();

    ml_dhcp_client_init(&client, "eth0", ML_LOG_ALL);
    ml_dhcp_client_start(&client);
    ml_dhcp_client_join(&client);

    mock_finalize();
}

int main()
{
    ml_init();

    return RUN_TESTS(
        start_join,
        start_failure_last_init_step,
        new,
        renew,
        renew_nack,
        renew_response_timeout,
        rebind_timeout,
        discovery_response_timeout,
        request_response_timeout,
        discard_offers_in_requesting,
        request_nack
    );
}
