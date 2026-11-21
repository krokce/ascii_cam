/* KMS display output for the DPI port (MDA monitor) and HDMI, written
 * against the kernel's own interface headers so that no library beyond
 * the C library is needed. See kms.h for the overall design. */
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <drm/drm.h>
#include <drm/drm_mode.h>
#include <drm/drm_fourcc.h>
#include "kms.h"

#define PTR(p) ((uint64_t)(uintptr_t)(p))

/* Values of the plane "type" property, fixed by the kernel interface. */
#define PLANE_TYPE_OVERLAY 0
#define PLANE_TYPE_PRIMARY 1

static int xioctl(int fd, unsigned long req, void *arg)
{
	int r;
	do {
		r = ioctl(fd, req, arg);
	} while (r == -1 && (errno == EINTR || errno == EAGAIN));
	return r;
}

/* ---- properties ---------------------------------------------------- */

static uint32_t prop_id(int fd, uint32_t obj, uint32_t type, const char *name, uint64_t *enum_out, const char *enum_name)
{
	struct drm_mode_obj_get_properties op;
	uint32_t ids[64];
	uint64_t vals[64];
	uint32_t i;

	memset(&op, 0, sizeof(op));
	op.obj_id = obj;
	op.obj_type = type;
	op.props_ptr = PTR(ids);
	op.prop_values_ptr = PTR(vals);
	op.count_props = 64;
	if (xioctl(fd, DRM_IOCTL_MODE_OBJ_GETPROPERTIES, &op) < 0)
		return 0;
	for (i = 0; i < op.count_props && i < 64; i++) {
		struct drm_mode_get_property p;
		struct drm_mode_property_enum en[16];

		memset(&p, 0, sizeof(p));
		p.prop_id = ids[i];
		if (xioctl(fd, DRM_IOCTL_MODE_GETPROPERTY, &p) < 0)
			continue;
		if (strcmp(p.name, name))
			continue;
		if (enum_out && enum_name && p.count_enum_blobs > 0 && p.count_enum_blobs <= 16) {
			uint32_t j, n = p.count_enum_blobs;
			memset(&p, 0, sizeof(p));
			p.prop_id = ids[i];
			p.count_enum_blobs = n;
			p.enum_blob_ptr = PTR(en);
			if (xioctl(fd, DRM_IOCTL_MODE_GETPROPERTY, &p) == 0)
				for (j = 0; j < n; j++)
					if (!strcmp(en[j].name, enum_name))
						*enum_out = en[j].value;
		}
		return ids[i];
	}
	return 0;
}

/* ---- atomic request builder ------------------------------------------ */

struct req {
	uint32_t objs[64], counts[64], props[512];
	uint64_t vals[512];
	int nobj, nprop;
};

static void add(struct req *r, uint32_t obj, uint32_t prop, uint64_t val)
{
	if (!prop || r->nprop >= 512)
		return;
	if (r->nobj == 0 || r->objs[r->nobj - 1] != obj) {
		if (r->nobj >= 64)
			return;
		r->objs[r->nobj] = obj;
		r->counts[r->nobj] = 0;
		r->nobj++;
	}
	r->props[r->nprop] = prop;
	r->vals[r->nprop] = val;
	r->nprop++;
	r->counts[r->nobj - 1]++;
}

static void add_named(int fd, struct req *r, uint32_t obj, uint32_t type, const char *name, uint64_t val)
{
	add(r, obj, prop_id(fd, obj, type, name, NULL, NULL), val);
}

static int commit(int fd, struct req *r, uint32_t flags)
{
	struct drm_mode_atomic a;

	memset(&a, 0, sizeof(a));
	a.flags = flags;
	a.count_objs = (uint32_t)r->nobj;
	a.objs_ptr = PTR(r->objs);
	a.count_props_ptr = PTR(r->counts);
	a.props_ptr = PTR(r->props);
	a.prop_values_ptr = PTR(r->vals);
	return xioctl(fd, DRM_IOCTL_MODE_ATOMIC, &a);
}

/* ---- buffers --------------------------------------------------------- */

