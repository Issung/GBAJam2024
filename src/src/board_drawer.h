#ifndef BOARD_DRAWER
#define BOARD_DRAWER

#include "bn_core.h"
#include "bn_log.h"
#include "bn_sprite_items_gem.h"
#include "bn_sprite_palette_item.h"
#include "bn_sprite_palette_ptr.h"
#include "bn_sprite_ptr.h"
#include "bn_vector.h"
#include "bn_math.h"
#include "bn_sprite_actions.h"
#include "bn_list.h"
#include "board.h"

// TODO: Do we have to define this here? T-T It's being used in anim_slide.
static bn::fixed_point positions[board::rows][board::cols]; // The point of each drawn sprite, saved for use by the selector. [row][col]

/*enum anim_type 
{
    slide
};

class anim
{
public:
    anim_type type;
};*/

enum class drawer_state
{
    Waiting,    // Waiting for player input.
    PlayingSlide,  // Animating the player's input.
    DestroyingMatches, // Animating matches being destroyed.
    DroppingGems,   // Animating gems dropping.
};

// TODO: Change all of these to int8_t.
class anim_slide {
private:
    int from_row;
    int from_col;

    int to_row;
    int to_col;

    bn::sprite_move_to_action action;
public:
    int frames_delay = 0;

    // Gem will be grabbed from `_to_row, _to_col`, color changed and moved to the `_from_row, _from_col` position, slid back 
    // to its `_to_row, _to_col` position.
    // Do everything so that when the animation ends, the sprite is where it needs to be with no further alterations.
    anim_slide(
        int _from_row, int _from_col,
        int _to_row, int _to_col,
        bn::sprite_ptr sprite,  // The sprite from _to_row, _to_col.
        bn::sprite_palette_ptr palette,  // The new palette for the gem at _to_row, _to_col.
        bool visible
    ) : from_row(_from_row), from_col(_from_col),
        to_row(_to_row), to_col(_to_col),
        action(
            determine_sprite(sprite, _from_row, _from_col),
            determine_duration(_from_row, _from_col, _to_row, _to_col),
            determine_to_position(_to_row, _to_col)
        )
    {
        sprite.set_visible(visible);
        sprite.set_palette(palette);

        //auto debug_distance = determine_duration(from_row, from_col, to_row, to_col) / 10;
        //BN_LOG("Created slide for ", from_row, ",", from_col, " to ", to_row, ",", to_col, ". Distance: ", debug_distance);

        //auto dbg_to = determine_to_position(to_row, to_col);
        //BN_LOG("Created slide for ", from.x(), ",", from.y(), " to ", dbg_to.x(), ",", dbg_to.y());
    }
    
    // Returns true if done.
    bool update()
    {
        auto done = action.done();

        if (done == false)
        {
            if (frames_delay > 0)
            {
                frames_delay -= 1;
            }
            else
            {
                action.update();
            }

            return false;
        }

        return true;
    }
private:
    // Horrible hack that allows us to set the sprite's position before we construct the tween action.
    static bn::sprite_ptr determine_sprite(bn::sprite_ptr sprite, int _from_row, int _from_col)
    {
        auto from = positions[bn::max(_from_row, 0)][_from_col];

        if (_from_row < 0)
        {
            from = bn::fixed_point(from.x(), (from.y() + (30 * _from_row)) - 5); // Subtract an extra 5 pixels so gems start completely off screen.
        }

        sprite.set_scale(1);
        sprite.set_position(from);
        return sprite;
    }

    static int determine_duration(int _from_row, int _from_col, int _to_row, int _to_col)
    {
        constexpr int framesPerSquare = 10;
        auto distance = bn::abs((_from_row - _to_row) + (_from_col - _to_col));
        return bn::max(distance * framesPerSquare, 1);  // Setting a duration of 0 causes error, use min of 1.
    }

    static bn::fixed_point determine_to_position(int _to_row, int _to_col)
    {
        auto position = positions[_to_row][_to_col];
        return position;
    }
};

