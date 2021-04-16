#include <math.hpp>
#include <terminal.hpp>

#include <algorithm>
#include <array>
#include <functional>
#include <ranges>

// clang-format off
constexpr auto maze_height = 20;
constexpr auto maze = std::array<const wchar_t*, maze_height>{
    L"+++++++++++++++++++++",
    L"+                   +",
    L"+              ++++ +",
    L"+      +++++     ++ +",
    L"+      +++++     +  +",
    L"+      +++++   +++ ++",
    L"+      +++++   +    +",
    L"+      +++++   + ++++",
    L"+              + ++++",
    L"+                   +",
    L"+                   +",
    L"+++++ ++++++ ++++++ +",
    L"+++++ ++++++ ++++++ +",
    L"+                   +",
    L"+               +   +",
    L"+     +             +",
    L"+  +           +    +",
    L"+      +   +        +",
    L"+                +  +",
    L"+++++++++++++++++++++",
};
// clang-format on

constexpr auto is_wall(const vec2i& pos) { return maze[pos.y][pos.x] == L'+'; }
constexpr auto is_wall(const vec2f& pos) { return is_wall(to_vec2i(pos)); }

//  The coordinates of each position/vector in the dda algorithm can be represented
// by the grid coordinate (i.e. snapped to integer value) and the accompanying distance
// along the ray that is being cast.
struct dda_coord
{
    int on_grid;
    float distance;

    // Two dda coordinates can be added simply by adding their value on the grid and
    // adding the distances along the ray
    constexpr dda_coord& operator+=(const dda_coord& other)
    {
        on_grid += other.on_grid;
        distance += other.distance;
        return *this;
    }
};

//  To cast a ray we start with the initial x and y coordinates and the step in x and y
// respectively. As long as the distance along the ray in the x-direction is shorter
// than that travelled in the y direction, then we increment x by the x-step. Otherwise
// we increment y by the y-step. When we hit a wall, we're finished.
//
// Note: we're assuming a closed map here to ensure that the ray actually hits something
// and the while loop terminates.
constexpr auto cast_ray(dda_coord x, dda_coord y, const dda_coord& x_step, const dda_coord& y_step)
{
    auto is_x_step = false;
    while (!is_wall(vec2i{x.on_grid, y.on_grid}))
    {
        is_x_step = x.distance < y.distance;
        if (is_x_step)
            x += x_step;
        else
            y += y_step;
    }

    // the result is whether the ray hit a wall while taking an x step and the grid coordinate
    // of the cell that was hit
    return std::pair(is_x_step, static_cast<float>(is_x_step ? x.on_grid : y.on_grid));
}

// Compute the start and step for a given x or y direction. Arguments are a coordinate (either
// x or y) of the camera position and the corresponding component of the ray direction.
constexpr auto initialize_dda_direction(const float pos, const float dir)
{
    const auto grid_pos = static_cast<int>(pos);

    // Step on grid is -1 or 1 depending on ray direction. Step distance along ray is the distance
    // travelled along the ray if we cross a cell in this direction (resolves nicely to |1/dir|).
    const auto step = dda_coord{.on_grid = (dir < 0.0f) ? -1 : 1, .distance = std::abs(1.0f / dir)};

    // Start on grid is the position of the camera snapped on to the grid. Start distance is the
    // distance travelled along the ray in order to reach the edge of the current cell that corresponds
    // to this direction (horizontal for x arguments, vertical for y arguments).
    const auto aligned_edge_offset = (dir < 0.0f) ? (pos - grid_pos) : (grid_pos + 1.0f - pos);
    const auto start = dda_coord{.on_grid = grid_pos, .distance = step.distance * aligned_edge_offset};
    return std::pair(start, step);
}

// A wall hit is a distance from the camera to the wall and the texture coordinate in x (which
// we use to determine whether the ray is hitting the left or right edge of a wall so that
// we can visually delimit the walls when rendering)
struct wall_hit
{
    float distance = 0.0f;
    float tx = 0.0f;
};

