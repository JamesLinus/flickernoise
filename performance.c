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

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <rtems.h>
#include <mtklib.h>

#include "input.h"
#include "messagebox.h"
#include "config.h"
#include "compiler.h"
#include "guirender.h"
#include "performance.h"

#define FILENAME_LEN 384

#define MAX_PATCHES 64

struct patch_info {
	char filename[FILENAME_LEN];
	struct patch *p;
};

static int npatches;
static struct patch_info patches[MAX_PATCHES];

static int add_patch(const char *filename)
{
	int i;

	for(i=0;i<npatches;i++) {
		if(strcmp(patches[i].filename, filename) == 0)
			return i;
	}
	if(npatches == MAX_PATCHES) return -1;
	strcpy(patches[npatches].filename, filename);
	patches[npatches].p = NULL;
	return npatches++;
}

static int firstpatch;
static int keyboard_patches[26];

static void add_firstpatch()
{
	const char *filename;

	firstpatch = -1;
	filename = config_read_string("firstpatch");
	if(filename == NULL) return;
	firstpatch = add_patch(filename);
}

static void add_keyboard_patches()
{
	int i;
	char config_key[6];
	const char *filename;

	strcpy(config_key, "key_");
	config_key[5] = 0;
	for(i=0;i<26;i++) {
		config_key[4] = 'a' + i;
		filename = config_read_string(config_key);
		if(filename != NULL)
			keyboard_patches[i] = add_patch(filename);
		else
			keyboard_patches[i] = -1;
	}
}

static int appid;
static int started;

static void close_callback(mtk_event *e, void *arg)
{
	mtk_cmd(appid, "w.close()");
}

void init_performance()
{
	appid = mtk_init_app("Performance");

	mtk_cmd_seq(appid,
		"g = new Grid()",

		"l_text = new Label()",
		"progress = new LoadDisplay()",
		"b_close = new Button(-text \"Close\")",

		"g.place(l_text, -column 1 -row 1)",
		"g.place(progress, -column 1 -row 2)",
		"g.place(b_close, -column 1 -row 3)",
		"g.columnconfig(1, -size 150)",

		"w = new Window(-content g -title \"Performance\")",
		0);

	mtk_bind(appid, "b_close", "commit", close_callback, NULL);
	mtk_bind(appid, "w", "close", close_callback, NULL);
}

static int compiled_patches;
#define UPDATE_PERIOD 20
static rtems_interval next_update;

static void dummy_rmc(const char *msg)
{
}

static struct patch *compile_patch(const char *filename)
{
	FILE *fd;
	int r;
	char buf[32768];

	fd = fopen(filename, "r");
	if(fd == NULL) return NULL;
	r = fread(buf, 1, 32767, fd);
	if(r <= 0) {
		fclose(fd);
		return NULL;
	}
	buf[r] = 0;
	fclose(fd);

	return patch_compile(buf, dummy_rmc);
}

static rtems_task comp_task(rtems_task_argument argument)
{
	for(;compiled_patches<npatches;compiled_patches++) {
		patches[compiled_patches].p = compile_patch(patches[compiled_patches].filename);
		if(patches[compiled_patches].p == NULL) {
			compiled_patches = -compiled_patches-1;
			break;
		}
	}
	rtems_task_delete(RTEMS_SELF);
}

static void free_patches()
{
	int i;

	for(i=0;i<npatches;i++) {
		if(patches[i].p != NULL)
			patch_free(patches[i].p);
	}
}

static void event_callback(mtk_event *e, int count)
{
}

static void stop_callback()
{
	free_patches();
	started = 0;
	input_delete_callback(event_callback);
}

static void refresh_callback(mtk_event *e, int count)
{
	rtems_interval t;
	
	t = rtems_clock_get_ticks_since_boot();
	if(t >= next_update) {
		if(compiled_patches >= 0) {
			mtk_cmdf(appid, "progress.barconfig(load, -value %d)", (100*compiled_patches)/npatches);
			if(compiled_patches == npatches) {
				/* All patches compiled. Start rendering. */
				input_delete_callback(refresh_callback);
				input_add_callback(event_callback);
				mtk_cmd(appid, "l_text.set(-text \"Done.\")");
				guirender(appid, patches[firstpatch].p, stop_callback);
				return;
			}
		} else {
			int error_patch;

			error_patch = -compiled_patches-1;
			mtk_cmdf(appid, "l_text.set(-text \"Failed to compile patch %s\")", patches[error_patch].filename);
			input_delete_callback(refresh_callback);
			started = 0;
			free_patches();
			return;
		}
		next_update = t + UPDATE_PERIOD;
	}
}

static rtems_id comp_task_id;

void start_performance()
{
	rtems_status_code sc;
	
	if(started) return;
	started = 1;

	/* build patch list */
	npatches = 0;
	add_firstpatch();
	if(npatches < 1) {
		messagebox("Error", "No first patch defined!\n");
		started = 0;
		return;
	}
	add_keyboard_patches();

	/* start patch compilation task */
	compiled_patches = 0;
	mtk_cmd(appid, "l_text.set(-text \"Compiling patches...\")");
	mtk_cmd(appid, "progress.barconfig(load, -value 0)");
	mtk_cmd(appid, "w.open()");
	next_update = rtems_clock_get_ticks_since_boot() + UPDATE_PERIOD;
	input_add_callback(refresh_callback);
	sc = rtems_task_create(rtems_build_name('C', 'O', 'M', 'P'), 20, 64*1024,
		RTEMS_PREEMPT | RTEMS_NO_TIMESLICE | RTEMS_NO_ASR,
		0, &comp_task_id);
	assert(sc == RTEMS_SUCCESSFUL);
	sc = rtems_task_start(comp_task_id, comp_task, 0);
	assert(sc == RTEMS_SUCCESSFUL);
}
