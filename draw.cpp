// Alt-Tab when game first boot up crashes because of font.

#include "draw.h"
#include "main.h"
#include "opengl.h"
#include "time_info.h"
#include "hud.h"
#include "pong.h"
#include "controller_view.h"

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
Shader *shader_basic_3d;

Texture_Map *overlay_background_map;

Controller xbox_controller;

/*
constexpr auto MESH_LIST_COUNT = 15; // @Temporary:
String xbox_mesh_list[] = {
    String("test_top_shell"),
    String("test_bottom_shell"),

    String("test_left_joystick"),

    String("test_dpad_left"),
    String("test_dpad_right"),
    String("test_dpad_up"),
    String("test_dpad_down"),

    String("test_button_a"),
    String("test_button_b"),
    String("test_button_x"),
    String("test_button_y"),

    String("test_L1"),
    String("test_L2"),
    String("test_R1"),
    String("test_R2"),
};

Vector3 part_position_list[] = {
    Vector3(0),
    Vector3(0),

    Vector3(-.53f, -.21f, .26f),

    Vector3(0),
    Vector3(0),
    Vector3(0),
    Vector3(0),

    Vector3(0),
    Vector3(0),
    Vector3(0),
    Vector3(0),

    Vector3(-.48f, -.16f, .61f), // L1
    Vector3(-.59f, -.08f, .52f), // L2
    Vector3(.48f, -.16f, .61f),  // R1
    Vector3(.59f, -.08f, .52f),  // R2
};
*/

// Axis for 3D rendering.
constexpr auto AXIS_RIGHT   = Vector3(1, 0, 0);
constexpr auto AXIS_FORWARD = Vector3(0, 1, 0);
constexpr auto AXIS_UP      = Vector3(0, 0, 1);

//
// These render target textures are used in post-processing.
// Right now, they are used for the ghosting and gaussian blur effects.
//
Texture_Map *the_accumulation_texture_a;
Texture_Map *the_accumulation_texture_b;
Texture_Map *the_blur_texture_a;
Texture_Map *the_blur_texture_b;

//
// Game textures:
//
Texture_Map *space_background_map;

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

    shader_basic_3d         = find_shader(String("basic_3d"));

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

    //
    // Now we do the game textures.
    //
    space_background_map = load_nearest_mipmapped_texture(String("space_background"));

    // @Temporary
    load_controller_info(&xbox_controller, String("data/controllers_info/xbox_one.controller_info"));
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
        // The size is always relative to height.
        h.x *= render_target_width;
        v.y *= render_target_height;

        // The position, however, is relative to width in x, and relative to height in y.
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

void set_object_to_world_matrix(Vector3 pos, Quaternion ori, f32 scale)
{
    auto r = Matrix4(1.0f);
    set_rotation(&r, ori);

    auto m = Matrix4(1.0f);

    m[3][0] = pos.x;
    m[3][1] = pos.y;
    m[3][2] = pos.z;

    m[0][0] = scale;
    m[1][1] = scale;
    m[2][2] = scale;

    object_to_world_matrix = m * r;
    refresh_transform();
}

void set_material_color(Vector4 fcolor)
{
    // This function is not so great, eventually we want a more sophisticated material pipeline,
    // but for this game, that is kind of out-of-scope. Anyways, it is worth checking out later.
    if (current_shader != shader_basic_3d) return;

    auto loc = glGetUniformLocation(current_shader->program, "material_color");
    if (loc < 0)
    {
        DumpGLErrors("draw:set_material_color");
        return;
    }

    glUniform4fv(loc, 1, reinterpret_cast<f32*>(&fcolor));
}

