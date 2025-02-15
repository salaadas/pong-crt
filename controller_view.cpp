// @Incomplete: We should do x-ray effect shader for the parts that are coverred up too. Maybe.

#include "controller_view.h"
#include "file_utils.h"
#include "main.h"
#include "events.h"

#include <stdarg.h>
__attribute__ ((format (printf, 3, 4)))
void error(u8 *c_agent, u32 line_number, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    printf("[%s]: Error on line %u: ", c_agent, line_number);
    vprintf(fmt, args);

    va_end(args);
}

__attribute__ ((format (printf, 3, 4)))
void warning(u8 *c_agent, u32 line_number, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    printf("[%s]: Warning on line %u: ", c_agent, line_number);
    vprintf(fmt, args);

    va_end(args);
}

// @Incomplete: @Cleanup: We may want to do a lightweight lexical parser with some type info
// because it will be useful for processing config files with a lot of different types.
static bool load_attributes_for_part_info(Controller_Part_Info *info, String data, u32 line_number, u8 *c_agent)
{
    if (!data) return true; // Returning true because we still want to keep loading the file.

    if (!info)
    {
        error(c_agent, line_number, "Cannot load attributes when there is no controller part info, '%s'!\n", temp_c_string(data));
        return false;
    }

    while (data)
    {
        auto [attribute_name, rhs] = break_by_spaces(data);

        if (attribute_name == String("mesh"))
        {
            auto [mesh_name, rhs2] = break_by_spaces(rhs);
            if (!mesh_name)
            {
                error(c_agent, line_number, "Cannot load the mesh name for part '%s'!\n", temp_c_string(info->name));
                return false;
            }

            if (info->mesh_name)
            {
                warning(c_agent, line_number, "Attempting to override the mesh name '%s' inside controller part '%s' with mesh name '%s'!\n", temp_c_string(info->mesh_name), temp_c_string(info->name), temp_c_string(mesh_name));
            }

            info->mesh_name = copy_string(mesh_name);
            data = rhs2;
        }
        else if (attribute_name == String("position"))
        {
            auto success = false;
            auto [relative_position, rhs2] = string_to_vec3(rhs, &success);

            if (!success)
            {
                error(c_agent, line_number, "Failed to parse the relative position of the part '%s'!\n", temp_c_string(info->name));
                return false;
            }

            info->relative_position = relative_position;
            data = rhs2;
        }
        else if (attribute_name == String("stick_directional_tilt"))
        {
            auto success = false;
            auto [do_directional_tilt, rhs2] = string_to_bool(rhs, &success);

            if (!success)
            {
                error(c_agent, line_number, "Failed to parse '%s' of the part '%s'!\n", temp_c_string(attribute_name), temp_c_string(info->name));
                return false;
            }

            info->stick_directional_tilt = do_directional_tilt;
            data = rhs2;
        }
        else if (attribute_name == String("tilt_direction"))
        {
            auto success = false;
            auto [tilt_direction, rhs2] = string_to_vec3(rhs, &success);

            if (!success)
            {
                error(c_agent, line_number, "Failed to parse '%s' of the part '%s'!\n", temp_c_string(attribute_name), temp_c_string(info->name));
                return false;
            }

            info->do_tilt_on_button_press = true;
            info->button_tilt_direction   = tilt_direction;
            normalize_or_zero(&info->button_tilt_direction); // :ButtonTiltDirection

            data = rhs2;
        }
        else if (attribute_name == String("scale_color"))
        {
            auto success = false;
            auto [scale_color, rhs2] = string_to_vec4(rhs, &success);

            if (!success)
            {
                error(c_agent, line_number, "Failed to parse '%s' of the part '%s'!\n", temp_c_string(attribute_name), temp_c_string(info->name));
                return false;
            }

            info->scale_color = scale_color;
            data = rhs2;
        }
        else if (attribute_name == String("trigger_pull"))
        {
            auto success = false;
            auto [do_trigger_pull, rhs2] = string_to_bool(rhs, &success);

            if (!success)
            {
                error(c_agent, line_number, "Failed to parse '%s' of the part '%s'!\n", temp_c_string(attribute_name), temp_c_string(info->name));
                return false;
            }

            info->do_trigger_pull = do_trigger_pull;
            data = rhs2;
        }
        else if (attribute_name == String("type"))
        {
            auto [part_type_string, rhs2] = break_by_spaces(rhs);
            auto part_type_c_string = (const char*)temp_c_string(part_type_string);

            auto maybe_part_type = Controller_Part_Type::_from_string_nothrow(part_type_c_string);
            if (!maybe_part_type)
            {
                // Invalid enum string, we do error report.
                error(c_agent, line_number, "The part type of '%s' is not one of the type that is valid for the attribute '%s'. Valid types are:\n", part_type_c_string, temp_c_string(attribute_name));

                for (auto names : Controller_Part_Type::_names()) logprint(c_agent, "    - '%s'\n", names);
                return false;
            }
            else
            {
                // 'maybe_part_type' has the type of enum_optional, what this means is that if there is
                // no enum value with that string representation, 'maybe_part_type' will be NULL, and if 
                // there is, the value is retrived by dereferencing the 'maybe_part_type' variable.
                info->type = *maybe_part_type;
            }

            data = rhs2;
        }
        else if (attribute_name == String("-"))
        {
            data = rhs;
        }
        else
        {
            error(c_agent, line_number, "Unhandled attribute named '%s' for part info '%s'!\n", temp_c_string(attribute_name), temp_c_string(info->name));
            return false;
        }
    }

    return true;
}

