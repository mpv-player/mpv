

#include <stdlib.h>
#include "pullup.h"




#ifdef HAVE_MMX
static int diff_y_mmx(unsigned char *a, unsigned char *b, int s)
{
	int ret;
	asm (
		"movl $4, %%ecx \n\t"
		"pxor %%mm4, %%mm4 \n\t"
		"pxor %%mm7, %%mm7 \n\t"
		
		".balign 16 \n\t"
		"1: \n\t"
		
		"movq (%%esi), %%mm0 \n\t"
		"movq (%%esi), %%mm2 \n\t"
		"addl %%eax, %%esi \n\t"
		"movq (%%edi), %%mm1 \n\t"
		"addl %%eax, %%edi \n\t"
		"psubusb %%mm1, %%mm2 \n\t"
		"psubusb %%mm0, %%mm1 \n\t"
		"movq %%mm2, %%mm0 \n\t"
		"movq %%mm1, %%mm3 \n\t"
		"punpcklbw %%mm7, %%mm0 \n\t"
		"punpcklbw %%mm7, %%mm1 \n\t"
		"punpckhbw %%mm7, %%mm2 \n\t"
		"punpckhbw %%mm7, %%mm3 \n\t"
		"paddw %%mm0, %%mm4 \n\t"
		"paddw %%mm1, %%mm4 \n\t"
		"paddw %%mm2, %%mm4 \n\t"
		"paddw %%mm3, %%mm4 \n\t"
		
		"decl %%ecx \n\t"
		"jnz fb \n\t"
		
		"movq %%mm4, %%mm3 \n\t"
		"punpcklwl %%mm7, %%mm4 \n\t"
		"punpckhwl %%mm7, %%mm3 \n\t"
		"paddl %%mm4, %%mm3 \n\t"
		"movq %%mm3, %%mm2 \n\t"
		"punpckllq %%mm7, %%mm3 \n\t"
		"punpckhlq %%mm7, %%mm2 \n\t"
		"paddl %%mm3, %%mm2 \n\t"
		"movl %%mm2, %eax"
		
		"emms \n\t"
		: "=a" (ret)
		: "S" (a), "D" (b), "a" (s)
		:
		);
	return ret;
}
#endif

#define ABS(a) (((a)^((a)>>31))-((a)>>31))

static int diff_y(unsigned char *a, unsigned char *b, int s)
{
	int i, j, diff=0;
	for (i=4; i; i--) {
		for (j=0; j<8; j++) diff += ABS(a[j]-b[j]);
		a+=s; b+=s;
	}
	return diff;
}

static int licomb_y(unsigned char *a, unsigned char *b, int s)
{
	int i, j, diff=0;
	for (i=8; i; i--) {
		for (j=0; j<8; j++)
			diff += ABS((a[j]<<1) - b[j-s] - b[j])
				+ ABS((b[j]<<1) - a[j] - a[j+s]);
		a+=s; b+=s;
	}
	return diff;
}









static void alloc_buffer(struct pullup_context *c, struct pullup_buffer *b)
{
	int i;
	if (b->planes) return;
	b->planes = calloc(c->nplanes, sizeof(unsigned char *));
	for (i = 0; i < c->nplanes; i++) {
		b->planes[i] = malloc(c->h[i]*c->stride[i]);
		/* Deal with idiotic 128=0 for chroma: */
		memset(b->planes[i], c->background[i], c->h[i]*c->stride[i]);
	}
}

struct pullup_buffer *pullup_lock_buffer(struct pullup_buffer *b, int parity)
{
	if (parity+1 & 1) b->lock[0]++;
	if (parity+1 & 2) b->lock[1]++;
	return b;
}

void pullup_release_buffer(struct pullup_buffer *b, int parity)
{
	if (parity+1 & 1) b->lock[0]--;
	if (parity+1 & 2) b->lock[1]--;
}

struct pullup_buffer *pullup_get_buffer(struct pullup_context *c, int parity)
{
	int i;

	/* Try first to get the sister buffer for the previous field */
	if (parity < 2 && c->last && parity != c->last->parity
	    && !c->last->buffer->lock[parity]) {
		alloc_buffer(c, c->last->buffer);
		return pullup_lock_buffer(c->last->buffer, parity);
	}
	
	/* Prefer a buffer with both fields open */
	for (i = 0; i < c->nbuffers; i++) {
		if (c->buffers[i].lock[0]) continue;
		if (c->buffers[i].lock[1]) continue;
		alloc_buffer(c, &c->buffers[i]);
		return pullup_lock_buffer(&c->buffers[i], parity);
	}