void draw_mesh(Triangle_Mesh *mesh, Vector3 position, Quaternion ori, f32 mesh_scale, Vector4 *scale_color = NULL)
{
    set_shader(shader_basic_3d);

    set_object_to_world_matrix(position, ori, mesh_scale);

    glBindBuffer(GL_ARRAY_BUFFER,         mesh->vertex_vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->index_vbo);

    // Because we are binding a different vbo, we have to set the XCNUU format again.
    set_vertex_format_to_XCNUU(current_shader);

    for (auto list_index = 0; list_index < mesh->triangle_list_info.count; ++list_index)
    {
        auto list = &mesh->triangle_list_info[list_index];

        Texture_Map *albedo_map = NULL;
        auto fcolor = Vector4(1);

        if (list->material_index >= 0)
        {
            auto render_mat = mesh->material_info[list->material_index];
            albedo_map = render_mat.albedo_map;
            fcolor     = render_mat.albedo_color;
        }

        if (!albedo_map) albedo_map = the_white_texture;
        set_texture(String("diffuse_texture"), albedo_map);

        fcolor.w = 1.0f;
        if (scale_color)
        {
            fcolor.x *= scale_color->x;
            fcolor.y *= scale_color->y;
            fcolor.z *= scale_color->z;
            fcolor.w *= scale_color->w;
        }

        set_material_color(fcolor);

        auto offset = list->first_index * sizeof(mesh->index_array[0]);
        glDrawElements(GL_TRIANGLES, list->num_indices, GL_UNSIGNED_INT, reinterpret_cast<void*>(offset));
    }

    object_to_world_matrix = Matrix4(1.0f); // Set it back to be nice? I dunno man.
    refresh_transform();
}

Matrix4 make_projection_matrix(f32 z_near, f32 z_far, f32 aspect_ratio, f32 fov_vertical) // @Cleanup: Make fov_vertical take radians.
{
    const f32 DEG2RAD = acosf(-1.0f) / 180.0f;
    f32 tangent = tanf(fov_vertical/2.0f * DEG2RAD); // Tangent of half fov vertical
    f32 top     = z_near * tangent;                  // Half height of near plane
    f32 right   = top * aspect_ratio;                // Half width of near plane

    auto proj   = Matrix4(1.0f);
    proj[0][0]  = z_near / right;
    proj[1][1]  = z_near / top;
    proj[2][2]  = -(z_far + z_near) / (z_far - z_near);
    proj[2][3]  = -1;
    proj[3][2]  = -(2 * z_far * z_near) / (z_far - z_near);
    proj[3][3]  = 0;

    return proj;
}

Matrix4 make_look_at_matrix(Vector3 eye_pos, Vector3 target_point, Vector3 up)
{
    auto direction = glm::normalize(target_point - eye_pos);
    up             = glm::normalize(up);
    auto right     = glm::normalize(glm::cross(direction, up));
    up             = glm::cross(right, direction);
    
    // auto right     = glm::normalize(glm::cross(direction, up));
    // up             = glm::normalize(glm::cross(right, direction));

    // auto direction = _direction;
    // normalize_or_zero(&direction);
    
    // Vector3 right = glm::cross(_direction, up);
    // normalize_or_zero(&right);

    // up = glm::cross(right, direction);
    // normalize_or_zero(&up);

    Matrix4 M = Matrix4(1.0f);
    M[0][0] = right.x;
    M[1][0] = right.y;
    M[2][0] = right.z;

    M[0][1] = up.x;
    M[1][1] = up.y;
    M[2][1] = up.z;

    M[0][2] = -direction.x;
    M[1][2] = -direction.y;
    M[2][2] = -direction.z;

    M[3][0] = -glm::dot(right, eye_pos);
    M[3][1] = -glm::dot(up, eye_pos);
    M[3][2] =  glm::dot(direction, eye_pos);

    return M;
}

Quaternion get_orientation_from_angles(f32 theta, f32 phi, f32 rho = 0)
{
    Quaternion rotation_theta = Quaternion(1, 0, 0, 0);
    Quaternion rotation_phi   = Quaternion(1, 0, 0, 0);
    Quaternion rotation_rho   = Quaternion(1, 0, 0, 0);

    get_ori_from_rot(&rotation_theta, Vector3(0, 0, 1), theta);

    auto new_y = AXIS_RIGHT;
    new_y = rotate(new_y, rotation_theta);

    get_ori_from_rot(&rotation_phi, new_y, phi);
    auto new_forward = rotate(rotate(AXIS_FORWARD, rotation_theta), rotation_phi);
    
    Quaternion result;
    if (rho)
    {
        get_ori_from_rot(&rotation_rho, new_forward, rho);
        result = rotation_rho * rotation_phi * rotation_theta;
    }
    else
    {
        result = rotation_phi * rotation_theta;
    }

    return result;
}

