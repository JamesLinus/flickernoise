/*
 * Flickernoise
 * Copyright (C) 2010, 2011 Sebastien Bourdeauducq
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
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <mtklib.h>

#include "util.h"
#include "config.h"
#include "cp.h"
#include "messagebox.h"
#include "filedialog.h"
#include "performance.h"

#include "oscsettings.h"

static int appid;
static struct filedialog *browse_dlg;

static int w_open;

static char osc_bindings[64][384];

static void load_config()
{
	char config_key[7];
	int i;
	const char *val;

	for(i=0;i<64;i++) {
		sprintf(config_key, "osc_%02x", i);
		val = config_read_string(config_key);
		if(val != NULL)
			strcpy(osc_bindings[i], val);
		else
			osc_bindings[i][0] = 0;
	}
}

static void set_config()
{
	char config_key[7];
	int i;

	for(i=0;i<64;i++) {
		sprintf(config_key, "osc_%02x", i);
		if(osc_bindings[i][0] != 0)
			config_write_string(config_key, osc_bindings[i]);
		else
			config_delete(config_key);
	}
}

static void update_list()
{
	char str[32768];
	char *p;
	int i;

	str[0] = 0;
	p = str;
	for(i=0;i<64;i++) {
		if(osc_bindings[i][0] != 0)
			p += sprintf(p, "%d: %s\n", i, osc_bindings[i]);
	}
	/* remove last \n */
	if(p != str) {
		p--;
		*p = 0;
	}
	mtk_cmdf(appid, "lst_existing.set(-text \"%s\" -selection 0)", str);
}

static void selchange_callback(mtk_event *e, void *arg)
{
	int sel;
	int count;
	int i;

	sel = mtk_req_i(appid, "lst_existing.selection");
	count = 0;
	for(i=0;i<64;i++) {
		if(osc_bindings[i][0] != 0) {
			if(count == sel) {
				count++;
				break;
			}
			count++;
		}
	}
	if(count != 0) {
		mtk_cmdf(appid, "e_number.set(-text \"%d\")", i);
		mtk_cmdf(appid, "e_filename.set(-text \"%s\")", osc_bindings[i]);
	}
}

static void close_window()
{
	mtk_cmd(appid, "w.close()");
	w_open = 0;
	close_filedialog(browse_dlg);
}

static void ok_callback(mtk_event *e, void *arg)
{
	set_config();
	cp_notify_changed();
	close_window();
}

static void cancel_callback(mtk_event *e, void *arg)
{
	close_window();
}

static void browse_ok_callback(void *arg)
{
	char filename[384];

	get_filedialog_selection(browse_dlg, filename, sizeof(filename));
	mtk_cmdf(appid, "e_filename.set(-text \"%s\")", filename);
}

static void browse_callback(mtk_event *e, void *arg)
{
	open_filedialog(browse_dlg);
}

static void clear_callback(mtk_event *e, void *arg)
{
	mtk_cmd(appid, "e_filename.set(-text \"\")");
}

static void addupdate_callback(mtk_event *e, void *arg)
{
	char key[8];
	int index;
	char *c;
	char filename[384];

	mtk_req(appid, key, sizeof(key), "e_number.text");
	mtk_req(appid, filename, sizeof(filename), "e_filename.text");
	index = strtol(key, &c, 0);
	if((*c != 0x00) || (index < 0) || (index > 63)) {
		messagebox("Error", "Invalid number.\nUse a decimal or hexadecimal (0x...) number between 0 and 63.");
		return;
	}
	strcpy(osc_bindings[index], filename);
	update_list();
	mtk_cmd(appid, "e_number.set(-text \"\")");
	mtk_cmd(appid, "e_filename.set(-text \"\")");
}

static int cmpstringp(const void *p1, const void *p2)
{
	return strcmp(*(char * const *)p1, *(char * const *)p2);
}

static void autobuild(int si, char *folder)
{
	DIR *d;
	struct dirent *entry;
	struct stat s;
	char fullname[384];
	char *c;
	char *files[384];
	int n_files;
	int max_files = 64 - si;
	int i;

	d = opendir(folder);
	if(!d) {
		messagebox("Auto build failed", "Unable to open directory");
		return;
	}
	n_files = 0;
	while((entry = readdir(d))) {
		if(entry->d_name[0] == '.') continue;
		snprintf(fullname, sizeof(fullname), "%s/%s", folder, entry->d_name);
		lstat(fullname, &s);
		if(!S_ISDIR(s.st_mode)) {
			c = strrchr(entry->d_name, '.');
			if((c != NULL) && (strcmp(c, ".fnp") == 0)) {
				if(n_files < 384) {
					files[n_files] = strdup(entry->d_name);
					n_files++;
				}
			}
		}
	}
	closedir(d);

	qsort(files, n_files, sizeof(char *), cmpstringp);

	for(i=0;i<n_files;i++) {
		if(i < max_files)
			sprintf(osc_bindings[si+i], "%s/%s", folder, files[i]);
		free(files[i]);
	}
}

