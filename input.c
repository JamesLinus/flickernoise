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

#include <stdio.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <rtems.h>

#include <mtklib.h>
#include <keycodes.h>

#include "input.h"

static inline void write_motion_event(mtk_event *e, int rel_x, int rel_y)
{
	e->type = EVENT_TYPE_MOTION;
	e->motion.rel_x = rel_x;
	e->motion.rel_y = rel_y;
}

static inline void write_press_event(mtk_event *e, int code)
{
	e->type = EVENT_TYPE_PRESS;
	e->press.code = code;
}

static inline void write_release_event(mtk_event *e, int code)
{
	e->type = EVENT_TYPE_RELEASE;
	e->release.code = code;
}

#define MOUSE_LEFT       0x01000000
#define MOUSE_RIGHT      0x02000000
#define MOUSE_HOR_MASK   0x00ff0000
#define MOUSE_VER_MASK   0x0000ff00
#define MOUSE_HOR_SHIFT  16
#define MOUSE_VER_SHIFT  8
#define MOUSE_HOR_NEG    0x00800000
#define MOUSE_VER_NEG    0x00008000

static unsigned int old_mstate;

static int handle_mouse_event(mtk_event *e, unsigned char *msg)
{
	unsigned int mstate;
	int n;

	mstate = ((unsigned int)msg[0] << 24)
		|((unsigned int)msg[1] << 16)
		|((unsigned int)msg[2] << 8)
		|((unsigned int)msg[3]);

	n = 0;
	/* left mouse button pressed */
	if(!(old_mstate & MOUSE_LEFT) && (mstate & MOUSE_LEFT)) {
		write_press_event(e+n, MTK_BTN_LEFT);
		n++;
	}

	/* left mouse button released */
	if((old_mstate & MOUSE_LEFT) && !(mstate & MOUSE_LEFT)) {
		write_release_event(e+n, MTK_BTN_LEFT);
		n++;
	}

	/* right mouse button pressed */
	if(!(old_mstate & MOUSE_RIGHT) && (mstate & MOUSE_RIGHT)) {
		write_press_event(e+n, MTK_BTN_RIGHT);
		n++;
	}

	/* right mouse button released */
	if((old_mstate & MOUSE_RIGHT) && !(mstate & MOUSE_RIGHT)) {
		write_release_event(e+n, MTK_BTN_RIGHT);
		n++;
	}

	/* check for mouse motion */
	if((mstate & MOUSE_HOR_MASK) || (mstate & MOUSE_VER_MASK)) {
		write_motion_event(e+n,
		      (mstate & MOUSE_HOR_NEG) ? (((mstate & MOUSE_HOR_MASK) >> MOUSE_HOR_SHIFT) | 0xFFFFFF00) : (mstate & MOUSE_HOR_MASK) >> MOUSE_HOR_SHIFT,
		      (mstate & MOUSE_VER_NEG) ?  (int)((mstate & MOUSE_VER_MASK) >> MOUSE_VER_SHIFT | 0xFFFFFF00) : (int)((mstate & MOUSE_VER_MASK) >> MOUSE_VER_SHIFT) );
		n++;
	}

	old_mstate = mstate;

	return n;
}

/* translation table for scancode set 2 to scancode set 1 conversion */
static int keyb_translation_table[256] = {
/*          0     1     2     3     4     5     6     7     8     9     a     b     c     d     e     f */
/* 00 */ 0xff, 0xff, 0xff, 0xff, 0x1e, 0x30, 0x2e, 0x20, 0x12, 0x21, 0x22, 0x23, 0x17, 0x24, 0x25, 0x26,
/* 10 */ 0x32, 0x31, 0x18, 0x19, 0x10, 0x13, 0x1f, 0x14, 0x16, 0x2f, 0x11, 0x2d, 0x15, 0x2c, 0x02, 0x03,
/* 20 */ 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x1c, 0x01, 0x0e, 0x0f, 0x39, 0xff, 0x2b, 0x1a,
/* 30 */ 0x1b, 0xff, 0x2b, 0x27, 0x28, 0xff, 0x33, 0x34, 0x35, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
/* 40 */ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
/* 50 */ 0xff, 0xff, 0xff, 0xff, 0x62, 0xff, 0x4a, 0xff, 0x60, 0x4f, 0x50, 0x51, 0x4b, 0x4c, 0x4e, 0x47,
/* 60 */ 0x48, 0x49, 0x52, 0x53, 0x56, 0xff, 0xff, 0x49, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
/* 70 */ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
/* 80 */ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
/* 90 */ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
/* a0 */ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
/* b0 */ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
/* c0 */ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
/* d0 */ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
/* e0 */ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
/* f0 */ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
};

static unsigned char old_modifiers;
static unsigned char old_keys[5];

static void check_modifier(mtk_event *e, int *n, unsigned char modifiers, unsigned char mask, int keycode)
{
	if((modifiers & mask) && !(old_modifiers & mask)) {
		write_press_event(e+(*n), keycode);
		(*n)++;
	}
	if(!(modifiers & mask) && (old_modifiers & mask)) {
		write_release_event(e+(*n), keycode);
		(*n)++;
	}
}

