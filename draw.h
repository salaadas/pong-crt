#include "common.h"

void init_draw();
void draw_game();

void rendering_2d_right_handed();
void rendering_2d_right_handed_unit_scale();

struct Dynamic_Font;

void draw_prepared_text(Dynamic_Font *font, i64 x, i64 y, Vector4 color, f32 theta = 0, f32 z_layer = 0);
i64 draw_text(Dynamic_Font *font, i64 x, i64 y, String text, Vector4 color);
