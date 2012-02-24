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
#include <stdio.h>
#include <mtklib.h>

#include "../version.h"
#include "flash.h"
#include "about.h"

static int appid;

static void close_callback(mtk_event *e, void *arg)
{
	mtk_cmd(appid, "w.close()");
}

static void flash_callback(mtk_event *e, void *arg)
{
	open_flash_window(0);
}

#define FLASH_OFFSET_MAC_ADDRESS (0x002200E0)

void init_about(void)
{
	unsigned char *macadr = (unsigned char *)FLASH_OFFSET_MAC_ADDRESS;

	appid = mtk_init_app("About");

	mtk_cmd_seq(appid,
		"g = new Grid()",

		"flickernoise = new Label(-text \"\eFlickernoise "VERSION" (built on "__DATE__")\")",
		"mtk = new Label(-text \"\eGUI toolkit: MTK "MTK_VERSION"\")",
		"rtems = new Label(-text \"\eOS: RTEMS "RTEMS_VERSION"\")",
		0);

	mtk_cmdf(appid, "platform = new Label(-text \"\ePlatform: Milkymist SoC %s\")", soc);
	mtk_cmd(appid, "cpu = new Label(-text \"\eCPU: LatticeMico32\")");
	mtk_cmdf(appid, "board = new Label(-text \"\eBoard: %s (PCB rev. %s)\")", pcb, pcb_rev);
	mtk_cmd(appid, "sep1 = new Separator(-vertical no)");
	mtk_cmdf(appid, "mac = new Label(-text \"\eEthernet MAC: %02x:%02x:%02x:%02x:%02x:%02x\")", macadr[0], macadr[1], macadr[2], macadr[3], macadr[4], macadr[5]);
	mtk_cmd_seq(appid, "sep2 = new Separator(-vertical no)",
		"info = new Label(-text \"Flickernoise is free software, released under GNU GPL version 3.\n"
		"Copyright (C) 2010, 2011, 2012 Flickernoise developers.\n"
		"Milkymist is a trademark of S\xe9""bastien Bourdeauducq.\n"
		"Web: www.milkymist.org\" -font \"monospaced\")",

		"g.place(flickernoise, -column 1 -row 1)",
		"g.place(mtk, -column 1 -row 2)",
		"g.place(rtems, -column 1 -row 3)",
		"g.place(platform, -column 1 -row 4)",
		"g.place(cpu, -column 1 -row 5)",
		"g.place(board, -column 1 -row 6)",
		"g.place(sep1, -column 1 -row 7)",
		"g.place(mac, -column 1 -row 8)",
		"g.place(sep2, -column 1 -row 9)",
		"g.place(info, -column 1 -row 10)",

		"g_btn = new Grid()",

		"b_flash = new Button(-text \"Update\")",
		"b_close = new Button(-text \"Close\")",

		"g_btn.place(b_flash, -column 1 -row 1)",
		"g_btn.place(b_close, -column 2 -row 1)",

		"g.place(g_btn, -column 1 -row 11)",

		"w = new Window(-content g -title \"About\")",
		0);


	mtk_bind(appid, "b_flash", "commit", flash_callback, NULL);
	mtk_bind(appid, "b_close", "commit", close_callback, NULL);

	mtk_bind(appid, "w", "close", close_callback, NULL);
}

void open_about_window(void)
{
	mtk_cmd(appid, "w.open()");
}
