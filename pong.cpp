// https://codeincomplete.com/articles/collision-detection-in-breakout/

// Inspirations:
// - https://www.lexaloffle.com/bbs/?pid=53977
// - Akranoid
// - Ricochet Xtreme
// - Kirby's Block Ball
// - DX-Ball
// - Super breakout
// - LBreakout2

#include "pong.h"
#include "main.h"
#include "events.h"
#include "time_info.h"

// These are relative to the offscreen buffer, the position is the center of the rect.
Vector2 paddle_position;
Vector2 paddle_size;
Vector4 paddle_color;

Vector4 ball_color;
Vector2 ball_position;
Vector2 ball_size;
Vector2 ball_direction;
bool    ball_stuck_to_paddle;
constexpr auto BALL_SPEED = .5f;

constexpr auto PADDLE_VELOCITY = .6f;

static Rect screen_rect = {.x = 1.f, .y = 1.f, .w = -1.f, .h = -1.f}; // xy is the top-right of the rect.

static u32 key_left;
static u32 key_right;

static Game_State current_game_state;

void init_game()
{
    current_game_state = Game_State::IN_GAME;
    // Player.
    auto ppos = &paddle_position;
    ppos->x   = .5f;
    ppos->y   = .2f;

    auto psize = &paddle_size;
    psize->x   = .3f;
    psize->y   = .15f;

    paddle_color = Vector4(.8f, .6f, .9f, 1);

    // Ball.
    ball_stuck_to_paddle = true;
    ball_direction = Vector2(1, 1); // Starts with a ball that wants to launch to top-right.
    ball_color     = Vector4(.85f, .4f, .2f, 1);
    ball_position  = Vector2(ppos->x, ppos->y + psize->y*.5f + ball_size.y*.5f); // Start on top of the paddle.

    auto ball_diameter = psize->y * .85f;
    ball_size.x = 1/OFFSCREEN_BUFFER_ASPECT * ball_diameter;
    ball_size.y = ball_diameter;
}

void linear_move(Vector2 *position, Vector2 velocity, f32 dt)
{
    position->x += velocity.x * dt;
    position->y += velocity.y * dt;
}

inline
f32 cross_2d(Vector2 p1, Vector2 p2)
{
    // This is the same as the determinant of 2x2 matrix.
    return (p1.x * p2.y) - (p2.x * p1.y);
}

bool intersection_of_2_segments(Vector2 line_1_start, Vector2 line_1_end, Vector2 line_2_start, Vector2 line_2_end, Vector2 *point_of_intersection, bool debug = false)
{
    // https://stackoverflow.com/a/565282

    auto delta_1 = line_1_end - line_1_start;
    auto delta_2 = line_2_end - line_2_start;

    auto slope_difference = cross_2d(delta_1, delta_2);

    // If the slope of the 2 lines are the same, then they are definitely not intersecting.
    constexpr auto EPSILON =.000001f;
    if (fabsf(slope_difference) < EPSILON) return false;

    //
    // We now find the scalar 's', and 't' such that:
    //    lerp(line_1_start, line_1_end, s) = lerp(line_2_start, line_2_end, t)
    // => line_1_start + s*delta_1 = line_2_start + t*delta_2
    //
    // ** FINDING 's' **
    // Do cross product on both sides with 'delta_2':
    // => (line_1_start + s*delta_1) x delta_2 = (line_2_start + t*delta_2) x delta_2
    // 
    // We know that 'delta_2 x delta_2' is 0 because the cross product is proportional to sin() of
    // 2 vectors. So,
    // => line_1_start x delta_2 + s*(delta_1 x delta_2) = line_2_start x delta_2
    // => s*(delta_1 x delta_2) = line_2_start x delta_2 - line_1_start x delta_2
    // => s = (line_2_start x delta_2 - line_1_start x delta_2) / (delta_1 x delta_2)
    // => s = ((line_2_start - line_1_start) x delta_2) / (delta_1 x delta_2)
    //
    // ** FINDING 't' **
    // Similarly,
    //     t = ((line_1_start - line_2_start) x delta_1) / (delta_2 x delta_1)
    // <=> t = ((line_2_start - line_1_start) x delta_1) / (delta_1 x delta_2)      [Switch the cross product order, switch the signs.]

    //
    // It is @Important to note that, because 's' and 't' are lerp factors for line_1 and line_2,
    // in order to know that the lines intersects, these inequalities must hold true:
    // 0 <= 's' <= 1
    // 0 <= 't' <= 1
    //

    auto line_start_difference = line_2_start - line_1_start;
    auto s = cross_2d(line_start_difference, delta_2) / slope_difference;

    if (s >= 0 && s <= 1)
    {
        auto t = cross_2d(line_start_difference, delta_1) / slope_difference;
        if (t >= 0 && t <= 1)
        {
            assert(point_of_intersection != NULL);
            *point_of_intersection = line_1_start + s * delta_1; // Using this instead of lerp() because we want to use the calculated variables.
            return true;
        }
    }

    return false;
}