static int buf_create(int fd, struct kms_buf *b, int w, int h)
{
	struct drm_mode_create_dumb cd;
	struct drm_mode_map_dumb md;
	struct drm_mode_fb_cmd2 fb;
	void *map;

	memset(&cd, 0, sizeof(cd));
	cd.width = (uint32_t)w;
	cd.height = (uint32_t)h;
	cd.bpp = 32;
	if (xioctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &cd) < 0)
		return -1;
	memset(&fb, 0, sizeof(fb));
	fb.width = (uint32_t)w;
	fb.height = (uint32_t)h;
	fb.pixel_format = DRM_FORMAT_ARGB8888;
	fb.handles[0] = cd.handle;
	fb.pitches[0] = cd.pitch;
	if (xioctl(fd, DRM_IOCTL_MODE_ADDFB2, &fb) < 0)
		return -1;
	memset(&md, 0, sizeof(md));
	md.handle = cd.handle;
	if (xioctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &md) < 0)
		return -1;
	map = mmap(NULL, cd.size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, (off_t)md.offset);
	if (map == MAP_FAILED)
		return -1;
	memset(map, 0, cd.size);
	b->fb_id = fb.fb_id;
	b->handle = cd.handle;
	b->pitch = cd.pitch;
	b->size = cd.size;
	b->map = map;
	b->w = w;
	b->h = h;
	return 0;
}

static void buf_destroy(int fd, struct kms_buf *b)
{
	struct drm_mode_destroy_dumb dd;

	if (!b->map)
		return;
	munmap(b->map, b->size);
	xioctl(fd, DRM_IOCTL_MODE_RMFB, &b->fb_id);
	memset(&dd, 0, sizeof(dd));
	dd.handle = b->handle;
	xioctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &dd);
	memset(b, 0, sizeof(*b));
}

/* A YUV 4:2:0 buffer: the brightness plane followed by the two colour
 * planes at half size. The display hardware converts it to RGB. */
static int buf_create_yuv(int fd, struct kms_buf *b, int w, int h)
{
	struct drm_mode_create_dumb cd;
	struct drm_mode_map_dumb md;
	struct drm_mode_fb_cmd2 fb;
	void *map;

	memset(&cd, 0, sizeof(cd));
	cd.width = (uint32_t)w;
	cd.height = (uint32_t)(h * 3 / 2);
	cd.bpp = 8;
	if (xioctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &cd) < 0)
		return -1;
	memset(&fb, 0, sizeof(fb));
	fb.width = (uint32_t)w;
	fb.height = (uint32_t)h;
	fb.pixel_format = DRM_FORMAT_YUV420;
	fb.handles[0] = fb.handles[1] = fb.handles[2] = cd.handle;
	fb.pitches[0] = cd.pitch;
	fb.pitches[1] = fb.pitches[2] = cd.pitch / 2;
	fb.offsets[0] = 0;
	fb.offsets[1] = cd.pitch * (uint32_t)h;
	fb.offsets[2] = fb.offsets[1] + cd.pitch / 2 * (uint32_t)(h / 2);
	if (xioctl(fd, DRM_IOCTL_MODE_ADDFB2, &fb) < 0)
		return -1;
	memset(&md, 0, sizeof(md));
	md.handle = cd.handle;
	if (xioctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &md) < 0)
		return -1;
	map = mmap(NULL, cd.size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, (off_t)md.offset);
	if (map == MAP_FAILED)
		return -1;
	memset(map, 0, cd.size);
	b->fb_id = fb.fb_id;
	b->handle = cd.handle;
	b->pitch = cd.pitch;
	b->size = cd.size;
	b->map = map;
	b->w = w;
	b->h = h;
	return 0;
}

static struct canvas canvas_of(struct kms_buf *b)
{
	struct canvas c = { b->map, b->w, b->h, (int)(b->pitch / 4) };
	return c;
}

/* ---- connectors and modes -------------------------------------------- */

/* Reads a connector. With probe set, the kernel checks the cable again
 * (for HDMI this reads the screen's EDID). Picks the mode to use. */