	if (parity == 2) return 0;
	
	/* Search for any half-free buffer */
	for (i = 0; i < c->nbuffers; i++) {
		if (parity+1 & 1 && c->buffers[i].lock[0]) continue;
		if (parity+1 & 2 && c->buffers[i].lock[1]) continue;
		alloc_buffer(c, &c->buffers[i]);
		return pullup_lock_buffer(&c->buffers[i], parity);
	}
	
	return 0;
}






static void compute_metric(struct pullup_context *c,
	struct pullup_field *fa, int pa,
	struct pullup_field *fb, int pb,
	int (*func)(unsigned char *, unsigned char *, int), int *dest)
{
	unsigned char *a, *b;
	int x, y;
	int xstep = c->bpp[0];
	int ystep = c->stride[0]<<3;
	int s = c->stride[0]<<1; /* field stride */
	int w = c->metric_w*xstep;

	if (!fa->buffer || !fb->buffer) return;

	/* Shortcut for duplicate fields (e.g. from RFF flag) */
	if (fa->buffer == fb->buffer && pa == pb) {
		memset(dest, 0, c->metric_len * sizeof(int));
		return;
	}

	a = fa->buffer->planes[0] + pa * c->stride[0] + c->metric_offset;
	b = fb->buffer->planes[0] + pb * c->stride[0] + c->metric_offset;

	for (y = c->metric_h; y; y--) {
		for (x = 0; x < w; x += xstep) {
			*dest++ = func(a + x, b + x, s);
		}
		a += ystep; b += ystep;
	}
}





static void alloc_metrics(struct pullup_context *c, struct pullup_field *f)
{
	f->diffs = calloc(c->metric_len, sizeof(int));
	f->licomb = calloc(c->metric_len, sizeof(int));
	/* add more metrics here as needed */
}

static struct pullup_field *make_field_queue(struct pullup_context *c, int len)
{
	struct pullup_field *head, *f;
	f = head = calloc(1, sizeof(struct pullup_field));
	alloc_metrics(c, f);
	for (; len > 0; len--) {
		f->next = calloc(1, sizeof(struct pullup_field));
		f->next->prev = f;
		f = f->next;
		alloc_metrics(c, f);
	}
	f->next = head;
	head->prev = f;
	return head;
}

static void check_field_queue(struct pullup_context *c)
{
	if (c->head->next == c->first) {
		struct pullup_field *f = calloc(1, sizeof(struct pullup_field));
		alloc_metrics(c, f);
		f->prev = c->head;
		f->next = c->first;
		c->head->next = f;
		c->first->prev = f;
	}
}

int pullup_submit_field(struct pullup_context *c, struct pullup_buffer *b, int parity)
{
	struct pullup_field *f;
	
	/* Grow the circular list if needed */
	check_field_queue(c);
	
	/* Cannot have two fields of same parity in a row; drop the new one */
	if (c->last && c->last->parity == parity) return 0;

	f = c->head;
	f->parity = parity;
	f->buffer = pullup_lock_buffer(b, parity);
	f->flags = 0;
	f->breaks = 0;
	f->affinity = 0;

	compute_metric(c, f, parity, f->prev->prev, parity, c->diff, f->diffs);
	compute_metric(c, parity?f->prev:f, 0, parity?f:f->prev, 1, c->licomb, f->licomb);

	/* Advance the circular list */
	if (!c->first) c->first = c->head;
	c->last = c->head;
	c->head = c->head->next;
}

void pullup_flush_fields(struct pullup_context *c)
{
	struct pullup_field *f;
	
	for (f = c->first; f && f != c->head; f = f->next) {
		pullup_release_buffer(f->buffer, f->parity);
		f->buffer = 0;
	}
	c->first = c->last = 0;
}








#define F_HAVE_BREAKS 1
#define F_HAVE_AFFINITY 2


#define BREAK_LEFT 1
#define BREAK_RIGHT 2




static int queue_length(struct pullup_field *begin, struct pullup_field *end)
{
	int count = 1;
	struct pullup_field *f;
	
	if (!begin || !end) return 0;
	for (f = begin; f != end; f = f->next) count++;
	return count;
}

static int find_first_break(struct pullup_field *f, int max)
{
	int i;
	for (i = 0; i < max; i++) {
		if (f->breaks & BREAK_RIGHT || f->next->breaks & BREAK_LEFT)
			return i+1;
		f = f->next;
	}
	return 0;
}