static void autobuild_callback(mtk_event *e, void *arg)
{
	char key[8];
	int sindex;
	char *c;
	char filename[384];
	int i;

	mtk_req(appid, key, sizeof(key), "e_number.text");
	mtk_req(appid, filename, sizeof(filename), "e_filename.text");
	sindex = strtol(key, &c, 0);
	if((*c != 0x00) || (sindex < 0) || (sindex > 63)) {
		messagebox("Error", "Invalid start number.\nUse a decimal or hexadecimal (0x...) number between 0 and 63.");
		return;
	}

	if(filename[0] == 0x00)
		strcpy(filename, SIMPLE_PATCHES_FOLDER);

	i = strlen(filename);
	if(filename[i-1] == '/')
		filename[i-1] = 0x00;

	autobuild(sindex, filename);
	update_list();
}

void init_oscsettings()
{
	appid = mtk_init_app("OSC");

	mtk_cmd_seq(appid,
		"g = new Grid()",

		"g_existing0 = new Grid()",
		"l_existing = new Label(-text \"Existing bindings\" -font \"title\")",
		"s_existing1 = new Separator(-vertical no)",
		"s_existing2 = new Separator(-vertical no)",
		"g_existing0.place(s_existing1, -column 1 -row 1)",
		"g_existing0.place(l_existing, -column 2 -row 1)",
		"g_existing0.place(s_existing2, -column 3 -row 1)",
		"lst_existing = new List()",
		"lst_existingf = new Frame(-content lst_existing -scrollx no -scrolly yes)",

		"g_addedit0 = new Grid()",
		"l_addedit = new Label(-text \"Add/edit\" -font \"title\")",
		"s_addedit1 = new Separator(-vertical no)",
		"s_addedit2 = new Separator(-vertical no)",
		"g_addedit0.place(s_addedit1, -column 1 -row 1)",
		"g_addedit0.place(l_addedit, -column 2 -row 1)",
		"g_addedit0.place(s_addedit2, -column 3 -row 1)",

		"g_addedit1 = new Grid()",
		"l_number = new Label(-text \"Number:\")",
		"e_number = new Entry()",
		"g_addedit1.place(l_number, -column 1 -row 1)",
		"g_addedit1.place(e_number, -column 2 -row 1)",
		"l_filename = new Label(-text \"Filename:\")",
		"e_filename = new Entry()",
		"b_filename = new Button(-text \"Browse\")",
		"b_filenameclear = new Button(-text \"Clear\")",
		"g_addedit1.place(l_filename, -column 1 -row 2)",
		"g_addedit1.place(e_filename, -column 2 -row 2)",
		"g_addedit1.place(b_filename, -column 3 -row 2)",
		"g_addedit1.place(b_filenameclear, -column 4 -row 2)",
		"g_addedit1.columnconfig(2, -size 200)",
		"g_addedit1.columnconfig(3, -size 0)",
		"g_addedit1.columnconfig(4, -size 0)",
		"b_addupdate = new Button(-text \"Add/update\")",
		"b_autobuild = new Button(-text \"Auto build\")",
		"g_btn = new Grid()",
		"b_ok = new Button(-text \"OK\")",
		"b_cancel = new Button(-text \"Cancel\")",
		"g_btn.columnconfig(1, -size 200)",
		"g_btn.place(b_ok, -column 2 -row 1)",
		"g_btn.place(b_cancel, -column 3 -row 1)",

		"g.place(g_existing0, -column 1 -row 1)",
		"g.place(lst_existingf, -column 1 -row 2)",
		"g.rowconfig(2, -size 130)",
		"g.place(g_addedit0, -column 1 -row 3)",
		"g.place(g_addedit1, -column 1 -row 4)",
		"g.place(b_addupdate, -column 1 -row 5)",
		"g.place(b_autobuild, -column 1 -row 6)",
		"g.rowconfig(7, -size 10)",
		"g.place(g_btn, -column 1 -row 8)",

		"w = new Window(-content g -title \"OSC settings\")",
		0);

	mtk_bind(appid, "lst_existing", "selchange", selchange_callback, NULL);
	mtk_bind(appid, "lst_existing", "selcommit", selchange_callback, NULL);
	mtk_bind(appid, "b_filename", "commit", browse_callback, NULL);
	mtk_bind(appid, "b_filenameclear", "commit", clear_callback, NULL);
	mtk_bind(appid, "b_addupdate", "commit", addupdate_callback, NULL);
	mtk_bind(appid, "b_autobuild", "commit", autobuild_callback, NULL);
	mtk_bind(appid, "b_ok", "commit", ok_callback, NULL);
	mtk_bind(appid, "b_cancel", "commit", cancel_callback, NULL);

	mtk_bind(appid, "w", "close", cancel_callback, NULL);

	browse_dlg = create_filedialog("OSC patch select", 0, "fnp", browse_ok_callback, NULL, NULL, NULL);
}

void open_oscsettings_window()
{
	if(w_open) return;
	w_open = 1;
	load_config();
	update_list();
	mtk_cmd(appid, "w.open()");
}