static int read_connector(int fd, struct kms_out *o, int probe, int is_hdmi)
{
	struct drm_mode_get_connector c;
	struct drm_mode_modeinfo modes[64];
	uint32_t encs[8];
	uint32_t n, i;
	int best = -1;

	memset(&c, 0, sizeof(c));
	c.connector_id = o->conn_id;
	if (probe && xioctl(fd, DRM_IOCTL_MODE_GETCONNECTOR, &c) < 0)
		return -1;
	n = c.count_modes;
	if (!probe)
		n = 64;
	memset(&c, 0, sizeof(c));
	c.connector_id = o->conn_id;
	c.count_modes = n > 64 ? 64 : n;
	c.modes_ptr = PTR(modes);
	c.count_encoders = 8;
	c.encoders_ptr = PTR(encs);
	if (n == 0) {
		/* count_modes must not be 0 here, or the kernel probes again. */
		static struct drm_mode_modeinfo dummy;
		c.count_modes = 1;
		c.modes_ptr = PTR(&dummy);
	}
	if (xioctl(fd, DRM_IOCTL_MODE_GETCONNECTOR, &c) < 0)
		return -1;
	o->connected = c.connection == 1 && n > 0;
	if (!o->connected)
		return 0;
	if (c.count_modes > 64)
		c.count_modes = 64;
	for (i = 0; i < c.count_modes; i++) {
		if (!is_hdmi) {
			best = 0;   /* the DPI panel has exactly one mode: ours */
			break;
		}
		if (modes[i].flags & DRM_MODE_FLAG_INTERLACE)
			continue;
		if (modes[i].hdisplay == 1280 && modes[i].vdisplay == 720 &&
		    modes[i].vrefresh >= 50 && modes[i].vrefresh <= 60) {
			best = (int)i;
			break;
		}
		if (best < 0 && (modes[i].type & DRM_MODE_TYPE_PREFERRED) &&
		    modes[i].hdisplay <= 1920 && modes[i].vdisplay <= 1200)
			best = (int)i;
	}
	if (best < 0 && c.count_modes > 0)
		best = 0;
	if (best < 0) {
		o->connected = 0;
		return 0;
	}
	memcpy(o->mode, &modes[best], sizeof(modes[best]));
	o->mode_w = modes[best].hdisplay;
	o->mode_h = modes[best].vdisplay;
	return 0;
}

static int find_device(void)
{
	char path[32];
	int i;

	for (i = 0; i < 4; i++) {
		struct drm_mode_card_res res;
		int fd;

		snprintf(path, sizeof(path), "/dev/dri/card%d", i);
		fd = open(path, O_RDWR | O_CLOEXEC);
		if (fd < 0)
			continue;
		memset(&res, 0, sizeof(res));
		if (xioctl(fd, DRM_IOCTL_MODE_GETRESOURCES, &res) == 0 && res.count_connectors > 0)
			return fd;
		close(fd);
	}
	return -1;
}

static int setup_memory_only(struct kms *k)
{
	k->mem = calloc(DOTS_W * DOTS_H * 2, 4);
	if (!k->mem)
		return -1;
	k->text_cv = (struct canvas){ (uint32_t *)k->mem, DOTS_W, DOTS_H, DOTS_W };
	k->gray_cv = (struct canvas){ (uint32_t *)k->mem + DOTS_W * DOTS_H, DOTS_W, DOTS_H, DOTS_W };
	return 0;
}