void set_matrix_for_3d()
{
    f32 w = render_target_width;
    f32 h = render_target_height;
    if (h < 1) h = 1;

    // @Hardcoded: Variables for the camera view.
    constexpr auto vFOV   = 40.f * (9/16.0f);
    constexpr auto z_near = 0.1f;
    constexpr auto z_far  = 200.0f;

//    auto camera_position = Vector3(0, -5, 0);
    auto camera_position = Vector3(0, -3, 3);

    auto camera_theta = 0;
//    auto camera_phi   = 0;
    auto camera_phi   = -M_PI / 5.0f;
    auto camera_orientation = get_orientation_from_angles(camera_theta, camera_phi);

    auto viewpoint = camera_position;
    auto look_dir  = rotate(AXIS_FORWARD, camera_orientation);
    auto view      = make_look_at_matrix(viewpoint, look_dir, AXIS_UP);
    auto proj      = make_projection_matrix(z_near, z_far, w/h, vFOV);

/*
    // auto view_1      = glm::lookAt(viewpoint, glm::normalize(look_dir), AXIS_UP);

    // printf("View A is:\n");
    // print_cmaj_as_rmaj(view);
    // printf("View B is:\n");
    // print_cmaj_as_rmaj(view_1);
    // newline();
*/

    view_to_proj_matrix    = proj;
    world_to_view_matrix   = view;
    object_to_world_matrix = Matrix4(1.0f);

    refresh_transform();
}

void update_controller_visuals(Controller *controller)
{
    auto dt = timez.ui_dt;

    for (auto info : controller->part_info)
    {
        // Update the highlight of the buttons
        if (info->button_press_t > 0.0f)
        {
            info->highlight_t = move_toward(info->highlight_t, 1.0f, dt * controller->highlight_fade_in_speed);
            info->highlight_duration += dt;
        }
        else
        {
            info->highlight_t = move_toward(info->highlight_t, 0.0f, dt * controller->highlight_fade_out_speed);
            if (!info->highlight_t) info->highlight_duration = 0;
        }
    }
}

Vector4 controller_get_highlight_color(Controller_Part_Info *info, Controller *controller)
{
    auto lerp_factor = 0.0f;

    if (info->highlight_t)
    {
        auto blend_factor = sinf(TAU * .5f * info->highlight_duration);
        blend_factor += 1.0f;
        blend_factor *= .5f;
        Clamp(&blend_factor, 0.f, 1.f);

        lerp_factor = (.6f * info->highlight_t) + (info->highlight_t * blend_factor * .4f);
    }

    auto color = lerp(info->scale_color, controller->highlight_color, lerp_factor);
    return color;
}

void draw_controller(Controller *controller)
{
    update_controller_visuals(controller);

    auto scale = 1.0f;
    for (auto it : controller->part_info)
    {
        auto mesh        = it->mesh;
        auto scale_color = controller_get_highlight_color(it, controller);

        auto pos = controller->position + it->relative_position;
        auto ori = controller->orientation;

        if (it->relative_position.x || it->relative_position.y || it->relative_position.z)
        {
            auto rotated_pos = controller->position + rotate(it->relative_position, ori);
            pos = rotated_pos;
        }

        ori = ori * it->orientation;
        draw_mesh(mesh, pos, ori, scale, &scale_color);
    }
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

    set_matrix_for_3d();
    draw_controller(&xbox_controller);

/*
    rendering_2d_right_handed();
    set_shader(shader_argb_and_texture);
    set_texture(String("diffuse_texture"), space_background_map);

    // Drawing the background.
    auto bg_w = render_target_width;
    auto bg_h = render_target_height;
    auto bg_k = .4f;
    immediate_image(Vector2(bg_w * .5f, bg_h * .5f), Vector2(bg_w, bg_h), Vector4(bg_k, bg_k, bg_k, 1), 0, false);

    // Drawing the player.
    set_shader(shader_argb_no_texture);
    immediate_image(paddle_position, paddle_size, paddle_color);

    immediate_image(ball_position, ball_size, ball_color);

    immediate_flush();
*/

    //
    // Post-processing time:
    //
    immediate_begin();
    rendering_2d_right_handed();

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
    glViewport(0, 0, bbw, bbh); // nocheckin.

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
