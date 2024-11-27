// Alt-Tab when game first boot up crashes because of font.

#include "draw.h"
#include "main.h"
#include "opengl.h"
#include "time_info.h"
#include "hud.h"

// In 2D this is flipped because our rendering system is right-handed.
constexpr auto Z_NEAR = -200.f;
constexpr auto Z_FAR  = 0.f;

Shader *shader_argb_and_texture;
Shader *shader_argb_no_texture;
Shader *shader_text;

Shader *shader_crt_filter; // This is the crt shader for the offscreen buffer.
Shader *shader_bezel_reflection;
Shader *shader_blur;
Shader *shader_accumulate;
Shader *shader_copy_pixels;
Shader *shader_blend;
Shader *shader_argb_filmic_texture;

Texture_Map *overlay_background_map;

//
// These render target textures are used in post-processing.
// Right now, they are used for the ghosting and gaussian blur effects.
//
Texture_Map *the_accumulation_texture_a;
Texture_Map *the_accumulation_texture_b;
Texture_Map *the_blur_texture_a;
Texture_Map *the_blur_texture_b;
// Texture_Map *the_crt_filtered_map;

// These are for testing, deprecate later!
Texture_Map *testing_background;
Texture_Map *mario_sprite;
Vector2 testing_pos;
f32 testing_size = 200.0f;
Vector2 testing_velocity = Vector2(1, 1);

inline
Shader *find_shader(String name)
{
    auto result = catalog_find(&shader_catalog, name);
    assert(result);

    return result;
}

void init_draw()
{
    shader_argb_and_texture = find_shader(String("argb_and_texture"));
    shader_argb_no_texture  = find_shader(String("argb_no_texture"));
    shader_text             = find_shader(String("text"));

    shader_argb_filmic_texture = find_shader(String("argb_filmic_texture"));

    shader_crt_filter       = find_shader(String("crt_filter"));
    shader_bezel_reflection = find_shader(String("bezel_reflection"));
    shader_blur             = find_shader(String("blur"));
    shader_accumulate       = find_shader(String("accumulate"));
    shader_copy_pixels      = find_shader(String("copy_pixels"));
    shader_blend            = find_shader(String("blend"));

    shader_argb_no_texture->backface_cull  = false;
    shader_argb_and_texture->backface_cull = false;

    shader_bezel_reflection->diffuse_texture_wraps = false;
    shader_crt_filter->diffuse_texture_wraps       = false;
    shader_blur->diffuse_texture_wraps        = false;
    shader_accumulate->diffuse_texture_wraps  = false;
    shader_copy_pixels->diffuse_texture_wraps = false;
    shader_blend->diffuse_texture_wraps       = false;

    //
    // Post-processing render target set up.
    // These render targets have the same dimensions as the offscreen buffer.
    //
    auto obw = the_offscreen_buffer->width;
    auto obh = the_offscreen_buffer->height;
    assert((obw > 0) && (obh > 0));

    // No depth components for these.
    the_accumulation_texture_a = create_texture_render_target(obw, obh, false).first;
    the_accumulation_texture_b = create_texture_render_target(obw, obh, false).first;

    the_blur_texture_a = create_texture_render_target(obw, obh, false).first;
    the_blur_texture_b = create_texture_render_target(obw, obh, false).first;

    overlay_background_map = load_nearest_mipmapped_texture(String("overlay_ibm_5175"));

    // @Temporary:
//    testing_background = load_nearest_mipmapped_texture(String("metal_slug"));
    testing_background = load_nearest_mipmapped_texture(String("the_witness")); // @Fixme: moire

//    testing_background = load_nearest_mipmapped_texture(String("Grid"));
//    testing_background = load_nearest_mipmapped_texture(String("Gray"));
//   testing_background = load_nearest_mipmapped_texture(String("Check"));

    mario_sprite       = load_nearest_mipmapped_texture(String("mario_small"));
    testing_pos        = Vector2(the_offscreen_buffer->width * .5f, the_offscreen_buffer->height * .5f);
}

void rendering_2d_right_handed()
{
    f32 w = render_target_width;
    f32 h = render_target_height;
    if (h < 1) h = 1;

    auto tm = glm::ortho(0.f, w, 0.f, h, Z_NEAR, Z_FAR);

    view_to_proj_matrix    = tm;
    world_to_view_matrix   = Matrix4(1.0);
    object_to_world_matrix = Matrix4(1.0);

    refresh_transform();
}