int kms_open(struct kms *k)
{
	struct drm_mode_card_res res;
	struct drm_mode_get_plane_res pr;
	uint32_t crtcs[8], conns[16], encs[16], planes[32];
	struct drm_set_client_cap cap;
	uint32_t i;
	int used_crtc = 0;

	memset(k, 0, sizeof(*k));
	k->fd = find_device();
	if (k->fd < 0)
		return -1;
	cap.capability = DRM_CLIENT_CAP_UNIVERSAL_PLANES;
	cap.value = 1;
	xioctl(k->fd, DRM_IOCTL_SET_CLIENT_CAP, &cap);
	cap.capability = DRM_CLIENT_CAP_ATOMIC;
	cap.value = 1;
	if (xioctl(k->fd, DRM_IOCTL_SET_CLIENT_CAP, &cap) < 0)
		goto fail;

	memset(&res, 0, sizeof(res));
	res.crtc_id_ptr = PTR(crtcs);
	res.count_crtcs = 8;
	res.connector_id_ptr = PTR(conns);
	res.count_connectors = 16;
	res.encoder_id_ptr = PTR(encs);
	res.count_encoders = 16;
	if (xioctl(k->fd, DRM_IOCTL_MODE_GETRESOURCES, &res) < 0)
		goto fail;
	if (res.count_crtcs > 8) res.count_crtcs = 8;
	if (res.count_connectors > 16) res.count_connectors = 16;

	for (i = 0; i < res.count_connectors; i++) {
		struct drm_mode_get_connector c;
		struct drm_mode_get_encoder e;
		struct drm_mode_modeinfo dummy;
		uint32_t enc_ids[8];
		struct kms_out *o = NULL;
		int j;

		memset(&c, 0, sizeof(c));
		c.connector_id = conns[i];
		c.count_modes = 1;
		c.modes_ptr = PTR(&dummy);
		c.count_encoders = 8;
		c.encoders_ptr = PTR(enc_ids);
		if (xioctl(k->fd, DRM_IOCTL_MODE_GETCONNECTOR, &c) < 0)
			continue;
		if (c.connector_type == DRM_MODE_CONNECTOR_DPI && !k->dpi.found)
			o = &k->dpi;
		else if (c.connector_type == DRM_MODE_CONNECTOR_HDMIA && !k->hdmi.found)
			o = &k->hdmi;
		if (!o || c.count_encoders == 0)
			continue;
		memset(&e, 0, sizeof(e));
		e.encoder_id = enc_ids[0];
		if (xioctl(k->fd, DRM_IOCTL_MODE_GETENCODER, &e) < 0)
			continue;
		for (j = 0; j < (int)res.count_crtcs; j++) {
			if ((e.possible_crtcs & (1u << j)) && !(used_crtc & (1 << j))) {
				used_crtc |= 1 << j;
				o->found = 1;
				o->conn_id = conns[i];
				o->crtc_id = crtcs[j];
				o->crtc_index = j;
				break;
			}
		}
	}

	/* Planes: one primary per CRTC, the overlays are shared. */
	memset(&pr, 0, sizeof(pr));
	pr.plane_id_ptr = PTR(planes);
	pr.count_planes = 32;
	if (xioctl(k->fd, DRM_IOCTL_MODE_GETPLANERESOURCES, &pr) < 0)
		goto fail;
	if (pr.count_planes > 32)
		pr.count_planes = 32;
	for (i = 0; i < pr.count_planes; i++) {
		struct drm_mode_get_plane gp;
		uint64_t type = 0;
		struct drm_mode_obj_get_properties op;
		uint32_t ids[32];
		uint64_t vals[32];
		uint32_t tp = prop_id(k->fd, planes[i], DRM_MODE_OBJECT_PLANE, "type", NULL, NULL);
		uint32_t j;

		memset(&gp, 0, sizeof(gp));
		gp.plane_id = planes[i];
		if (xioctl(k->fd, DRM_IOCTL_MODE_GETPLANE, &gp) < 0)
			continue;
		memset(&op, 0, sizeof(op));
		op.obj_id = planes[i];
		op.obj_type = DRM_MODE_OBJECT_PLANE;
		op.props_ptr = PTR(ids);
		op.prop_values_ptr = PTR(vals);
		op.count_props = 32;
		if (xioctl(k->fd, DRM_IOCTL_MODE_OBJ_GETPROPERTIES, &op) == 0)
			for (j = 0; j < op.count_props && j < 32; j++)
				if (ids[j] == tp)
					type = vals[j];
		if (type == PLANE_TYPE_PRIMARY) {
			if (k->dpi.found && (gp.possible_crtcs & (1u << k->dpi.crtc_index)) && !k->dpi.primary)
				k->dpi.primary = planes[i];
			else if (k->hdmi.found && (gp.possible_crtcs & (1u << k->hdmi.crtc_index)) && !k->hdmi.primary)
				k->hdmi.primary = planes[i];
		} else if (type == PLANE_TYPE_OVERLAY && k->n_overlays < 16 &&
			   k->hdmi.found && (gp.possible_crtcs & (1u << k->hdmi.crtc_index))) {
			k->overlays[k->n_overlays++] = planes[i];
		}
	}

	if (buf_create(k->fd, &k->text, DOTS_W, DOTS_H) < 0 ||
	    buf_create(k->fd, &k->gray, DOTS_W, DOTS_H) < 0 ||
	    buf_create(k->fd, &k->blank, DOTS_W, DOTS_H) < 0)
		goto fail;
	k->text_cv = canvas_of(&k->text);
	k->gray_cv = canvas_of(&k->gray);
	if (k->dpi.found)
		read_connector(k->fd, &k->dpi, 1, 0);
	if (k->hdmi.found)
		read_connector(k->fd, &k->hdmi, 1, 1);
	return 0;
fail:
	close(k->fd);
	k->fd = -1;
	return -1;
}

/* ---- colour tables ---------------------------------------------------- */