class anim_destroy {
private:
    static constexpr bn::fixed target = bn::fixed(0.02);
    static constexpr int frames = 10;

    static bn::sprite_scale_to_action create_action(bn::sprite_ptr& sprite)
    {
        sprite.set_scale(1);
        return bn::sprite_scale_to_action(sprite, frames, target);
    }
public:
    bn::sprite_scale_to_action action;

    // Gem will be grabbed from `_to_row, _to_col`, color changed and moved to the `_from_row, _from_col` position, slid back 
    // to its `_to_row, _to_col` position.
    // Do everything so that when the animation ends, the sprite is where it needs to be with no further alterations.
    anim_destroy(bn::sprite_ptr& sprite) : action(create_action(sprite))
    {
        
    }
};

namespace
{
    const static bn::sprite_palette_ptr create_palette(int r, int g, int b)
    {
        auto color = bn::color(r, g, b);
        bn::color colors[16];
        for (int i = 0; i < 16; i++)
        {
            colors[i] = color;
        }
        auto span = bn::span<bn::color>(colors);
        auto palette = bn::sprite_palette_item(span, bn::bpp_mode::BPP_4);
        auto palette_ptr = palette.create_palette();
        return palette_ptr;
    }
}


// Class responsible for handling board drawing (including animations).
class board_drawer
{
private: 
    board& b;
    bn::vector<bn::sprite_ptr, board::total_gems> gem_sprites;  // Each gem sprite, can be accessed with `(row * board::cols) + col`.
    bn::list<anim_slide, board::total_gems> slides;
    bn::list<anim_destroy, board::total_gems> destroys;
    // Move to ROM to avoid ram usage. https://gvaliente.github.io/butano/faq.html
    drawer_state current_state;
    bn::random rand = bn::random();
public:
    const bn::sprite_palette_ptr colors[board::max_colors] =
    {
        create_palette(31, 0, 0),   // Red
        create_palette(0, 31, 0),   // Green
        create_palette(0, 0, 31),   // Blue
        create_palette(31, 0, 31),  // Purple
        create_palette(31, 16, 0),  // Orange
        create_palette(31, 31, 31), // White (Wildcard)
        // No color for empty at the moment, just make the sprite invisible.
    };

    board_drawer(board& _b) : b(_b)
    {
        for (int r = 0; r < board::rows; r++)
        {
            for (int c = 0; c < board::cols; c++)
            {
                //BN_LOG("Making gem r: ", r, " c: ", c);

                auto x = (30 * c) - 100;
                auto y = (30 * r) - 60;
                positions[r][c] = bn::fixed_point(x, y);
                auto gem_sprite = bn::sprite_items::gem.create_sprite(x, y);
                auto palette_index = (uint8_t)b.gems[r][c];
                auto palette = colors[palette_index];
                gem_sprite.set_palette(palette);
                gem_sprites.push_back(gem_sprite);

                //BN_LOG("Made gem r: ", r, " c: ", c);
            }
        }

        //BN_LOG("Finished constructing board_drawer.");
    }

    void hide()
    {
        for (int r = 0; r < board::rows; r++)
        {
            for (int c = 0; c < board::cols; c++)
            {
                gem_sprites[r * board::cols + c].set_visible(false);
            }
        }
    }

    void show()
    {
        for (int r = 0; r < board::rows; r++)
        {
            for (int c = 0; c < board::cols; c++)
            {
                gem_sprites[r * board::cols + c].set_visible(true);
            }
        }
    }

    drawer_state state()
    {
        return current_state;
    }

    bool animating()
    {
        return slides.size() > 0;
    }