bool /*collided*/ check_if_ball_collides_with(RArr<Collision_Info> *info_array, Collision_Info::Collision_Target target_type, Rect target_rect, Vector2 old_position, Vector2 size, Vector2 new_position)
{
    bool collided = false;
    Collision_Info info = {
        .target = target_type,
    };

    auto ball_half_size = size * .5f;

    {
        auto start_x = target_rect.x - ball_half_size.x;
        auto end_x   = target_rect.x + target_rect.w + ball_half_size.x;

        if (old_position.y < new_position.y)
        {
            // Moving up, so we target the bottom of the rect.
            info.side = Collision_Info::BALL_TOP;

            // Subtracting half size of the ball because the ball's position is at the center,
            // so we want the boundaries of the target shifted out towards the ball by half of its size.
            // We do this for all the other collisions check too.
            auto target_y = target_rect.y - ball_half_size.y;
            collided = intersection_of_2_segments(old_position, new_position, Vector2(start_x, target_y), Vector2(end_x, target_y), &info.point_of_contact);
        }
        else
        {
            // Moving down, so we target the top of the rect.
            info.side = Collision_Info::BALL_BOTTOM;

            auto target_y = target_rect.y + target_rect.h + ball_half_size.y;
            collided = intersection_of_2_segments(old_position, new_position, Vector2(start_x, target_y), Vector2(end_x, target_y), &info.point_of_contact, true);
        }
    }

    if (!collided)
    {
        auto start_y = target_rect.y - ball_half_size.y;
        auto end_y   = target_rect.y + target_rect.h + ball_half_size.y;

        if (old_position.x < new_position.x)
        {
            // Moving right, so we target the left of the rect.
            info.side = Collision_Info::BALL_RIGHT;

            auto target_x = target_rect.x - ball_half_size.x;
            collided = intersection_of_2_segments(old_position, new_position, Vector2(target_x, start_y), Vector2(target_x, end_y), &info.point_of_contact);
        }
        else
        {
            // Moving left, so we target the right of the rect.
            info.side = Collision_Info::BALL_LEFT;

            auto target_x = target_rect.x + target_rect.w + ball_half_size.x;
            collided = intersection_of_2_segments(old_position, new_position, Vector2(target_x, start_y), Vector2(target_x, end_y), &info.point_of_contact);
        }
    }

    if (collided) array_add(info_array, info);
    return collided;
}

Rect rect_from_center_and_size(Vector2 center_position, Vector2 size)
{
    Rect result = {
        .x = center_position.x - size.x * .5f,
        .y = center_position.y - size.y * .5f,
        .w = size.x,
        .h = size.y
    };
    return result;
}

void resolve_ball_collision(Vector2 *ball_position, Vector2 *ball_direction, Collision_Info info)
{
    //
    // Put the ball at the contact point and change the ball direction according
    // to what target the ball hitted.
    //
    *ball_position = info.point_of_contact;

    switch (info.side)
    {
        case Collision_Info::BALL_LEFT:
        case Collision_Info::BALL_RIGHT: {
            ball_direction->x *= -1;
        } break;

        case Collision_Info::BALL_TOP:
        case Collision_Info::BALL_BOTTOM: {
            ball_direction->y *= -1;
        } break;
    }
}

