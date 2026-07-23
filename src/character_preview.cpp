#if defined( TILES )

// 角色创建预览逻辑移植并适配自 Cataclysm-BN。

#include "character_preview.h"

#include <algorithm>
#include <list>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "avatar.h"
#include "bionics.h"
#include "cata_tiles.h"
#include "cata_utility.h"
#include "creature.h"
#include "cursesport.h"
#include "game.h"
#include "item.h"
#include "itype.h"
#include "output.h"
#include "overlay_ordering.h"
#include "profession.h"
#include "sdltiles.h"
#include "translations.h"

namespace
{

static const flag_id json_flag_auto_wield( "auto_wield" );
static const flag_id json_flag_no_auto_equip( "no_auto_equip" );

int termx_to_pixel_value()
{
    return std::max( 1, projected_window_width() / std::max( 1, TERMX ) /
                     std::max( 1, get_scaling_factor() ) );
}

int termy_to_pixel_value()
{
    return std::max( 1, projected_window_height() / std::max( 1, TERMY ) /
                     std::max( 1, get_scaling_factor() ) );
}

bool is_clothing_overlay( const std::string &id )
{
    return id.rfind( "worn_", 0 ) == 0 || id.rfind( "wielded_", 0 ) == 0;
}

void add_profession_bionic_overlay( const bionic_id &bio, std::set<bionic_id> &visited,
                                    std::multimap<int, std::pair<std::string, std::string>> &overlays )
{
    if( !bio.is_valid() || !visited.insert( bio ).second ) {
        return;
    }

    const std::string overlay_id = bio.str();
    overlays.emplace( get_overlay_order_of_mutation( overlay_id ),
                      std::make_pair( "mutation_" + overlay_id, "" ) );
    for( const bionic_id &included : bio->included_bionics ) {
        add_profession_bionic_overlay( included, visited, overlays );
    }
}

class char_preview_adapter : public cata_tiles
{
  public:
    static char_preview_adapter *convert( cata_tiles *tiles )
    {
        return static_cast<char_preview_adapter *>( tiles );
    }

    void display_avatar_preview_with_overlays( const avatar &ch, const tripoint &p,
            const bool with_clothing )
    {
        int height_3d = 0;
        const int previous_height_3d = height_3d;
        const int rotation = ch.facing == FacingDirection::LEFT ? -1 : 0;
        const std::string entity_id = ch.male ? "player_male" : "player_female";

        draw_from_id_string( entity_id, TILE_CATEGORY::NONE, "", p, corner, rotation,
                             lit_level::LIT, false, height_3d );

        std::vector<std::pair<std::string, std::string>> overlays;
        const std::vector<std::pair<std::string, std::string>> current_overlays =
            ch.get_overlay_ids();
        overlays.reserve( current_overlays.size() + 16 );

        for( const std::pair<std::string, std::string> &overlay : current_overlays ) {
            if( with_clothing || !is_clothing_overlay( overlay.first ) ) {
                overlays.push_back( overlay );
            }
        }

        // 角色创建早期的临时角色尚未建立完整身体；不要通过 add_bionic() 构造职业义体预览。
        std::multimap<int, std::pair<std::string, std::string>> profession_bionics;
        std::set<bionic_id> visited_bionics;
        if( ch.prof != nullptr ) {
            for( const bionic_id &bio : ch.prof->CBMs() ) {
                add_profession_bionic_overlay( bio, visited_bionics, profession_bionics );
            }
        }
        for( const auto &entry : profession_bionics ) {
            if( std::find( overlays.begin(), overlays.end(), entry.second ) == overlays.end() ) {
                overlays.push_back( entry.second );
            }
        }

        if( with_clothing && ch.prof != nullptr ) {
            const std::list<item> profession_items = ch.prof->items( ch.male, ch.get_mutations() );
            for( const item &it : profession_items ) {
                if( it.has_flag( json_flag_no_auto_equip ) ) {
                    continue;
                }
                const std::string variant = it.has_itype_variant() ? it.itype_variant().id : "";
                if( it.has_flag( json_flag_auto_wield ) ) {
                    overlays.emplace_back( "wielded_" + it.typeId().str(), variant );
                } else if( it.is_armor() && ch.can_wear( it ).success() ) {
                    overlays.emplace_back( "worn_" + it.typeId().str(), variant );
                }
            }
        }

        for( const std::pair<std::string, std::string> &overlay : overlays ) {
            std::string draw_id = overlay.first;
            if( !find_overlay_looks_like( ch.male, overlay.first, overlay.second, draw_id ) ) {
                continue;
            }
            int overlay_height_3d = previous_height_3d;
            draw_from_id_string( draw_id, TILE_CATEGORY::NONE, "", p, corner, rotation,
                                 lit_level::LIT, false, overlay_height_3d );
            height_3d = std::max( height_3d, overlay_height_3d );
        }
    }
};

} // namespace

character_preview_window::~character_preview_window()
{
    clear();
}

void character_preview_window::init( avatar *new_character )
{
    character = new_character;
}