static bool maybe_add_part_info(Controller *controller, Controller_Part_Info *info, u8 *c_agent)
{
    if (!controller || !info) return true;

    if (!info->mesh_name)
    {
        logprint(c_agent, "Controller part '%s' was not assigned a mesh name to it, this is required!\n", temp_c_string(info->name));
        return false;
    }

    auto mesh = catalog_find(&mesh_catalog, info->mesh_name);
    if (!mesh)
    {
        logprint(c_agent, "Could not find the mesh for the file named '%s' in controller part '%s'!\n", temp_c_string(info->mesh_name), temp_c_string(info->name));
        return false;
    }

    info->mesh = mesh;
    array_add(&controller->part_info, info);

    return true;
}

static bool read_controller_property(Controller *controller, String data, u32 line_number, u8 *c_agent)
{
    if (!controller)
    {
        error(c_agent, line_number, "Got a NULL controller variable to read data '%s' from!\n", temp_c_string(data));
        return false;
    }

    auto [property, rhs] = break_by_spaces(data);

    if (property == String("trigger_max_angle"))
    {
        auto success = false;
        auto [max_angle, rest] = string_to_float(rhs, &success);

        if (!success)
        {
            error(c_agent, line_number, "Failed to read the max angle for the triggers (ex: L2 R2)!\n");
            return false;
        }

        if (rest) warning(c_agent, line_number, "Junk at end of line '%s'!\n", temp_c_string(rest));

        controller->trigger_max_angle = max_angle;
    }
    else if (property == String("controller_tilt_max_angle"))
    {
        auto success = false;
        auto [max_angle, rest] = string_to_float(rhs, &success);

        if (!success)
        {
            error(c_agent, line_number, "Failed to read the max tilt angle for the controller!\n");
            return false;
        }

        if (rest) warning(c_agent, line_number, "Junk at end of line '%s'!\n", temp_c_string(rest));

        controller->controller_tilt_max_angle = max_angle;
    }
    else if (property == String("stick_max_angle"))
    {
        auto success = false;
        auto [max_angle, rest] = string_to_float(rhs, &success);

        if (!success)
        {
            error(c_agent, line_number, "Failed to read the max angle for the joysticks!\n");
            return false;
        }

        if (rest) warning(c_agent, line_number, "Junk at end of line '%s'!\n", temp_c_string(rest));

        controller->stick_max_angle = max_angle;
    }
    else if (property == String("show_deadzone"))
    {
        auto success = false;
        auto [show_deadzone, rest] = string_to_bool(rhs, &success);

        if (!success)
        {
            error(c_agent, line_number, "Failed to read show_deadzone value '%s'!\n", temp_c_string(rhs));
            return false;
        }

        if (rest) warning(c_agent, line_number, "Junk at end of line '%s'!\n", temp_c_string(rest));

        controller->show_deadzone = show_deadzone;
    }
    else if (property == String("deadzone_color"))
    {
        auto success = false;
        auto [deadzone_color, rest] = string_to_vec4(rhs, &success);

        if (!success)
        {
            error(c_agent, line_number, "Failed to read deadzone color '%s'!\n", temp_c_string(rhs));
            return false;
        }

        if (rest) warning(c_agent, line_number, "Junk at end of line '%s'!\n", temp_c_string(rest));

        controller->deadzone_color = deadzone_color;
    }
    else if (property == String("highlight_color"))
    {
        auto success = false;
        auto [highlight_color, rest] = string_to_vec4(rhs, &success);

        if (!success)
        {
            error(c_agent, line_number, "Failed to read highlight color '%s'!\n", temp_c_string(rhs));
            return false;
        }

        if (rest) warning(c_agent, line_number, "Junk at end of line '%s'!\n", temp_c_string(rest));

        controller->highlight_color = highlight_color;
    }
    else
    {
        error(c_agent, line_number, "Unknown property for the controller '%s'!\n", temp_c_string(property));
        return false;
    }

    return true;
}