static void colour_rgb(enum colour c, double rgb[3])
{
	switch (c) {
	case COL_AMBER: rgb[0] = 1.00; rgb[1] = 0.69; rgb[2] = 0.00; break;
	case COL_WHITE: rgb[0] = 0.94; rgb[1] = 0.94; rgb[2] = 1.00; break;
	case COL_RED:   rgb[0] = 1.00; rgb[1] = 0.15; rgb[2] = 0.12; break;
	default:        rgb[0] = 0.25; rgb[1] = 1.00; rgb[2] = 0.20; break;
	}
}

/* Gray 0x80 (normal) becomes 65 percent of the colour, gray 0xFF
 * (bright) the full colour, with straight lines in between so the
 * 16-level gray view keeps its order. */
static uint32_t make_gamma(int fd, enum colour c)
{
	struct drm_color_lut lut[256];
	struct drm_mode_create_blob cb;
	double rgb[3];
	int i;

	colour_rgb(c, rgb);
	for (i = 0; i < 256; i++) {
		double x = i / 255.0, y;
		y = x <= 0.5 ? x / 0.5 * 0.65 : 0.65 + (x - 0.5) / 0.5 * 0.35;
		if (y > 1.0) y = 1.0;
		lut[i].red = (uint16_t)(rgb[0] * y * 65535.0);
		lut[i].green = (uint16_t)(rgb[1] * y * 65535.0);
		lut[i].blue = (uint16_t)(rgb[2] * y * 65535.0);
		lut[i].reserved = 0;
	}
	memset(&cb, 0, sizeof(cb));
	cb.data = PTR(lut);
	cb.length = sizeof(lut);
	if (xioctl(fd, DRM_IOCTL_MODE_CREATEPROPBLOB, &cb) < 0)
		return 0;
	return cb.blob_id;
}

static uint32_t make_mode_blob(int fd, const uint8_t *mode)
{
	struct drm_mode_create_blob cb;

	memset(&cb, 0, sizeof(cb));
	cb.data = PTR(mode);
	cb.length = sizeof(struct drm_mode_modeinfo);
	if (xioctl(fd, DRM_IOCTL_MODE_CREATEPROPBLOB, &cb) < 0)
		return 0;
	return cb.blob_id;
}

static void destroy_blob(int fd, uint32_t *id)
{
	struct drm_mode_destroy_blob db;

	if (!*id)
		return;
	db.blob_id = *id;
	xioctl(fd, DRM_IOCTL_MODE_DESTROYPROPBLOB, &db);
	*id = 0;
}

/* ---- emulation layers -------------------------------------------------- */

/* Faint scan lines: each of the 350 lines gets a darker band at its
 * bottom edge, drawn once at the HDMI resolution. */
static void draw_scanlines(struct kms *k, int w, int h)
{
	int y, x;

	if (k->scan.map && (k->scan.w != w || k->scan.h != h))
		buf_destroy(k->fd, &k->scan);
	if (!k->scan.map && buf_create(k->fd, &k->scan, w, h) < 0)
		return;
	for (y = 0; y < h; y++) {
		double pos = (y + 0.5) * DOTS_H / (double)h;
		double frac = pos - (int)pos;
		uint32_t v = frac > 0.62 ? 0x58000000u : 0;
		uint32_t *p = k->scan.map + (size_t)y * (k->scan.pitch / 4);
		for (x = 0; x < w; x++)
			p[x] = v;
	}
}

/* The glow layer is the picture shrunk by four each way. The display
 * hardware enlarges it again with smooth filtering under the sharp
 * text, which gives a soft halo round lit characters. */
void kms_update_glow(struct kms *k)
{
	int gx, gy, x, y;
	int gw = DOTS_W / 4, gh = (DOTS_H + 3) / 4;

	if (!k->glow.map)
		return;
	for (gy = 0; gy < gh; gy++) {
		for (gx = 0; gx < gw; gx++) {
			unsigned sum = 0, n = 0;
			for (y = gy * 4; y < gy * 4 + 4 && y < DOTS_H; y++)
				for (x = gx * 4; x < gx * 4 + 4; x++) {
					sum += k->text_cv.px[y * k->text_cv.stride + x] & 0xFF;
					n++;
				}
			sum = n ? sum / n : 0;
			sum = sum * 3 / 5;   /* the glow is fainter than the text */
			k->glow.map[gy * (k->glow.pitch / 4) + gx] = 0xFF000000u | sum * 0x010101u;
		}
	}
}

