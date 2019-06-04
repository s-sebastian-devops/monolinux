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

#define _GNU_SOURCE

#include <sys/mman.h>
#include <string.h>
#include <stdint.h>
#include <sys/timerfd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include "ml/ml.h"

#define BOOTP_MESSAGE_TYPE_BOOT_REQUEST 1
#define BOOTP_MESSAGE_TYPE_BOOT_RESPONSE 2
#define HARDWARE_TYPE_ETHERNET 1
#define HARDWARE_ADDRESS_LENGTH 6
#define BOOT_FLAGS 0x8000
#define MAGIC_COOKIE_DHCP 0x63825363
#define OPTION_SUBNET_MASK 1
#define OPTION_ROUTER 2
#define OPTION_DOMAIN_NAME_SERVER 6
#define OPTION_HOST_NAME 12
#define OPTION_REQUSETED_IP_ADDRESS 50
#define OPTION_IP_ADDRESS_LEASE_TIME 51
#define OPTION_DHCP_MESSAGE_TYPE 53
#define OPTION_DHCP_SERVER_IDENTIFIER 54
#define OPTION_PARAMETER_REQUEST_LIST 55
#define OPTION_MAXIMUM_DHCP_MESSAGE_SIZE 57
#define OPTION_RENEWAL_TIME_INTERVAL 58
#define OPTION_REBINDING_TIME_INTERVAL 59
#define OPTION_END 255
#define MESSAGE_TYPE_DISCOVER 1
#define MESSAGE_TYPE_OFFER 2
#define MESSAGE_TYPE_REQUEST 3
#define MESSAGE_TYPE_ACK 5

#define WAIT_FOREVER -1

struct options_t {
    uint8_t message_type;
    bool message_type_valid;
    uint32_t subnet_mask;
    bool subnet_mask_valid;
};

static bool is_offer(struct ml_dhcp_client_t *self_p)
{
    return (self_p->packet_type == ml_dhcp_client_packet_type_offer_t);
}

static bool is_ack(struct ml_dhcp_client_t *self_p)
{
    return (self_p->packet_type == ml_dhcp_client_packet_type_ack_t);
}

static bool is_nak(struct ml_dhcp_client_t *self_p)
{
    return (self_p->packet_type == ml_dhcp_client_packet_type_nak_t);
}

static bool is_response_timeout(struct ml_dhcp_client_t *self_p)
{
    return (self_p->response_timer_expired);
}

static bool is_renewal_timeout(struct ml_dhcp_client_t *self_p)
{
    return (self_p->renewal_timer_expired);
}

static bool is_rebinding_timeout(struct ml_dhcp_client_t *self_p)
{
    return (self_p->rebinding_timer_expired);
}

static bool is_init_timeout(struct ml_dhcp_client_t *self_p)
{
    return (self_p->init_timer_expired);
}

static int unpack_options(struct ml_dhcp_client_t *self_p,
                          struct options_t *options_p,
                          const uint8_t *buf_p,
                          size_t size)
{
    int fd;
    int res;
    uint8_t option;
    uint8_t length;
    uint8_t buf[255];

    fd = memfd_create("packet", 0);

    if (fd == -1) {
        return (fd);
    }

    if (write(fd, buf_p, size) != (ssize_t)size) {
        return (-1);
    }

    if (lseek(fd, 0, SEEK_SET) == -1) {
        return (-1);
    }

    memset(options_p, 0, sizeof(*options_p));

    while (true) {
        res = read(fd, &option, sizeof(option));

        if (res != sizeof(option)) {
            return (-1);
        }

        if (option == OPTION_END) {
            break;
        }

        res = read(fd, &length, sizeof(length));

        if (res != sizeof(length)) {
            return (-1);
        }

        res = read(fd, &buf[0], length);

        if (res != length) {
            return (-1);
        }

        switch (option) {

        case OPTION_SUBNET_MASK:
            if (length != 4) {
                return (-1);
            }

            options_p->subnet_mask = ((buf[0] << 24)
                                      | (buf[1] << 16)
                                      | (buf[2] << 8)
                                      | (buf[3] << 0));
            options_p->subnet_mask_valid = true;
            break;

        case OPTION_DHCP_MESSAGE_TYPE:
            if (length != 1) {
                return (-1);
            }

            options_p->message_type = buf[0];
            options_p->message_type_valid = true;
            break;

        default:
            ML_DEBUG(&self_p->log_object,
                     "Ignoring DHCP option %d.",
                     option);
            break;
        }
    }

    return (0);
}