void load_controller_info(Controller *controller, String full_path)
{
    Text_File_Handler handler;
    String agent("load_controller_info");

    start_file(&handler, full_path, agent);
    if (handler.failed) return;
    defer { deinit(&handler); };

    auto c_agent = temp_c_string(agent);

    Controller_Part_Info *current_part_info = NULL;

    auto abort = false;

    while (true)
    {
        auto [line, found] = consume_next_line(&handler);

        if (!found) break;

        if (line[0] == ':')
        {
            //
            // Start a new part.
            //
            advance(&line, 1);
            eat_spaces(&line);

            auto [part_name, rhs] = break_by_spaces(line);

            // Adding the part info to the controller.
            auto success = maybe_add_part_info(controller, current_part_info, c_agent);
            if (!success)
            {
                abort = true;
                break;
            }

            current_part_info = New<Controller_Part_Info>();
            current_part_info->name = copy_string(part_name);

            line = rhs;
        }
        else if (line[0] == '!')
        {
            //
            // Controller's properties:
            //
            advance(&line, 1);
            eat_spaces(&line);

            auto success = read_controller_property(controller, line, handler.line_number, c_agent);
            if (!success)
            {
                abort = true;
                break;
            }

            continue;
        }

        auto success = load_attributes_for_part_info(current_part_info, line, handler.line_number, c_agent);
        if (!success)
        {
            abort = true;
            break;
        }
    }

    if (!abort)
    {
        // Adding the last part if there is one.
        abort = !maybe_add_part_info(controller, current_part_info, c_agent);
    }

    if (abort)
    {
        logprint(c_agent, "Abort loading the controller info at '%s'!\n", temp_c_string(full_path));

        // @Fixme: Do memory release for the controller info.
        return;
    }
}

Controller_Part_Info *find_part_info_from_name(Controller *controller, String part_name)
{
    for (auto it : controller->part_info)
    {
        if (it->name == part_name) return it;
    }

    return NULL;
}

static void do_controller_button_press(Controller_Part_Info *part_info, bool pressed)
{
    part_info->button_press_t = pressed ? 1.f : 0.f; // @Incomplete: We want to handle analog inputs too like for the hall effects inputs, right now we are doing discrete inputs.
}