/* ---- applying the settings --------------------------------------------- */

static void plane_set(int fd, struct req *r, uint32_t plane, uint32_t crtc, struct kms_buf *b,
		      int sw, int sh, int dx, int dy, int dw, int dh, int zpos, int nearest)
{
	uint64_t nn = 0;
	uint32_t sf = prop_id(fd, plane, DRM_MODE_OBJECT_PLANE, "SCALING_FILTER", &nn, "Nearest Neighbor");

	add_named(fd, r, plane, DRM_MODE_OBJECT_PLANE, "FB_ID", b->fb_id);
	add_named(fd, r, plane, DRM_MODE_OBJECT_PLANE, "CRTC_ID", crtc);
	add_named(fd, r, plane, DRM_MODE_OBJECT_PLANE, "SRC_X", 0);
	add_named(fd, r, plane, DRM_MODE_OBJECT_PLANE, "SRC_Y", 0);
	add_named(fd, r, plane, DRM_MODE_OBJECT_PLANE, "SRC_W", (uint64_t)sw << 16);
	add_named(fd, r, plane, DRM_MODE_OBJECT_PLANE, "SRC_H", (uint64_t)sh << 16);
	add_named(fd, r, plane, DRM_MODE_OBJECT_PLANE, "CRTC_X", (uint64_t)dx);
	add_named(fd, r, plane, DRM_MODE_OBJECT_PLANE, "CRTC_Y", (uint64_t)dy);
	add_named(fd, r, plane, DRM_MODE_OBJECT_PLANE, "CRTC_W", (uint64_t)dw);
	add_named(fd, r, plane, DRM_MODE_OBJECT_PLANE, "CRTC_H", (uint64_t)dh);
	if (zpos >= 0)
		add_named(fd, r, plane, DRM_MODE_OBJECT_PLANE, "zpos", (uint64_t)zpos);
	if (sf)
		add(r, plane, sf, nearest ? nn : 0);
}

static void plane_off(int fd, struct req *r, uint32_t plane)
{
	add_named(fd, r, plane, DRM_MODE_OBJECT_PLANE, "FB_ID", 0);
	add_named(fd, r, plane, DRM_MODE_OBJECT_PLANE, "CRTC_ID", 0);
}

static void crtc_on(int fd, struct req *r, struct kms_out *o, uint32_t gamma)
{
	destroy_blob(fd, &o->mode_blob);
	o->mode_blob = make_mode_blob(fd, o->mode);
	add_named(fd, r, o->conn_id, DRM_MODE_OBJECT_CONNECTOR, "CRTC_ID", o->crtc_id);
	add_named(fd, r, o->crtc_id, DRM_MODE_OBJECT_CRTC, "MODE_ID", o->mode_blob);
	add_named(fd, r, o->crtc_id, DRM_MODE_OBJECT_CRTC, "ACTIVE", 1);
	add_named(fd, r, o->crtc_id, DRM_MODE_OBJECT_CRTC, "GAMMA_LUT", gamma);
}

static void crtc_off(int fd, struct req *r, struct kms_out *o)
{
	add_named(fd, r, o->conn_id, DRM_MODE_OBJECT_CONNECTOR, "CRTC_ID", 0);
	add_named(fd, r, o->crtc_id, DRM_MODE_OBJECT_CRTC, "MODE_ID", 0);
	add_named(fd, r, o->crtc_id, DRM_MODE_OBJECT_CRTC, "ACTIVE", 0);
}

