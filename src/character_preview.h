#pragma once
#ifndef CATA_SRC_CHARACTER_PREVIEW_H
#define CATA_SRC_CHARACTER_PREVIEW_H

#if defined( TILES )

#include <cstdint>

#include "cursesdef.h"
#include "point.h"

class avatar;

/** 角色创建界面中的贴图角色预览窗口。 */
struct character_preview_window {
    enum orientation_type : std::uint8_t {
        TOP_LEFT,
        TOP_RIGHT,
        BOTTOM_LEFT,
        BOTTOM_RIGHT
    };

    struct margin {
        int left = 0;
        int right = 0;
        int top = 0;
        int bottom = 0;
    };

    struct orientation {
        orientation_type type = TOP_RIGHT;
        margin margins;
    };

    character_preview_window() = default;
    ~character_preview_window();

    void init( avatar *character );
    void prepare( int nlines, int ncols, const orientation &where, int hide_below_ncols );
    void zoom_in();
    void zoom_out();
    void toggle_clothes();
    void display() const;
    void clear();

    bool clothes_showing() const;
    bool visible() const;
    int width() const;

  private:
    static constexpr int min_zoom = 32;
    static constexpr int max_zoom = 128;
    static constexpr int default_zoom = 128;

    point pos = point_zero;
    int termx_pixels = 0;
    int termy_pixels = 0;
    int zoom = default_zoom;
    int hide_below_ncols = 0;
    int ncols_width = 0;
    int nlines_height = 0;
    avatar *character = nullptr;
    catacurses::window w_preview;
    bool show_clothes = true;
    bool prepared = false;
};

#endif // TILES

#endif // CATA_SRC_CHARACTER_PREVIEW_H