static void compute_breaks(struct pullup_context *c, struct pullup_field *f0)
{
	int i;
	struct pullup_field *f1 = f0->next;
	struct pullup_field *f2 = f1->next;
	struct pullup_field *f3 = f2->next;
	int l, max_l=0, max_r=0;

	if (f0->flags & F_HAVE_BREAKS) return;
	f0->flags |= F_HAVE_BREAKS;

	/* Special case when fields are 100% identical */
	if (f0->buffer == f2->buffer && f1->buffer != f3->buffer) {
		f0->breaks |= BREAK_LEFT;
		f2->breaks |= BREAK_RIGHT;
		return;
	}

	for (i = 0; i < c->metric_len; i++) {
		l = f2->diffs[i] - f3->diffs[i];
		if (l > max_l) max_l = l;
		if (-l > max_r) max_r = -l;
	}
	/* Don't get tripped up when differences are mostly quant error */
	if (max_l + max_r < 256) return;
	if (max_l > 4*max_r) f1->breaks |= BREAK_LEFT;
	if (max_r > 4*max_l) f2->breaks |= BREAK_RIGHT;
	//printf("max_l=%d max_r=%d\n", max_l, max_r);
}

static void compute_affinity(struct pullup_context *c, struct pullup_field *f)
{
	int i;
	int max_l=0, max_r=0, l, t;
	if (f->flags & F_HAVE_AFFINITY) return;
	f->flags |= F_HAVE_AFFINITY;
	for (i = 0; i < c->metric_len; i++) {
		l = f->licomb[i] - f->next->licomb[i];
		if (l > max_l) max_l = l;
		if (-l > max_r) max_r = -l;
	}
	if (max_l + max_r < 256) return;
	if (max_r > 3*max_l) f->affinity = -1;
	else if (max_l > 3*max_r) f->affinity = 1;
	else if (max_l + max_r > 2048) {
		for (i = 0; i < c->metric_len; i++) {
			l += f->licomb[i] - f->next->licomb[i];
			t += ABS(f->licomb[i] - f->next->licomb[i]);
		}
		if (-l*4 > t) f->affinity = -1;
		else if (l*4 > t) f->affinity = 1;
		//printf("affinity from avg: %d\n", f->affinity);
	}
}

static void foo(struct pullup_context *c)
{
	struct pullup_field *f = c->first;
	int i, n = queue_length(f, c->last);
	for (i = 0; i < n-1; i++) {
		if (i < n-3) compute_breaks(c, f);
		compute_affinity(c, f);
		f = f->next;
	}
}

static int decide_frame_length(struct pullup_context *c)
{
	int n;
	struct pullup_field *f0 = c->first;
	struct pullup_field *f1 = f0->next;
	struct pullup_field *f2 = f1->next;
	struct pullup_field *f3 = f2->next;
	struct pullup_field *f4 = f3->next;
	struct pullup_field *f5 = f4->next;
	
	if (queue_length(c->first, c->last) < 6) return 0;
	foo(c);

	n = find_first_break(f0, 3);

	if (f0->affinity == -1) return 1;

	switch (n) {
	case 1:
		return 1;
	case 2:
		if (f1->affinity == 1) return 1;
		else return 2;
	case 3:
		if (f1->affinity == -1) return 2;
		else if (f1->affinity == 1) return 1;
		else return 3;
	default:
		if (f1->affinity == 1) return 1;
		else if (f1->affinity == -1) return 2;
		else if (f2->affinity == 1) return 2;
		else if (f0->affinity == 1 && f2->affinity == -1) return 3;
		else if (f2->affinity == 0 && f3->affinity == 1) return 3;
		else return 2;
	}
}


static void print_aff_and_breaks(struct pullup_context *c, struct pullup_field *f)
{
	int i;
	int max_l, max_r, l;
	struct pullup_field *f0 = f;
	const char aff_l[] = "+..", aff_r[] = "..+";
	printf("\naffinity: ");
	for (i = 0; i < 6; i++) {
		printf("%c%d%c", aff_l[1+f->affinity], i, aff_r[1+f->affinity]);
		f = f->next;
	}
	f = f0;
	printf("\nbreaks:   ");
	for (i=0; i<6; i++) {
		printf("%c%d%c", f->breaks & BREAK_LEFT ? '|' : '.', i, f->breaks & BREAK_RIGHT ? '|' : '.');
		f = f->next;
	}
	printf("\n");
}





struct pullup_frame *pullup_get_frame(struct pullup_context *c)
{
	int i;
	struct pullup_frame *fr = c->frame;
	int n = decide_frame_length(c);

	if (!n) return 0;
	if (fr->lock) return 0;

	print_aff_and_breaks(c, c->first);
	printf("duration: %d    \n", n);