void character_preview_window::prepare( const int nlines, const int ncols,
                                        const orientation &where, const int hide_below )
{
    prepared = false;
    w_preview = catacurses::window();

    if( character == nullptr || !tilecontext || !tilecontext->is_valid() ) {
        return;
    }

    // 页面切换不应因文本区大小不同而改变同一角色的预览尺寸。
    static_cast<void>( nlines );
    static_cast<void>( ncols );
    zoom = default_zoom;
    tilecontext->set_draw_scale( zoom );
    termx_pixels = termx_to_pixel_value();
    termy_pixels = termy_to_pixel_value();
    hide_below_ncols = hide_below;

    int tile_width = tilecontext->get_tile_width();
    int tile_height = tilecontext->get_tile_height();

    while( zoom > min_zoom &&
           ( tile_width + 4 * termx_pixels > projected_window_width() ||
             tile_height + 3 * termy_pixels > projected_window_height() ) ) {
        zoom /= 2;
        tilecontext->set_draw_scale( zoom );
        tile_width = tilecontext->get_tile_width();
        tile_height = tilecontext->get_tile_height();
    }

    ncols_width = std::max( 5, divide_round_up( tile_width, termx_pixels ) + 4 );
    nlines_height = std::max( 5, divide_round_up( tile_height, termy_pixels ) + 3 );

    switch( where.type ) {
        case TOP_LEFT:
            pos = point_zero;
            break;
        case TOP_RIGHT:
            pos = point( TERMX - ncols_width, 0 );
            break;
        case BOTTOM_LEFT:
            pos = point( 0, TERMY - nlines_height );
            break;
        case BOTTOM_RIGHT:
            pos = point( TERMX - ncols_width, TERMY - nlines_height );
            break;
    }

    pos.x += where.margins.left - where.margins.right;
    pos.y += where.margins.top - where.margins.bottom;
    pos.x = clamp( pos.x, 0, std::max( 0, TERMX - ncols_width ) );
    pos.y = clamp( pos.y, 0, std::max( 0, TERMY - nlines_height ) );

    w_preview = catacurses::newwin( nlines_height, ncols_width, pos );
    prepared = true;
}

void character_preview_window::zoom_in()
{
    if( !prepared || !tilecontext ) {
        return;
    }
    zoom *= 2;
    if( zoom > max_zoom ) {
        zoom = min_zoom;
    }
    tilecontext->set_draw_scale( zoom );
}

void character_preview_window::zoom_out()
{
    if( !prepared || !tilecontext ) {
        return;
    }
    zoom /= 2;
    if( zoom < min_zoom ) {
        zoom = max_zoom;
    }
    tilecontext->set_draw_scale( zoom );
}

void character_preview_window::toggle_clothes()
{
    show_clothes = !show_clothes;
}

void character_preview_window::display() const
{
    if( !visible() || character == nullptr || !tilecontext || !tilecontext->is_valid() ) {
        return;
    }

    tilecontext->set_draw_scale( zoom );

    werase( w_preview );
    draw_border( w_preview, BORDER_COLOR, _( "角色预览" ), BORDER_COLOR );
    wnoutrefresh( w_preview );

    const int tile_width = tilecontext->get_tile_width();
    const int tile_height = tilecontext->get_tile_height();
    const int inner_left = ( pos.x + 1 ) * termx_pixels;
    const int inner_top = ( pos.y + 1 ) * termy_pixels;
    const int inner_width = std::max( 1, ( ncols_width - 2 ) * termx_pixels );
    const int inner_height = std::max( 1, ( nlines_height - 2 ) * termy_pixels );
    const point draw_origin( inner_left + ( inner_width - tile_width ) / 2,
                             inner_top + ( inner_height - tile_height ) / 2 );

    const SDL_Renderer_Ptr &renderer = get_sdl_renderer();
    if( !renderer ) {
        return;
    }
    const bool had_clip = SDL_RenderIsClipEnabled( renderer.get() ) == SDL_TRUE;
    SDL_Rect previous_clip = {};
    if( had_clip ) {
        SDL_RenderGetClipRect( renderer.get(), &previous_clip );
    }
    const SDL_Rect clip_rect = { inner_left, inner_top, inner_width, inner_height };
    SDL_RenderSetClipRect( renderer.get(), &clip_rect );

    const point old_o = cata_tiles::get_o();
    const point old_op = cata_tiles::get_op();
    const int old_width = cata_tiles::get_screentile_wdith();
    const int old_height = cata_tiles::get_screentile_height();

    cata_tiles::get_o() = point_zero;
    cata_tiles::get_op() = draw_origin;
    cata_tiles::get_screentile_wdith() = 1;
    cata_tiles::get_screentile_height() = 1;

    char_preview_adapter::convert( tilecontext.get() )->display_avatar_preview_with_overlays(
        *character, tripoint_zero, show_clothes );

    cata_tiles::get_o() = old_o;
    cata_tiles::get_op() = old_op;
    cata_tiles::get_screentile_wdith() = old_width;
    cata_tiles::get_screentile_height() = old_height;

    if( had_clip ) {
        SDL_RenderSetClipRect( renderer.get(), &previous_clip );
    } else {
        SDL_RenderSetClipRect( renderer.get(), nullptr );
    }
}

void character_preview_window::clear()
{
    if( prepared && tilecontext && tilecontext->is_valid() ) {
        tilecontext->set_draw_scale( DEFAULT_TILESET_ZOOM );
    }
    w_preview = catacurses::window();
    prepared = false;
}

bool character_preview_window::clothes_showing() const
{
    return show_clothes;
}

bool character_preview_window::visible() const
{
    return prepared && TERMX - ncols_width >= hide_below_ncols;
}

int character_preview_window::width() const
{
    return visible() ? ncols_width : 0;
}

#endif // TILES