void rendering_2d_right_handed_unit_scale()
{
    // @Bug: This function does not take in x and y in range of [-1 to 1] like I thought!
    // @Bug: This function does not take in x and y in range of [-1 to 1] like I thought!
    // @Bug: This function does not take in x and y in range of [-1 to 1] like I thought!
    // @Bug: This function does not take in x and y in range of [-1 to 1] like I thought!

    // @Note: Cutnpaste from 'rendering_2d_right_handed'.
    f32 h = render_target_height / (f32)render_target_width;

    // This is a GL-style projection matrix mapping to [-1, 1] for x and y.
    auto tm = Matrix4(1.0);
    tm[0][0] = 2;
    tm[1][1] = 2 / h;
    tm[3][0] = -1;
    tm[3][1] = -1;

    view_to_proj_matrix    = tm;
    world_to_view_matrix   = Matrix4(1.0);
    object_to_world_matrix = Matrix4(1.0);

    refresh_transform();
}

void draw_generated_quads(Dynamic_Font *font, Vector4 color, f32 theta, f32 z_layer)
{
    rendering_2d_right_handed();

    immediate_begin();
    set_shader(shader_text);

    // glBlendFunc(GL_SRC1_COLOR, GL_ONE_MINUS_SRC1_COLOR); // @Investigate: Why don't we use this?

    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY, 1.0); // @Investigate: What is anisotropy have to do with font rendering?
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_BLEND);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    GLuint last_texture_id = 0xffffffff;

    for (auto quad : font->current_quads)
    {
        auto page = quad.glyph->page;
        auto map  = &page->texture;

        if (page->dirty)
        {
            page->dirty = false;
            auto bitmap = page->bitmap_data;

            // Regenerating the texture. Or should we not?
            if (map->id == 0xffffffff || !map->id)
            {
                // printf("Generating a texture for font page\n");
                glGenTextures(1, &map->id);
            }

            glBindTexture(GL_TEXTURE_2D, map->id);
            map->format = bitmap->XXX_format;
            update_texture_from_bitmap(map, bitmap);
        }

        if (last_texture_id == 0xffffffff || map->id != last_texture_id)
        {
            // @Speed
            // This will cause a flush for every call to draw_text.
            // But if we don't do this then we won't set the texture.
            // Need to refactor the text rendering code so that we don't have to deal with this
            immediate_flush();
            last_texture_id = map->id;
            set_texture(String("diffuse_texture"), map);
        }

        immediate_letter_quad(quad, color, theta, z_layer);
        // immediate_flush();
    }

    immediate_flush();
}

void draw_prepared_text(Dynamic_Font *font, i64 x, i64 y, Vector4 color, f32 theta, f32 z_layer)
{
    generate_quads_for_prepared_text(font, x, y);
    draw_generated_quads(font, color, theta, z_layer);
}

i64 draw_text(Dynamic_Font *font, i64 x, i64 y, String text, Vector4 color)
{
    auto width = prepare_text(font, text);
    draw_prepared_text(font, x, y, color);

    return width;
}

void immediate_image(Vector2 position, Vector2 size, Vector4 color, f32 theta = 0, bool relative_to_render_height = true, f32 z_layer = 0)
{
    auto h = Vector2(size.x*.5f, 0);
    auto v = Vector2(0, size.y*.5f);

    auto pos = position;

    if (relative_to_render_height)
    {
        h.x *= render_target_height;
        v.y *= render_target_height;

        pos *= Vector2(render_target_width, render_target_height);
    }

    if (theta)
    {
        auto radians = theta * (TAU / 360.0f);

        h = rotate(h, radians);
        v = rotate(v, radians);
    }

    auto p0 = pos - h - v;
    auto p1 = pos + h - v;
    auto p2 = pos + h + v;
    auto p3 = pos - h + v;

    immediate_quad(p0, p1, p2, p3, argb_color(color), z_layer);
}

