#pragma once

#include "catalog.h"
#include "shader_catalog.h"
#include "texture_catalog.h"

extern RArr<Catalog_Base*> all_catalogs;
extern Shader_Catalog      shader_catalog;
extern Texture_Catalog     texture_catalog;

extern i32 BIG_FONT_SIZE;
extern const String FONT_FOLDER;

extern bool should_quit;
extern bool was_window_resized_this_frame;
extern bool window_in_focus;

Texture_Map *load_linear_mipmapped_texture(String short_name);
Texture_Map *load_nearest_mipmapped_texture(String short_name);