    // Slide gems between row/col A and row/col B.
    void slide(
        int row_a, int col_a,
        int row_b, int col_b
    )
    {
        current_state = drawer_state::PlayingSlide;

        auto sprite_a = gem_sprites[(row_a * board::cols) + col_a];
        auto type_a = b.gems[row_a][col_a];
        auto palette_a = colors[(int)type_a];
        slides.push_back(anim_slide(row_b, col_b, row_a, col_a, sprite_a, palette_a, type_a != gem_type::Empty));

        auto sprite_b = gem_sprites[(row_b * board::cols) + col_b];
        auto type_b = b.gems[row_b][col_b];
        auto palette_b = colors[(int)type_b];
        slides.push_back(anim_slide(row_a, col_a, row_b, col_b, sprite_b, palette_b, type_b != gem_type::Empty));
    }

    // Alters state, to either be:
    // - `Waiting` (for player input) if matches is empty.
    // - `DestroyingMatches` if there was atleast one match to be animated.
    void play_matches(match_collection& matches)
    {
        if (matches.size() == 0)
        {
            current_state = drawer_state::Waiting;
        }
        else
        {
            current_state = drawer_state::DestroyingMatches;

            bool created_animation_positions[board::rows][board::cols] = {};    // {} initialises all values to false.

            for (auto& m : matches)
            {
                for (auto& pos : m.positions)
                {
                    // If we haven't already created a animation for the gem at this position.
                    // This could happen if a single gem is used within 2 matches.
                    if (!created_animation_positions[pos.row][pos.col])
                    {
                        // Create animation.
                        bn::sprite_ptr& sprite = gem_sprites[(pos.row * board::cols) + pos.col];
                        destroys.push_back(anim_destroy(sprite));
                        
                        // Mark the position as true so no more animations are created for it.
                        created_animation_positions[pos.row][pos.col] = true;
                    }
                }
            }

            // TODO: Is this necessary?
            matches.clear();
        }
    }

    // Animate a collection of drops. Alters state to `DroppingGems`.
    void play_drops(gem_drops_collection& drops)
    {
        BN_ASSERT(drops.size() > 0, "play_drops function should never be given an empty collection.");

        current_state = drawer_state::DroppingGems;

        // Create animation for each drop.
        for (auto& drop : drops)
        {
            auto spr = gem_sprites[(drop.to_row * board::cols) + drop.col];
            auto palette = colors[(uint8_t)drop.type];
            slides.push_back(anim_slide(drop.from_row, drop.col, drop.to_row, drop.col, spr, palette, drop.type != gem_type::Empty));
        }
    }

    // Returns true if animations are complete.
    bool update()
    {
        animate_slides();
        animate_destroys();

        return slides.size() == 0 && destroys.size() == 0;
    }

    void animate_random_drop_all_in()
    {
        // TestMethodHere

        // TODO Make a util class that returns 2d int arrays with numbers 0 - 29 to indicate gem drop order, rather than repeat the anim logic for every variant.

        int i = rand.get_int(4);
        if (i == 0) animate_drop_all_in();
        else if (i == 1) animate_drop_all_in_alternating_rows();
        else if (i == 2) animate_scattered_drop_all_in();
        else if (i == 3) animate_scattered_drop_all_in_row_by_row();
        else BN_ASSERT(false, "Invalid value for random all-drop-in animation.");

        current_state = drawer_state::DroppingGems;
    }

    static constexpr int delay_between_each_gem_drop = 2;

    void animate_drop_all_in()
    {
        int delay = 0;

        // TODO: Bottom row drop in from left-to-right, 2nd from bottom drop in from right-to-left, alternate up to top.
        for (int row = board::rows - 1; row >= 0; --row)
        {
            for (int col = board::cols - 1; col >= 0; --col)
            {
                auto spr = gem_sprites[(row * board::cols) + col];
                auto palette = colors[(uint8_t)b.gems[row][col]];
                auto slide = anim_slide(row - board::rows, col, row, col, spr, palette, true);
                slide.frames_delay = delay;
                slides.push_back(slide);
                delay += delay_between_each_gem_drop;
            }
        }
    }