// Given a start position and a ray direction from that position compute the wall hit
constexpr wall_hit compute_wall_hit(const vec2f& pos, const vec2f& dir)
{
    const auto [x_start, x_step] = initialize_dda_direction(pos.x, dir.x);
    const auto [y_start, y_step] = initialize_dda_direction(pos.y, dir.y);

    const auto [is_x, hit_pos] = cast_ray(x_start, y_start, x_step, y_step);

    // Say we ended up hitting a wall while stepping in x, then we compute how far
    // we had to cast the ray in the x-direction (which is the hit pos minus the
    // start pos - but we have to correct for the snapped pos being in one
    // corner of the cell: if we were travelling in the negative direction, then
    // we hit the wall at the end of a step rather than at the beginning of the
    // step so our hit pos is actually one too far. ((1 - step) >> 1) is just one
    // if step is negative and other wise zero). Once we have the distance
    // traversed in the given direction, then we just divide by the corresponding
    // component of the direction vector to get the distance (see also how the
    // start distance was calculated).
    const auto distance = is_x ? (hit_pos - pos.x + ((1 - x_step.on_grid) >> 1)) / dir.x
                               : (hit_pos - pos.y + ((1 - y_step.on_grid) >> 1)) / dir.y;

    // if we hit in the x direction then the tex coord is the fractional component
    // of the y coordinate of the point where the ray hits the wall. And vice versa
    // if we hit in the y direction.
    const auto tx = is_x ? pos.y + distance * dir.y : pos.x + distance * dir.x;
    return {distance, tx - std::floor(tx)};
}

// For a given fraction (i.e. x in [0, 1]) return the character that best represents that
// fraction of a whole block (used to generate the smoothing effect on the top and bottom
// of walls)
constexpr const char* fractional_block(const float x)
{
    constexpr auto chars = std::array{" ", "\u2581", "\u2582", "\u2583", "\u2584", "\u2585", "\u2586", "\u2587"};
    const auto index = static_cast<int>(x * (chars.size() - 1e-6f));
    return chars[index];
}

// given the screen height and the corresponding wall hit, draw a column of characters representing
// the ceiling, wall and floor that are visible in that column. Note that this could be simplified
// if we always smoothed the edges and did not bother with the blocky mode, but for comparison
// purposes the smoothing can be turned on and off.
void draw_column(const os::terminal& term, const int x, const int screen_height, const wall_hit hit,
                 const bool is_blocky)
{
    // The floating point height of the wall projected into screen space
    const auto exact_wall_height = static_cast<float>(screen_height) / hit.distance;

    // The number of whole characters that would be needed to represent the wall. If we're
    // smoothing the edges then the number of whole chars is always even because an odd
    // truncated wall height is achieved using an even number of whole blocks with a half
    // block on the top and the bottom (that way the walls are always centered correctly)
    const auto truncated_wall_height = static_cast<int>(exact_wall_height);
    const auto num_whole_chars = truncated_wall_height - (is_blocky ? 0 : (truncated_wall_height % 2));

    // The y-coordinate (or row position within the column) of the top and bottom of the wall.
    // This is where the fractional blocks will go if we're smoothing the edges
    const auto wall_top = ((screen_height - num_whole_chars) / 2) - 1;
    const auto wall_bottom = wall_top + num_whole_chars + 2;

    // Where the sequence of wall and floor chars start (add one if we're smoothing the edges
    // to make space for the fractional blocks)
    const auto wall_start = wall_top + (is_blocky ? 0 : 1);
    const auto floor_start = wall_bottom + (is_blocky ? 0 : 1);

    // anything on the left or right edge of a wall cell is rendered using a different character
    // (wall chars are rendered with the invert flag set to true so " " is actually a solid block)
    const auto wall_char = ((hit.tx < 0.1f) or (hit.tx > 0.9)) ? "\u2502" : " ";

    // the range of y coordinates between min and max, clamped to the screen and empty if max < min
    const auto block_between = [&](int min, int max) {
        min = std::max(0, min);
        max = std::min(screen_height, max);
        return std::ranges::iota_view(std::min(min, max), max);
    };

    // print a (possibly inverted) character to the current column
    const auto print = [&](const char* c, const bool invert = false) {
        return [&, c, invert](const int y) { term.print_char(x, y, c, invert); };
    };

    // render the ceiling, wall and floor characters respectively
    std::ranges::for_each(block_between(0, wall_top), print(" "));
    std::ranges::for_each(block_between(wall_start, wall_bottom), print(wall_char, true));
    std::ranges::for_each(block_between(floor_start, screen_height), print("."));

    // if we're smoothing the edges and the edges are on the screen, then print the fractional blocks
    if (!is_blocky and (wall_top >= 0))
    {
        // split the left over bit of the wall height after rendering the whole blocks over
        // the top and bottom fractional blocks
        const auto fraction = 0.5f * (exact_wall_height - static_cast<float>(num_whole_chars));
        print(fractional_block(fraction))(wall_top);
        print(fractional_block(1.0f - fraction), true)(wall_bottom);
    }
}