static int unpack_packet(struct ml_dhcp_client_t *self_p,
                         const uint8_t *buf_p,
                         size_t size)
{
    (void)size;

    int res;
    struct options_t options;

    if (buf_p[0] != BOOTP_MESSAGE_TYPE_BOOT_RESPONSE) {
        return (-1);
    }

    if (buf_p[1] != HARDWARE_TYPE_ETHERNET) {
        return (-1);
    }

    if (size <= 240) {
        return (-1);
    }

    res = unpack_options(self_p,
                         &options,
                         &buf_p[240],
                         size - 240);

    if (res != 0) {
        return (res);
    }

    if (!options.message_type_valid) {
        return (-1);
    }

    switch (options.message_type) {

    case 2:
        self_p->packet_type = ml_dhcp_client_packet_type_offer_t;
        break;

    case 5:
        self_p->packet_type = ml_dhcp_client_packet_type_ack_t;
        break;

    case 6:
        self_p->packet_type = ml_dhcp_client_packet_type_nak_t;
        break;

    default:
        ML_DEBUG(&self_p->log_object,
                 "Invalid packet type %d.",
                 options.message_type);
        res = -1;
        break;
    }

    printf("packet_type: %d\n", self_p->packet_type);

    return (res);
}

static void configure_interface(struct ml_dhcp_client_t *self_p)
{
    (void)self_p;
}

static bool is_timeout(struct pollfd *fd_p)
{
    bool timeout;
    ssize_t size;
    uint64_t value;

    timeout = false;

    if (fd_p->revents & POLLIN) {
        size = ml_read(fd_p->fd, &value, sizeof(value));

        if (size == sizeof(value)) {
            timeout = true;
        }
    }

    return (timeout);
}

static void update_events(struct ml_dhcp_client_t *self_p)
{
    uint8_t buf[1024];
    ssize_t size;

    PRINT_FILE_LINE();

    /* Default values. */
    self_p->packet_type = ml_dhcp_client_packet_type_none_t;

    if (self_p->fds[0].revents & POLLIN) {
        size = ml_read(self_p->fds[0].fd, &buf[0], sizeof(buf));

        if (size > 0) {
            unpack_packet(self_p, &buf[0], size);
        }
    }

    self_p->renewal_timer_expired = is_timeout(&self_p->fds[1]);
    self_p->rebinding_timer_expired = is_timeout(&self_p->fds[2]);
    self_p->response_timer_expired = is_timeout(&self_p->fds[3]);
    self_p->init_timer_expired = is_timeout(&self_p->fds[4]);
}

static int set_timer(int fd, int seconds)
{
    struct itimerspec timeout;

    memset(&timeout, 0, sizeof(timeout));
    timeout.it_value.tv_sec = seconds;

    return (timerfd_settime(fd, 0, &timeout, NULL));
}

static int set_renewal_timer(struct ml_dhcp_client_t *self_p)
{
    return (set_timer(self_p->fds[1].fd, 50));
}

static int set_rebinding_timer(struct ml_dhcp_client_t *self_p)
{
    return (set_timer(self_p->fds[2].fd, 60));
}

static int set_response_timer(struct ml_dhcp_client_t *self_p)
{
    return (set_timer(self_p->fds[3].fd, 5));
}

static void cancel_rebinding_timer(struct ml_dhcp_client_t *self_p)
{
    set_timer(self_p->fds[2].fd, 0);
}

static void cancel_response_timer(struct ml_dhcp_client_t *self_p)
{
    set_timer(self_p->fds[3].fd, 0);
}

static int set_init_timer(struct ml_dhcp_client_t *self_p, int seconds)
{
    return (set_timer(self_p->fds[4].fd, seconds));
}

static bool send_packet(struct ml_dhcp_client_t *self_p,
                        uint8_t *buf_p,
                        size_t size)
{
    bool ok;
    ssize_t res;

    ok = false;
    res = ml_write(self_p->fds[0].fd, buf_p, size);

    if (res == (ssize_t)size) {
        res = set_response_timer(self_p);

        if (res != -1) {
            ok = true;
        }
    }

    return (ok);
}