// @Fixme: When holding down a joystick and alt-tab, we are stuck at that position.
// @Fixme: When holding down a joystick and alt-tab, we are stuck at that position.
// @Fixme: When holding down a joystick and alt-tab, we are stuck at that position.
// @Fixme: When holding down a joystick and alt-tab, we are stuck at that position.
// @Fixme: When holding down a joystick and alt-tab, we are stuck at that position.
// @Fixme: When holding down a joystick and alt-tab, we are stuck at that position.
static void do_joystick_direction(Controller_Part_Info *part_info, Vector2 direction, bool pressed, bool repeat)
{
    if (!repeat) part_info->stick_direction += pressed ? direction : -direction;

    auto dir = part_info->stick_direction;
    normalize_or_zero(&dir);
    part_info->button_press_t = glm::length(dir); // @Speed: We are normalizing everything...
}

static void reset_all_joysticks(Controller *controller)
{
/*
    for (auto it : controller->part_info)
    {
        if (it->type != (+Controller_Part_Type::JOYSTICK)) continue;
        it->stick_direction = Vector2(0);
    }
*/
}

static inline Quaternion rotate_in_3_axes(Vector3 direction_of_rotation_in_xyz, f32 amount)
{
    Quaternion ori_x, ori_y, ori_z;
    auto amount_of_rotation_in_xyz = direction_of_rotation_in_xyz * amount;

    get_ori_from_rot(&ori_x, Vector3(1, 0, 0), -amount_of_rotation_in_xyz.x); // @Note: Flipping the x because I think it is more natural to think that positive will rotate up while negative will rotate down here...
    get_ori_from_rot(&ori_y, Vector3(0, 1, 0),  amount_of_rotation_in_xyz.y);
    get_ori_from_rot(&ori_z, Vector3(0, 0, 1),  amount_of_rotation_in_xyz.z);

    return ori_z * ori_y * ori_x;
}

static void do_tilting_from_info(Controller_Part_Info *part_info, Controller *controller)
{
    //
    // Do the tilting for the whole controller.
    //
    if (part_info->do_tilt_on_button_press)
    {
        auto tilt_direction = part_info->button_tilt_direction;
        tilt_direction *= part_info->button_press_t;

        if (tilt_direction.x || tilt_direction.y || tilt_direction.z)
        {
            // @Note: :ButtonTiltDirection We assume that the tilt direction is normalized when loaded already.
            const auto CONTROLLER_TURN_ANGLE = controller->controller_tilt_max_angle;
            auto rotation_xyz = rotate_in_3_axes(tilt_direction, CONTROLLER_TURN_ANGLE);

            controller->orientation = rotation_xyz * controller->orientation;
        }
    }

    switch (part_info->type)
    {
        case Controller_Part_Type::REAR_TRIGGER: {
            //
            // Do the tilting for the rear triggers.
            //
            auto ori = Quaternion(1, 0, 0, 0); // Orientation of the trigger.

            if (part_info->do_trigger_pull)
            {
                auto pull_t = part_info->button_press_t;
                Clamp(&pull_t, 0.f, 1.f);
            
                if (pull_t > 0)
                {
                    get_ori_from_rot(&ori, Vector3(1, 0, 0), -pull_t * controller->trigger_max_angle);
                }
            }

            part_info->orientation = ori;
        } break;
        case Controller_Part_Type::JOYSTICK: {
            //
            // Do the tilting for the joysticks.
            //
            auto dir = part_info->stick_direction;
            normalize_or_zero(&dir);

            auto ori = Quaternion(1, 0, 0, 0); // Orientation of the joystick.

            if (dir.x || dir.y)
            {
                const auto STICK_MAX_ANGLE = controller->stick_max_angle;
                ori = rotate_in_3_axes(Vector3(dir.y, 0, dir.x), STICK_MAX_ANGLE);

                if (part_info->stick_directional_tilt)
                {
                    // Tilt the whole controller to whichever direction we are moving.
                    controller->orientation = ori * controller->orientation;
                }
            }

            part_info->orientation = ori;
        } break;
    }
}

