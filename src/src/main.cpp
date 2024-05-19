#include "bn_bg_palettes.h"
#include "bn_core.h"
#include "bn_sprite_ptr.h"
#include "bn_sprite_text_generator.h"
#include "bn_vector.h"
#include "bn_keypad.h"

#include "gj_big_sprite_font.h"
#include "bn_sprite_palette_item.h"
#include "bn_sstream.h"
#include "bn_string.h"
#include "bn_format.h"
#include "bn_music.h"
#include "bn_music_item.h"
#include "bn_music_items.h"
#include "bn_sound_items.h"
#include "player.h"
#include "bn_sprite_items_gem.h"
#include "bn_sprite_items_selector.h"
#include "bn_fixed_point.h"
#include "bn_fixed.h"
#include "bn_sprite_palettes.h"
#include "bn_sprite_palette_ptr.h"
#include "bn_log.h"
#include "bn_random.h"
#include "bn_sprite_actions.h"
#include "bn_unordered_set.h"
#include "bn_list.h"
#include "bn_unique_ptr.h"
#include "bn_math.h"

enum class gem_color {
    Red,
    Green,
    Blue,
    Purple,
    Orange
};

enum class tween_type {
    SpriteMove
};

const int cols = 6;   // The amount of columns to draw.
const int rows = 5;   // The amount of rows to draw.
const int total_gems = rows * cols; // Total gems on the board (rows * cols).
const int max_colors = 5;   // The amount of colors.
bn::fixed_point positions[rows][cols]; // The point of each drawn sprite, saved for use by the selector.
gem_color gems[rows][cols];

class tween {
public:
    tween_type type;
    void *action;
    //void (*complete_callback)();

    tween(bn::sprite_move_to_action action)
    {
        this->type = tween_type::SpriteMove;
        this->action = &action;
    }

    // Returns true if done.
    bool update()
    {
        bool done = false;
        if (type == tween_type::SpriteMove)
        {
            auto a_ptr = static_cast<bn::sprite_move_to_action*>(action);
            //BN_LOG("Updates remaining: ", a_ptr->duration_updates());
            done = a_ptr->done();
            if (done == false)
            {
                a_ptr->update();
            }
        }

        // TODO: Callback & discard tween.
        if (done)
        {
            delete(action);
            return true;
        }
        else
        {
            return false;
        }
    }
};

//bn::list<tween, 32> tweens;

class anim_slide {
public:
    int from_row;
    int from_col;
    int to_row;
    int to_col;
    bn::sprite_move_to_action action;

    anim_slide(
        int from_row, int from_col,
        int to_row, int to_col,
        bn::sprite_ptr sprite
    ) : 
        from_row(from_row), from_col(from_col),
        to_row(to_row), to_col(to_col),
        action(
            determine_sprite(sprite, from_row, from_col),
            determine_distance(from_row, from_col, to_row, to_col),
            determine_to_position(to_row, to_col)
        )
    {
        auto from = positions[bn::max(from_row, 0)][from_col];

        if (from_row < 0)
        {
            from = bn::fixed_point(from.x(), from.y() + (30 * from_row));
        }

        sprite.set_position(from);

        auto debug_distance = determine_distance(from_row, from_col, to_row, to_col);
        auto dbg_to = determine_to_position(to_row, to_col);
        //BN_LOG("Created slide for ", from_row, ",", from_col, " to ", to_row, ",", to_col, ". Distance: ", debug_distance);
        BN_LOG("Created slide for ", from.x(), ",", from.y(), " to ", dbg_to.x(), ",", dbg_to.y());
    }
    
private:
    // Horrible hack that allows us to set the sprite's position before we construct the tween action.
    static bn::sprite_ptr determine_sprite(bn::sprite_ptr sprite, int from_row, int from_col)
    {
        auto from = positions[bn::max(from_row, 0)][from_col];

        if (from_row < 0)
        {
            from = bn::fixed_point(from.x(), from.y() + (30 * from_row));
        }

        sprite.set_position(from);
        return sprite;
    }

    static int determine_distance(int from_row, int from_col, int to_row, int to_col)
    {
        auto distance = bn::abs((from_row - to_row) + (from_col - to_col)) * 10;
        return distance;
    }

