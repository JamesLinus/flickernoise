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

#include <bsp.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <dopelib.h>
#include <vscreen.h>
#include <bsp/milkymist_ac97.h>

#include "util.h"
#include "version.h"
#include "flash.h"
#include "about.h"

static long appid;

static int mixer_fd;

static int prev_line_vol;
static int prev_line_mute;
static int prev_mic_vol;
static int prev_mic_mute;

static int line_vol;
static int line_mute;
static int mic_vol;
static int mic_mute;

static void set_level(int channel, unsigned int val)
{
	int request;

	val = val | (val << 8);
	request = channel ? SOUND_MIXER_WRITE(SOUND_MIXER_MIC) : SOUND_MIXER_WRITE(SOUND_MIXER_LINE);
	ioctl(mixer_fd, request, &val);
}

static void load_levels()
{
	if(line_mute)
		set_level(0, 0);
	else
		set_level(0, line_vol);
	if(mic_mute)
		set_level(1, 0);
	else
		set_level(1, mic_vol);
}

static void slide_callback(dope_event *e, void *arg)
{
	unsigned int channel = (unsigned int)arg;
	unsigned int val;

	val = 100 - dope_req_l(appid, channel ? "s_micvol.value" : "s_linevol.value");
	if(channel)
		mic_vol = val;
	else
		line_vol = val;
	if(channel ? !mic_mute : !line_mute)
		set_level(channel, val);
}

static void mute_callback(dope_event *e, void *arg)
{
	unsigned int channel = (unsigned int)arg;

	if(channel) {
		mic_mute = !mic_mute;
		if(mic_mute)
			set_level(1, 0);
		else
			set_level(1, mic_vol);
		dope_cmdf(appid, "b_mutmic.set(-state %s)", mic_mute ? "on" : "off");
	} else {
		line_mute = !line_mute;
		if(line_mute)
			set_level(0, 0);
		else
			set_level(0, line_vol);
		dope_cmdf(appid, "b_mutline.set(-state %s)", line_mute ? "on" : "off");
	}
}

/* TODO: spawn a task to avoid freezing GUI */
#define TEST_NSAMPLES (0x3fffc/4)
#define TEST_NBUFFERS 4
static void test_callback(dope_event *e, void *arg)
{
	int fd;
	int i;
	struct snd_buffer *snd_buffers[TEST_NBUFFERS];
	struct snd_buffer *ret_buffers[TEST_NBUFFERS];

	fd = open("/dev/snd", O_RDWR);
	if(fd == -1) {
		perror("Unable to open audio device");
		return;
	}

	for(i=0;i<TEST_NBUFFERS;i++) {
		snd_buffers[i] = malloc(TEST_NSAMPLES*4+sizeof(struct snd_buffer));
		if(snd_buffers[i] == NULL) {
			perror("malloc");
			return;
		}
		snd_buffers[i]->nsamples = TEST_NSAMPLES;
	}

	for(i=0;i<TEST_NBUFFERS;i++)
		ioctl(fd, SOUND_SND_SUBMIT_RECORD, snd_buffers[i]);

	for(i=0;i<TEST_NBUFFERS;i++)
		ioctl(fd, SOUND_SND_COLLECT_RECORD, &ret_buffers[i]);

	for(i=0;i<TEST_NBUFFERS;i++)
		ioctl(fd, SOUND_SND_SUBMIT_PLAY, ret_buffers[i]);

	for(i=0;i<TEST_NBUFFERS;i++)
		ioctl(fd, SOUND_SND_COLLECT_PLAY, &ret_buffers[i]);

	for(i=0;i<TEST_NBUFFERS;i++)
		free(snd_buffers[i]);

	close(fd);
}

static void ok_callback(dope_event *e, void *arg)
{
	dope_cmd(appid, "w.close()");
}

static void close_callback(dope_event *e, void *arg)
{
	dope_cmd(appid, "w.close()");

	line_vol = prev_line_vol;
	line_mute = prev_line_mute;
	mic_vol = prev_mic_vol;
	mic_mute = prev_mic_mute;

	load_levels();
	dope_cmdf(appid, "s_linevol.set(-value %d)", 100-line_vol);
	dope_cmdf(appid, "b_mutline.set(-state %s)", line_mute ? "on" : "off");
	dope_cmdf(appid, "s_micvol.set(-value %d)", 100-mic_vol);
	dope_cmdf(appid, "b_mutmic.set(-state %s)", mic_mute ? "on" : "off");
}

void init_audio()
{
	appid = dope_init_app("Audio");

	dope_cmd_seq(appid,
		"g = new Grid()",

		"gv = new Grid()",
		"l_linevol = new Label(-text \"Line volume\")",
		"s_linevol = new Scale(-from 0 -to 100 -value 0 -orient vertical)",
		"l_micvol = new Label(-text \"Mic volume\")",
		"s_micvol = new Scale(-from 0 -to 100 -value 0 -orient vertical)",

		"b_mutline = new Button(-text \"Mute\")",
		"b_mutmic = new Button(-text \"Mute\")",

		"gv.place(l_linevol, -column 1 -row 1)",
		"gv.place(s_linevol, -column 1 -row 2)",
		"gv.place(b_mutline, -column 1 -row 3)",
		"gv.place(l_micvol, -column 2 -row 1)",
		"gv.place(s_micvol, -column 2 -row 2)",
		"gv.place(b_mutmic, -column 2 -row 3)",

		"gv.rowconfig(2, -size 150)",

		"g.place(gv, -column 1 -row 1)",

		"g_btn = new Grid()",

		"b_test = new Button(-text \"Audio test\")",
		"sep = new Separator(-vertical yes)",
		"b_ok = new Button(-text \"OK\")",
		"b_cancel = new Button(-text \"Cancel\")",

		"g_btn.place(b_test, -column 2 -row 1)",
		"g_btn.place(sep, -column 3 -row 1)",
		"g_btn.place(b_ok, -column 4 -row 1)",
		"g_btn.place(b_cancel, -column 5 -row 1)",

		"g_btn.columnconfig(1, -size 90)",

		"g.place(g_btn, -column 1 -row 2)",

		"w = new Window(-content g -title \"Audio settings\")",
		0);

	dope_bind(appid, "s_linevol", "change", slide_callback, (void *)0);
	dope_bind(appid, "s_micvol", "change", slide_callback, (void *)1);

	dope_bind(appid, "b_mutline", "press", mute_callback, (void *)0);
	dope_bind(appid, "b_mutmic", "press", mute_callback, (void *)1);

	dope_bind(appid, "b_test", "commit", test_callback, NULL);
	dope_bind(appid, "b_ok", "commit", ok_callback, NULL);
	dope_bind(appid, "b_cancel", "commit", close_callback, NULL);

	dope_bind(appid, "w", "close", close_callback, NULL);

	mixer_fd = open("/dev/mixer", O_RDWR);
	if(mixer_fd == -1)
		perror("Unable to open audio mixer");

	line_vol = 100;
	line_mute = 0;
	mic_vol = 100;
	mic_mute = 0;
	load_levels();
}

void open_audio_window()
{
	prev_line_vol = line_vol;
	prev_line_mute = line_mute;
	prev_mic_vol = mic_vol;
	prev_mic_mute = mic_mute;
	dope_cmd(appid, "w.open()");
}