void update_ball_position(f32 dt)
{
    if (ball_stuck_to_paddle)
    {
        // Moving the ball with the paddle.
        auto new_position = paddle_position;
        new_position.y += paddle_size.y * .5f + ball_size.y * .5f;

        ball_position = new_position;
        return;
    }

    auto new_position  = ball_position; // We test on the new position before assigning it to the ball position.
    auto ball_velocity = ball_direction * BALL_SPEED;
    linear_move(&new_position, ball_velocity, dt);
    
    RArr<Collision_Info> collisions_this_update;
    collisions_this_update.allocator = {global_context.temporary_storage, __temporary_allocator};

    // Collision with the screen border.
    check_if_ball_collides_with(&collisions_this_update, Collision_Info::SCREEN_BORDER, screen_rect, ball_position, ball_size, new_position);

    // Collision with the paddle.
    auto paddle_rect = rect_from_center_and_size(paddle_position, paddle_size);
    check_if_ball_collides_with(&collisions_this_update, Collision_Info::PADDLE, paddle_rect, ball_position, ball_size, new_position);

    if (collisions_this_update)
    {
        // @Incomplete: Sort the collisions using the 's' and 't' factor and pick the closest one
        // instead of choosing the first one always.
        auto info = collisions_this_update[0];
        resolve_ball_collision(&ball_position, &ball_direction, info);
    }
    else
    {
        ball_position = new_position;
    }

    ball_direction = unit_vector(ball_direction);
}

bool is_overlap(Rect a, Rect b)
{
    if (a.x > b.x + b.w) return false;
    if (a.y > b.y + b.h) return false;
    if (a.x + a.w < b.x) return false;
    if (a.y + a.h < b.y) return false;
    return true;
}

void ball_check_for_paddle_ram(f32 dt)
{
    if (ball_stuck_to_paddle) return;

    auto paddle_rect    = rect_from_center_and_size(paddle_position, paddle_size);
    auto ball_rect      = rect_from_center_and_size(ball_position, ball_size);
    auto ball_half_size = ball_size * .5f;

    if ((ball_position.y - ball_half_size.y) >= paddle_rect.y + paddle_rect.h) return; // Ball is still above the paddle.
    if (!is_overlap(paddle_rect, ball_rect)) return;

    auto paddle_dx   = key_left ? -1 : 1;
    ball_direction.x = fabsf(ball_direction.x) * paddle_dx;
    // @Incomplete: Add extra speed to the ball when rammed.

    if (ball_position.x < paddle_position.x) ball_position.x = paddle_rect.x - ball_half_size.x;
    else ball_position.x = paddle_rect.x + paddle_rect.w + ball_half_size.x;

    Collision_Info info = {
        .side   = key_left ? Collision_Info::BALL_RIGHT : Collision_Info::BALL_LEFT,
        .target = Collision_Info::PADDLE,
        .point_of_contact = ball_position, // We already do the change in position of the ball.
    };
//     resolve_ball_collision(&ball_position, &ball_direction, info);
}

void simulate()
{
    auto dt = timez.current_dt;

    auto paddle_velocity = Vector2(0);

    if (key_left)
    {
        paddle_velocity.x -= 1;
        if (ball_stuck_to_paddle) ball_direction.x = abs(ball_direction.x) * -1;
    }

    if (key_right)
    {
        paddle_velocity.x += 1;
        if (ball_stuck_to_paddle) ball_direction.x = abs(ball_direction.x) * 1;
    }

    if (paddle_velocity.x || paddle_velocity.y)
    {
        paddle_velocity = unit_vector(paddle_velocity);
        paddle_velocity *= PADDLE_VELOCITY;

        linear_move(&paddle_position, paddle_velocity, dt);

        auto x0 = (paddle_size.x * .5f) + 0.01f; // @Hardcoded margin value.
        auto x1 = 1 - x0;

        Clamp(&paddle_position.x, x0, x1);
    }

    // Update the ball and resolve any bounce if it hits anything.
    update_ball_position(dt);
    if (key_left || key_right) // Only check for ramming when we move left or right.
    {
        ball_check_for_paddle_ram(dt);
    }
}

void launch_ball_from_paddle()
{
    if (!ball_stuck_to_paddle) return;
    ball_stuck_to_paddle = false;

    ball_direction = unit_vector(ball_direction);
}

#include "draw.h" // @Temporary for 'xbox_controller'.

void read_input()
{
    simulate_controller_from_input(&xbox_controller);

    for (auto event : events_this_frame)
    {
        if (event.type != EVENT_KEYBOARD) continue;

        auto key     = event.key_code;
        auto pressed = event.key_pressed;

        switch (key)
        {
            case Key_Code::CODE_ARROW_LEFT:  key_left  = pressed; break;
            case Key_Code::CODE_ARROW_RIGHT: key_right = pressed; break;
            case Key_Code::CODE_SPACEBAR: {
                if (pressed) launch_ball_from_paddle();
            } break;
        }
    }
}