static int handle_keybd_event(mtk_event *e, unsigned char *msg)
{
	int i, j;
	int n;

	if(msg[7] != 0x00)
		/* error */
		return 0;

	n = 0;

#ifdef DUMP_KEYBOARD
	if(msg[0] != 0x00)
		printf("modifiers: %02x\n", msg[0]);
#endif

	/* handle modifiers */
	check_modifier(e, &n, msg[0], 0x02, MTK_KEY_LEFTSHIFT);
	check_modifier(e, &n, msg[0], 0x01, MTK_KEY_LEFTCTRL);
	check_modifier(e, &n, msg[0], 0x08, MTK_KEY_LEFTMETA);
	check_modifier(e, &n, msg[0], 0x04, MTK_KEY_LEFTALT);
	check_modifier(e, &n, msg[0], 0x40, MTK_KEY_RIGHTALT);
	check_modifier(e, &n, msg[0], 0x80, MTK_KEY_RIGHTMETA);
	check_modifier(e, &n, msg[0], 0x10, MTK_KEY_RIGHTCTRL);
	check_modifier(e, &n, msg[0], 0x20, MTK_KEY_RIGHTSHIFT);

	/* handle normal key presses */
	for(i=0;i<5;i++) {
		if(msg[i+2] != 0x00) {
			int already_pressed;

			already_pressed = 0;
			for(j=0;j<5;j++)
				if(old_keys[j] == msg[i+2]) {
					already_pressed = 1;
					break;
				}
			if(!already_pressed) {
				int k;

				k = keyb_translation_table[msg[i+2]];
				if(k != 0xff) {
					write_press_event(e+n, k);
					n++;
				}
			}
		}
	}

	/* handle normal key releases */
	for(i=0;i<5;i++) {
		if(old_keys[i] != 0x00) {
			int still_pressed;

			still_pressed = 0;
			for(j=0;j<5;j++)
				if(msg[j+2] == old_keys[i]) {
					still_pressed = 1;
					break;
				}
			if(!still_pressed) {
				int k;

				k = keyb_translation_table[old_keys[i]];
				if(k != 0xff) {
					write_release_event(e+n, k);
					n++;
				}
			}
		}
	}

	/* update state */
	old_modifiers = msg[0];
	for(i=0;i<5;i++)
		old_keys[i] = msg[i+2];

	return n;
}

static int input_fd;
static int midi_fd;
static int ir_fd;

static rtems_id input_q;

struct input_message {
	int fd;
	int len;
	unsigned char data[8];
};

static rtems_task event_task(rtems_task_argument argument)
{
	struct input_message m;

	m.fd = (int)argument;
	while(1) {
		m.len = read(m.fd, m.data, 8);
		rtems_message_queue_send(input_q, &m, sizeof(struct input_message));
	}
}

static void start_event_task(const char *dev, rtems_name name, int *fd)
{
	rtems_id task_id;
	rtems_status_code sc;

	*fd = open(dev, O_RDONLY);
	assert(*fd != -1);
	sc = rtems_task_create(name, 9, RTEMS_MINIMUM_STACK_SIZE,
		RTEMS_PREEMPT | RTEMS_NO_TIMESLICE | RTEMS_NO_ASR,
		0, &task_id);
	assert(sc == RTEMS_SUCCESSFUL);
	sc = rtems_task_start(task_id, event_task, (rtems_task_argument)(*fd));
	assert(sc == RTEMS_SUCCESSFUL);
}

static input_callback callback;

void init_input(input_callback cb)
{
	rtems_status_code sc;

	sc = rtems_message_queue_create(
		rtems_build_name('I', 'N', 'P', 'T'),
		48,
		sizeof(struct input_message),
		0,
		&input_q);
	assert(sc == RTEMS_SUCCESSFUL);
	start_event_task("/dev/usbinput", rtems_build_name('I', 'N', 'P', 'U'), &input_fd);
	start_event_task("/dev/midi", rtems_build_name('I', 'N', 'P', 'M'), &midi_fd);
	start_event_task("/dev/ir", rtems_build_name('I', 'N', 'P', 'I'), &ir_fd);

	callback = cb;
}

void input_set_callback(input_callback cb)
{
	callback = cb;
}

/* The maximum number of mtk_events that can be generated out of a single message on input_q */
#define WORST_CASE_EVENTS 18

void input_eventloop()
{
	mtk_event e[MAX_EVENTS];
	int total, n;
	rtems_status_code sc;
	struct input_message m;
	size_t s;

	while(1) {
		total = 0;
		while(total < (MAX_EVENTS-WORST_CASE_EVENTS)) {
			sc = rtems_message_queue_receive(
				input_q,
				&m,
				&s,
				RTEMS_WAIT,
				1
			);
			if(sc == RTEMS_TIMEOUT)
				break;
			if(m.fd == input_fd) {
				if(m.len == 4)
					n = handle_mouse_event(&e[total], m.data);
				else
					n = handle_keybd_event(&e[total], m.data);
			} else
				n = 0; /* XXX */
			total += n;
		}
		callback(e, total);
	}
}