// Represent a player by the position, the forward direction unit vector and a second unit
// vector, perpendicular to the forward vector, pointing to the right of the player that
// is used both for strafing and computing the (non-unit) ray direction vectors
class player
{
public:
    [[nodiscard]] constexpr vec2f pos() const { return pos_; }

    // Imagine a screen one unit in front of the player, parallel to the right pointing
    // vector, with coordinates starting at the very left of the screen at zero and
    // ending at the very right of the screen at one. If you pass in a screen
    // coordinate between zero and one, this function returns a vector that starts
    // at the player position and ends at the corresponding point on the imagined
    // screen. Note that only at 0.5 - i.e. the center of the screen - will this
    // be a unit vector.
    [[nodiscard]] constexpr vec2f line_of_sight(const float normalized_screen_x) const
    {
        const auto increment = (2.0f * normalized_screen_x) - 1.0f;
        return forward_ + right_ * increment;
    }

    constexpr void walk(const float factor) { move(forward_ * factor * run_speed); }
    constexpr void strafe(const float factor) { move(right_ * factor * run_speed); }
    constexpr void turn(const float factor)
    {
        forward_ = rotate(forward_, factor * turn_speed);
        right_ = rotate(right_, factor * turn_speed);
    }

private:
    constexpr void move(const vec2f& v)
    {
        const auto p = pos_ + v;
        if (!is_wall(p)) pos_ = p;  // very primitive collision detection
    }

    vec2f pos_ = vec2f{.x = 5.0f, .y = 5.0f};
    vec2f forward_ = vec2f{.x = 0.0f, .y = 1.0f};
    vec2f right_ = vec2f{.x = 0.8f, .y = 0.0f};

    constexpr static float run_speed = 0.5f;
    constexpr static float turn_speed = 0.1f;
};

// Draw the 3D scene
void draw_scene(const os::terminal& term, const int screen_width, const int screen_height, const player& plyr,
                const bool is_blocky)
{
    // For each screen column, get the ray direction, compute the wall hit and draw the column
    for (int i = 0; i < screen_width; ++i)
    {
        const auto ray_dir = plyr.line_of_sight(static_cast<float>(i) / static_cast<float>(screen_width - 1));
        draw_column(term, i, screen_height, compute_wall_hit(plyr.pos(), ray_dir), is_blocky);
    }
}

void draw_map(const os::terminal& term, const player& plyr)
{
    // print each line of the map
    for (auto i = maze_height; const auto line : maze)
        term.print(0, --i, line);

    // print the player on the map as a small arrow pointing in the direction that the player
    // is looking
    const auto [x, y] = to_vec2i(plyr.pos());
    const auto dir = (pi / 16.0f) + (to_radians(plyr.line_of_sight(0.5f)) / (pi * 2.0f));
    const auto dir_index = (7 + static_cast<int>(dir * 8.0f)) % 8;
    constexpr auto dir_chars =
        std::array{"\u25c0", "\u25e3", "\u25bc", "\u25e2", "\u25b6", "\u25e5", "\u25b2", "\u25e4"};
    term.print_char(x, maze_height - y - 1, dir_chars[dir_index]);
}

// render the scene (and possibly the map) to the terminal
void render(os::terminal& term, const player& plyr, bool is_blocky, bool is_draw_map)
{
    const auto [screen_width, screen_height] = term.screen_size();
    draw_scene(term, screen_width, screen_height, plyr, is_blocky);
    if (is_draw_map) draw_map(term, plyr);
}

int main()
{
    auto term = os::terminal{};

    auto plyr = player{};

    // variable settings
    bool is_blocky = false;
    bool is_map_visible = false;

    // Events are a key and a function to execute when that key is pressed
    using event = std::pair<int, std::function<void()>>;
    const auto events = std::array{
        event{'a', [&] { plyr.turn(1.0f); }},         event{'d', [&] { plyr.turn(-1.0f); }},
        event{'w', [&] { plyr.walk(1.0f); }},         event{'s', [&] { plyr.walk(-1.0f); }},
        event{'m', [&] { plyr.strafe(1.0f); }},       event{'n', [&] { plyr.strafe(-1.0f); }},
        event{'h', [&] { is_blocky = !is_blocky; }},  event{'p', [&] { is_map_visible = !is_map_visible; }},
        event{os::escape_key, [&] { std::exit(0); }},
    };

    while (true)
    {
        render(term, plyr, is_blocky, is_map_visible);
        if (const auto it = std::ranges::find(events, getch(), &event::first); it != events.end()) it->second();
    }
}