void simulate_controller_from_input(Controller *controller)
{
    // @Incomplete: Right now, this function is simulating the keyboard as a controller.
    // This is useful when the user does not have a controller but still wants to visualize.
    // However, we also need to consider the actual controller inputs too.

    // @Speed: find_part_info_from_name is looping through all the parts for each find.
    auto left_button  = find_part_info_from_name(controller, String("dpad_left"));
    auto right_button = find_part_info_from_name(controller, String("dpad_right"));
    auto up_button    = find_part_info_from_name(controller, String("dpad_up"));
    auto down_button  = find_part_info_from_name(controller, String("dpad_down"));

    auto a_button = find_part_info_from_name(controller, String("button_a"));
    auto b_button = find_part_info_from_name(controller, String("button_b"));
    auto x_button = find_part_info_from_name(controller, String("button_x"));
    auto y_button = find_part_info_from_name(controller, String("button_y"));

    auto l1_button = find_part_info_from_name(controller, String("L1"));
    auto l2_button = find_part_info_from_name(controller, String("L2"));
    auto r1_button = find_part_info_from_name(controller, String("R1"));
    auto r2_button = find_part_info_from_name(controller, String("R2"));

    auto left_stick  = find_part_info_from_name(controller, String("left_joystick"));
    auto right_stick = find_part_info_from_name(controller, String("right_joystick"));

    reset_all_joysticks(controller);

    // @Cleanup: @Incomplete: Make config file for what key maps to what buttons.
    for (auto event : events_this_frame)
    {
        if (event.type != EVENT_KEYBOARD) continue; // @Incomplete: Only doing keyboard inputs for now.

        auto pressed = event.key_pressed;
        auto repeat  = event.repeat;

        switch (event.key_code)
        {
            case Key_Code::CODE_A: {
                do_controller_button_press(left_button, pressed);
                do_joystick_direction(left_stick, Vector2(-1, 0), pressed, repeat);
            } break;
            case Key_Code::CODE_W: {
                do_controller_button_press(up_button, pressed);
                do_joystick_direction(left_stick, Vector2(0, 1), pressed, repeat);
            } break;
            case Key_Code::CODE_D: {
                do_controller_button_press(right_button, pressed);
                do_joystick_direction(left_stick, Vector2(1, 0), pressed, repeat);
            } break;
            case Key_Code::CODE_S: {
                do_controller_button_press(down_button, pressed);
                do_joystick_direction(left_stick, Vector2(0, -1), pressed, repeat);
            } break;

            case Key_Code::CODE_J:         do_controller_button_press(a_button, pressed); break;
            case Key_Code::CODE_K:         do_controller_button_press(b_button, pressed); break;
            case Key_Code::CODE_L:         do_controller_button_press(x_button, pressed); break;
            case Key_Code::CODE_SEMICOLON: do_controller_button_press(y_button, pressed); break;

            case Key_Code::CODE_U: do_controller_button_press(l1_button, pressed); break;
            case Key_Code::CODE_I: do_controller_button_press(l2_button, pressed); break;
            case Key_Code::CODE_O: do_controller_button_press(r1_button, pressed); break;
            case Key_Code::CODE_P: do_controller_button_press(r2_button, pressed); break;

            case Key_Code::CODE_ARROW_UP:    do_joystick_direction(right_stick, Vector2(0,  1), pressed, repeat); break;
            case Key_Code::CODE_ARROW_DOWN:  do_joystick_direction(right_stick, Vector2(0, -1), pressed, repeat); break;
            case Key_Code::CODE_ARROW_LEFT:  do_joystick_direction(right_stick, Vector2(-1, 0), pressed, repeat); break;
            case Key_Code::CODE_ARROW_RIGHT: do_joystick_direction(right_stick, Vector2( 1, 0), pressed, repeat); break;
        }
    }

    controller->orientation = Quaternion(1, 0, 0, 0); // Reset the orientation every frame so that we don't have to worry about rotating it back.

    for (auto info : controller->part_info)
    {
        do_tilting_from_info(info, controller);
    }
}
