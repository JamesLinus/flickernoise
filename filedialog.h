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

#ifndef __FILEDIALOG_H
#define __FILEDIALOG_H

int create_filedialog(const char *name, int is_save, void (*ok_callback)(mtk_event *,void *), void *ok_callback_arg, void (*cancel_callback)(mtk_event *,void *), void *cancel_callback_arg);
void open_filedialog(int appid, const char *folder);
void close_filedialog(int appid);
void get_filedialog_selection(int appid, char *buffer, int buflen);

#endif /* __FILEDIALOG_H */