static bool send_discover(struct ml_dhcp_client_t *self_p)
{
    uint8_t buf[576];
    uint32_t transaction_id;

    transaction_id = 0x32493678;

    memset(&buf[0], 0, sizeof(buf));
    buf[0] = BOOTP_MESSAGE_TYPE_BOOT_REQUEST;
    buf[1] = HARDWARE_TYPE_ETHERNET;
    buf[2] = HARDWARE_ADDRESS_LENGTH;
    buf[4] = (transaction_id >> 24);
    buf[5] = (transaction_id >> 16);
    buf[6] = (transaction_id >> 8);
    buf[7] = (transaction_id >> 0);
    buf[10] = (uint8_t)(BOOT_FLAGS >> 8);
    buf[11] = (uint8_t)(BOOT_FLAGS >> 0);
    memcpy(&buf[28],
           &self_p->self.mac_address[0],
           sizeof(self_p->self.mac_address));
    buf[236] = (uint8_t)(MAGIC_COOKIE_DHCP >> 24);
    buf[237] = (uint8_t)(MAGIC_COOKIE_DHCP >> 16);
    buf[238] = (uint8_t)(MAGIC_COOKIE_DHCP >> 8);
    buf[239] = (uint8_t)(MAGIC_COOKIE_DHCP >> 0);
    buf[240] = OPTION_DHCP_MESSAGE_TYPE;
    buf[241] = 1;
    buf[242] = MESSAGE_TYPE_DISCOVER;
    buf[243] = OPTION_MAXIMUM_DHCP_MESSAGE_SIZE;
    buf[244] = 2;
    buf[245] = (uint8_t)(1024 >> 8);
    buf[246] = (uint8_t)(1024 >> 0);
    buf[247] = OPTION_PARAMETER_REQUEST_LIST;
    buf[248] = 3;
    buf[249] = 1;
    buf[250] = 6;
    buf[251] = 3;
    buf[252] = OPTION_END;

    return (send_packet(self_p, &buf[0], sizeof(buf)));
}

static bool send_request(struct ml_dhcp_client_t *self_p)
{
    uint8_t buf[576];
    uint32_t transaction_id;

    transaction_id = 0x32493678;

    memset(&buf[0], 0, sizeof(buf));
    buf[0] = BOOTP_MESSAGE_TYPE_BOOT_REQUEST;
    buf[1] = HARDWARE_TYPE_ETHERNET;
    buf[2] = HARDWARE_ADDRESS_LENGTH;
    buf[4] = (transaction_id >> 24);
    buf[5] = (transaction_id >> 16);
    buf[6] = (transaction_id >> 8);
    buf[7] = (transaction_id >> 0);
    buf[10] = (uint8_t)(BOOT_FLAGS >> 8);
    buf[11] = (uint8_t)(BOOT_FLAGS >> 0);
    memcpy(&buf[28],
           &self_p->self.mac_address[0],
           sizeof(self_p->self.mac_address));
    buf[236] = (uint8_t)(MAGIC_COOKIE_DHCP >> 24);
    buf[237] = (uint8_t)(MAGIC_COOKIE_DHCP >> 16);
    buf[238] = (uint8_t)(MAGIC_COOKIE_DHCP >> 8);
    buf[239] = (uint8_t)(MAGIC_COOKIE_DHCP >> 0);
    buf[240] = OPTION_DHCP_MESSAGE_TYPE;
    buf[241] = 1;
    buf[242] = MESSAGE_TYPE_REQUEST;
    buf[243] = OPTION_DHCP_SERVER_IDENTIFIER;
    buf[244] = 4;
    buf[245] = (self_p->server.ip_address >> 24);
    buf[246] = (self_p->server.ip_address >> 16);
    buf[247] = (self_p->server.ip_address >> 8);
    buf[248] = (self_p->server.ip_address >> 0);
    buf[249] = OPTION_REQUSETED_IP_ADDRESS;
    buf[250] = 4;
    buf[251] = (self_p->offer.ip_address >> 24);
    buf[252] = (self_p->offer.ip_address >> 16);
    buf[253] = (self_p->offer.ip_address >> 8);
    buf[254] = (self_p->offer.ip_address >> 0);
    buf[255] = OPTION_IP_ADDRESS_LEASE_TIME;
    buf[256] = 4;
    buf[257] = (self_p->offer.lease_time >> 24);
    buf[258] = (self_p->offer.lease_time >> 16);
    buf[259] = (self_p->offer.lease_time >> 8);
    buf[260] = (self_p->offer.lease_time >> 0);
    buf[261] = OPTION_END;

    return (send_packet(self_p, &buf[0], sizeof(buf)));
}

