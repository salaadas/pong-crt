#include "common.h"

struct Rect
{
    f32 x = 0, y = 0;
    f32 w = 0, h = 0;
};

// This is the collision info for the ball.
struct Collision_Info
{
    enum Collision_Side // Which side of the ball is being collided.
    {
        BALL_TOP = 0,
        BALL_BOTTOM,
        BALL_LEFT,
        BALL_RIGHT
    };

    enum Collision_Target
    {
        SCREEN_BORDER = 0,
        PADDLE,
        BRICKS
    };

    Collision_Side side;
    Collision_Target target;
    Vector2 point_of_contact;
};

enum class Game_State
{
    // NEW_GAME = 0, // When the game starts up.

    IN_GAME = 0,      // After the player starts moving.
};

constexpr auto DEVELOPER_MODE = true;

// These are relative to the offscreen buffer, the position is the center of the rect.
extern Vector2 paddle_position;
extern Vector2 paddle_size;
extern Vector4 paddle_color;

extern Vector4 ball_color;
extern Vector2 ball_position;
extern Vector2 ball_size;

void init_game();
void read_input();
void simulate();