void do_blur_from_accumulate_texture(Texture_Map *source_texture_a, Texture_Map *source_texture_b, Texture_Map *blur_texture_a, Texture_Map *blur_texture_b, f32 blur_radius)
{
    immediate_begin();

    // Using the dimension of one of the texture because they supposedly have the same dimentions!
    f32 width  = blur_texture_a->width;
    f32 height = blur_texture_a->height;

    auto p0 = Vector2(0);
    auto p1 = Vector2(width, 0);
    auto p2 = Vector2(width, height);
    auto p3 = Vector2(0,     height);

    //
    // Blurring texture_b first because blurring texture_a might depend on texture_b.
    //
    set_render_target(0, blur_texture_b);
    set_shader(shader_blur);

    auto loc = glGetUniformLocation(current_shader->program, "blur_direction");
    glUniform2f(loc, blur_radius / width, 0.0f); // Blur in the x axis.
    
    set_texture(String("diffuse_texture"), source_texture_b);
    immediate_quad(p0, p1, p2, p3, 0xffffffff);
    immediate_flush();

    //
    // Then, we blur texture_a.
    //
    set_render_target(0, blur_texture_a);
    set_shader(shader_blur);

    loc = glGetUniformLocation(current_shader->program, "blur_direction");
    glUniform2f(loc, 0.0f, blur_radius / height); // Blur in the y axis.

    set_texture(String("diffuse_texture"), source_texture_a);
    immediate_quad(p0, p1, p2, p3, 0xffffffff);
    immediate_flush();
}

