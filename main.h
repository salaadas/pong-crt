#pragma once

#include "catalog.h"
#include "shader_catalog.h"
#include "texture_catalog.h"
#include "mesh_catalog.h"

// @Hack: @Incomplete: We want the ability to switch between different sizes too, not just 320 by 240.
constexpr auto OFFSCREEN_BUFFER_SCALE  = 3.0f;
constexpr auto OFFSCREEN_BUFFER_WIDTH  = 320 * OFFSCREEN_BUFFER_SCALE;
constexpr auto OFFSCREEN_BUFFER_HEIGHT = 240 * OFFSCREEN_BUFFER_SCALE;
constexpr auto OFFSCREEN_BUFFER_ASPECT = OFFSCREEN_BUFFER_WIDTH / OFFSCREEN_BUFFER_HEIGHT;

extern RArr<Catalog_Base*> all_catalogs;
extern Shader_Catalog      shader_catalog;
extern Texture_Catalog     texture_catalog;
extern Mesh_Catalog        mesh_catalog;

extern Texture_Map *the_white_texture;

extern i32 BIG_FONT_SIZE;
extern const String FONT_FOLDER;

extern bool should_quit;
extern bool was_window_resized_this_frame;
extern bool window_in_focus;

Texture_Map *load_linear_mipmapped_texture(String short_name);
Texture_Map *load_nearest_mipmapped_texture(String short_name);