	fr->lock++;
	fr->length = n;
	fr->parity = c->first->parity;
	fr->buffer = 0;
	for (i = 0; i < n; i++) {
		/* We cheat and steal the buffer without release+relock */
		fr->fields[i] = c->first->buffer;
		c->first->buffer = 0;
		c->first = c->first->next;
	}
	/* Export the entire frame as one buffer, if possible! */
	if (n == 2 && fr->fields[0] == fr->fields[1]) {
		fr->buffer = fr->fields[0];
		pullup_lock_buffer(fr->buffer, 2);
		return fr;
	}
	/* (loop is in case we ever support frames longer than 3 fields) */
	for (i = 1; i < n-1; i++) {
		if (fr->fields[i] == fr->fields[i-1]
		    || fr->fields[i] == fr->fields[i+1]) {
			fr->buffer = fr->fields[i];
			pullup_lock_buffer(fr->buffer, 2);
			break;
		}
	}
	return fr;
}

static void copy_field(struct pullup_context *c, struct pullup_buffer *dest,
	struct pullup_buffer *src, int parity)
{
	int i, j;
	unsigned char *d, *s;
	for (i = 0; i < c->nplanes; i++) {
		s = src->planes[i] + parity*c->stride[i];
		d = dest->planes[i] + parity*c->stride[i];
		for (j = c->h[i]>>1; j; j--) {
			memcpy(d, s, c->stride[i]);
			s += c->stride[i]<<1;
			d += c->stride[i]<<1;
		}
	}
}

void pullup_pack_frame(struct pullup_context *c, struct pullup_frame *fr)
{
	int i;
	int par = fr->parity;
	if (fr->buffer) return;
	if (fr->length < 2) return; /* FIXME: deal with this */
	for (i = 0; i < fr->length; i++)
	{
		if (fr->fields[i]->lock[par ^ (i&1) ^ 1]) continue;
		fr->buffer = fr->fields[i];
		pullup_lock_buffer(fr->buffer, 2);
		copy_field(c, fr->buffer, fr->fields[i+(i>0?-1:1)], par^(i&1)^1);
		return;
	}
	fr->buffer = pullup_get_buffer(c, 2);
	copy_field(c, fr->buffer, fr->fields[0], par);
	copy_field(c, fr->buffer, fr->fields[1], par^1);
}

void pullup_release_frame(struct pullup_frame *fr)
{
	int i;
	for (i = 0; i < fr->length; i++)
		pullup_release_buffer(fr->fields[i], fr->parity ^ (i&1));
	if (fr->buffer) pullup_release_buffer(fr->buffer, 2);
	fr->lock--;
}






struct pullup_context *pullup_alloc_context()
{
	struct pullup_context *c;

	c = calloc(1, sizeof(struct pullup_context));

	return c;
}

void pullup_preinit_context(struct pullup_context *c)
{
	c->bpp = calloc(c->nplanes, sizeof(int));
	c->w = calloc(c->nplanes, sizeof(int));
	c->h = calloc(c->nplanes, sizeof(int));
	c->stride = calloc(c->nplanes, sizeof(int));
	c->background = calloc(c->nplanes, sizeof(int));
}

void pullup_init_context(struct pullup_context *c)
{
	if (c->nbuffers < 10) c->nbuffers = 10;
	c->buffers = calloc(c->nbuffers, sizeof (struct pullup_buffer));

	c->metric_w = (c->w[0] - (c->junk_left + c->junk_right << 3)) >> 3;
	c->metric_h = (c->h[0] - (c->junk_top + c->junk_bottom << 1)) >> 3;
	c->metric_offset = c->junk_left*c->bpp[0] + (c->junk_top<<1)*c->stride[0];
	c->metric_len = c->metric_w * c->metric_h;
	
	c->head = make_field_queue(c, 8);

	c->frame = calloc(1, sizeof (struct pullup_frame));
	c->frame->fields = calloc(3, sizeof (struct pullup_buffer *));

	switch(c->format) {
	case PULLUP_FMT_Y:
		c->diff = diff_y;
		c->licomb = licomb_y;
#ifdef HAVE_MMX
		if (c->cpu & PULLUP_CPU_MMX) c->diff = diff_y_mmx;
#endif
		break;
#if 0
	case PULLUP_FMT_YUY2:
		c->diff = diff_yuy2;
		break;
	case PULLUP_FMT_RGB32:
		c->diff = diff_rgb32;
		break;
#endif
	}
}

void pullup_free_context(struct pullup_context *c)
{
	/* FIXME: free! */
}