void draw_game()
{

    //
    // Offscreen buffer rendering time:
    //

    set_render_target(0, the_offscreen_buffer, the_depth_buffer);
    glClearColor(.8, .4, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    immediate_begin();

    rendering_2d_right_handed();
    set_shader(shader_argb_and_texture);
    set_texture(String("diffuse_texture"), testing_background);

    auto bg_w = render_target_width;
    auto bg_h = render_target_height;
    auto bg_size_scale = 1.f;
    immediate_image(Vector2(bg_w * .5f, bg_h * .5f), Vector2(bg_size_scale * bg_w, bg_size_scale * bg_h), Vector4(1, 1, 1, 1), 0, false);

    set_shader(shader_argb_and_texture);
    set_texture(String("diffuse_texture"), mario_sprite);
    immediate_image(testing_pos, Vector2(testing_size), Vector4(1, 1, 1, 1), 0, false);

    immediate_flush();

    auto min_test_pos_x = testing_pos.x - testing_size * .5f;
    auto max_test_pos_x = testing_pos.x + testing_size * .5f;
    if (max_test_pos_x >= the_offscreen_buffer->width || min_test_pos_x <= 0)
    {
        Clamp(&testing_pos.x, min_test_pos_x, max_test_pos_x);
        testing_velocity.x *= -1;
    }

    auto min_test_pos_y = testing_pos.y - testing_size * .5f;
    auto max_test_pos_y = testing_pos.y + testing_size * .5f;
    if (max_test_pos_y >= the_offscreen_buffer->height || min_test_pos_y <= 0)
    {
        Clamp(&testing_pos.y, min_test_pos_y, max_test_pos_y);
        testing_velocity.y *= -1;
    }

    testing_velocity = unit_vector(testing_velocity);
    testing_pos     += testing_velocity * timez.current_dt * 300.f;

    //
    // Post-processing time:
    //
    immediate_begin();

    // Blurring the previous accumulate texture.
    do_blur_from_accumulate_texture(the_blur_texture_b, the_accumulation_texture_b,
                                    the_blur_texture_a, the_blur_texture_b,
                                    1.0f);

    auto obw = the_offscreen_buffer->width;
    auto obh = the_offscreen_buffer->height;

    auto ob_p0 = Vector2(0);
    auto ob_p1 = Vector2(obw, 0);
    auto ob_p2 = Vector2(obw, obh);
    auto ob_p3 = Vector2(0,   obh);

    // Update the current accumulation texture.
    {
        set_render_target(0, the_accumulation_texture_a);
        set_shader(shader_accumulate);

        set_texture(String("diffuse_texture"), the_offscreen_buffer);
        set_texture(String("blend_texture"),   the_blur_texture_a);
        immediate_quad(ob_p0, ob_p1, ob_p2, ob_p3, 0xffffffff);
    }

    // Storing a copy of the accumulation texture.
    // Because when we do the blending with the offscreen buffer, we cannot sample the texture from oursevles.
    {
        immediate_begin();

        set_render_target(0, the_accumulation_texture_b);
        set_shader(shader_copy_pixels);
        set_texture(String("diffuse_texture"), the_accumulation_texture_a);

        immediate_quad(ob_p0, ob_p1, ob_p2, ob_p3, 0xffffffff);
        immediate_flush();
    }

    // Blending the accumulation texture and the offscreen buffer.
    {
        set_render_target(0, the_accumulation_texture_a);
        set_shader(shader_blend);

        set_texture(String("diffuse_texture"), the_offscreen_buffer);
        set_texture(String("blend_texture"),   the_accumulation_texture_b);
        immediate_quad(ob_p0, ob_p1, ob_p2, ob_p3, 0xffffffff);
    }

    // Adding a slight blur.
    do_blur_from_accumulate_texture(the_blur_texture_b,         the_accumulation_texture_a,
                                    the_accumulation_texture_a, the_blur_texture_b,
                                    0.17f);

    // Creating a fully blurred version.
    do_blur_from_accumulate_texture(the_blur_texture_b, the_accumulation_texture_a,
                                    the_blur_texture_a, the_blur_texture_b,
                                    1.0f);

    //
    // Back buffer time!
    //
    immediate_begin();
    set_render_target(0, the_back_buffer);
    rendering_2d_right_handed();

    glClearColor(.1, .8, .35, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    f32 bbw = the_back_buffer->width;
    f32 bbh = the_back_buffer->height;
    glViewport(0, 0, bbw, bbh);

    // Draw the overlay background first.
    {
        set_shader(shader_argb_filmic_texture);
        set_texture(String("diffuse_texture"), overlay_background_map);

        auto p0 = Vector2(0);
        auto p1 = Vector2(bbw, 0);
        auto p2 = Vector2(bbw, bbh);
        auto p3 = Vector2(0,   bbh);

        immediate_quad(p0, p1, p2, p3, 0xffffffff);
    }

    // Draw the inner content.
    {
        // For transparency sake, we have to do the bezel reflection first, 
        // then do the crt content inside the screen.
        immediate_begin();

        auto center = Vector2(bbw * .5f, bbh * .567f);
        center.x = floorf(center.x); // Just in case...
        center.y = floorf(center.y);

        auto game_without_bezel_size = Vector2(320, 240) * 5.55f * (900 / 2160.0f); // @Incomplete: Do sizing properly!

        GLint loc;

        //
        // The bezel reflection:
        //
        set_shader(shader_bezel_reflection);

        set_texture(String("diffuse_texture"), the_blur_texture_a);
        set_texture(String("overlay_texture"), overlay_background_map);

        auto game_plus_bezel_size = Vector2(320, 244) * 6.04f * (900 / 2160.0f); // @Incomplete: Do sizing properly!

        loc = glGetUniformLocation(current_shader->program, "game_plus_bezel_size");
        glUniform2f(loc, game_plus_bezel_size.x, game_plus_bezel_size.y);

        loc = glGetUniformLocation(current_shader->program, "game_without_bezel_size");
        glUniform2f(loc, game_without_bezel_size.x, game_without_bezel_size.y);

        loc = glGetUniformLocation(current_shader->program, "bezel_center_in_overlay_space");
        glUniform2f(loc, center.x, center.y);

        // This is actually the back buffer size.
        loc = glGetUniformLocation(current_shader->program, "overlay_size");
        glUniform2f(loc, bbw, bbh);

        immediate_image(center, game_plus_bezel_size, Vector4(1, 1, 1, 1), 0, false);

        //
        // The actual game screen.
        //
        set_shader(shader_crt_filter);

        last_applied_diffuse_map = NULL; // @Fixme: @Hack We have to do this hack because otherwise, the wrong texture will be set as diffuse_texture (this is possibly because we are doing set_vertex_format_to_XCNUU every draw call).
        set_texture(String("diffuse_texture"), the_accumulation_texture_a); // This is our main game screen.
        set_texture(String("blend_texture"),   the_blur_texture_a);         // This is the blurred game screen.

        loc = glGetUniformLocation(current_shader->program, "game_time");
        glUniform1f(loc, timez.current_time);

        loc = glGetUniformLocation(current_shader->program, "render_target_size"); // :Rename
        glUniform2f(loc, game_without_bezel_size.x, game_without_bezel_size.y);

        immediate_image(center, game_without_bezel_size, Vector4(1, 1, 1, 1), 0, false);

        immediate_flush();
    }

    immediate_flush();

    draw_hud();
    assert(num_immediate_vertices == 0);
}