    static bn::fixed_point determine_to_position(int to_row, int to_col)
    {
        auto position = positions[to_row][to_col];
        return position;
    }
};

namespace
{
    void tet()
    {

    }

    bn::sprite_ptr draw_caveman()
    {
        bn::sprite_ptr caveman = bn::sprite_items::caveman.create_sprite(-50, 0);
        caveman.set_bg_priority(2);
        caveman.set_z_order(1);
        return caveman;
    }

    // Restricted to 4bit color depth for now.
    // TODO: Figure out if there's better ways to do this.
    bn::sprite_palette_ptr create_palette(int r, int g, int b)
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

int main()
{
    bn::core::init();
    BN_LOG("INIT!");

    auto rand = bn::random();
    int sel_row = 0;    // Current row of the selector.
    int sel_col = 0;    // Current column of the selector.
    bool animating = false; // Is an animation currently playing out.

    bn::bg_palettes::set_transparent_color(bn::color(0, 0, 0));

    bn::sprite_text_generator text_generator(gj::fixed_32x64_sprite_font);

    text_generator.set_left_alignment();
    bn::vector<bn::sprite_ptr, 32> text_sprites;
    text_generator.generate(+70, -70, "SCORE", text_sprites);   // TODO: Fix Y position when gems border is added.
    bn::vector<bn::sprite_ptr, total_gems> gem_sprites;

    //auto mi = bn::music_item(0);
    //mi.play(0.5);
    bn::music_items::cyberrid.play(0.05);

    int i = 0;

    auto spr_selector = bn::sprite_items::selector.create_sprite(0, 0);
    
    //auto red = bn::color(31, 0, 0);
    //bn::color reds[16];
    //for (int i = 0; i < 16; i++)
    //{
    //    reds[i] = red;
    //}
    //auto span = bn::span<bn::color>(reds);
    //auto red_palette = bn::sprite_palette_item(span, bn::bpp_mode::BPP_4);
    bn::sprite_palette_ptr colors[max_colors] = 
    {
        create_palette(31, 0, 0),   // Red
        create_palette(0, 31, 0),   // Green
        create_palette(0, 0, 31),   // Blue
        create_palette(31, 0, 31),  // Purple
        create_palette(31, 16, 0),  // Orange
    };
    //auto red_palette_ptr = create_palette(31, 0, 0);

    for (int r = 0; r < rows; r++)
    {
        for (int c = 0; c < cols; c++)
        {
            auto val = rand.get_int(max_colors);
            gems[r][c] = (gem_color)val;
        }
    }

    for (int r = 0; r < rows; r++)
    {
        for (int c = 0; c < cols; c++)
        {
            auto x = (30 * c) - 100;
            auto y = (30 * r) - 60;
            positions[r][c] = bn::fixed_point(x, y);
            auto gem_sprite = bn::sprite_items::gem.create_sprite(x, y);
            auto palette_index = (int)gems[r][c];
            auto palette = colors[palette_index];
            gem_sprite.set_palette(palette);

            gem_sprites.push_back(gem_sprite);
            BN_LOG("Made gem c: ", r, " r: ", c);
        }
    }

    bn::list<anim_slide, total_gems> slides;

    while (true)
    {
        //if (bn::keypad::a_pressed() || bn::keypad::b_pressed() || bn::keypad::l_pressed() || bn::keypad::r_pressed())
        //{
        //    bn::sound_items::up.play(0.6);
        //}
        //if (bn::keypad::a_released() || bn::keypad::b_released() || bn::keypad::l_released() || bn::keypad::r_released())
        //{
        //    bn::sound_items::down.play(1);
        //}

        if (bn::keypad::a_held())
        {
            int move_row = 0;
            int move_col = 0;

            // TODO: Validation with error sfx and animation.
            if (bn::keypad::left_pressed() && sel_col > 0)
            {
                move_col = -1;
            }
            else if (bn::keypad::right_pressed() && sel_col < cols - 1)
            {
                move_col = +1;
            }
            else if (bn::keypad::up_pressed() && sel_row > 0)
            {
                move_row = -1;
            }
            else if (bn::keypad::down_pressed() && sel_row < rows - 1)
            {
                move_row = +1;
            }

            if (move_row != 0 || move_col != 0)
            {
                auto current_gem = gems[sel_row][sel_col];
                auto target_gem = gems[sel_row + move_row][sel_col + move_col];
                gems[sel_row][sel_col] = target_gem;
                gems[sel_row + move_row][sel_col + move_col] = current_gem;

                auto current_gem_sprite = gem_sprites[(sel_row * cols) + sel_col];
                //auto current_gem_to_pos = points[sel_row + move_row][sel_col + move_col];
                //auto current_gem_action = bn::sprite_move_to_action(current_gem_sprite, 60, current_gem_to_pos);
                //move_actions.push_back(current_gem_action);
                slides.push_back(anim_slide(sel_row, sel_col, sel_row + move_row, sel_col + move_col, current_gem_sprite));

                auto target_gem_sprite = gem_sprites[((sel_row + move_row) * cols) + (sel_col + move_col)];
                //auto target_gem_to_pos = points[sel_row][sel_col];
                //auto target_gem_action = bn::sprite_move_to_action(target_gem_sprite, 60, target_gem_to_pos);
                //move_actions.push_back(target_gem_action);
                slides.push_back(anim_slide(sel_row + move_row, sel_col + move_col, sel_row, sel_col, target_gem_sprite));
            }
        }
        else
        {
            if (bn::keypad::left_pressed() && sel_col > 0)
            {
                sel_col -= 1;
            }
            else if (bn::keypad::right_pressed() && sel_col < cols - 1)
            {
                sel_col += 1;
            }

            if (bn::keypad::up_pressed() && sel_row > 0)
            {
                sel_row -= 1;
            }
            else if (bn::keypad::down_pressed() && sel_row < rows - 1)
            {
                sel_row += 1;
            }
        }

        if (bn::keypad::start_pressed())
        {
            for (int r = 0; r < rows; r++)
            {
                for (int c = 0; c < cols; c++)
                {
                    auto gem_sprite = gem_sprites[(r * cols) + c];
                    slides.push_back(anim_slide(r - rows, c, r, c, gem_sprite));
                }
            }
        }

        auto selector_point = positions[sel_row][sel_col];
        spr_selector.set_position(selector_point);

        // Update all gem palettes.
        // TODO: Temporary for now, is not needed every frame.
        //for (int r = 0; r < rows; r++)
        //{
        //    for (int c = 0; c < cols; c++)
        //    {
        //        auto gem_sprite = gem_sprites[(r * cols) + c];
        //        auto gem_value = gems[r][c];
        //        auto palette = colors[(int)gem_value];
        //        gem_sprite.set_palette(palette);
        //    }
        //}

        //for (int i = 0; i < tweens.size(); i++)
        //{
        //    // TODO: This will break if some remove themselves during this loop.
        //    auto done = tweens[i].update();
        //    if (done)
        //    {
        //        tweens.erase(i, i);
        //    }
        //}

        auto it = slides.begin();
        auto end = slides.end();
        auto i = 0;
        while (it != end)
        {
            auto done = it->action.done();
            BN_LOG("Processing tween index: ", i, ". Done: ", done);

            if (done == false)
            {
                it->action.update();
                ++it;
            }
            else
            {
                // Reset gem pos & palette.
                auto sprite = it->action.sprite();

                // If the row is negative then determine its actual starting position.
                // TODO: This isn't gonna work once we have gem matching and breaking, add 2 extra properties to anim_slide.
                auto from_row = it->from_row < 0 ? it->from_row + rows : it->from_row;

                // Don't expect negative cols at the moment.
                //auto from_col = it->from_col < 0 ? it->from_col + cols : it->from_col;
                auto from_col = it->from_col;

                sprite.set_position(positions[from_row][from_col]);
                auto gem_type = gems[from_row][from_col];
                auto palette = colors[(int)gem_type];
                sprite.set_palette(palette);

                // Remove from list.
                it = slides.erase(it);
            }

            i++;
        }

        BN_LOG("slides size: ", slides.size());

        //i = i + 1;
        //auto istr = bn::to_string<16>(i);
        //auto text = "Frame: " + istr;
        //auto draw_player = player.update(); // sprite only gets drawn if there's still a reference held to it? sprite_ptr is actually a smart pointer
        bn::core::update();
    }
}