static const char *state_string(enum ml_dhcp_client_state_t state)
{
    const char *res_p;

    switch (state) {

    case ml_dhcp_client_state_init_t:
        res_p = "INIT";
        break;

    case ml_dhcp_client_state_selecting_t:
        res_p = "SELECTING";
        break;

    case ml_dhcp_client_state_requesting_t:
        res_p = "REQUESTING";
        break;

    case ml_dhcp_client_state_bound_t:
        res_p = "BOUND";
        break;

    case ml_dhcp_client_state_renewing_t:
        res_p = "RENEWING";
        break;

    case ml_dhcp_client_state_rebinding_t:
        res_p = "REBINDING";
        break;

    default:
        res_p = "UNKNOWN";
        break;
    }

    return (res_p);
}

static void change_state(struct ml_dhcp_client_t *self_p,
                         enum ml_dhcp_client_state_t state)
{
    ML_INFO(&self_p->log_object,
            "State change from %s to %s.",
            state_string(self_p->state),
            state_string(state));

    self_p->state = state;
}

static void enter_init(struct ml_dhcp_client_t *self_p)
{
    cancel_response_timer(self_p);
    cancel_rebinding_timer(self_p);
    set_init_timer(self_p, 10);
    change_state(self_p, ml_dhcp_client_state_init_t);
}

static void enter_selecting(struct ml_dhcp_client_t *self_p)
{
    change_state(self_p, ml_dhcp_client_state_selecting_t);
}

static void enter_requesting(struct ml_dhcp_client_t *self_p)
{
    change_state(self_p, ml_dhcp_client_state_requesting_t);
}

static void enter_bound(struct ml_dhcp_client_t *self_p)
{
    cancel_response_timer(self_p);
    set_renewal_timer(self_p);
    set_rebinding_timer(self_p);
    configure_interface(self_p);
    change_state(self_p, ml_dhcp_client_state_bound_t);
}

static void enter_renewing(struct ml_dhcp_client_t *self_p)
{
    change_state(self_p, ml_dhcp_client_state_renewing_t);
}

static void enter_rebinding(struct ml_dhcp_client_t *self_p)
{
    change_state(self_p, ml_dhcp_client_state_rebinding_t);
}

static void process_events_init(struct ml_dhcp_client_t *self_p)
{
    if (is_init_timeout(self_p)) {
        if (send_discover(self_p)) {
            enter_selecting(self_p);
        } else {
            set_init_timer(self_p, 10);
        }
    }
}

static void process_events_selecting(struct ml_dhcp_client_t *self_p)
{
    if (is_offer(self_p)) {
        if (send_request(self_p)) {
            enter_requesting(self_p);
        } else {
            enter_init(self_p);
        }
    } else if (is_response_timeout(self_p)) {
        enter_init(self_p);
    }
}

static void process_events_requesting(struct ml_dhcp_client_t *self_p)
{
    if (is_ack(self_p)) {
        enter_bound(self_p);
    } else if (is_response_timeout(self_p)) {
        enter_init(self_p);
    }
}

static void process_events_bound(struct ml_dhcp_client_t *self_p)
{
    if (is_renewal_timeout(self_p)) {
        if (send_request(self_p)) {
            enter_renewing(self_p);
        } else {
            enter_init(self_p);
        }
    }
}

static void process_events_renewing(struct ml_dhcp_client_t *self_p)
{
    if (is_ack(self_p)) {
        enter_bound(self_p);
    } else if (is_response_timeout(self_p)) {
        enter_init(self_p);
    } else if (is_rebinding_timeout(self_p)) {
        if (send_request(self_p)) {
            enter_rebinding(self_p);
        } else {
            enter_init(self_p);
        }
    }
}

static void process_events_rebinding(struct ml_dhcp_client_t *self_p)
{
    if (is_ack(self_p)) {
        enter_bound(self_p);
    } else if (is_nak(self_p)) {
        enter_init(self_p);
    } else if (is_response_timeout(self_p)) {
        enter_init(self_p);
    }
}

static void init_pollfd(struct pollfd *fd_p, int fd)
{
    fd_p->fd = fd;
    fd_p->events = POLLIN;
}

static int setup_socket(struct ml_dhcp_client_t *self_p)
{
    int sock;

    sock = socket(AF_INET, SOCK_DGRAM, 0);

    if (sock != -1) {
        init_pollfd(&self_p->fds[0], sock);
        sock = 0;
    }

    return (sock);
}

static int setup_renewal_timer(struct ml_dhcp_client_t *self_p)
{
    int fd;

    fd = timerfd_create(CLOCK_REALTIME, 0);

    if (fd != -1) {
        init_pollfd(&self_p->fds[1], fd);
        fd = 0;
    }

    return (fd);
}