static int build_and_commit(struct kms *k, const struct settings *s, int test_only)
{
	struct req r;
	int i, ov = 0;
	int want_dpi = k->dpi.found && k->dpi.connected;
	int want_hdmi = k->hdmi.found && k->hdmi.connected && s->output != OUT_MDA;
	int emu = s->output == OUT_EMU;
	int dpi_picture = s->output == OUT_BOTH || s->output == OUT_MDA;
	uint32_t flags = DRM_MODE_ATOMIC_ALLOW_MODESET | (test_only ? DRM_MODE_ATOMIC_TEST_ONLY : 0);

	memset(&r, 0, sizeof(r));

	/* DPI: the MDA monitor. Its sync keeps running in every output
	 * mode, to protect the monitor; outside the both and mda outputs
	 * it shows a black picture. */
	if (want_dpi && k->dpi.primary) {
		crtc_on(k->fd, &r, &k->dpi, 0);
		if (!dpi_picture)
			plane_set(k->fd, &r, k->dpi.primary, k->dpi.crtc_id, &k->blank,
				  DOTS_W, DOTS_H, 0, 0, k->dpi.mode_w, k->dpi.mode_h, -1, 1);
		else if (k->dpi_doubled)
			plane_set(k->fd, &r, k->dpi.primary, k->dpi.crtc_id, &k->dpi2,
				  k->dpi2.w, DOTS_H, 0, 0, k->dpi.mode_w, k->dpi.mode_h, -1, 1);
		else
			plane_set(k->fd, &r, k->dpi.primary, k->dpi.crtc_id, &k->text,
				  DOTS_W, DOTS_H, 0, 0, k->dpi.mode_w, k->dpi.mode_h, -1, 1);
	} else if (k->dpi.found) {
		if (k->dpi.primary)
			plane_off(k->fd, &r, k->dpi.primary);
		crtc_off(k->fd, &r, &k->dpi);
	}

	/* HDMI in colour mode: the camera picture, in its own proportions,
	 * centred, with no colour table. */
	if (want_hdmi && k->hdmi.primary && s->mode == MODE_COLOUR && k->colour.map) {
		int h = k->hdmi.mode_h, w = h * k->colour.w / k->colour.h;
		int x = (k->hdmi.mode_w - w) / 2;

		if (x < 0) {
			w = k->hdmi.mode_w;
			h = w * k->colour.h / k->colour.w;
			x = 0;
		}
		crtc_on(k->fd, &r, &k->hdmi, 0);
		plane_set(k->fd, &r, k->hdmi.primary, k->hdmi.crtc_id, &k->colour,
			  k->colour.w, k->colour.h, x, (k->hdmi.mode_h - h) / 2, w, h, -1, 0);
	} else
	/* HDMI: a centred 4 by 3 area with black bars at the sides. */
	if (want_hdmi && k->hdmi.primary) {
		int h = k->hdmi.mode_h, w = h * 4 / 3;
		int x = (k->hdmi.mode_w - w) / 2;
		struct kms_buf *pic = (!emu && s->hdmi_gray) ? &k->gray : &k->text;

		if (x < 0) {
			w = k->hdmi.mode_w;
			x = 0;
		}
		destroy_blob(k->fd, &k->hdmi.gamma_blob);
		k->hdmi.gamma_blob = make_gamma(k->fd, emu ? s->phosphor : s->hdmi_colour);
		crtc_on(k->fd, &r, &k->hdmi, k->hdmi.gamma_blob);
		if (emu && s->glow && k->n_overlays >= 1) {
			if (!k->glow.map)
				buf_create(k->fd, &k->glow, DOTS_W / 4, (DOTS_H + 3) / 4);
			kms_update_glow(k);
			plane_set(k->fd, &r, k->hdmi.primary, k->hdmi.crtc_id, &k->glow,
				  DOTS_W / 4, DOTS_H / 4, x, 0, w, h, -1, 0);
			plane_set(k->fd, &r, k->overlays[ov], k->hdmi.crtc_id, pic,
				  DOTS_W, DOTS_H, x, 0, w, h, ov + 1, 1);
			ov++;
		} else {
			plane_set(k->fd, &r, k->hdmi.primary, k->hdmi.crtc_id, pic,
				  DOTS_W, DOTS_H, x, 0, w, h, -1, 1);
		}
		if (emu && s->scanlines && ov < k->n_overlays) {
			draw_scanlines(k, w, h);
			if (k->scan.map) {
				plane_set(k->fd, &r, k->overlays[ov], k->hdmi.crtc_id, &k->scan,
					  w, h, x, 0, w, h, ov + 1, 0);
				ov++;
			}
		}
	} else if (k->hdmi.found) {
		if (k->hdmi.primary)
			plane_off(k->fd, &r, k->hdmi.primary);
		crtc_off(k->fd, &r, &k->hdmi);
	}
	for (i = ov; i < k->n_overlays; i++)
		plane_off(k->fd, &r, k->overlays[i]);

	k->dpi.active = want_dpi && !test_only;
	k->hdmi.active = want_hdmi && !test_only;
	return commit(k->fd, &r, flags);
}

