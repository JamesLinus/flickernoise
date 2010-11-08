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

#ifndef __FRAMEDESCRIPTOR_H
#define __FRAMEDESCRIPTOR_H

#include <rtems.h>
#include <bsp/milkymist_ac97.h>

#define FPS			24
#define FRD_COUNT 		(4)
#define FRD_AUDIO_NSAMPLES	(48000/FPS)

enum {
	FRD_STATUS_NEW = 0,
	FRD_STATUS_SAMPLING,
	FRD_STATUS_SAMPLED,
	FRD_STATUS_EVALUATED,
	FRD_STATUS_USED
};

struct frame_descriptor {
	int status;

	float time;
	struct snd_buffer *snd_buf;
	float bass, mid, treb;
	float bass_att, mid_att, treb_att;
	float idmx1, idmx2, idmx3, idmx4;

	float decay;
	float wave_mode;
	float wave_scale;
	float wave_additive;
	float wave_usedots;
	float wave_brighten;
	float wave_thick;
	float wave_x, wave_y;
	float wave_r, wave_g, wave_b, wave_a;
	float ob_size;
	float ob_r, ob_g, ob_b, ob_a;
	float ib_size;
	float ib_r, ib_g, ib_b, ib_a;
	float mv_x, mv_y;
	float mv_dx, mv_dy;
	float mv_l;
	float mv_r, mv_g, mv_b, mv_a;
	float tex_wrap;
	float vecho_alpha;
	float vecho_zoom;
	float vecho_orientation;
	float dmx1, dmx2, dmx3, dmx4;
	struct tmu_vertex *vertices;
};

typedef void (*frd_callback)(struct frame_descriptor *);

struct frame_descriptor *new_frame_descriptor();
void free_frame_descriptor(struct frame_descriptor *frd);

#endif /* __FRAMEDESCRIPTOR_H */
