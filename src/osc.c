/*
 * Flickernoise
 * Copyright (C) 2010 Sebastien Bourdeauducq
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <rtems.h>
#include <assert.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

#include <lop/lop_lowlevel.h>

#include "input.h"
#include "renderer/framedescriptor.h"
#include "renderer/osd.h"

#include "osc.h"

static int midi_method(const char *path, const char *types,
	lop_arg **argv, int argc, lop_message msg,
	void *user_data)
{
	unsigned char *midimsg;

	midimsg = (unsigned char *)argv[0];
	input_inject_midi(&midimsg[1]);
	return 0;
}

static int patch_method(const char *path, const char *types,
	lop_arg **argv, int argc, lop_message msg,
	void *user_data)
{
	unsigned int patchnr;

	patchnr = *((unsigned int *)argv[0]);
	input_inject_osc(patchnr);
	return 0;
}

static float osc_variables[OSC_COUNT];

static int variable_method(const char *path, const char *types,
	lop_arg **argv, int argc, lop_message msg,
	void *user_data)
{
	unsigned int varnr;
	float value;

	varnr = *((unsigned int *)argv[0]);
	value = *((float *)argv[1]);

	varnr--;
	if(varnr < OSC_COUNT)
		osc_variables[varnr] = value;
	return 0;
}

void get_osc_variables(float *out)
{
	int i;

	for(i=0;i<OSC_COUNT;i++)
		out[i] = osc_variables[i];
}

static int osd_method(const char *path, const char *types,
	lop_arg **argv, int argc, lop_message msg,
	void *user_data)
{
	osd_event((const char *)argv[0]);
	return 0;
}

static void error_handler(int num, const char *msg, const char *where)
{
	printf("liboscparse error in %s: %s\n", where, msg);
}

static int udpsocket;
static struct sockaddr_in local;
static struct sockaddr_in remote;

static void send_handler(const char *msg, size_t len, void *arg)
{
	socklen_t slen;

	slen = sizeof(struct sockaddr_in);
	sendto(udpsocket, msg, len, 0, (struct sockaddr *)&remote, slen);
}

static void set_socket_timeout(int s, unsigned int microseconds)
{
	struct timeval tv;

	tv.tv_sec = microseconds / 1000000;
	tv.tv_usec = microseconds % 1000000;

	setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(tv));
}

static rtems_task osc_task(rtems_task_argument argument)
{
	int r;
	socklen_t slen;
	char buf[1024];
	lop_server server;

	server = lop_server_new(error_handler, send_handler, NULL);
	assert(server != NULL);

	lop_server_add_method(server, "/midi", "m", midi_method, NULL);
	lop_server_add_method(server, "/patch", "i", patch_method, NULL);
	lop_server_add_method(server, "/variable", "if", variable_method, NULL);
	lop_server_add_method(server, "/osd", "s", osd_method, NULL);
	
	udpsocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if(udpsocket == -1) {
		printf("Unable to create socket for OSC server\n");
		return;
	}

	memset((char *)&local, 0, sizeof(struct sockaddr_in));
	local.sin_family = AF_INET;
	local.sin_port = htons(4444);
	local.sin_addr.s_addr = htonl(INADDR_ANY);
	r = bind(udpsocket, (struct sockaddr *)&local, sizeof(struct sockaddr_in));
	if(r == -1) {
		printf("Unable to bind socket for OSC server\n");
		close(udpsocket);
		return;
	}

	while(1) {
		set_socket_timeout(udpsocket, ((double)1000000.0)*lop_server_next_event_delay(server));
		slen = sizeof(struct sockaddr_in);
		r = recvfrom(udpsocket, buf, 1024, 0, (struct sockaddr *)&remote, &slen);
		if(r > 0)
			lop_server_dispatch_data(server, buf, r);
		else
			lop_server_dispatch_data(server, NULL, 0);
	}
}

static rtems_id osc_task_id;

void init_osc()
{
	rtems_status_code sc;

	sc = rtems_task_create(rtems_build_name('O', 'S', 'C', ' '), 15, 32*1024,
		RTEMS_PREEMPT | RTEMS_NO_TIMESLICE | RTEMS_NO_ASR,
		0, &osc_task_id);
	assert(sc == RTEMS_SUCCESSFUL);
	sc = rtems_task_start(osc_task_id, osc_task, 0);
	assert(sc == RTEMS_SUCCESSFUL);
}