int kms_apply(struct kms *k, const struct settings *s)
{
	if (k->fd < 0) {
		if (!k->mem)
			return setup_memory_only(k);
		return 0;
	}
	/* First check that the display hardware accepts doubling the 720
	 * dots to 1440 pixels. If not, draw into a buffer twice as wide. */
	if (!k->dpi_doubled && build_and_commit(k, s, 1) < 0 && k->dpi.found && k->dpi.connected) {
		if (!k->dpi2.map && buf_create(k->fd, &k->dpi2, DOTS_W * 2, DOTS_H) == 0) {
			k->dpi_doubled = 1;
			k->dpi2_cv = canvas_of(&k->dpi2);
			log_msg("The display hardware would not double the picture width for the MDA monitor, so a double-width picture is drawn instead. This uses a little more processor time.");
		}
	}
	if (build_and_commit(k, s, 0) < 0) {
		int err = errno;
		/* A second try without the HDMI output, so the MDA monitor
		 * always gets its sync signals. */
		int was = k->hdmi.connected;
		k->hdmi.connected = 0;
		if (build_and_commit(k, s, 0) < 0) {
			log_msg("The display settings could not be applied (%s).", strerror(errno));
			k->hdmi.connected = was;
			return -1;
		}
		if (was)
			log_msg("The HDMI screen could not be used (%s), so only the MDA output is running.", strerror(err));
	}
	return 0;
}

/* Called every 2 seconds: checks whether an HDMI screen was plugged in
 * or unplugged and starts or stops that output. Returns 1 on a change. */
int kms_poll_hotplug(struct kms *k, const struct settings *s)
{
	int before, w, h;

	if (k->fd < 0 || !k->hdmi.found || s->output == OUT_MDA)
		return 0;
	before = k->hdmi.connected;
	w = k->hdmi.mode_w;
	h = k->hdmi.mode_h;
	if (read_connector(k->fd, &k->hdmi, 1, 1) < 0)
		return 0;
	if (before == k->hdmi.connected && (!before || (w == k->hdmi.mode_w && h == k->hdmi.mode_h)))
		return 0;
	if (k->hdmi.connected)
		log_msg("An HDMI screen was found and is now used at %d by %d.", k->hdmi.mode_w, k->hdmi.mode_h);
	else
		log_msg("The HDMI screen was disconnected; the other outputs carry on.");
	kms_apply(k, s);
	return 1;
}

void kms_colour_frame(struct kms *k, const struct settings *s, const uint8_t *yuv, int w, int h, int stride)
{
	const uint8_t *src;
	uint8_t *dst;
	int y, plane, apply = 0;

	if (k->fd < 0 || !k->hdmi.found)
		return;
	if (k->colour.map && (k->colour.w != w || k->colour.h != h))
		buf_destroy(k->fd, &k->colour);
	if (!k->colour.map) {
		if (buf_create_yuv(k->fd, &k->colour, w, h) < 0) {
			log_msg("The colour picture buffer could not be created (%s), so HDMI keeps showing text.", strerror(errno));
			memset(&k->colour, 0, sizeof(k->colour));
			return;
		}
		apply = 1;
	}
	/* Brightness plane, then the two half-size colour planes. */
	src = yuv;
	dst = (uint8_t *)k->colour.map;
	for (y = 0; y < h; y++)
		memcpy(dst + (size_t)y * k->colour.pitch, src + (size_t)y * stride, (size_t)w);
	src = yuv + (size_t)stride * h;
	dst = (uint8_t *)k->colour.map + (size_t)k->colour.pitch * h;
	for (plane = 0; plane < 2; plane++) {
		for (y = 0; y < h / 2; y++)
			memcpy(dst + (size_t)y * (k->colour.pitch / 2), src + (size_t)y * (stride / 2), (size_t)w / 2);
		src += (size_t)(stride / 2) * (h / 2);
		dst += (size_t)(k->colour.pitch / 2) * (h / 2);
	}
	if (apply)
		kms_apply(k, s);
}

void kms_close(struct kms *k)
{
	if (k->fd >= 0) {
		buf_destroy(k->fd, &k->colour);
		buf_destroy(k->fd, &k->text);
		buf_destroy(k->fd, &k->gray);
		buf_destroy(k->fd, &k->blank);
		buf_destroy(k->fd, &k->glow);
		buf_destroy(k->fd, &k->scan);
		buf_destroy(k->fd, &k->dpi2);
		close(k->fd);
		k->fd = -1;
	}
	free(k->mem);
	k->mem = NULL;
}