    void animate_drop_all_in_alternating_rows()
    {
        int delay = 0;

        for (int row = board::rows - 1; row >= 0; --row)
        {
            auto even = row % 2 == 0;
            for (int col = (even ? board::cols - 1 : 0); 
                even ? col >= 0 : col < board::cols; 
                even ? --col : ++col)
            {
                auto spr = gem_sprites[(row * board::cols) + col];
                auto palette = colors[(uint8_t)b.gems[row][col]];
                auto slide = anim_slide(row - board::rows, col, row, col, spr, palette, true);
                slide.frames_delay = delay;
                slides.push_back(slide);
                delay += 2;
            }
        }
    }

    // Drop gems into random columns until full.
    void animate_scattered_drop_all_in()
    {
        int total_slots = board::rows * board::cols;
        bool filled[board::rows][board::cols] = {};
        int filled_count = 0;

        while (filled_count < total_slots)
        {
            int col = rand.get_int(board::cols);

            for (int row = board::rows - 1; row >= 0; --row)
            {
                if (filled[row][col] == false)
                {
                    auto spr = gem_sprites[(row * board::cols) + col];
                    auto palette = colors[(uint8_t)b.gems[row][col]];
                    auto slide = anim_slide(row - board::rows, col, row, col, spr, palette, true);
                    slide.frames_delay = filled_count * delay_between_each_gem_drop;
                    slides.push_back(slide);

                    filled[row][col] = true;
                    filled_count += 1;
                    break;
                }
            }
        }
    }

    // Drop gems into random columns until full.
    void animate_scattered_drop_all_in_row_by_row()
    {
        int delay = 0;
        
        for (int row = board::rows - 1; row >= 0; --row)
        {
            bool filled[board::cols] = {};
            int filled_count = 0;

            while (filled_count < board::cols)
            {
                int col = rand.get_int(board::cols);

                if (filled[col] == false)
                {
                    auto spr = gem_sprites[(row * board::cols) + col];
                    auto palette = colors[(uint8_t)b.gems[row][col]];
                    auto slide = anim_slide(row - board::rows, col, row, col, spr, palette, true);
                    slide.frames_delay = delay;
                    slides.push_back(slide);

                    filled[col] = true;
                    filled_count += 1;
                    delay += delay_between_each_gem_drop;
                }
            }
        }
    }

    // Reset all animation state.
    void reset()
    {
        slides.clear();
        destroys.clear();

        for (int r = 0; r < board::rows; ++r)
        {
            for (int c = 0; c < board::cols; ++c)
            {
                auto sprite = gem_sprites[(r * board::cols) + c];
                sprite.set_position(positions[r][c]);
                sprite.set_visible(true);
            }
        }

        set_all_gems_palette();
    }

private:
    void set_all_gems_palette()
    {
        for (int r = 0; r < board::rows; ++r)
        {
            for (int c = 0; c < board::cols; ++c)
            {
                auto gem_sprite = gem_sprites[(r * board::cols) + c];
                auto gem_value = b.gems[r][c];

                if (gem_value == gem_type::Empty)
                {
                    gem_sprite.set_visible(false);
                }
                else
                {
                    auto palette = colors[(uint8_t)gem_value];
                    gem_sprite.set_palette(palette);
                    gem_sprite.set_visible(true);
                }
            }
        }
    }
    
    void animate_slides()
    {
        auto it = slides.begin();
        auto end = slides.end();
        while (it != end)
        {
            auto done = it->update();
            //BN_LOG("Processing tween index: ", i, ". Done: ", done);

            if (done == false)
            {
                ++it;
            }
            else
            {
                // Remove from list.
                it = slides.erase(it);
            }
        }
    }

    void animate_destroys()
    {
        auto it = destroys.begin();
        auto end = destroys.end();
        while (it != end)
        {
            auto done = it->action.done();
            //BN_LOG("Processing tween index: ", i, ". Done: ", done);

            if (done == false)
            {
                it->action.update();
                ++it;
            }
            else
            {
                // Remove from list.
                it = destroys.erase(it);
            }
        }
    }
};

#endif