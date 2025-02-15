#pragma once

#include "common.h"

struct Triangle_Mesh;

#include "meta_enums.h"
BETTER_ENUM(Controller_Part_Type, u8,
            BUTTON = 0,
            REAR_TRIGGER,
            JOYSTICK)

struct Controller_Part_Info
{
    String name;

    Controller_Part_Type type = Controller_Part_Type::BUTTON;

    Quaternion orientation = Quaternion(1, 0, 0, 0);

    String mesh_name;
    Triangle_Mesh *mesh = NULL;

    Vector3 relative_position; // Position of this part relative to the controller's root bone.
    Vector4 scale_color = Vector4(1, 1, 1, 1);

    // For regular buttons and the triggers:
    f32 button_press_t = 0.f;
    bool do_tilt_on_button_press  = false; // This is true only when we set the field in the *.controller_info file.
    Vector3 button_tilt_direction = Vector3(0); // The direction to tilt the controller when press the button.

    // For L2 and R2 triggers, you may want to set this to 'true'.
    bool do_trigger_pull = false;

    // For joysticks:
    Vector2 stick_direction; // If this part is a joystick, this is the analog value of the stick.
    bool stick_directional_tilt = false; // Tilting the controller based on the stick_direction.

    f32 highlight_t = 0.f;
    f32 highlight_duration = 0.f;

// // @Incomplete: For touchpad like the those in the Dualshock controllers:
//    Vector2 touch_region_size;
//    Vector2 touch_region_position;
};

struct Controller
{
    RArr<Controller_Part_Info*> part_info;

    Vector3    position; // Position of the controller in world space.
    Quaternion orientation = Quaternion(1, 0, 0, 0);

    //
    // Properties for the parts within the controller:
    //
    f32 trigger_max_angle   = 0.0f;
    f32 stick_max_angle     = 0.0f;
    bool show_deadzone      = false;
    Vector4 deadzone_color  = Vector4(1, 0, 0, 1);

    Vector4 highlight_color = Vector4(0, 1, 0, 1);
    f32 highlight_fade_in_speed  = 12.0f; // Higher is faster; Example: 12 == 1/12 second for full fade in.
    f32 highlight_fade_out_speed = 3.0f;

    f32 controller_tilt_max_angle = 0.0f;
};

void load_controller_info(Controller *controller, String full_path);
Controller_Part_Info *find_part_info_from_name(Controller *controller, String part_name);

void simulate_controller_from_input(Controller *controller);
