// -*- c-basic-offset: 8; indent-tabs-mode: t -*-
// vim:ts=8:sw=8:noet:ai:
/*
 * Copyright (C) 2006 Evgeniy Stepanov <eugeni.stepanov@gmail.com>
 *
 * This file is part of libass.
 *
 * libass is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * libass is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with libass; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef LIBASS_CACHE_H
#define LIBASS_CACHE_H

#include "ass.h"
#include "ass_font.h"
#include "ass_bitmap.h"

void ass_font_cache_init(void);
ass_font_t* ass_font_cache_find(ass_font_desc_t* desc);
void* ass_font_cache_add(ass_font_t* font);
void ass_font_cache_done(void);


// Create definitions for bitmap_hash_key and glyph_hash_key
#define CREATE_STRUCT_DEFINITIONS
#include "ass_cache_template.c"

typedef struct bitmap_hash_val_s {
	bitmap_t* bm; // the actual bitmaps
	bitmap_t* bm_o;
	bitmap_t* bm_s;
} bitmap_hash_val_t;

void ass_bitmap_cache_init(void);
void* cache_add_bitmap(bitmap_hash_key_t* key, bitmap_hash_val_t* val);
bitmap_hash_val_t* cache_find_bitmap(bitmap_hash_key_t* key);
void ass_bitmap_cache_reset(void);
void ass_bitmap_cache_done(void);


// Cache for composited bitmaps
typedef struct composite_hash_key_s {
	int aw, ah, bw, bh;
	int ax, ay, bx, by;
	bitmap_hash_key_t a;
	bitmap_hash_key_t b;
} composite_hash_key_t;

typedef struct composite_hash_val_s {
	unsigned char* a;
	unsigned char* b;
} composite_hash_val_t;

void ass_composite_cache_init(void);
void* cache_add_composite(composite_hash_key_t* key, composite_hash_val_t* val);
composite_hash_val_t* cache_find_composite(composite_hash_key_t* key);
void ass_composite_cache_reset(void);
void ass_composite_cache_done(void);


typedef struct glyph_hash_val_s {
	FT_Glyph glyph;
	FT_Glyph outline_glyph;
	FT_BBox bbox_scaled; // bbox after scaling, but before rotation
	FT_Vector advance; // 26.6, advance distance to the next bitmap in line
} glyph_hash_val_t;

void ass_glyph_cache_init(void);
void* cache_add_glyph(glyph_hash_key_t* key, glyph_hash_val_t* val);
glyph_hash_val_t* cache_find_glyph(glyph_hash_key_t* key);
void ass_glyph_cache_reset(void);
void ass_glyph_cache_done(void);

typedef struct hashmap_s hashmap_t; 
typedef void (*hashmap_item_dtor_t)(void* key, size_t key_size, void* value, size_t value_size);
typedef int (*hashmap_key_compare_t)(void* key1, void* key2, size_t key_size);
typedef unsigned (*hashmap_hash_t)(void* key, size_t key_size);

hashmap_t* hashmap_init(size_t key_size, size_t value_size, int nbuckets,
			hashmap_item_dtor_t item_dtor, hashmap_key_compare_t key_compare,
			hashmap_hash_t hash);
void hashmap_done(hashmap_t* map);
void* hashmap_insert(hashmap_t* map, void* key, void* value);
void* hashmap_find(hashmap_t* map, void* key);

#endif /* LIBASS_CACHE_H */
