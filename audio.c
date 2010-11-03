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
#include <mtklib.h>
#include <bsp/milkymist_ac97.h>

#include "config.h"
#include "cp.h"
#include "util.h"
#include "version.h"
#include "flash.h"
#include "about.h"

static int appid;

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

static void slide_callback(mtk_event *e, void *arg)
{
	unsigned int channel = (unsigned int)arg;
	unsigned int val;

	val = 100 - mtk_req_l(appid, channel ? "s_micvol.value" : "s_linevol.value");
	if(channel)
		mic_vol = val;
	else
		line_vol = val;
	if(channel ? !mic_mute : !line_mute)
		set_level(channel, val);
}

static void mute_callback(mtk_event *e, void *arg)
{
	unsigned int channel = (unsigned int)arg;

	if(channel) {
		mic_mute = !mic_mute;
		if(mic_mute)
			set_level(1, 0);
		else
			set_level(1, mic_vol);
		mtk_cmdf(appid, "b_mutmic.set(-state %s)", mic_mute ? "on" : "off");
	} else {
		line_mute = !line_mute;
		if(line_mute)
			set_level(0, 0);
		else
			set_level(0, line_vol);
		mtk_cmdf(appid, "b_mutline.set(-state %s)", line_mute ? "on" : "off");
	}
}

static void load_config()
{
	line_vol = config_read_int("line_vol", 100);
	line_mute = config_read_int("line_mute", 0);
	mic_vol = config_read_int("mic_vol", 100);
	mic_mute = config_read_int("mic_mute", 0);
}

static void set_config()
{
	config_write_int("line_vol", line_vol);
	config_write_int("line_mute", line_mute);
	config_write_int("mic_vol", mic_vol);
	config_write_int("mic_mute", mic_mute);
	cp_notify_changed();
}

static int w_open;

static void ok_callback(mtk_event *e, void *arg)
{
	w_open = 0;
	mtk_cmd(appid, "w.close()");
	set_config();
}

static void close_callback(mtk_event *e, void *arg)
{
	w_open = 0;

	mtk_cmd(appid, "w.close()");

	line_vol = prev_line_vol;
	line_mute = prev_line_mute;
	mic_vol = prev_mic_vol;
	mic_mute = prev_mic_mute;

	load_levels();
	mtk_cmdf(appid, "s_linevol.set(-value %d)", 100-line_vol);
	mtk_cmdf(appid, "b_mutline.set(-state %s)", line_mute ? "on" : "off");
	mtk_cmdf(appid, "s_micvol.set(-value %d)", 100-mic_vol);
	mtk_cmdf(appid, "b_mutmic.set(-state %s)", mic_mute ? "on" : "off");
}

void init_audio()
{
	appid = mtk_init_app("Audio");

	mtk_cmd_seq(appid,
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

		"b_ok = new Button(-text \"OK\")",
		"b_cancel = new Button(-text \"Cancel\")",

		"g_btn.columnconfig(1, -size 90)",
		"g_btn.place(b_ok, -column 2 -row 1)",
		"g_btn.place(b_cancel, -column 3 -row 1)",

		"g.place(g_btn, -column 1 -row 2)",

		"w = new Window(-content g -title \"Audio settings\")",
		0);

	mtk_bind(appid, "s_linevol", "change", slide_callback, (void *)0);
	mtk_bind(appid, "s_micvol", "change", slide_callback, (void *)1);

	mtk_bind(appid, "b_mutline", "press", mute_callback, (void *)0);
	mtk_bind(appid, "b_mutmic", "press", mute_callback, (void *)1);

	mtk_bind(appid, "b_ok", "commit", ok_callback, NULL);
	mtk_bind(appid, "b_cancel", "commit", close_callback, NULL);

	mtk_bind(appid, "w", "close", close_callback, NULL);

	mixer_fd = open("/dev/mixer", O_RDWR);
	if(mixer_fd == -1)
		perror("Unable to open audio mixer");

	load_config();
	load_levels();
}

void open_audio_window()
{
	if(w_open) return;
	w_open = 1;
	prev_line_vol = line_vol;
	prev_line_mute = line_mute;
	prev_mic_vol = mic_vol;
	prev_mic_mute = mic_mute;
	mtk_cmd(appid, "w.open()");
}