static int setup_rebinding_timer(struct ml_dhcp_client_t *self_p)
{
    int fd;

    fd = timerfd_create(CLOCK_REALTIME, 0);

    if (fd != -1) {
        init_pollfd(&self_p->fds[2], fd);
        fd = 0;
    }

    return (fd);
}

static int setup_response_timer(struct ml_dhcp_client_t *self_p)
{
    int fd;

    fd = timerfd_create(CLOCK_REALTIME, 0);

    if (fd != -1) {
        init_pollfd(&self_p->fds[3], fd);
        fd = 0;
    }

    return (fd);
}

static int setup_init_timer(struct ml_dhcp_client_t *self_p)
{
    int fd;

    fd = timerfd_create(CLOCK_REALTIME, 0);

    if (fd != -1) {
        init_pollfd(&self_p->fds[4], fd);
        fd = 0;
    }

    return (fd);
}

static int init(struct ml_dhcp_client_t *self_p)
{
    int res;

    res = setup_socket(self_p);

    if (res != 0) {
        goto err1;
    }

    res = setup_renewal_timer(self_p);

    if (res != 0) {
        goto err2;
    }

    res = setup_rebinding_timer(self_p);

    if (res != 0) {
        goto err3;
    }

    res = setup_response_timer(self_p);

    if (res != 0) {
        goto err4;
    }

    res = setup_init_timer(self_p);

    if (res != 0) {
        goto err5;
    }

    return (res);

 err5:
    ml_close(self_p->fds[3].fd);

 err4:
    ml_close(self_p->fds[2].fd);

 err3:
    ml_close(self_p->fds[1].fd);

 err2:
    ml_close(self_p->fds[0].fd);

 err1:

    return (res);
}

static void destroy(struct ml_dhcp_client_t *self_p)
{
    size_t i;

    for (i = 0; i < membersof(self_p->fds); i++) {
        ml_close(self_p->fds[i].fd);
    }
}

static void process_events(struct ml_dhcp_client_t *self_p)
{
    switch (self_p->state) {

    case ml_dhcp_client_state_init_t:
        process_events_init(self_p);
        break;

    case ml_dhcp_client_state_selecting_t:
        process_events_selecting(self_p);
        break;

    case ml_dhcp_client_state_requesting_t:
        process_events_requesting(self_p);
        break;

    case ml_dhcp_client_state_bound_t:
        process_events_bound(self_p);
        break;

    case ml_dhcp_client_state_renewing_t:
        process_events_renewing(self_p);
        break;

    case ml_dhcp_client_state_rebinding_t:
        process_events_rebinding(self_p);
        break;

    default:
        break;
    }
}

static void *client_main(void *arg_p)
{
    int res;
    struct ml_dhcp_client_t *self_p;

    self_p = (struct ml_dhcp_client_t *)arg_p;

    while (true) {
        PRINT_FILE_LINE();
        res = poll(&self_p->fds[0], membersof(self_p->fds), WAIT_FOREVER);

        if (res <= 0) {
            break;
        }

        update_events(self_p);
        process_events(self_p);
    }

    return (NULL);
}

void ml_dhcp_client_init(struct ml_dhcp_client_t *self_p,
                         const char *interface_name_p,
                         int log_mask)
{
    self_p->interface_name_p = interface_name_p;
    self_p->state = ml_dhcp_client_state_init_t;
    self_p->self.mac_address[0] = 1;
    self_p->self.mac_address[1] = 2;
    self_p->self.mac_address[2] = 3;
    self_p->self.mac_address[3] = 4;
    self_p->self.mac_address[4] = 5;
    self_p->self.mac_address[5] = 6;
    self_p->server.ip_address = 0x01020304;
    self_p->offer.ip_address = 0x05060708;
    self_p->offer.lease_time = 0x090a0b0c;
    ml_log_object_init(&self_p->log_object,
                       "dhcp_client",
                       log_mask);
}

int ml_dhcp_client_start(struct ml_dhcp_client_t *self_p)
{
    int res;

    res = init(self_p);

    if (res == 0) {
        set_init_timer(self_p, 0);

        res = pthread_create(&self_p->pthread, NULL, client_main, self_p);

        if (res != 0) {
            destroy(self_p);
        }
    }

    return (res);
}

void ml_dhcp_client_stop(struct ml_dhcp_client_t *self_p)
{
    destroy(self_p);
}

int ml_dhcp_client_join(struct ml_dhcp_client_t *self_p)
{
    return (pthread_join(self_p->pthread, NULL));
}