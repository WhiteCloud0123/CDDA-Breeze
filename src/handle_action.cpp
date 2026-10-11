#include "game.h" // IWYU pragma: associated

#include <algorithm>
#include <chrono>
#include <cmath>
#include <initializer_list>
#include <map>
#include <sstream>
#include <string>
#include <thread>
#include <utility>

#include "action.h"
#include "achievement.h"
#include "activity_actor_definitions.h"
#include "activity_type.h"
#include "advanced_inv.h"
#include "auto_note.h"
#include "auto_pickup.h"
#include "avatar.h"
#include "avatar_action.h"
#include "bionics.h"
#include "bodygraph.h"
#include "bodypart.h"
#include "cached_options.h"
#include "calendar.h"
#include "catacharset.h"
#include "character.h"
#include "character_martial_arts.h"
#include "clzones.h"
#include "colony.h"
#include "color.h"
#include "construction.h"
#include "creature_tracker.h"
#include "creature.h"
#include "creature_throw.h"
#include "cursesdef.h"
#include "cursesport.h"
#include "d20_roll.h"
#include "damage.h"
#include "debug.h"
#include "debug_menu.h"
#include "diary.h"
#include "distraction_manager.h"
#include "do_turn.h"
#include "event.h"
#include "event_bus.h"
#include "faction.h"
#include "field.h"
#include "field_type.h"
#include "flag.h"
#include "game_constants.h"
#include "game_inventory.h"
#include "gamemode.h"
#include "gates.h"
#include "gun_mode.h"
#include "help.h"
#include "input.h"
#include "item.h"
#include "item_group.h"
#include "itype.h"
#include "iuse.h"
#include "level_cache.h"
#include "lightmap.h"
#include "line.h"
#include "magic.h"
#include "make_static.h"
#include "map.h"
#include "map_iterator.h"
#include "mapdata.h"
#include "mapsharing.h"
#include "messages.h"
#include "memory_fast.h"
#include "monster.h"
#include "move_mode.h"
#include "mtype.h"
#include "npc.h"
#include "mutation.h"
#include "options.h"
#include "output.h"
#include "overmap_ui.h"
#include "parallel_hashmap/btree.h"
#include "panels.h"
#include "player_activity.h"
#include "popup.h"
#include "ranged.h"
#include "rng.h"
#include "safemode_ui.h"
#include "scores_ui.h"
#include "sdltiles.h"
#include "sounds.h"
#include "string_formatter.h"
#include "timed_event.h"
#include "translations.h"
#include "ui.h"
#include "ui_manager.h"
#include "units.h"
#include "value_ptr.h"
#include "veh_type.h"
#include "vehicle.h"
#include "vpart_position.h"
#include "vpart_range.h"
#include "weather.h"
#include "weather_type.h"
#include "worldfactory.h"
#include "monstergenerator.h"
#include "item_factory.h"


#include"network.h"

static const activity_id ACT_FERTILIZE_PLOT("ACT_FERTILIZE_PLOT");
static const activity_id ACT_MOVE_LOOT("ACT_MOVE_LOOT");
static const activity_id ACT_MULTIPLE_BUTCHER("ACT_MULTIPLE_BUTCHER");
static const activity_id ACT_MULTIPLE_CHOP_PLANKS("ACT_MULTIPLE_CHOP_PLANKS");
static const activity_id ACT_MULTIPLE_CHOP_TREES("ACT_MULTIPLE_CHOP_TREES");
static const activity_id ACT_MULTIPLE_CONSTRUCTION("ACT_MULTIPLE_CONSTRUCTION");
static const activity_id ACT_MULTIPLE_DIS("ACT_MULTIPLE_DIS");
static const activity_id ACT_MULTIPLE_FARM("ACT_MULTIPLE_FARM");
static const activity_id ACT_MULTIPLE_MINE("ACT_MULTIPLE_MINE");
static const activity_id ACT_MULTIPLE_MOP("ACT_MULTIPLE_MOP");
static const activity_id ACT_PULP("ACT_PULP");
static const activity_id ACT_SPELLCASTING("ACT_SPELLCASTING");
static const activity_id ACT_VEHICLE_DECONSTRUCTION("ACT_VEHICLE_DECONSTRUCTION");
static const activity_id ACT_VEHICLE_REPAIR("ACT_VEHICLE_REPAIR");
static const activity_id ACT_WAIT("ACT_WAIT");
static const activity_id ACT_WAIT_STAMINA("ACT_WAIT_STAMINA");
static const activity_id ACT_WAIT_WEATHER("ACT_WAIT_WEATHER");

static const bionic_id bio_remote("bio_remote");

static const efftype_id effect_alarm_clock("alarm_clock");
static const efftype_id effect_blind("blind");
static const efftype_id effect_controlled_for_shove("controlled_for_shove");
static const efftype_id effect_downed("downed");
static const efftype_id effect_grabbed("grabbed");
static const efftype_id effect_grabbed_by_player("grabbed_by_player");
static const efftype_id effect_grabbing("grabbing");
static const efftype_id effect_player_grab_fresh("player_grab_fresh");
static const efftype_id effect_incorporeal("incorporeal");
static const efftype_id effect_no_sight("no_sight");
static const efftype_id effect_laserlocked("laserlocked");
static const efftype_id effect_relax_gas("relax_gas");
static const efftype_id effect_stunned("stunned");

static const flag_id json_flag_MOP("MOP");

static const gun_mode_id gun_mode_AUTO("AUTO");

static const itype_id fuel_type_animal("animal");
static const itype_id itype_radiocontrol("radiocontrol");
static const itype_id itype_monster_controller_d("怪物遥控器-D");

static const json_character_flag json_flag_ALARMCLOCK("ALARMCLOCK");

static const material_id material_glass("glass");

static const proficiency_id proficiency_prof_helicopter_pilot("prof_helicopter_pilot");

static const quality_id qual_CUT("CUT");

static const skill_id skill_melee("melee");
static const skill_id skill_throw("throw");
static const skill_id skill_unarmed("unarmed");

static const trait_id trait_BRAWLER("BRAWLER");
static const trait_id trait_HIBERNATE("HIBERNATE");
static const trait_id trait_PROF_CHURL("PROF_CHURL");
static const trait_id trait_SHELL2("SHELL2");
static const trait_id trait_SHELL3("SHELL3");
static const trait_id trait_WATERSLEEP("WATERSLEEP");
static const trait_id trait_WATERSLEEPER("WATERSLEEPER");
static const trait_id trait_WAYFARER("WAYFARER");

static const zone_type_id zone_type_CHOP_TREES("CHOP_TREES");
static const zone_type_id zone_type_CONSTRUCTION_BLUEPRINT("CONSTRUCTION_BLUEPRINT");
static const zone_type_id zone_type_FARM_PLOT("FARM_PLOT");
static const zone_type_id zone_type_LOOT_CORPSE("LOOT_CORPSE");
static const zone_type_id zone_type_LOOT_UNSORTED("LOOT_UNSORTED");
static const zone_type_id zone_type_LOOT_WOOD("LOOT_WOOD");
static const zone_type_id zone_type_MINING("MINING");
static const zone_type_id zone_type_MOPPING("MOPPING");
static const zone_type_id zone_type_VEHICLE_DECONSTRUCT("VEHICLE_DECONSTRUCT");
static const zone_type_id zone_type_VEHICLE_REPAIR("VEHICLE_REPAIR");
static const zone_type_id zone_type_zone_disassemble("zone_disassemble");
static const zone_type_id zone_type_zone_strip("zone_strip");
static const zone_type_id zone_type_zone_unload_all("zone_unload_all");

static const std::string flag_CANT_DRAG("CANT_DRAG");

static const species_id species_ZOMBIE("ZOMBIE");
static const efftype_id effect_pet("pet");
static const efftype_id effect_wait_here("在这里等待");

static const mtype_id mon_devourer("mon_devourer");

#define dbg(x) DebugLog((x),D_GAME) << __FILE__ << ":" << __LINE__ << ": "

#if defined(__ANDROID__)
extern phmap::btree_map<std::string, std::list<input_event>> quick_shortcuts_map;
extern bool add_best_key_for_action_to_quick_shortcuts(action_id action,
    const std::string& category, bool back);
extern bool add_key_to_quick_shortcuts(int key, const std::string& category, bool back);
#endif




class monster_data_retrieval_ui_callback : public uilist_callback
{
public:
    // last menu entry
    int lastent;
    // feedback message
    std::string msg;
    // spawn friendly critter?
    bool friendly;
    bool hallucination;
    // Number of monsters to spawn.
    int group;
    // scrap critter for monster::print_info
    monster tmp;

    // 用来决定当前信息页面展示的信息
    int index = 0;



    const std::vector<const mtype*>& mtypes;

    explicit monster_data_retrieval_ui_callback(const std::vector<const mtype*>& mtypes)
        : mtypes(mtypes) {
        friendly = false;
        hallucination = false;
        group = 0;
        lastent = -2;
    }

    bool key(const input_context&, const input_event& event, int /*entnum*/,
        uilist* /*menu*/) override {
        if (event.get_first_input() == '>') {

            index = index + 1;

            if (index > 1) {

                index = 1;

            }

            return true;
        }
        else if (event.get_first_input() == '<') {

            index = index - 1;

            if (index < 0) {

                index = 0;

            }

            return true;
        }

        return false;
    }

    void refresh(uilist* menu) override {
        catacurses::window w_info = catacurses::newwin(menu->w_height - 2, menu->pad_right,
            point(menu->w_x + menu->w_width - 1 - menu->pad_right, 1));

        const int entnum = menu->selected;
        const bool valid_entnum = entnum >= 0 && static_cast<size_t>(entnum) < mtypes.size();
        if (entnum != lastent) {
            lastent = entnum;
            if (valid_entnum) {
                tmp = monster(mtypes[entnum]->id);
                if (friendly) {
                    tmp.friendly = -1;
                }
            }
            else {
                tmp = monster();
            }
        }

        werase(w_info);

        draw_border(w_info);



        if (valid_entnum) {
            tmp.print_info(w_info, 2, 5, 1);

            std::string header = string_format("#%d: %s (%d)%s", entnum, tmp.type->id.str(),
                group, hallucination ? _(" (hallucination)") : "");
            mvwprintz(w_info, point((getmaxx(w_info) - utf8_width(header)) / 2, 0), c_cyan, header);

        }

        if (valid_entnum) {
            if (index == 0) {

                std::ostringstream oss;

                oss << tmp.type->get_description();
                /*oss << "\n";
                oss << "\n";
                oss << "物种 : ";
                oss<<"";*/



                if (!tmp.type->petfood.food.empty()) {

                    oss << "\n";
                    oss << "\n";

                    oss << "可驯服          食物 : ";
                    for (const std::string& food_ref : tmp.type->petfood.food) {

                        oss << food_ref << " ";

                    }


                }


                oss << "\n";
                oss << "\n";
                oss << "<color_white>" << "HP : " << "</color>";
                oss << "<color_white>" << tmp.get_hp_max() << "</color>";
                oss << "\n";
                oss << "\n";
                oss << "白天视野 : ";
                oss << tmp.type->vision_day;
                oss << "          ";
                oss << "夜间视野 : ";
                oss << tmp.type->vision_night;
                oss << "\n";
                oss << "\n";
                oss << "士气 : ";
                oss << tmp.type->morale;


                oss << "\n";
                oss << "\n";
                oss << "近战技能 : ";
                oss << tmp.type->melee_skill;
                oss << "\n";
                oss << "\n";
                oss << "近战骰子面数 : ";
                oss << tmp.type->melee_sides;
                oss << "          ";
                oss << "近战骰子个数 : ";
                oss << tmp.type->melee_dice;
                oss << "\n";
                oss << "\n";
                oss << "闪避技能 : ";
                oss << tmp.get_dodge_base();
                oss << "\n";
                oss << "\n";
                oss << "速度 : ";
                oss << tmp.get_speed_base();
                oss << "\n";
                oss << "\n";
                oss << "钝击防御 : ";
                oss << tmp.type->armor_bash;
                oss << "          ";
                oss << "刺击防御 : ";
                oss << tmp.type->armor_stab;

                if (tmp.type->regenerates != 0) {
                    oss << "\n";
                    oss << "\n";
                    oss << "再生能力 : ";
                    oss << tmp.type->regenerates;
                }

                oss << "\n";
                oss << "\n";
                oss << "威胁度 : ";
                oss << tmp.type->difficulty;



                fold_and_print(w_info, point(1, 9), getmaxx(w_info) - 2, c_white, oss.str());

            }
            else if (index == 1) {


                std::ostringstream oss;

                if (!tmp.type->death_drops.is_empty()) {


                    oss << "<color_white>" << "掉落物: " << "</color>";

                    for (const auto& item : item_group::every_possible_item_from(tmp.type->death_drops)) {

                        oss << "<color_green>" << item::nname(item->get_id()) << "</color>";
                        oss << "<color_green>" << "    " << "</color>";



                    }


                }

                fold_and_print(w_info, point(1, 9), getmaxx(w_info) - 2, c_white, oss.str());








            }

        }













        wnoutrefresh(w_info);
    }






    ~monster_data_retrieval_ui_callback() override = default;
};




class user_turn
{

private:
    std::chrono::time_point<std::chrono::steady_clock> user_turn_start;
public:
    user_turn() {
        user_turn_start = std::chrono::steady_clock::now();
    }

    bool has_timeout_elapsed() {
        return moves_elapsed() > 100;
    }

    int moves_elapsed() {
        const float turn_duration = get_option<float>("TURN_DURATION");
        // Magic number 0.005 chosen due to option menu's 2 digit precision and
        // the option menu UI rounding <= 0.005 down to "0.00" in the display.
        // This conditional will catch values (e.g. 0.003) that the options menu
        // would round down to "0.00" in the options menu display. This prevents
        // the user from being surprised by floating point rounding near zero.
        if (turn_duration <= 0.005) {
            return 0;
        }
        std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
        std::chrono::milliseconds elapsed_ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(now - user_turn_start);
        return elapsed_ms.count() / (10.0 * turn_duration);
    }

};

input_context game::get_player_input(std::string& action)
{
    input_context ctxt;
    if (uquit == QUIT_WATCH) {
        ctxt = input_context("DEFAULTMODE", keyboard_mode::keycode);
        ctxt.set_iso(true);
        // The list of allowed actions in death-cam mode in game::handle_action
        // *INDENT-OFF*
        for (const action_id id : {
            ACTION_TOGGLE_MAP_MEMORY,
                ACTION_CENTER,
                ACTION_SHIFT_N,
                ACTION_SHIFT_NE,
                ACTION_SHIFT_E,
                ACTION_SHIFT_SE,
                ACTION_SHIFT_S,
                ACTION_SHIFT_SW,
                ACTION_SHIFT_W,
                ACTION_SHIFT_NW,
                ACTION_LOOK,
                ACTION_KEYBINDINGS,
        }) {
            ctxt.register_action(action_ident(id));
        }
        // *INDENT-ON*
        ctxt.register_action("QUIT", to_translation("Accept your fate"));
    }
    else {
        ctxt = get_default_mode_input_context();
    }

    m.update_visibility_cache(u.posz());
    const visibility_variables& cache = m.get_visibility_variables_cache();
    const level_cache& map_cache = m.get_cache_ref(u.posz());
    const auto& visibility_cache = map_cache.visibility_cache;

    user_turn current_turn;

    if (use_animation) {
        const int TOTAL_VIEW = MAX_VIEW_DISTANCE * 2 + 1;
        point iStart((TERRAIN_WINDOW_WIDTH > TOTAL_VIEW) ? (TERRAIN_WINDOW_WIDTH - TOTAL_VIEW) / 2 : 0,
            (TERRAIN_WINDOW_HEIGHT > TOTAL_VIEW) ? (TERRAIN_WINDOW_HEIGHT - TOTAL_VIEW) / 2 :
            0);
        point iEnd((TERRAIN_WINDOW_WIDTH > TOTAL_VIEW) ? TERRAIN_WINDOW_WIDTH -
            (TERRAIN_WINDOW_WIDTH - TOTAL_VIEW) /
            2 :
            TERRAIN_WINDOW_WIDTH, (TERRAIN_WINDOW_HEIGHT > TOTAL_VIEW) ? TERRAIN_WINDOW_HEIGHT -
            (TERRAIN_WINDOW_HEIGHT - TOTAL_VIEW) /
            2 : TERRAIN_WINDOW_HEIGHT);

        if (fullscreen) {
            iStart.x = 0;
            iStart.y = 0;
            iEnd.x = TERMX;
            iEnd.y = TERMY;
        }

        //x% of the Viewport, only shown on visible areas
        const weather_animation_t weather_info = weather.weather_id->weather_animation;
        point offset(u.view_offset.xy() + point(-getmaxx(w_terrain) / 2 + u.posx(),
            -getmaxy(w_terrain) / 2 + u.posy()));

        if (g->is_tileset_isometric()) {
            iStart.x = 0;
            iStart.y = 0;
            iEnd.x = MAPSIZE_X;
            iEnd.y = MAPSIZE_Y;
            offset.x = 0;
            offset.y = 0;
        }


        // TODO: Move the weather calculations out of here.
        const bool bWeatherEffect = weather_info.symbol != NULL_UNICODE;
        const int dropCount = static_cast<int>(iEnd.x * iEnd.y * weather_info.factor);

        weather_printable wPrint;
        wPrint.colGlyph = weather_info.color;
        wPrint.cGlyph = weather_info.symbol;
        wPrint.wtype = weather.weather_id;
        wPrint.vdrops.clear();

        ctxt.set_timeout(125);

        shared_ptr_fast<game::draw_callback_t> animation_cb =
            make_shared_fast<game::draw_callback_t>([&]() {
            draw_weather(wPrint);

            if (uquit != QUIT_WATCH) {
                draw_sct();
            }
                });
        add_draw_callback(animation_cb);

        creature_tracker& creatures = get_creature_tracker();
        do {
            if (bWeatherEffect && get_option<bool>("ANIMATION_RAIN")) {
                /*
                Location to add rain drop animation bits! Since it refreshes w_terrain it can be added to the animation section easily
                Get tile information from above's weather information:
                WEATHER_ACID_DRIZZLE | WEATHER_ACID_RAIN = "weather_acid_drop"
                WEATHER_DRIZZLE | WEATHER_LIGHT_DRIZZLE | WEATHER_RAINY | WEATHER_RAINSTORM | WEATHER_THUNDER | WEATHER_LIGHTNING = "weather_rain_drop"
                WEATHER_FLURRIES | WEATHER_SNOW | WEATHER_SNOWSTORM = "weather_snowflake"
                */
                invalidate_main_ui_adaptor();

                wPrint.vdrops.clear();

                for (int i = 0; i < dropCount; i++) {
                    const point iRand(rng(iStart.x, iEnd.x - 1), rng(iStart.y, iEnd.y - 1));
                    const point map(iRand + offset);

                    const tripoint mapp(map, u.posz());

                    const lit_level lighting = visibility_cache[mapp.x][mapp.y];

                    if (m.is_outside(mapp) && m.get_visibility(lighting, cache) == visibility_type::CLEAR &&
                        !creatures.creature_at(mapp, true)) {
                        // Suppress if a critter is there
                        wPrint.vdrops.emplace_back(std::make_pair(iRand.x, iRand.y));
                    }
                }
            }
            // don't bother calculating SCT if we won't show it
            if (uquit != QUIT_WATCH && get_option<bool>("ANIMATION_SCT") && !SCT.vSCT.empty()) {
                invalidate_main_ui_adaptor();

                SCT.advanceAllSteps();

                //Check for creatures on all drawing positions and offset if necessary
                for (auto iter = SCT.vSCT.rbegin(); iter != SCT.vSCT.rend(); ++iter) {
                    const direction oCurDir = iter->getDirection();
                    const int width = utf8_width(iter->getText());
                    for (int i = 0; i < width; ++i) {
                        tripoint tmp(iter->getPosX() + i, iter->getPosY(), get_map().get_abs_sub().z());
                        const Creature* critter = creatures.creature_at(tmp, true);

                        if (critter != nullptr && u.sees(*critter)) {
                            i = -1;
                            int iPos = iter->getStep() + iter->getStepOffset();
                            for (auto iter2 = iter; iter2 != SCT.vSCT.rend(); ++iter2) {
                                if (iter2->getDirection() == oCurDir &&
                                    iter2->getStep() + iter2->getStepOffset() <= iPos) {
                                    if (iter2->getType() == "hp") {
                                        iter2->advanceStepOffset();
                                    }

                                    iter2->advanceStepOffset();
                                    iPos = iter2->getStep() + iter2->getStepOffset();
                                }
                            }
                        }
                    }
                }
            }

            if (pixel_minimap_option) {
                // TODO: more granular control to only redraw pixel minimap
                invalidate_main_ui_adaptor();
            }

            std::unique_ptr<static_popup> deathcam_msg_popup;
            if (uquit == QUIT_WATCH) {
                deathcam_msg_popup = std::make_unique<static_popup>();
                deathcam_msg_popup
                    ->wait_message(c_red, _("Press %s to accept your fate…"), ctxt.get_desc("QUIT"))
                    .on_top(true);
            }

            ui_manager::redraw_invalidated();
        } while (handle_mouseview(ctxt, action) && uquit != QUIT_WATCH
            && (action != "TIMEOUT" || !current_turn.has_timeout_elapsed()));
        ctxt.reset_timeout();
    }
    else {
        ctxt.set_timeout(125);
        while (handle_mouseview(ctxt, action)) {
            if (action == "TIMEOUT" && current_turn.has_timeout_elapsed()) {
                break;
            }
        }
        ctxt.reset_timeout();
    }

    return ctxt;
}

static void rcdrive(const point& d)
{
    Character& player_character = get_player_character();
    map& here = get_map();
    std::stringstream car_location_string(player_character.get_value("remote_controlling"));

    if (car_location_string.str().empty()) {
        //no turned radio car found
        add_msg(m_warning, _("No radio car connected."));
        return;
    }
    tripoint c;
    car_location_string >> c.x >> c.y >> c.z;

    auto rc_pairs = here.get_rc_items(c);
    auto rc_pair = rc_pairs.begin();
    for (; rc_pair != rc_pairs.end(); ++rc_pair) {
        if (rc_pair->second->has_flag(flag_RADIOCAR) && rc_pair->second->active) {
            break;
        }
    }
    if (rc_pair == rc_pairs.end()) {
        add_msg(m_warning, _("No radio car connected."));
        player_character.remove_value("remote_controlling");
        return;
    }
    item* rc_car = rc_pair->second;

    tripoint dest(c + d);
    if (here.impassable(dest) || !here.can_put_items_ter_furn(dest) ||
        here.has_furn(dest)) {
        sounds::sound(dest, 7, sounds::sound_t::combat,
            _("sound of a collision with an obstacle."), true, "misc", "rc_car_hits_obstacle");
        return;
    }
    else if (!here.add_item_or_charges(dest, *rc_car).is_null()) {
        tripoint src(c);
        //~ Sound of moving a remote controlled car
        sounds::sound(src, 6, sounds::sound_t::movement, _("zzz…"), true, "misc", "rc_car_drives");
        player_character.moves -= 50;
        here.i_rem(src, rc_car);
        car_location_string.clear();
        car_location_string << dest.x << ' ' << dest.y << ' ' << dest.z;
        player_character.set_value("remote_controlling", car_location_string.str());
        return;
    }
}

static void pldrive(const tripoint& p)
{
    if (!g->check_safe_mode_allowed()) {
        return;
    }
    vehicle* veh = g->remoteveh();
    bool remote = true;
    int part = -1;
    Character& player_character = get_player_character();
    map& here = get_map();
    if (!veh) {
        if (const optional_vpart_position vp = here.veh_at(player_character.pos())) {
            veh = &vp->vehicle();
            part = vp->part_index();
        }
        remote = false;
    }
    if (!veh) {
        dbg(D_ERROR) << "game::pldrive: can't find vehicle!  Drive mode is now off.";
        debugmsg("game::pldrive error: can't find vehicle!  Drive mode is now off.");
        player_character.in_vehicle = false;
        return;
    }
    if (veh->is_on_ramp && p.x != 0) {
        add_msg(m_bad, _("You can't turn the vehicle while on a ramp."));
        return;
    }
    if (!remote) {
        const bool has_animal_controls = veh->part_with_feature(part, "CONTROL_ANIMAL", true) >= 0;
        const bool has_controls = veh->part_with_feature(part, "CONTROLS", true) >= 0;
        const bool has_animal = veh->has_engine_type(fuel_type_animal, false) &&
            veh->has_harnessed_animal();
        if (!has_controls && !has_animal_controls) {
            add_msg(m_info, _("You can't drive the vehicle from here.  You need controls!"));
            player_character.controlling_vehicle = false;
            return;
        }
        else if (!has_controls && has_animal_controls && !has_animal) {
            add_msg(m_info, _("You can't drive this vehicle without an animal to pull it."));
            player_character.controlling_vehicle = false;
            return;
        }
    }
    else {
        if (empty(veh->get_avail_parts("REMOTE_CONTROLS"))) {
            add_msg(m_info, _("Can't drive this vehicle remotely.  It has no working controls."));
            return;
        }
    }
    if (p.z != 0) {
        if (!veh->is_airship() && !player_character.has_proficiency(proficiency_prof_helicopter_pilot)) {
            player_character.add_msg_if_player(m_info, _("You have no idea how to make the vehicle fly."));
            return;
        }
        if (!veh->is_flyable()) {
            player_character.add_msg_if_player(m_info, _("This vehicle doesn't look very airworthy."));
            return;
        }
    }
    if (p.z == -1) {
        if (veh->check_heli_descend(player_character)) {
            player_character.add_msg_if_player(m_info, _("You steer the vehicle into a descent."));
        }
        else {
            return;
        }
    }
    else if (p.z == 1) {
        if (veh->check_heli_ascend(player_character)) {
            player_character.add_msg_if_player(m_info, _("You steer the vehicle into an ascent."));
        }
        else {
            return;
        }
    }
    veh->pldrive(get_avatar(), p.xy(), p.z);
}

static void pldrive(point d)
{
    return pldrive(tripoint(d, 0));
}

static bool is_mounted_passage_openable( map &here, const tripoint &p )
{
    // 车门仍按载具交互处理。地形与家具只要当前状态或开关后的状态属于门窗，
    // 就允许骑乘时操作，从而同时识别关闭和已经打开的门窗。
    if( here.veh_at( p ) ) {
        return false;
    }

    const ter_t &ter = here.ter( p ).obj();
    const furn_t &furn = here.furn( p ).obj();
    const auto is_passage = []( const map_data_common_t &data ) {
        return data.has_flag( ter_furn_flag::TFLAG_DOOR ) ||
               data.has_flag( ter_furn_flag::TFLAG_WINDOW );
    };

    return is_passage( ter ) || is_passage( furn ) ||
           ( ter.open && is_passage( ter.open.obj() ) ) ||
           ( ter.close && is_passage( ter.close.obj() ) ) ||
           ( furn.open && is_passage( furn.open.obj() ) ) ||
           ( furn.close && is_passage( furn.close.obj() ) );
}

static void open()
{
    avatar& player_character = get_avatar();
    const std::optional<tripoint> openp_ = choose_adjacent_highlight(_("Open where?"),
        pgettext("no door, gate, curtain, etc.", "There is nothing that can be opened nearby."),
        ACTION_OPEN, false);

    if (!openp_) {
        return;
    }
    const tripoint openp = *openp_;
    map& here = get_map();

    if( player_character.is_mounted() && !is_mounted_passage_openable( here, openp ) ) {
        add_msg( m_info, _( "骑乘时只能开门或开窗。" ) );
        return;
    }

    player_character.moves -= 100;

    // Is a vehicle part here?
    if (const optional_vpart_position vp = here.veh_at(openp)) {
        vehicle* const veh = &vp->vehicle();
        // Check for potential thievery, and restore moves if action is canceled
        if (!veh->handle_potential_theft(player_character)) {
            player_character.moves += 100;
            return;
        }
        // Check if vehicle has a part here that can be opened
        int openable = veh->next_part_to_open(vp->part_index());
        if (openable >= 0) {
            // If player is inside vehicle, open the door/window/curtain
            const vehicle* player_veh = veh_pointer_or_null(here.veh_at(player_character.pos()));
            const std::string part_name = veh->part(openable).name();
            bool outside = !player_veh || player_veh != veh;
            if (!outside) {
                veh->open(openable);
                //~ %1$s - vehicle name, %2$s - part name
                player_character.add_msg_if_player(_("You open the %1$s's %2$s."), veh->name, part_name);
            }
            else {
                // Outside means we check if there's anything in that tile outside-openable.
                // If there is, we open everything on tile. This means opening a closed,
                // curtained door from outside is possible, but it will magically open the
                // curtains as well.
                int outside_openable = veh->next_part_to_open(vp->part_index(), true);
                if (outside_openable == -1) {
                    add_msg(m_info, _("That %s can only be opened from the inside."), part_name);
                    player_character.moves += 100;
                }
                else {
                    veh->open_all_at(openable);
                    //~ %1$s - vehicle name, %2$s - part name
                    player_character.add_msg_if_player(_("You open the %1$s's %2$s."), veh->name, part_name);
                }
            }
        }
        else {
            // If there are any OPENABLE parts here, they must be already open
            if (const std::optional<vpart_reference> already_open = vp.part_with_feature("OPENABLE",
                true)) {
                const std::string name = already_open->info().name();
                add_msg(m_info, _("That %s is already open."), name);
            }
            player_character.moves += 100;
        }
        return;
    }
    // Not a vehicle part, just a regular door
    bool didit = here.open_door(player_character, openp, !here.is_outside(player_character.pos()));
    if (didit) {
        player_character.add_msg_if_player(_("You open the %s."), here.name(openp));
    }
    else {
        const ter_str_id tid = here.ter(openp).id();

        if (here.has_flag(ter_furn_flag::TFLAG_LOCKED, openp)) {
            add_msg(m_info, _("The door is locked!"));
            return;
        }
        else if (tid.obj().close) {
            // if the following message appears unexpectedly, the prior check was for t_door_o
            add_msg(m_info, _("That door is already open."));
            player_character.moves += 100;
            return;
        }
        add_msg(m_info, _("No door there."));
        player_character.moves += 100;
    }
}

static void close()
{
    if( const std::optional<tripoint> pnt = choose_adjacent_highlight( _( "Close where?" ),
            pgettext( "no door, gate, etc.", "There is nothing that can be closed nearby." ),
            ACTION_CLOSE, false ) ) {
        map &here = get_map();
        avatar &player_character = get_avatar();
        if( player_character.is_mounted() && !is_mounted_passage_openable( here, *pnt ) ) {
            add_msg( m_info, _( "骑乘时只能关门或关窗。" ) );
            return;
        }
        doors::close_door( here, player_character, *pnt );
    }
}


namespace
{

Creature *find_player_grabbed_creature( avatar &you )
{
    if( !you.has_effect( effect_grabbing ) ) {
        return nullptr;
    }
    creature_tracker &creatures = get_creature_tracker();
    map &here = get_map();
    for( const tripoint &p : here.points_in_radius( you.pos(), 1, 0 ) ) {
        Creature *const target = creatures.creature_at<Creature>( p );
        if( target != nullptr && target != &you && target->has_effect( effect_grabbed_by_player ) ) {
            return target;
        }
    }
    return nullptr;
}

Creature *find_player_controlled_creature( avatar &you )
{
    creature_tracker &creatures = get_creature_tracker();
    map &here = get_map();
    for( const tripoint &p : here.points_in_radius( you.pos(), 1, 0 ) ) {
        Creature *const target = creatures.creature_at<Creature>( p );
        if( target != nullptr && target != &you &&
            target->has_effect( effect_controlled_for_shove ) ) {
            return target;
        }
    }
    return nullptr;
}

bool ipman_throw_specialist( const avatar &you )
{
    static const matype_id style_ipman( "style_wc_rework_ipman" );
    return you.martial_arts_data->selected_is_style( style_ipman ) &&
           you.get_skill_level( skill_unarmed ) >= 5;
}

void release_player_creature_grab( avatar &you, Creature *target )
{
    if( target != nullptr ) {
        target->remove_effect( effect_grabbed_by_player );
        target->remove_effect( effect_grabbed );
    }
    you.remove_effect( effect_grabbing );
}

int d20_dex_modifier( const int dex )
{
    if( dex >= 8 ) {
        return ( dex - 8 ) / 3;
    }
    return -( ( 8 - dex + 2 ) / 3 );
}

int d20_target_size_modifier( const Creature &target )
{
    switch( target.get_size() ) {
        case creature_size::tiny:
            return -3;
        case creature_size::small:
            return -1;
        case creature_size::medium:
            return 0;
        case creature_size::large:
            return 1;
        case creature_size::huge:
            return 2;
        case creature_size::num_sizes:
            break;
    }
    return 0;
}

int d20_dodge_difficulty( const Creature &target )
{
    return std::max( 0, static_cast<int>(
                         std::ceil( std::max( 0.0f, target.get_dodge() ) / 3.0f ) ) );
}

enum class contact_roll_result {
    hit,
    inaccurate,
    near_miss,
    dodged
};

contact_roll_result contact_d20_result( const avatar &you, const Creature &target,
                                       const int skill_bonus,
                                       const int distance_penalty,
                                       const int flat_bonus,
                                       const bool show_state_message )
{
    // 未被目标发现时同样占据优势（与射击、弱点判定同源）
    const bool unseen = !target.sees( you );
    const d20_roll_state state = get_d20_roll_state( you, target, unseen );
    if( show_state_message ) {
        if( state == d20_roll_state::advantage ) {
            add_msg( m_good, _( "你在这次抓取中占据优势。" ) );
        } else if( state == d20_roll_state::disadvantage ) {
            add_msg( m_warning, _( "你在这次抓取中处于劣势。" ) );
        }
    }

    const int roll = roll_d20( state );
    if( roll == 1 ) {
        return contact_roll_result::inaccurate;
    }
    if( roll == 20 ) {
        return contact_roll_result::hit;
    }

    const int attack_score = roll + skill_bonus +
                             d20_dex_modifier( you.get_dex() ) +
                             flat_bonus - distance_penalty;
    const int silhouette_dc = std::clamp(
                                  2 - d20_target_size_modifier( target ), 2, 19 );
    const int final_dc = std::clamp(
                             silhouette_dc + d20_dodge_difficulty( target ), 2, 19 );

    if( attack_score < 2 ) {
        return contact_roll_result::inaccurate;
    }
    if( attack_score < silhouette_dc ) {
        return contact_roll_result::near_miss;
    }
    if( attack_score < final_dc ) {
        return contact_roll_result::dodged;
    }
    return contact_roll_result::hit;
}

bool grab_contact_hits( const avatar &you, const Creature &target,
                        const bool armed_control, const int flat_bonus )
{
    const int skill = you.get_skill_level( armed_control ? skill_melee : skill_unarmed );
    return contact_d20_result(
               you, target, skill / 3, 0, flat_bonus, true ) ==
           contact_roll_result::hit;
}

void mark_player_attack_attempt( avatar &you, Creature &target )
{
    if( npc *const guy = target.as_npc(); guy != nullptr ) {
        guy->on_attacked( you );
    } else if( monster *const mon = target.as_monster(); mon != nullptr ) {
        mon->on_hit( &you, body_part_torso.id() );
    }
}

void credit_player_collision_death( avatar &you, Creature &target )
{
    if( target.is_dead_state() ) {
        target.die( &you );
    }
}

bool confirm_nonhostile_npc_on_trajectory(
    avatar &you, const target_handler::trajectory &trajectory )
{
    creature_tracker &creatures = get_creature_tracker();
    for( const tripoint &p : trajectory ) {
        npc *const guy = creatures.creature_at<npc>( p );
        if( guy != nullptr && !guy->is_enemy() ) {
            return query_yn( _( "这样做会被视为攻击，继续吗？" ) );
        }
    }
    return true;
}

void complete_jackie_chan_achievement()
{
    static const achievement_id achievement_jackie_chan( "achievement_jackie_chan" );
    if( !achievement_jackie_chan.is_valid() ) {
        return;
    }

    achievements_tracker &tracker = get_achievements();
    if( tracker.is_completed( achievement_jackie_chan ) == achievement_completion::pending ) {
        tracker.report_achievement(
            &achievement_jackie_chan.obj(), achievement_completion::completed );
    }
}

int creature_weight_kg( const Creature &target )
{
    return std::max( 1, static_cast<int>( units::to_gram( target.get_weight() ) / 1000 ) );
}

int push_weight_stability_bonus( const Creature &target )
{
    const int kg = creature_weight_kg( target );
    if( kg < 40 ) {
        return -2;
    }
    if( kg <= 90 ) {
        return 0;
    }
    if( kg <= 150 ) {
        return 2;
    }
    if( kg <= 250 ) {
        return 4;
    }
    if( kg <= 400 ) {
        return 6;
    }
    return 8;
}

int push_stamina_cost( const avatar &you, const Creature &target, const bool armed )
{
    const int skill = you.get_skill_level( armed ? skill_melee : skill_unarmed );
    const int kg = creature_weight_kg( target );
    const int base = 220 + std::max( 0, kg - 50 ) * 2;
    const float efficiency = std::clamp( 1.0f - skill * 0.05f, 0.60f, 1.0f );
    return std::clamp( static_cast<int>( base * efficiency ), 120, 900 );
}

void break_monster_grab_on_player( avatar &you, Creature &target )
{
    monster *const pushed_mon = target.as_monster();
    if( pushed_mon == nullptr || !pushed_mon->has_effect( effect_grabbing ) ) {
        return;
    }

    pushed_mon->remove_effect( effect_grabbing );

    creature_tracker &creatures = get_creature_tracker();
    map &here = get_map();
    bool another_grabber_remains = false;
    for( const tripoint &p : here.points_in_radius( you.pos(), 1, 0 ) ) {
        const monster *const mon = creatures.creature_at<monster>( p );
        if( mon != nullptr && mon != pushed_mon && mon->has_effect( effect_grabbing ) ) {
            another_grabber_remains = true;
            break;
        }
    }

    if( !another_grabber_remains ) {
        you.remove_effect( effect_grabbed );
    }
    add_msg( m_good, _( "%s的抓取被打断了。" ), target.disp_name() );
}

bool target_can_be_pushed_down( const avatar &you, const Creature &target )
{
    if( target.is_immune_effect( effect_downed ) ) {
        return false;
    }
    if( const monster *const mon = target.as_monster(); mon != nullptr && mon->flies() ) {
        return false;
    }

    const int size_delta = static_cast<int>( target.get_size() ) -
                           static_cast<int>( you.get_size() );
    if( size_delta > 0 ) {
        const int required_strength = 12 + ( size_delta - 1 ) * 4;
        if( you.get_arm_str() < required_strength ) {
            return false;
        }
    }

    const int kg = creature_weight_kg( target );
    const int rough_weight_limit = you.get_arm_str() * 20 + 50;
    return kg <= rough_weight_limit;
}

bool attempt_pushdown( avatar &you, Creature &target, const bool armed,
                       const bool release_full_grab )
{
    const int full_cost = push_stamina_cost( you, target, armed );
    const int failure_cost = std::max( 80, full_cost * 7 / 10 );
    you.mod_moves( -100 );

    if( !target_can_be_pushed_down( you, target ) ) {
        you.mod_stamina( -failure_cost );
        add_msg( m_info, _( "%s太稳或太沉重，你没能将其推倒。" ), target.disp_name() );
        if( release_full_grab ) {
            release_player_creature_grab( you, &target );
        } else {
            target.remove_effect( effect_controlled_for_shove );
        }
        return true;
    }

    const int skill = you.get_skill_level( armed ? skill_melee : skill_unarmed );
    const int player_roll = you.get_arm_str() + skill * 2 + rng( -4, 4 );
    const int target_roll = static_cast<int>( std::round( target.stability_roll() ) ) +
                            push_weight_stability_bonus( target );

    if( player_roll >= target_roll ) {
        you.mod_stamina( -full_cost );
        break_monster_grab_on_player( you, target );
        target.add_effect( effect_downed, creature_throw::shoved_creature_downed_duration );
        add_msg( m_good, _( "你破坏了%s的平衡，将其推倒在地。" ), target.disp_name() );
    } else {
        you.mod_stamina( -failure_cost );
        add_msg( m_warning, _( "你试图推倒%s，但它稳住了身体。" ), target.disp_name() );
    }

    if( release_full_grab ) {
        release_player_creature_grab( you, &target );
    } else {
        target.remove_effect( effect_controlled_for_shove );
    }
    return true;
}

bool push_controlled_creature( avatar &you )
{
    Creature *const target = find_player_controlled_creature( you );
    if( target == nullptr ) {
        return false;
    }
    return attempt_pushdown( you, *target, true, false );
}

void apply_throw_downed( Creature &target )
{
    if( target.is_dead_state() || target.is_immune_effect( effect_downed ) ) {
        return;
    }
    const monster *const mon = target.as_monster();
    if( mon != nullptr && mon->flies() ) {
        return;
    }
    target.add_effect( effect_downed, creature_throw::thrown_creature_downed_duration );
}

void animate_thrown_furniture_step()
{
    if( !get_option<bool>( "ANIMATIONS" ) ) {
        return;
    }

    g->invalidate_main_ui_adaptor();
    ui_manager::redraw_invalidated();
    refresh_display();

    const int delay_ms = get_option<int>( "ANIMATION_DELAY" );
    if( delay_ms > 0 ) {
        std::this_thread::sleep_for( std::chrono::milliseconds( delay_ms ) );
        inp_mngr.pump_events();
    }
}

bool throw_grabbed_creature( avatar &you )
{
    Creature *const target = find_player_grabbed_creature( you );
    if( target == nullptr ) {
        if( you.has_effect( effect_grabbing ) ) {
            you.remove_effect( effect_grabbing );
        }
        return false;
    }

    const bool ipman = ipman_throw_specialist( you );
    const creature_size target_size = target->get_size();
    const creature_size thrower_size = you.get_size();
    const int target_size_value = static_cast<int>( target_size );
    const int thrower_size_value = static_cast<int>( thrower_size );
    const int target_weight_grams = std::max(
                                        1, static_cast<int>( units::to_gram(
                                                target->get_weight() ) ) );
    const int target_weight = std::max( 1, target_weight_grams / 1000 );
    const int throw_strength = you.get_arm_str() + ( ipman ? 2 : 0 );

    if( !creature_throw::can_throw_grabbed_creature(
            thrower_size, throw_strength, target_size, target_weight_grams ) ) {
        return attempt_pushdown( you, *target, false, true );
    }
    if( you.get_stamina() < you.get_stamina_max() / 10 ) {
        add_msg( m_info, _( "你已经太疲惫了，无法完成摔投。" ) );
        return true;
    }

    const float stamina_factor = std::max(
                                     0.25f, static_cast<float>( you.get_stamina() ) /
                                     std::max( 1, you.get_stamina_max() ) );
    float throwforce = ( throw_strength * ( 1.1f + stamina_factor ) +
                         you.get_skill_level( skill_unarmed ) * 2.0f +
                         you.get_skill_level( skill_throw ) / 4.0f +
                         thrower_size_value - target_size_value ) * 2.0f;

    if( target_weight > 70 ) {
        throwforce *= std::clamp( 70.0f / target_weight, 0.35f, 1.0f );
    }

    map &here = get_map();
    if( here.has_flag( ter_furn_flag::TFLAG_DEEP_WATER, target->pos() ) ) {
        throwforce *= 0.25f;
    }
    const int range = static_cast<int>( throwforce / 10.0f );
    if( range < 1 ) {
        return attempt_pushdown( you, *target, false, true );
    }

    const target_handler::trajectory trajectory = target_handler::mode_throw_creature(
                you, *target, range );
    if( trajectory.empty() ) {
        return true;
    }
    if( trajectory.back() == you.pos() ) {
        add_msg( m_info, _( "你不能把%s摔在自己脚下。" ), target->disp_name() );
        return true;
    }
    if( trajectory.back().z != target->posz() ) {
        add_msg( m_info, _( "你无法将%s直接摔向另一层。" ), target->disp_name() );
        return true;
    }

    if( !confirm_nonhostile_npc_on_trajectory( you, trajectory ) ) {
        return true;
    }

    if( npc *const guy = target->as_npc(); guy != nullptr && !guy->is_enemy() ) {
        if( !query_yn( _( "这样做会被视为攻击，继续吗？" ) ) ) {
            return true;
        }
    }

    const tripoint original_pos = target->pos();
    const tripoint destination = trajectory.back();

    const tripoint release_step(
        std::clamp( destination.x - you.posx(), -1, 1 ),
        std::clamp( destination.y - you.posy(), -1, 1 ), 0 );
    const tripoint release_origin = you.pos() + release_step;
    const bool turns_across_body = release_origin != original_pos;

    creature_tracker &creatures = get_creature_tracker();
    Creature *const release_occupant = creatures.creature_at<Creature>( release_origin );
    const bool release_is_free =
        release_origin != you.pos() &&
        release_occupant == nullptr &&
        !here.impassable( release_origin );

    int flight_distance = std::max( 0, rl_dist( release_origin, destination ) );

    const bool close_pivot_slam = turns_across_body && !release_is_free;
    if( close_pivot_slam ) {
        flight_distance = 1;
    }

    const float velocity = creature_throw::grabbed_throw_velocity(
                               std::max( 1, flight_distance ) );
    const units::angle target_angle = coord_to_angle(
                                          close_pivot_slam ?
                                          original_pos : release_origin,
                                          destination );

    const int stamina_cost = creature_throw::grabbed_stamina_cost(
                                 velocity, target_weight_grams,
                                 you.get_skill_level( skill_unarmed ),
                                 you.get_skill_level( skill_throw ),
                                 you.get_dex() );

    break_monster_grab_on_player( you, *target );
    release_player_creature_grab( you, target );
    you.mod_moves( -100 );
    you.mod_stamina( -stamina_cost );

    if( npc *const guy = target->as_npc(); guy != nullptr ) {
        guy->make_angry();
    } else if( monster *const mon = target->as_monster(); mon != nullptr ) {
        mon->on_hit( &you, body_part_torso.id(), 0.0f, nullptr );
    }

    if( turns_across_body && release_is_free ) {
        target->setpos( release_origin );

        if( destination == release_origin ) {
            add_msg( _( "你将%s甩到了身体另一侧。" ), target->disp_name() );
            if( !target->is_dead_state() &&
                !target->is_immune_effect( effect_downed ) ) {
                const monster *const mon = target->as_monster();
                if( mon == nullptr || !mon->flies() ) {
                    target->add_effect( effect_downed, 1_turns );
                }
            }
            return true;
        }
    }

    add_msg( _( "你将%s摔了出去。" ), target->disp_name() );
    g->fling_creature( target, target_angle, velocity, false, &you );
    apply_throw_downed( *target );

    return true;
}

int furniture_throw_difficulty( const furn_t &furn )
{
    return std::clamp( std::max( 1, furn.move_str_req ), 1, 30 );
}

int furniture_collision_force( const avatar &you, const furn_t &furn,
                               const int traveled_distance )
{
    const int difficulty = furniture_throw_difficulty( furn );
    const int strength_surplus = std::max( 0, you.get_arm_str() - difficulty );

    // This is raw impulse, not direct damage.  Difficulty remains the furniture
    // identity, while normal strength only adds a moderate amount.  Very high
    // debug strength is handled separately by the displacement check.
    const int difficulty_term = difficulty * 3 + difficulty * difficulty / 4;
    const int distance_term = std::max( 0, traveled_distance - 1 ) * 2;
    const int strength_term = std::min( 15, strength_surplus );

    return std::max( 10, 8 + difficulty_term + distance_term + strength_term );
}

int furniture_direct_damage( const avatar &you, const furn_t &furn,
                             const int traveled_distance )
{
    const int difficulty = furniture_throw_difficulty( furn );

    const int furniture_base = std::clamp( 6 + difficulty, 10, 30 );

    const int strength_delta = you.get_arm_str() - 8;
    const int strength_bonus = strength_delta >= 0 ?
                               strength_delta / 2 :
                               -( ( -strength_delta + 1 ) / 2 );

    const int distance_bonus = std::min(
                                   4, std::max( 0, traveled_distance - 1 ) / 3 );

    return std::max( 1, furniture_base + strength_bonus + distance_bonus );
}

int furniture_knockback_distance( const avatar &you, Creature &target,
                                  const furn_t &furn, const int traveled_distance )
{
    const int difficulty = furniture_throw_difficulty( furn );
    const int strength_surplus = std::max( 0, you.get_arm_str() - difficulty );

    if( monster *const mon = target.as_monster();
        mon != nullptr && target.get_size() == creature_size::medium &&
        mon->type->in_species( species_ZOMBIE ) ) {
        const int baseline_control = difficulty +
                                     std::min( 10, strength_surplus / 2 ) +
                                     std::min( 4, std::max( 0, traveled_distance - 1 ) );
        return baseline_control >= 12 ? 2 : 1;
    }

    const int target_size = static_cast<int>( target.get_size() );
    const int thrower_size = static_cast<int>( you.get_size() );
    const int size_resistance = std::max( 0, target_size - thrower_size ) * 7;
    const int size_advantage = std::max( 0, thrower_size - target_size ) * 4;

    const int control_roll = difficulty * 2 +
                             std::min( 20, strength_surplus / 2 ) +
                             std::max( 0, strength_surplus - 20 ) / 4 +
                             std::max( 0, traveled_distance - 1 ) / 2 +
                             size_advantage + rng( -3, 3 );
    const int resistance_roll = static_cast<int>(
                                    std::round( target.stability_roll() ) ) +
                                size_resistance;

    const int margin = control_roll - resistance_roll;
    int distance = 0;
    if( margin >= 12 ) {
        distance = 2;
    } else if( margin >= 3 ) {
        distance = 1;
    }

    if( distance > 0 && strength_surplus > 30 ) {
        distance += std::clamp( ( strength_surplus - 30 ) / 12, 0, 6 );
    }

    return distance;
}

contact_roll_result furniture_throw_hit_result(
    avatar &you, Creature &target, const furn_t &, const int traveled_distance )
{
    const int throw_skill = you.get_skill_level( skill_throw );
    const int distance_penalty =
        ( std::max( 0, traveled_distance - 3 ) + 2 ) / 3;

    return contact_d20_result(
               you, target, throw_skill / 2, distance_penalty, 0, false );
}

int furniture_stamina_cost( const avatar &you, const furn_t &furn,
                            const int actual_distance )
{
    const int difficulty = furniture_throw_difficulty( furn );
    const int throw_skill = you.get_skill_level( skill_throw );

    const float efficiency = std::clamp(
                                 1.0f - throw_skill * 0.03f -
                                 std::max( 0, you.get_dex() - 8 ) * 0.005f,
                                 0.70f, 1.0f );

    const int base = 30 + difficulty * difficulty * 3 +
                     actual_distance * ( 6 + difficulty * 2 );

    return std::clamp(
               static_cast<int>( std::round( base * efficiency ) ),
               50, 1200 );
}

bool throw_grabbed_furniture( avatar &you )
{
    if( you.get_grab_type() != object_type::FURNITURE ) {
        return false;
    }

    map &here = get_map();
    const tripoint source = you.pos() + you.grab_point;
    if( !here.has_furn( source ) ) {
        you.grab( object_type::NONE );
        return true;
    }

    const furn_id thrown_furn = here.furn( source );
    const furn_t &furn = thrown_furn.obj();
    const int strength_req = furniture_throw_difficulty( furn );
    const int strength = you.get_arm_str();

    if( strength < strength_req ) {
        add_msg( m_bad, _( "你没有足够的力量将%s扔出去。" ), furn.name() );
        you.mod_moves( -100 );
        return true;
    }

    const int range = std::clamp( 1 + ( strength - strength_req ) / 2, 1, 30 );
    target_handler::trajectory trajectory = target_handler::mode_throw_object(
                you, source, range );

    if( trajectory.empty() ) {
        return true;
    }
    if( trajectory.back() == you.pos() ) {
        add_msg( m_info, _( "你不能把%s扔在自己脚下。" ), furn.name() );
        return true;
    }
    for( const tripoint &p : trajectory ) {
        if( p.z != source.z ) {
            add_msg( m_info, _( "你无法将%s直接扔向另一层。" ), furn.name() );
            return true;
        }
    }

    creature_tracker &creatures = get_creature_tracker();
    const int aimed_distance = std::max( 1, rl_dist( source, trajectory.back() ) );

    if( creatures.creature_at<Creature>( trajectory.back() ) != nullptr &&
        trajectory.size() >= 2 ) {
        const int extension_len = std::max( 1, range - aimed_distance );
        std::vector<tripoint> extension = continue_line( trajectory, extension_len );
        trajectory.reserve( trajectory.size() + extension.size() );
        trajectory.insert( trajectory.end(), extension.begin(), extension.end() );
    }

    if( !confirm_nonhostile_npc_on_trajectory( you, trajectory ) ) {
        return true;
    }

    tripoint flight_origin = source;
    for( std::size_t i = 0; i + 1 < trajectory.size(); ++i ) {
        if( trajectory[i] == you.pos() ) {
            flight_origin = trajectory[i + 1];
            break;
        }
    }

    tripoint furniture_pos = source;
    bool moved = false;
    bool furniture_released = false;

    auto move_furniture_to = [&]( const tripoint &dest ) {
        here.furn_set( dest, here.furn( furniture_pos ) );
        here.furn_set( furniture_pos, f_null, true );
        furniture_pos = dest;
        moved = true;
        furniture_released = true;
        animate_thrown_furniture_step();
    };

    auto resolve_creature_hit = [&]( Creature &hit, const tripoint &hit_pos,
                                     const tripoint &from_pos, const int traveled ) {
        if( npc *const guy = hit.as_npc(); guy != nullptr ) {
            guy->on_attacked( you );
        }

        const contact_roll_result hit_result =
            furniture_throw_hit_result( you, hit, furn, traveled );
        if( hit_result != contact_roll_result::hit ) {
            switch( hit_result ) {
                case contact_roll_result::inaccurate:
                    add_msg( m_info, _( "你把%s扔偏了。" ), furn.name() );
                    break;
                case contact_roll_result::near_miss:
                    add_msg( m_info, _( "飞来的%s从%s身边擦了过去。" ),
                             furn.name(), hit.disp_name() );
                    break;
                case contact_roll_result::dodged:
                    add_msg( m_info, _( "%s闪开了飞来的%s。" ),
                             hit.disp_name(), furn.name() );
                    break;
                case contact_roll_result::hit:
                    break;
            }
            return false;
        }

        if( monster *const mon = hit.as_monster(); mon != nullptr ) {
            mon->on_hit( &you, body_part_torso.id(), 0.0f, nullptr );
        }

        furniture_released = true;
        const int raw_damage = furniture_direct_damage( you, furn, traveled );
        const dealt_damage_instance dealt = hit.deal_damage(
                &you, body_part_torso.id(),
                damage_instance( damage_type::BASH, static_cast<float>( raw_damage ) ) );
        const int dealt_amount = dealt.total_damage();

        add_msg( m_good, _( "%s砸中了%s，造成%d点撞击伤害。" ),
                 furn.name(), hit.disp_name(), dealt_amount );

        credit_player_collision_death( you, hit );
        if( !hit.is_dead_state() ) {
            const int knockback = furniture_knockback_distance(
                                      you, hit, furn, traveled );

            const monster *const hit_mon = hit.as_monster();
            const bool flying = hit_mon != nullptr && hit_mon->flies();
            const bool can_fall = !flying && !hit.is_immune_effect( effect_downed );

            if( can_fall && knockback > 0 ) {
                hit.add_effect( effect_downed, 3_turns );
            }

            if( knockback > 0 ) {
                const units::angle angle = coord_to_angle( from_pos, hit_pos );
                const float fling_velocity = 5.0f + knockback * 10.0f;
                g->fling_creature( &hit, angle, fling_velocity, false, &you );
            }
        }
        return true;
    };

    tripoint previous_flight_pos = source;

    for( const tripoint &next : trajectory ) {
        if( next == source ) {
            continue;
        }

        if( next == you.pos() ) {
            previous_flight_pos = next;
            continue;
        }

        const int traveled = std::max( 1, rl_dist( flight_origin, next ) );

        if( Creature *const hit = creatures.creature_at<Creature>( next );
            hit != nullptr && hit != &you ) {
            const bool connected = resolve_creature_hit(
                                       *hit, next, previous_flight_pos, traveled );
            if( connected ) {
                if( creatures.creature_at<Creature>( next ) == nullptr &&
                    !here.veh_at( next ) && !here.has_furn( next ) &&
                    !here.impassable( next ) ) {
                    move_furniture_to( next );
                }
                break;
            }

            previous_flight_pos = next;
            continue;
        }

        const int bash_force = std::max(
                                   5, furniture_collision_force( you, furn, traveled ) / 2 );
        if( here.veh_at( next ) || here.has_furn( next ) || here.impassable( next ) ) {
            here.bash( next, bash_force );
            if( here.veh_at( next ) || here.has_furn( next ) || here.impassable( next ) ) {
                break;
            }
        }

        move_furniture_to( next );
        previous_flight_pos = next;
    }

    you.grab( object_type::NONE );
    const int actual_distance = std::max( 1, rl_dist( flight_origin, furniture_pos ) );
    const int stamina_cost = furniture_stamina_cost( you, furn, actual_distance );

    you.mod_moves( -100 - 20 * std::max( 0, actual_distance - 1 ) );
    you.mod_stamina( -stamina_cost );

    if( furniture_released ) {
        complete_jackie_chan_achievement();
    }

    if( moved ) {
        add_msg( _( "你将%s猛地扔了出去。" ), furn.name() );
        if( !here.has_floor( furniture_pos ) &&
            !here.has_flag( ter_furn_flag::TFLAG_FLAT, furniture_pos ) ) {
            here.drop_furniture( furniture_pos );
        }
    } else {
        add_msg( m_info, _( "%s撞上了障碍，没有飞出去。" ), furn.name() );
    }

    return true;
}

bool throw_grabbed_vehicle( avatar &you )
{
    if( you.get_grab_type() != object_type::VEHICLE ) {
        return false;
    }

    map &here = get_map();
    creature_tracker &creatures = get_creature_tracker();

    const optional_vpart_position vp = here.veh_at( you.pos() + you.grab_point );
    if( !vp ) {
        you.grab( object_type::NONE );
        return true;
    }

    vehicle *veh = &vp->vehicle();

    if( you.in_vehicle ) {
        const optional_vpart_position occupied = here.veh_at( you.pos() );
        if( occupied && &occupied->vehicle() == veh ) {
            add_msg( m_info, _( "你正在%s里面，无法将它投掷出去。" ), veh->disp_name() );
            return true;
        }
    }

    if( !veh->handle_potential_theft( you ) ) {
        return true;
    }

    if( veh->has_harnessed_animal() ) {
        add_msg( m_info, _( "有动物仍连接在%s上，无法将它投掷出去。" ), veh->disp_name() );
        return true;
    }

    veh->invalidate_mass();

    const int raw_strength = you.get_arm_str();
    const int size_penalty = std::max(
                                 0, static_cast<int>( creature_size::large ) -
                                 static_cast<int>( you.get_size() ) ) * 5;
    const int effective_strength = std::max( 0, raw_strength - size_penalty );
    const int strength_req = std::max(
                                 1, static_cast<int>( veh->total_mass() / 100_kilogram ) );

    if( effective_strength < strength_req ) {
        add_msg( m_bad, _( "你没有足够的力量和杠杆将%s投掷出去。" ), veh->disp_name() );
        you.mod_moves( -100 );
        return true;
    }

    const int range = std::clamp(
                          ( effective_strength - strength_req ) / 2 + 1, 1, 30 );

    const int grabbed_part = vp->part_index();
    const tripoint source = veh->global_part_pos3( grabbed_part );

    const target_handler::trajectory trajectory = target_handler::mode_throw_object(
                you, source, range, true );

    if( trajectory.empty() ) {
        return true;
    }

    if( trajectory.back() == you.pos() ) {
        add_msg( m_info, _( "你不能把%s投在自己所在的位置。" ), veh->disp_name() );
        return true;
    }

    for( const tripoint &p : trajectory ) {
        if( p.z != source.z ) {
            add_msg( m_info, _( "你无法将%s直接投向另一层。" ), veh->disp_name() );
            return true;
        }
    }

    std::size_t pivot_index = trajectory.size();
    for( std::size_t i = 0; i < trajectory.size(); ++i ) {
        if( trajectory[i] == you.pos() ) {
            pivot_index = i;
            break;
        }
    }

    target_handler::trajectory flight_trajectory = trajectory;
    tripoint flight_origin = source;
    bool pivot_repositioned = false;

    if( pivot_index < trajectory.size() ) {
        // 多格载具不能像单格生物一样直接让正常车辆移动穿过玩家。
        // 先检查抓取部件沿轨迹绕过玩家时，整辆车的每个占格是否都有空间。
        // 中间姿态只允许覆盖投掷者本人，不允许穿墙，穿家具，穿其他车辆或生物。
        const auto &vehicle_points = veh->get_points();

        auto pose_overlaps_thrower = [&]( const tripoint &candidate_anchor ) {
            const tripoint delta = candidate_anchor - source;
            for( const tripoint &part_pos : vehicle_points ) {
                if( part_pos + delta == you.pos() ) {
                    return true;
                }
            }
            return false;
        };

        auto pivot_pose_is_clear = [&]( const tripoint &candidate_anchor ) {
            const tripoint delta = candidate_anchor - source;

            for( const tripoint &part_pos : vehicle_points ) {
                const tripoint dest = part_pos + delta;

                if( !here.inbounds( dest ) ) {
                    return false;
                }

                // 玩家本人只在抽象转身阶段作为枢轴被忽略。
                if( dest == you.pos() ) {
                    continue;
                }

                if( creatures.creature_at<Creature>( dest ) != nullptr ) {
                    return false;
                }

                const optional_vpart_position other_vp = here.veh_at( dest );
                if( other_vp && &other_vp->vehicle() != veh ) {
                    return false;
                }

                if( here.impassable( dest ) || here.has_furn( dest ) ) {
                    return false;
                }
            }

            return true;
        };

        std::size_t release_index = trajectory.size();

        // 从抓取部件到达玩家格开始检查整车扫过姿态。
        // 第一个整车完全离开玩家格的位置，就是正式释放位置。
        for( std::size_t i = pivot_index; i < trajectory.size(); ++i ) {
            const tripoint &candidate_anchor = trajectory[i];

            if( !pivot_pose_is_clear( candidate_anchor ) ) {
                break;
            }

            if( i > pivot_index && !pose_overlaps_thrower( candidate_anchor ) ) {
                release_index = i;
                break;
            }
        }

        if( release_index == trajectory.size() ) {
            add_msg( m_info, _( "你周围没有足够的空间将%s甩到身体另一侧。" ),
                     veh->disp_name() );
            return true;
        }

        const tripoint release_anchor = trajectory[release_index];

        flight_trajectory.clear();
        for( std::size_t i = release_index; i < trajectory.size(); ++i ) {
            flight_trajectory.push_back( trajectory[i] );
        }

        // 抽象转身不会撞到轨迹前半段的生物。
        // 只有正式释放后的真实飞行轨迹需要攻击确认。
        if( !confirm_nonhostile_npc_on_trajectory( you, flight_trajectory ) ) {
            return true;
        }

        const tripoint pivot_delta = release_anchor - source;

        // displace_vehicle 只负责把整辆车放到已经验证过的释放姿态。
        // 它不会执行车辆碰撞，因此玩家不会被自己的车撞击或带着一起移动。
        if( !here.displace_vehicle( *veh, pivot_delta, false ) ) {
            add_msg( m_info, _( "你没能将%s调整到合适的释放位置。" ),
                     veh->disp_name() );
            return true;
        }

        // displace_vehicle 可能使旧 vehicle 指针失效，因此必须从地图重新取得。
        const optional_vpart_position released_vp = here.veh_at( release_anchor );
        if( !released_vp ) {
            debugmsg( "Thrown vehicle disappeared after pivot release" );
            you.grab( object_type::NONE );
            return true;
        }

        veh = &released_vp->vehicle();
        flight_origin = veh->global_part_pos3( grabbed_part );
        pivot_repositioned = true;
    } else {
        if( !confirm_nonhostile_npc_on_trajectory( you, flight_trajectory ) ) {
            return true;
        }
    }

    const int shove_velocity = std::clamp(
                                   1000 + 125 * std::max(
                                       0, effective_strength - strength_req ),
                                   1000, 2000 );

    bool attempted_flight = false;
    const tripoint current_release_pos = veh->global_part_pos3( grabbed_part );
    for( const tripoint &target_pos : flight_trajectory ) {
        if( target_pos != current_release_pos ) {
            attempted_flight = true;
            break;
        }
    }

    bool moved = pivot_repositioned;

    // collision_source 只在正式飞行阶段存在，用于把撞击和击杀归属给玩家。
    // 这里不再让通用车辆碰撞系统对投掷者本人免疫。
    if( attempted_flight ) {
        veh->collision_source = &you;
    }

    for( const tripoint &target_pos : flight_trajectory ) {
        const tripoint current_part_pos = veh->global_part_pos3( grabbed_part );

        if( target_pos == current_part_pos ) {
            continue;
        }

        const tripoint delta(
            std::clamp( target_pos.x - current_part_pos.x, -1, 1 ),
            std::clamp( target_pos.y - current_part_pos.y, -1, 1 ), 0 );

        if( delta == tripoint_zero ) {
            continue;
        }

        veh->skidding = true;
        veh->velocity = shove_velocity;

        vehicle *const moved_vehicle = here.move_vehicle( *veh, delta, veh->face );

        if( moved_vehicle == nullptr ||
            moved_vehicle->global_part_pos3( grabbed_part ) == current_part_pos ) {
            break;
        }

        veh = moved_vehicle;
        moved = true;
    }

    veh->velocity = 0;
    veh->skidding = false;

    if( attempted_flight ) {
        veh->collision_source = nullptr;
    }

    if( moved ) {
        const int actual_distance = std::max(
                                        1, rl_dist(
                                            flight_origin,
                                            veh->global_part_pos3( grabbed_part ) ) );

        const int strength_margin = std::max(
                                        0, effective_strength - strength_req );

        const float strength_efficiency = std::clamp(
                                              1.25f - strength_margin * 0.005f,
                                              0.70f, 1.25f );

        const int vehicle_stamina_cost = std::clamp(
                                             static_cast<int>(
                                                 ( 200 + actual_distance * 35 +
                                                   strength_req * 45 ) *
                                                 strength_efficiency ),
                                             150, 2000 );

        if( pivot_repositioned && !attempted_flight ) {
            add_msg( _( "你将%s甩到了身体另一侧。" ), veh->disp_name() );
        } else {
            add_msg( _( "你猛地将%s掷了出去。" ), veh->disp_name() );
        }

        you.grab( object_type::NONE );
        you.mod_moves( -100 - 25 * std::max( 0, actual_distance - 1 ) );
        you.mod_stamina( -vehicle_stamina_cost );
    } else {
        add_msg( m_info, _( "%s纹丝不动。" ), veh->disp_name() );
        you.mod_moves( -100 );
    }

    return true;
}

} // namespace

// Establish or release a grab on a creature, vehicle, or piece of furniture
static void grab()
{
    avatar &you = get_avatar();
    map &here = get_map();
    creature_tracker &creatures = get_creature_tracker();

    if( you.has_effect( effect_grabbing ) ) {
        Creature *const target = find_player_grabbed_creature( you );
        if( target != nullptr ) {
            add_msg( _( "你松开了%s。" ), target->disp_name() );
        }
        release_player_creature_grab( you, target );
        return;
    }

    if( you.get_grab_type() != object_type::NONE ) {
        if( const optional_vpart_position vp = here.veh_at( you.pos() + you.grab_point ) ) {
            add_msg( _( "You release the %s." ), vp->vehicle().name );
        } else if( here.has_furn( you.pos() + you.grab_point ) ) {
            add_msg( _( "You release the %s." ), here.furnname( you.pos() + you.grab_point ) );
        }
        you.grab( object_type::NONE );
        return;
    }

    std::vector<tripoint> grab_candidates;
    for( const tripoint &candidate : here.points_in_radius( you.pos(), 1, 0 ) ) {
        if( candidate == you.pos() ) {
            continue;
        }

        bool valid = false;
        if( Creature *const target = creatures.creature_at<Creature>( candidate );
            target != nullptr && target != &you &&
            !target->is_hallucination() && !target->blocks_physical_contact() &&
            !target->has_effect( effect_grabbed ) ) {
            valid = true;
        }

        if( const optional_vpart_position candidate_vp = here.veh_at( candidate ) ) {
            if( !candidate_vp.part_with_feature( VPFLAG_WALL_MOUNTED, false ) &&
                !candidate_vp->vehicle().has_tag( flag_CANT_DRAG ) ) {
                valid = true;
            }
        }

        if( here.has_furn( candidate ) && here.furn( candidate ).obj().is_movable() ) {
            valid = true;
        }

        if( valid ) {
            grab_candidates.push_back( candidate );
        }
    }

    if( grab_candidates.empty() ) {
        add_msg( m_info, _( "附近没有可以抓住的目标。" ) );
        return;
    }

    tripoint grabp;
    if( grab_candidates.size() == 1 ) {
        grabp = grab_candidates.front();
    } else {
        shared_ptr_fast<game::draw_callback_t> grab_targets_cb =
        make_shared_fast<game::draw_callback_t>( [&grab_candidates]() {
            for( const tripoint &candidate : grab_candidates ) {
                g->draw_highlight( candidate );
            }
        } );
        g->add_draw_callback( grab_targets_cb );

        const std::optional<tripoint> grabp_ = choose_adjacent( _( "抓住哪里？" ) );
        if( !grabp_ ) {
            add_msg( _( "算了。" ) );
            return;
        }
        grabp = *grabp_;

        if( std::find( grab_candidates.begin(), grab_candidates.end(), grabp ) ==
            grab_candidates.end() ) {
            add_msg( m_info, _( "那里没有可以抓住的目标。" ) );
            return;
        }
    }

    if( grabp == you.pos() ) {
        add_msg( _( "You get a hold of yourself." ) );
        return;
    }

    if( !( here.veh_at( grabp ) || here.has_furn( grabp ) ||
           creatures.creature_at<Creature>( grabp ) ) ) {
        if( here.has_flag( ter_furn_flag::TFLAG_RAMP_UP, grabp ) ||
            here.has_flag( ter_furn_flag::TFLAG_RAMP_UP, you.pos() ) ) {
            grabp.z += 1;
        } else if( here.has_flag( ter_furn_flag::TFLAG_RAMP_DOWN, grabp ) ||
                   here.has_flag( ter_furn_flag::TFLAG_RAMP_DOWN, you.pos() ) ) {
            grabp.z -= 1;
        }
    }

    if( Creature *const target = creatures.creature_at<Creature>( grabp );
        target != nullptr && target != &you ) {
        if( target->is_hallucination() || target->blocks_physical_contact() ) {
            add_msg( m_info, _( "你无法控制这个目标。" ) );
            return;
        }
        if( target->has_effect( effect_grabbed ) ) {
            add_msg( m_info, _( "这个目标已经被抓住了。" ) );
            return;
        }
        if( you.get_working_arm_count() < 1 ) {
            add_msg( m_info, _( "你的手臂状态不足以完成这个动作。" ) );
            return;
        }
        if( you.get_stamina() < 300 ) {
            add_msg( m_info, _( "你已经太疲惫了，无法稳稳控制对方。" ) );
            return;
        }
        if( npc *const guy = target->as_npc(); guy != nullptr && !guy->is_enemy() ) {
            if( !query_yn( _( "这样做会被视为攻击，继续吗？" ) ) ) {
                return;
            }
            guy->on_attacked( you );
        }

        const bool armed_control = you.is_armed();
        const bool ipman = !armed_control && ipman_throw_specialist( you );
        you.mod_moves( -100 );
        you.mod_stamina( -20 );
        const bool contact_hit = grab_contact_hits(
                                     you, *target, armed_control, ipman ? 2 : 0 );
        if( !contact_hit ) {
            if( armed_control ) {
                add_msg( m_warning, _( "你试图用武器控制%s，但没有成功。" ),
                         target->disp_name() );
            } else {
                add_msg( m_warning, _( "你伸手试图控制%s，但没有抓住。" ),
                         target->disp_name() );
            }
            return;
        }

        if( monster *const mon = target->as_monster(); mon != nullptr ) {
            mon->on_hit( &you, body_part_torso.id() );
        }

        if( armed_control ) {
            target->add_effect( effect_controlled_for_shove, 2_turns );
            add_msg( _( "你用武器和身体短暂控制住了%s。" ), target->disp_name() );
            return;
        }

        const int grab_strength = std::clamp(
                                      6 + you.get_arm_str() +
                                      you.get_skill_level( skill_unarmed ) * 2 +
                                      you.get_skill_level( skill_melee ) / 2 +
                                      static_cast<int>( you.get_size() ) * 2 +
                                      ( ipman ? 4 : 0 ),
                                      1, 100 );
        // Player grappling uses its own relationship marker.  Do not apply the
        // vanilla monster grabbed movement-impairing effect: the target may
        // still attack and act while wrestling with the player.
        target->add_effect(
            effect_grabbed_by_player, 10_turns, body_part_torso, false, grab_strength );
        target->add_effect( effect_player_grab_fresh, 2_turns );
        you.add_effect(
            effect_grabbing, 10_turns, body_part_torso, false, grab_strength );
        add_msg( _( "你抓住了%s。" ), target->disp_name() );
        return;
    }

    if( const optional_vpart_position vp = here.veh_at( grabp ) ) {
        if( !vp->vehicle().handle_potential_theft( you ) ) {
            return;
        }
        if( vp.part_with_feature( VPFLAG_WALL_MOUNTED, false ) ) {
            add_msg( m_info, _( "它固定在墙上，无法移动。" ) );
            return;
        }
        if( vp->vehicle().has_tag( flag_CANT_DRAG ) ) {
            add_msg( m_info, _( "There's nothing to grab there!" ) );
            return;
        }
        you.grab( object_type::VEHICLE, grabp - you.pos() );
        add_msg( _( "You grab the %s." ), vp->vehicle().name );
    } else if( here.has_furn( grabp ) ) {
        if( !here.furn( grabp ).obj().is_movable() ) {
            add_msg( _( "You can not grab the %s" ), here.furnname( grabp ) );
            return;
        }
        you.grab( object_type::FURNITURE, grabp - you.pos() );
        if( !here.can_move_furniture( grabp, &you ) ) {
            add_msg( _( "You grab the %s. It feels really heavy." ),
                     here.furnname( grabp ) );
        } else {
            add_msg( _( "You grab the %s." ), here.furnname( grabp ) );
        }
    } else {
        add_msg( m_info, _( "There's nothing to grab there!" ) );
    }
}

static void haul()
{
    Character& player_character = get_player_character();
    map& here = get_map();

    if (player_character.is_hauling()) {
        player_character.stop_hauling();
    }
    else {
        if (here.veh_at(player_character.pos())) {
            add_msg(m_info, _("You cannot haul inside vehicles."));
        }
        else if (here.has_flag(ter_furn_flag::TFLAG_DEEP_WATER, player_character.pos())) {
            add_msg(m_info, _("You cannot haul while in deep water."));
        }
        else if (!here.can_put_items(player_character.pos())) {
            add_msg(m_info, _("You cannot haul items here."));
        }
        else if (!here.has_haulable_items(player_character.pos())) {
            add_msg(m_info, _("There are no items to haul here."));
        }
        else {
            player_character.start_hauling();
        }
    }
}

static void smash()
{
    avatar& player_character = get_avatar();
    map& here = get_map();
    if (player_character.is_mounted()) {
        auto* mons = player_character.mounted_creature.get();
        if (mons->has_flag(MF_RIDEABLE_MECH)) {
            if (!mons->check_mech_powered()) {
                add_msg(m_bad, _("Your %s refuses to move as its batteries have been drained."),
                    mons->get_name());
                return;
            }
        }
    }
    const int move_cost = !player_character.is_armed() ? 80 :
        player_character.get_wielded_item()->attack_time(player_character) *
        0.8;
    bool mech_smash = false;
    int smashskill;
    ///\EFFECT_STR increases smashing capability
    if (player_character.is_mounted()) {
        auto* mon = player_character.mounted_creature.get();
        smashskill = player_character.get_arm_str() + mon->mech_str_addition() + mon->type->melee_dice *
            mon->type->melee_sides;
        mech_smash = true;
    }
    else {
        smashskill = player_character.get_arm_str();
        if (player_character.get_wielded_item()) {
            smashskill += player_character.get_wielded_item()->damage_melee(damage_type::BASH);
        }
    }

    const bool allow_floor_bash = debug_mode; // Should later become "true"
    const std::optional<tripoint> smashp_ = choose_adjacent(_("Smash where?"), allow_floor_bash);
    if (!smashp_) {
        return;
    }
    tripoint smashp = *smashp_;

    bool smash_floor = false;
    if (smashp.z != player_character.posz()) {
        if (smashp.z > player_character.posz()) {
            // TODO: Knock on the ceiling
            return;
        }

        smashp.z = player_character.posz();
        smash_floor = true;
    }
    get_event_bus().send<event_type::character_smashes_tile>(
        player_character.getID(), here.ter(smashp).id(), here.furn(smashp).id());
    if (player_character.is_mounted()) {
        monster* crit = player_character.mounted_creature.get();
        if (crit->has_flag(MF_RIDEABLE_MECH)) {
            crit->use_mech_power(3_kJ);
        }
    }
    for (std::pair<const field_type_id, field_entry>& fd_to_smsh : here.field_at(smashp)) {
        const map_bash_info& bash_info = fd_to_smsh.first->bash_info;
        if (bash_info.str_min == -1) {
            continue;
        }
        if (smashskill < bash_info.str_min && one_in(10)) {
            add_msg(m_neutral, _("You don't seem to be damaging the %s."), fd_to_smsh.first->get_name());
            return;
        }
        else if (smashskill >= rng(bash_info.str_min, bash_info.str_max)) {
            sounds::sound(smashp, bash_info.sound_vol, sounds::sound_t::combat, bash_info.sound, true, "smash",
                "field");
            here.remove_field(smashp, fd_to_smsh.first);
            here.spawn_items(smashp, item_group::items_from(bash_info.drop_group, calendar::turn));
            player_character.mod_moves(-bash_info.fd_bash_move_cost);
            add_msg(m_info, bash_info.field_bash_msg_success.translated());
            return;
        }
        else {
            sounds::sound(smashp, bash_info.sound_fail_vol, sounds::sound_t::combat, bash_info.sound_fail,
                true, "smash",
                "field");
            return;
        }
    }

    bool should_pulp = false;
    for (const item& maybe_corpse : here.i_at(smashp)) {
        if (maybe_corpse.is_corpse() && maybe_corpse.damage() < maybe_corpse.max_damage() &&
            maybe_corpse.can_revive()) {
            if (maybe_corpse.get_mtype()->bloodType()->has_acid &&
                !player_character.is_immune_field(fd_acid)) {
                if (!query_yn(_("Are you sure you want to pulp an acid filled corpse?"))) {
                    return; // Player doesn't want an acid bath
                }
            }
            should_pulp = true; // There is at least one corpse to pulp
        }
    }

    if (should_pulp) {
        // do activity forever. ACT_PULP stops itself
        player_character.assign_activity(ACT_PULP, calendar::INDEFINITELY_LONG, 0);
        player_character.activity.placement = here.getglobal(smashp);
        return; // don't smash terrain if we've smashed a corpse
    }

    vehicle* veh = veh_pointer_or_null(here.veh_at(smashp));
    if (veh != nullptr) {
        if (!veh->handle_potential_theft(player_character)) {
            return;
        }
    }

    if (!player_character.has_weapon()) {
        const bodypart_id bp_null("bp_null");
        std::pair<bodypart_id, int> best_part_to_smash = { bp_null, 0 };
        int tmp_bash_armor = 0;
        for (const bodypart_id& bp : player_character.get_all_body_parts()) {
            tmp_bash_armor += player_character.worn.damage_resist(damage_type::BASH, bp);
            for (const trait_id& mut : player_character.get_mutations()) {
                const resistances& res = mut->damage_resistance(bp);
                tmp_bash_armor += std::floor(res.type_resist(damage_type::BASH));
            }
            if (tmp_bash_armor > best_part_to_smash.second) {
                best_part_to_smash = { bp, tmp_bash_armor };
            }
        }
        if (best_part_to_smash.first != bp_null && here.is_bashable(smashp)) {
            std::string name_to_bash = _("thing");
            if (here.is_bashable_furn(smashp)) {
                name_to_bash = here.furnname(smashp);
            }
            else if (here.is_bashable_ter(smashp)) {
                name_to_bash = here.tername(smashp);
            }
            if (!best_part_to_smash.first->smash_message.empty()) {
                add_msg(best_part_to_smash.first->smash_message, name_to_bash);
            }
            else {
                add_msg(_("You use your %s to smash the %s."),
                    body_part_name_accusative(best_part_to_smash.first), name_to_bash);
            }
        }
        const int min_smashskill = smashskill * best_part_to_smash.first->smash_efficiency;
        const int max_smashskill = smashskill * (1.0f + best_part_to_smash.first->smash_efficiency);
        smashskill = std::min(best_part_to_smash.second + min_smashskill, max_smashskill);
    }
    const bash_params bash_result = here.bash(smashp, smashskill, false, false, smash_floor);
    // Weariness scaling
    float weary_mult = 1.0f;
    item_location weapon = player_character.used_weapon();
    if (bash_result.did_bash) {
        if (!mech_smash) {
            player_character.set_activity_level(MODERATE_EXERCISE);
            player_character.handle_melee_wear(weapon);
            weary_mult = 1.0f / player_character.exertion_adjusted_move_multiplier(MODERATE_EXERCISE);

            const int mod_sta = 2 * player_character.get_standard_stamina_cost();
            player_character.mod_stamina(mod_sta);

            if (player_character.get_skill_level(skill_melee) == 0) {
                player_character.practice(skill_melee, rng(0, 1) * rng(0, 1));
            }
            if (weapon) {
                const int glass_portion = weapon->made_of(material_glass);
                float glass_fraction = glass_portion / static_cast<float>(weapon->type->mat_portion_total);
                if (std::isnan(glass_fraction) || glass_fraction > 1.f) {
                    glass_fraction = 0.f;
                }
                const int vol = weapon->volume() * glass_fraction / units::legacy_volume_factor;
                if (glass_portion && rng(0, vol + 3) < vol) {
                    add_msg(m_bad, _("Your %s shatters!"), weapon->tname());
                    weapon->spill_contents(player_character.pos());
                    sounds::sound(player_character.pos(), 24, sounds::sound_t::combat, "CRACK!", true, "smash",
                        "glass");
                    player_character.deal_damage(nullptr, bodypart_id("hand_r"), damage_instance(damage_type::CUT,
                        rng(0,
                            vol)));
                    if (vol > 20) {
                        // Hurt left arm too, if it was big
                        player_character.deal_damage(nullptr, bodypart_id("hand_l"), damage_instance(damage_type::CUT,
                            rng(0,
                                static_cast<int>(vol * .5))));
                    }
                    player_character.remove_weapon();
                    player_character.check_dead_state();
                }
            }
        }
        player_character.moves -= move_cost * weary_mult;
        player_character.recoil = MAX_RECOIL;

        if (!bash_result.success) {
            if (smashskill < here.bash_resistance(smashp) && one_in(10)) {
                if (here.has_furn(smashp) && here.furn(smashp).obj().bash.str_min != -1) {
                    // %s is the smashed furniture
                    add_msg(m_neutral, _("You don't seem to be damaging the %s."), here.furnname(smashp));
                }
                else {
                    // %s is the smashed terrain
                    add_msg(m_neutral, _("You don't seem to be damaging the %s."), here.tername(smashp));
                }
            }
        }

    }
    else {
        add_msg(_("There's nothing there to smash!"));
    }
}

static int try_set_alarm()
{
    uilist as_m;
    const bool already_set = get_player_character().has_effect(effect_alarm_clock);

    as_m.text = already_set ?
        _("You already have an alarm set.  What do you want to do?") :
        _("You have an alarm clock.  What do you want to do?");

    as_m.entries.emplace_back(0, true, 'w', already_set ?
        _("Keep the alarm and wait a while") :
        _("Wait a while"));
    as_m.entries.emplace_back(1, true, 'a', already_set ?
        _("Change your alarm") :
        _("Set an alarm for later"));
    as_m.query();

    return as_m.ret;
}

static void wait()
{
    std::map<int, time_duration> durations;
    uilist as_m;
    Character& player_character = get_player_character();
    bool setting_alarm = false;
    map& here = get_map();

    if (player_character.controlling_vehicle) {
        const vehicle& veh = here.veh_at(player_character.pos())->vehicle();
        if (!veh.can_use_rails() && (   // control optional if on rails
            veh.is_flying_in_air() ||   // control required: fuel is consumed even at hover
            veh.is_falling ||           // *not* vertical_velocity, which is only used for collisions
            veh.velocity ||             // is moving
            (veh.cruise_velocity && (  // would move if it could
                (veh.is_watercraft() && veh.can_float()) || // is viable watercraft floating on water
                veh.sufficient_wheel_config() // is viable land vehicle on ground or fording shallow water
                )) ||
            (veh.is_in_water(true) && !veh.can_float()) // is sinking in deep water
            )) {
            popup(_("You can't pass time while controlling a moving vehicle."));
            return;
        }
    }

    if (player_character.has_alarm_clock()) {
        int alarm_query = try_set_alarm();
        if (alarm_query == UILIST_CANCEL) {
            return;
        }
        setting_alarm = alarm_query == 1;
    }

    const bool has_watch = player_character.has_watch() || setting_alarm;

    const auto add_menu_item = [&as_m, &durations, has_watch]
    (int retval, int hotkey, const std::string& caption = "",
        const time_duration& duration = time_duration::from_turns(calendar::INDEFINITELY_LONG)) {

            std::string text(caption);

            if (has_watch && duration != time_duration::from_turns(calendar::INDEFINITELY_LONG)) {
                const std::string dur_str(to_string(duration));
                text += (text.empty() ? dur_str : string_format(" (%s)", dur_str));
            }
            as_m.addentry(retval, true, hotkey, text);
            durations.emplace(retval, duration);
    };

    if (setting_alarm) {

        add_menu_item(0, '0', "", 30_minutes);

        for (int i = 1; i <= 9; ++i) {
            add_menu_item(i, '0' + i, "", i * 1_hours);
        }

    }
    else {
        if (player_character.get_stamina() < player_character.get_stamina_max()) {
            as_m.addentry(14, true, 'w', _("Wait until you catch your breath"));
            durations.emplace(14, 15_minutes); // to hide it from showing
        }
        add_menu_item(1, '1', !has_watch ? _("Wait 20 heartbeats") : "", 20_seconds);
        add_menu_item(2, '2', !has_watch ? _("Wait 60 heartbeats") : "", 1_minutes);
        add_menu_item(3, '3', !has_watch ? _("Wait 300 heartbeats") : "", 5_minutes);
        add_menu_item(4, '4', !has_watch ? _("Wait 1800 heartbeats") : "", 30_minutes);

        if (has_watch) {
            add_menu_item(5, '5', "", 1_hours);
            add_menu_item(6, '6', "", 2_hours);
            add_menu_item(7, '7', "", 3_hours);
            add_menu_item(8, '8', "", 6_hours);
        }
    }

    if (here.get_abs_sub().z() >= 0 || has_watch) {
        const time_point last_midnight = calendar::turn - time_past_midnight(calendar::turn);
        const auto diurnal_time_before = [](const time_point& p) {
            // Either the given time is in the future (e.g. waiting for sunset while it's early morning),
            // than use it directly. Otherwise (in the past), add a single day to get the same time tomorrow
            // (e.g. waiting for sunrise while it's noon).
            const time_point target_time = p > calendar::turn ? p : p + 1_days;
            return target_time - calendar::turn;
        };

        add_menu_item(9, 'd',
            setting_alarm ? _("Set alarm for dawn") : _("Wait till daylight"),
            diurnal_time_before(daylight_time(calendar::turn)));
        add_menu_item(10, 'n',
            setting_alarm ? _("Set alarm for noon") : _("Wait till noon"),
            diurnal_time_before(last_midnight + 12_hours));
        add_menu_item(11, 'k',
            setting_alarm ? _("Set alarm for dusk") : _("Wait till night"),
            diurnal_time_before(night_time(calendar::turn)));
        add_menu_item(12, 'm',
            setting_alarm ? _("Set alarm for midnight") : _("Wait till midnight"),
            diurnal_time_before(last_midnight));
        if (setting_alarm) {
            if (player_character.has_effect(effect_alarm_clock)) {
                add_menu_item(13, 'x', _("Cancel the currently set alarm."),
                    0_turns);
            }
        }
        else {
            add_menu_item(13, 'W', _("Wait till weather changes"));
        }
    }

    // NOLINTNEXTLINE(cata-text-style): spaces required for concatenation
    as_m.text = has_watch ? string_format(_("It's %s now.  "),
        to_string_time_of_day(calendar::turn)) : "";
    as_m.text += setting_alarm ? _("Set alarm for when?") : _("Wait for how long?");
    as_m.query(); /* calculate key and window variables, generate window, and loop until we get a valid answer */

    const auto dur_iter = durations.find(as_m.ret);
    if (dur_iter == durations.end()) {
        return;
    }
    const time_duration time_to_wait = dur_iter->second;

    if (setting_alarm) {
        // Setting alarm
        player_character.remove_effect(effect_alarm_clock);
        if (as_m.ret == 13) {
            add_msg(_("You cancel your alarm."));
        }
        else {
            player_character.add_effect(effect_alarm_clock, time_to_wait);
            add_msg(_("You set your alarm."));
        }

    }
    else {
        // Waiting
        activity_id actType;
        if (as_m.ret == 13) {
            actType = ACT_WAIT_WEATHER;
        }
        else if (as_m.ret == 14) {
            actType = ACT_WAIT_STAMINA;
        }
        else {
            actType = ACT_WAIT;
        }

        player_activity new_act(actType, 100 * to_turns<int>(time_to_wait), 0);

        player_character.assign_activity(new_act, false);
    }
}

static void sleep()
{
    avatar& player_character = get_avatar();
    if (player_character.is_mounted()) {
        add_msg(m_info, _("You cannot sleep while mounted."));
        return;
    }

    vehicle* const boat = veh_pointer_or_null(get_map().veh_at(player_character.pos()));
    if (get_map().has_flag(ter_furn_flag::TFLAG_DEEP_WATER, player_character.pos()) &&
        !player_character.has_trait(trait_WATERSLEEPER) &&
        !player_character.has_trait(trait_WATERSLEEP) &&
        boat == nullptr) {
        add_msg(m_info, _("You cannot sleep while swimming."));
        return;
    }

    uilist as_m;
    as_m.text = _("<color_white>Are you sure you want to sleep?</color>");
    // (Y)es/(S)ave before sleeping/(N)o
    as_m.entries.emplace_back(0, true,
        get_option<bool>("FORCE_CAPITAL_YN") ? 'Y' : 'y',
        _("Yes."));
    as_m.entries.emplace_back(1, g->get_moves_since_last_save(),
        get_option<bool>("FORCE_CAPITAL_YN") ? 'S' : 's',
        _("Yes, and save game before sleeping."));
    as_m.entries.emplace_back(2, true,
        get_option<bool>("FORCE_CAPITAL_YN") ? 'N' : 'n',
        _("No."));

    // List all active items, bionics or mutations so player can deactivate them
    std::vector<std::string> active;
    for (item_location& it : player_character.all_items_loc()) {
        if (it->has_flag(flag_LITCIG) || (it->active && it->ammo_sufficient(&player_character) &&
            it->is_tool() && !it->has_flag(flag_SLEEP_IGNORE))) {
            active.push_back(it->tname());
        }
    }
    for (int i = 0; i < player_character.num_bionics(); i++) {
        const bionic& bio = player_character.bionic_at_index(i);
        if (!bio.powered) {
            continue;
        }

        // some bionics
        // bio_alarm is useful for waking up during sleeping
        if (bio.info().has_flag(STATIC(json_character_flag("BIONIC_SLEEP_FRIENDLY")))) {
            continue;
        }

        const bionic_data& info = bio.info();
        if (info.power_over_time > 0_kJ) {
            active.push_back(info.name.translated());
        }
    }
    for (auto& mut : player_character.get_mutations()) {
        const mutation_branch& mdata = mut.obj();
        if (mdata.cost > 0 && player_character.has_active_mutation(mut)) {
            active.push_back(player_character.mutation_name(mut));
        }
    }

    // check for deactivating any currently played music instrument.
    for (item*& item : player_character.inv_dump()) {
        if (item->active && item->get_use("musical_instrument") != nullptr) {
            player_character.add_msg_if_player(_("You stop playing your %s before trying to sleep."),
                item->tname());
            // deactivate instrument
            item->active = false;
        }
    }

    // ask for deactivation
    std::stringstream data;
    if (!active.empty()) {
        as_m.selected = 2;
        data << as_m.text << std::endl;
        data << _("You may want to extinguish or turn off:") << std::endl;
        data << " " << std::endl;
        for (auto& a : active) {
            data << "<color_red>" << a << "</color>" << std::endl;
        }
        as_m.text = data.str();
    }

    /* Calculate key and window variables, generate window,
       and loop until we get a valid answer. */
    as_m.query();

    if (as_m.ret == 1) {
        g->quicksave();
    }
    else if (as_m.ret == 2 || as_m.ret < 0) {
        return;
    }

    time_duration try_sleep_dur = 24_hours;
    std::string deaf_text;
    if (player_character.is_deaf() && !player_character.has_flag(json_flag_ALARMCLOCK)) {
        deaf_text = _("<color_c_red> (DEAF!)</color>");
    }
    if (player_character.has_alarm_clock()) {
        /* Reuse menu to ask player whether they want to set an alarm. */
        bool can_hibernate = player_character.get_hunger() < -60 &&
            player_character.has_active_mutation(trait_HIBERNATE);

        as_m.reset();
        as_m.text = can_hibernate ?
            _("You're engorged to hibernate.  The alarm would only attract attention.  "
                "Set an alarm anyway?") :
            _("You have an alarm clock.  Set an alarm?");
        as_m.text += deaf_text;

        as_m.entries.emplace_back(0, true,
            get_option<bool>("FORCE_CAPITAL_YN") ? 'N' : 'n',
            _("No, don't set an alarm."));

        for (int i = 3; i <= 9; ++i) {
            as_m.entries.emplace_back(i, true, '0' + i,
                string_format(_("Set alarm to wake up in %i hours."), i) + deaf_text);
        }

        as_m.query();
        if (as_m.ret >= 3 && as_m.ret <= 9) {
            player_character.add_effect(effect_alarm_clock, 1_hours * as_m.ret);
            try_sleep_dur = 1_hours * as_m.ret + 1_turns;
        }
        else if (as_m.ret < 0) {
            return;
        }
    }

    player_character.moves = 0;
    player_character.try_to_sleep(try_sleep_dur);
}



std::pair<point, point> draw_position() {


    return {
        point(TERMX / 4, TERMY / 4),
        point(TERMX / 2, TERMY / 2)
    };


}




void show_profession_status() {


    avatar& player_avatar = get_avatar();

    catacurses::window w_01;
    input_context ctxt("");
    ctxt.register_action("QUIT");

    ui_adaptor ui_01;
    ui_01.on_screen_resize([&](ui_adaptor&) {
        const std::pair<point, point> beg_and_max = draw_position();
        const point& beg = beg_and_max.first;
        w_01 = catacurses::newwin(20, 40, beg + point(0, 0));
        ui_01.position_from_window(w_01);
        });




    ui_01.mark_resize();





    ui_01.on_redraw([&](const ui_adaptor&) {
        werase(w_01);
        draw_border(w_01);
        trim_and_print(w_01, point(1, 1), 25, c_white, "当前职业: 丧尸主宰");
        trim_and_print(w_01, point(1, 4), 25, c_white, "等级: %s", player_avatar.dominator_of_zombies_lv);
        trim_and_print(w_01, point(1, 7), 25, c_white, "经验: %s", player_avatar.dominator_of_zombies_exp);
        trim_and_print(w_01, point(1, 10), 25, c_white, "已经支配了 %s 只丧尸", player_avatar.dominator_of_zombies_number_of_zombies_controlled);
        wnoutrefresh(w_01);
        });








    while (true) {
        ui_01.invalidate_ui();
        ui_manager::redraw_invalidated();
        const std::string action = ctxt.handle_input();
        if (action == "QUIT") {
            break;
        }
    }





}

// 网络功能
void handle_action_network() {


    uilist menu;

    enum choce_list {

        启动服务器 = 0,
        作为客户端加入

    };


    menu.addentry(启动服务器, true, '0', _("启动服务器"));
    menu.addentry(作为客户端加入, true, '1', _("作为客户端加入"));

    menu.query();

    int choice = menu.ret;

    if (choice == 启动服务器) {
    
        //std::string_view host = "127.0.0.1";
        //std::string_view port = "4454";
        //
       

        //server_breeze.bind_recv([](std::shared_ptr<asio2::udp_session>& session_ptr, std::string_view data)
        //    {
        //        //printf("%zu %.*s", data.size(), (int)data.size(), data.data());
        //        add_msg(m_good,_("%s"), data.data());
        //        session_ptr->async_send(data);

        //    });

        //server_breeze.start(host,port);

        //add_msg(m_good, _("服务器启动"));
    

    }
    else if(choice == 作为客户端加入) {
        
        /*std::string_view host = "127.0.0.1";
        std::string_view port = "4454";
        
        client_breeze.start(host,port);

        client_breeze.async_send("你好");*/


    
    
    
    
    }


}


void handle_action_data_retrieval() {

    uilist menu;

    enum choce_list {

        怪物数据检索 = 0

    };

    menu.addentry(怪物数据检索, true, '0', _("怪物数据检索"));

    menu.query();

    int choice = menu.ret;

    if (choice == 怪物数据检索) {

        std::vector<const mtype*> mtype_vec;

        uilist monster_data_retrieval_ui;

        monster_data_retrieval_ui.w_x_setup = 0;

        monster_data_retrieval_ui.w_width_setup = []() -> int {
            return TERMX;
        };
        monster_data_retrieval_ui.pad_right_setup = []() -> int {
            return TERMX - 30;
        };

        monster_data_retrieval_ui.selected = uistate.wishmonster_selected;

        monster_data_retrieval_ui_callback m_d_r_u_c(mtype_vec);

        monster_data_retrieval_ui.callback = &m_d_r_u_c;


        int i = 0;

        for (const mtype& montype : MonsterGenerator::generator().get_all_mtypes()) {


            monster_data_retrieval_ui.addentry(i, true, 0, montype.nname());
            monster_data_retrieval_ui.entries[i].extratxt.txt = montype.sym;
            monster_data_retrieval_ui.entries[i].extratxt.color = montype.color;
            monster_data_retrieval_ui.entries[i].extratxt.left = 1;
            i++;
            mtype_vec.push_back(&montype);

        }




        do {


            monster_data_retrieval_ui.query();


            if (monster_data_retrieval_ui.ret >= 0) {

                uistate.wishmonster_selected = monster_data_retrieval_ui.selected;

            }





        } while (monster_data_retrieval_ui.ret >= 0);






    }

}

#if defined(__ANDROID__)
void manage_extra_buttons() {
    jni_env->CallVoidMethod(j_activity, method_id_show_button_manage);
}
#endif



static void loot()
{
    enum ZoneFlags {
        None = 1,
        SortLoot = 2,
        SortLootStatic = 4,
        SortLootPersonal = 8,
        FertilizePlots = 16,
        ConstructPlots = 64,
        MultiFarmPlots = 128,
        Multichoptrees = 256,
        Multichopplanks = 512,
        Multideconvehicle = 1024,
        Multirepairvehicle = 2048,
        MultiButchery = 4096,
        MultiMining = 8192,
        MultiDis = 16384,
        MultiMopping = 32768,
        UnloadLoot = 65536
    };

    Character& player_character = get_player_character();
    int flags = 0;
    zone_manager& mgr = zone_manager::get_manager();
    const bool has_fertilizer = player_character.has_item_with_flag(flag_FERTILIZER);

    // reset any potentially disabled zones from a past activity
    mgr.reset_disabled();

    // cache should only happen if we have personal zones defined
    if (mgr.has_personal_zones()) {
        mgr.cache_data();
    }

    // Manually update vehicle cache.
    // In theory this would be handled by the related activity (activity_on_turn_move_loot())
    // but with a stale cache we never get that far.
    mgr.cache_vzones();

    flags |= g->check_near_zone(zone_type_LOOT_UNSORTED,
        player_character.pos()) ? SortLoot : 0;
    flags |= g->check_near_zone(zone_type_zone_unload_all, player_character.pos()) ||
        g->check_near_zone(zone_type_zone_strip, player_character.pos()) ? UnloadLoot : 0;
    if (g->check_near_zone(zone_type_FARM_PLOT, player_character.pos())) {
        flags |= FertilizePlots;
        flags |= MultiFarmPlots;
    }
    flags |= g->check_near_zone(zone_type_CONSTRUCTION_BLUEPRINT,
        player_character.pos()) ? ConstructPlots : 0;

    flags |= g->check_near_zone(zone_type_CHOP_TREES,
        player_character.pos()) ? Multichoptrees : 0;
    flags |= g->check_near_zone(zone_type_LOOT_WOOD,
        player_character.pos()) ? Multichopplanks : 0;
    flags |= g->check_near_zone(zone_type_VEHICLE_DECONSTRUCT,
        player_character.pos()) ? Multideconvehicle : 0;
    flags |= g->check_near_zone(zone_type_VEHICLE_REPAIR,
        player_character.pos()) ? Multirepairvehicle : 0;
    flags |= g->check_near_zone(zone_type_LOOT_CORPSE,
        player_character.pos()) ? MultiButchery : 0;
    flags |= g->check_near_zone(zone_type_MINING, player_character.pos()) ? MultiMining : 0;
    flags |= g->check_near_zone(zone_type_zone_disassemble,
        player_character.pos()) ? MultiDis : 0;
    flags |= g->check_near_zone(zone_type_MOPPING, player_character.pos()) ? MultiMopping : 0;
    if (flags == 0) {
        add_msg(m_info, _("There is no compatible zone nearby."));
        add_msg(m_info, _("Compatible zones are %s and %s"),
            mgr.get_name_from_type(zone_type_LOOT_UNSORTED),
            mgr.get_name_from_type(zone_type_FARM_PLOT));
        return;
    }

    uilist menu;
    menu.text = _("Pick action:");
    menu.desc_enabled = true;

    if (flags & SortLoot) {
        menu.addentry_desc(SortLootStatic, true, 'o', _("Sort out my loot (static zones only)"),
            _("Sorts out the loot from Loot: Unsorted zone to nearby appropriate Loot zones ignoring personal zones.  Uses empty space in your inventory or utilizes a cart, if you are holding one."));
    }

    if (flags & SortLoot) {
        menu.addentry_desc(SortLootPersonal, true, 'O', _("Sort out my loot (personal zones only)"),
            _("Sorts out the loot from Loot: Unsorted zone to nearby appropriate Loot zones ignoring static zones.  Uses empty space in your inventory or utilizes a cart, if you are holding one."));
    }

    if (flags & SortLoot) {
        menu.addentry_desc(SortLoot, true, 'I', _("Sort out my loot (all)"),
            _("Sorts out the loot from Loot: Unsorted zone to nearby appropriate Loot zones.  Uses empty space in your inventory or utilizes a cart, if you are holding one."));
    }

    if (flags & UnloadLoot) {
        menu.addentry_desc(UnloadLoot, true, 'U', _("Unload nearby containers"),
            _("Unloads any corpses or containers that are in their respective zones."));
    }

    if (flags & FertilizePlots) {
        menu.addentry_desc(FertilizePlots, has_fertilizer, 'f',
            !has_fertilizer ? _("Fertilize plots… you don't have any fertilizer") : _("Fertilize plots"),
            _("Fertilize any nearby Farm: Plot zones."));
    }

    if (flags & ConstructPlots) {
        menu.addentry_desc(ConstructPlots, true, 'c', _("Construct plots"),
            _("Work on any nearby Blueprint: construction zones."));
    }
    if (flags & MultiFarmPlots) {
        menu.addentry_desc(MultiFarmPlots, true, 'm', _("Farm plots"),
            _("Till and plant on any nearby farm plots - auto-fetch seeds and tools."));
    }
    if (flags & Multichoptrees) {
        menu.addentry_desc(Multichoptrees, true, 'C', _("Chop trees"),
            _("Chop down any trees in the designated zone - auto-fetch tools."));
    }
    if (flags & Multichopplanks) {
        menu.addentry_desc(Multichopplanks, true, 'P', _("Chop planks"),
            _("Auto-chop logs in wood loot zones into planks - auto-fetch tools."));
    }
    if (flags & Multideconvehicle) {
        menu.addentry_desc(Multideconvehicle, true, 'v', _("Deconstruct vehicle"),
            _("Auto-deconstruct vehicle in designated zone - auto-fetch tools."));
    }
    if (flags & Multirepairvehicle) {
        menu.addentry_desc(Multirepairvehicle, true, 'V', _("Repair vehicle"),
            _("Auto-repair vehicle in designated zone - auto-fetch tools."));
    }
    if (flags & MultiButchery) {
        menu.addentry_desc(MultiButchery, true, 'B', _("Butcher corpses"),
            _("Auto-butcher anything in corpse loot zones - auto-fetch tools."));
    }
    if (flags & MultiMining) {
        menu.addentry_desc(MultiMining, true, 'M', _("Mine Area"),
            _("Auto-mine anything in mining zone - auto-fetch tools."));
    }
    if (flags & MultiDis) {
        menu.addentry_desc(MultiDis, true, 'D', _("Disassemble items"),
            _("Auto-disassemble anything in disassembly zone - auto-fetch tools."));

    }
    if (flags & MultiMopping) {
        menu.addentry_desc(MultiMopping, true, 'p', _("Mop area"), _("Mop clean the area."));
    }

    menu.query();
    flags = (menu.ret >= 0) ? menu.ret : None;
    bool recache = false;

    switch (flags) {
    case None:
        add_msg(_("Never mind."));
        break;
    case SortLoot:
        player_character.assign_activity(ACT_MOVE_LOOT);
        break;
    case SortLootStatic:
        //temporarily disable personal zones
        for (const auto& i : mgr.get_zones()) {
            zone_data& zone = i.get();
            if (zone.get_is_personal() && zone.get_enabled()) {
                zone.set_enabled(false);
                zone.set_temporary_disabled(true);
                recache = true;
            }
        }
        if (recache) {
            mgr.cache_data();
        }
        player_character.assign_activity(ACT_MOVE_LOOT);
        break;
    case SortLootPersonal:
        //temporarily disable non personal zones
        for (const auto& i : mgr.get_zones()) {
            zone_data& zone = i.get();
            if (!zone.get_is_personal() && zone.get_enabled()) {
                zone.set_enabled(false);
                zone.set_temporary_disabled(true);
                recache = true;
            }
        }
        if (recache) {
            mgr.cache_data();
        }
        player_character.assign_activity(ACT_MOVE_LOOT);
        break;
    case UnloadLoot:
        player_character.assign_activity(
            player_activity(unload_loot_activity_actor()));
        break;
    case FertilizePlots:
        player_character.assign_activity(ACT_FERTILIZE_PLOT);
        break;
    case ConstructPlots:
        player_character.assign_activity(ACT_MULTIPLE_CONSTRUCTION);
        break;
    case MultiFarmPlots:
        player_character.assign_activity(ACT_MULTIPLE_FARM);
        break;
    case Multichoptrees:
        player_character.assign_activity(ACT_MULTIPLE_CHOP_TREES);
        break;
    case Multichopplanks:
        player_character.assign_activity(ACT_MULTIPLE_CHOP_PLANKS);
        break;
    case Multideconvehicle:
        player_character.assign_activity(ACT_VEHICLE_DECONSTRUCTION);
        break;
    case Multirepairvehicle:
        player_character.assign_activity(ACT_VEHICLE_REPAIR);
        break;
    case MultiButchery:
        player_character.assign_activity(ACT_MULTIPLE_BUTCHER);
        break;
    case MultiMining:
        player_character.assign_activity(ACT_MULTIPLE_MINE);
        break;
    case MultiDis:
        player_character.assign_activity(ACT_MULTIPLE_DIS);
        break;
    case MultiMopping:
        player_character.assign_activity(ACT_MULTIPLE_MOP);
        break;
    default:
        debugmsg("Unsupported flag");
        break;
    }
}

static void wear()
{
    avatar& player_character = get_avatar();
    item_location loc = game_menus::inv::wear(player_character);

    if (loc) {
        player_character.wear(loc);
    }
    else {
        add_msg(_("Never mind."));
    }
}

static void takeoff()
{
    avatar& player_character = get_avatar();
    item_location loc = game_menus::inv::take_off(player_character);

    if (loc) {
        player_character.takeoff(loc.obtain(player_character));
    }
    else {
        add_msg(_("Never mind."));
    }
}

static void read()
{
    avatar& player_character = get_avatar();
    // Can read items from inventory or within one tile (including in vehicles)
    item_location loc = game_menus::inv::read(player_character);

    if (loc) {
        if (loc->type->can_use("learn_spell")) {
            item spell_book = *loc.get_item();
            spell_book.get_use("learn_spell")->call(player_character, spell_book,
                spell_book.active, player_character.pos());
        }
        else {
            item_location ereader;
            if( loc.has_parent() ) {
                item_location parent = loc.parent_item();
                if( parent && parent->is_ebook_storage() ) {
                    ereader = parent;
                }
            }
            if( ereader ) {
                if( !ereader.held_by( player_character ) &&
                    !ereader->has_flag( flag_ALLOWS_REMOTE_USE ) ) {
                    const std::vector<const item *> ebooks = ereader->ebooks();
                    const auto selected_book = std::find( ebooks.begin(), ebooks.end(), loc.get_item() );
                    if( selected_book == ebooks.end() ) {
                        return;
                    }
                    const size_t book_index = std::distance( ebooks.begin(), selected_book );
                    ereader = ereader.obtain( player_character );
                    if( !ereader ) {
                        return;
                    }
                    const std::vector<const item *> obtained_ebooks = ereader->ebooks();
                    if( book_index >= obtained_ebooks.size() ) {
                        return;
                    }
                    loc = item_location( ereader, const_cast<item *>( obtained_ebooks[book_index] ) );
                }
                player_character.read( loc, ereader );
            } else {
                loc = loc.obtain( player_character );
                player_character.read( loc );
            }
        }
    }
    else {
        add_msg(_("Never mind."));
    }
}

// Perform a reach attach using wielded weapon
static void reach_attack(avatar& you)
{
    g->temp_exit_fullscreen();

    target_handler::trajectory traj = target_handler::mode_reach(you, you.get_wielded_item());

    if (!traj.empty()) {
        you.reach_attack(traj.back());
    }
    g->reenter_fullscreen();
}

static void fire()
{
    avatar& player_character = get_avatar();
    map& here = get_map();

    // Use vehicle turret or draw a pistol from a holster if unarmed
    if (!player_character.is_armed()) {

        const optional_vpart_position vp = here.veh_at(player_character.pos());

        turret_data turret;
        if (vp && (turret = vp->vehicle().turret_query(player_character.pos()))) {
            if (player_character.has_trait(trait_BRAWLER)) {
                add_msg(m_bad, _("You refuse to use the turret."));
                return;
            }
            avatar_action::fire_turret_manual(player_character, here, turret);
            return;
        }

        if (vp.part_with_feature("CONTROLS", true)) {
            if (player_character.has_trait(trait_BRAWLER)) {
                add_msg(m_bad, _("You refuse to use the turret."));
                return;
            }
            if (vp->vehicle().turrets_aim_and_fire_all_manual()) {
                return;
            }
        }

        std::vector<std::string> options;
        std::vector<std::function<void()>> actions;

        player_character.worn.fire_options(player_character, options, actions);
        if (!options.empty()) {
            int sel = uilist(_("Draw what?"), options);
            if (sel >= 0) {
                actions[sel]();
            }
        }
    }

    const item_location weapon = player_character.get_wielded_item();
    if (!weapon) {
        return;
    }

    if (weapon->is_gun() && !weapon->gun_current_mode().melee()) {
        avatar_action::fire_wielded_weapon(player_character);
    }
    else if (weapon->current_reach_range(player_character) > 1) {
        if (player_character.has_effect(effect_relax_gas)) {
            if (one_in(8)) {
                add_msg(m_good, _("Your willpower asserts itself, and so do you!"));
                reach_attack(player_character);
            }
            else {
                player_character.moves -= rng(2, 8) * 10;
                add_msg(m_bad, _("You're too pacified to strike anything…"));
            }
        }
        else {
            reach_attack(player_character);
        }
    }
}

static void open_movement_mode_menu()
{
    avatar& player_character = get_avatar();
    const std::vector<move_mode_id>& modes = move_modes_by_speed();
    const int cycle = 1027;
    uilist as_m;

    as_m.text = _("Change to which movement mode?");

    for (size_t i = 0; i < modes.size(); ++i) {
        const move_mode_id& curr = modes[i];
        std::string mode_name = curr->name();
        if( player_character.get_steed_type() == steed_type::ANIMAL ) {
            switch( curr->type() ) {
                case move_mode_type::RUNNING:
                    mode_name = _( "疾驰，基础消耗25" );
                    break;
                case move_mode_type::WALKING:
                    mode_name = _( "快步，基础消耗30" );
                    break;
                case move_mode_type::CROUCHING:
                    mode_name = _( "慢行，基础消耗50" );
                    break;
                case move_mode_type::PRONE:
                    mode_name = _( "缓步，基础消耗100" );
                    break;
            }
        }
        as_m.entries.emplace_back(static_cast<int>(i), player_character.can_switch_to(curr),
            curr->letter(), mode_name);
    }
    as_m.entries.emplace_back(cycle,
        player_character.can_switch_to(player_character.current_movement_mode()->cycle()),
        hotkey_for_action(ACTION_OPEN_MOVEMENT, /*maximum_modifier_count=*/1),
        _("Cycle move mode"));
    // This should select the middle move mode
    as_m.selected = std::floor(modes.size() / 2);
    as_m.query();

    if (as_m.ret != UILIST_CANCEL) {
        if (as_m.ret == cycle) {
            player_character.cycle_move_mode();
        }
        else {
            player_character.set_movement_mode(modes[as_m.ret]);
        }
    }
}

static void cast_spell()
{
    Character& player_character = get_player_character();

    std::vector<spell_id> spells = player_character.magic->spells();

    if (spells.empty()) {
        add_msg(game_message_params{ m_bad, gmf_bypass_cooldown },
            _("You don't know any spells to cast."));
        return;
    }

    bool can_cast_spells = false;
    for (const spell_id& sp : spells) {
        spell temp_spell = player_character.magic->get_spell(sp);
        if (temp_spell.can_cast(player_character)) {
            can_cast_spells = true;
        }
    }

    if (!can_cast_spells) {
        add_msg(game_message_params{ m_bad, gmf_bypass_cooldown },
            _("You can't cast any of the spells you know!"));
    }

    const int spell_index = player_character.magic->select_spell(player_character);
    if (spell_index < 0) {
        return;
    }

    spell& sp = *player_character.magic->get_spells()[spell_index];
    player_character.set_value("before_select_spell_id",string_format("%s",spell_index));
    player_character.cast_spell(sp, false, std::nullopt);
}

// returns true if the spell was assigned
bool Character::cast_spell(spell& sp, bool fake_spell,
    const std::optional<tripoint>& target = std::nullopt)
{
    if (is_armed() && !sp.has_flag(spell_flag::NO_HANDS) &&
        !get_wielded_item()->has_flag(flag_MAGIC_FOCUS) && !sp.check_if_component_in_hand(*this)) {
        add_msg(game_message_params{ m_bad, gmf_bypass_cooldown },
            _("You need your hands free to cast this spell!"));
        return false;
    }

    if (!magic->has_enough_energy(*this, sp)) {
        add_msg(game_message_params{ m_bad, gmf_bypass_cooldown },
            _("You don't have enough %s to cast the spell."),
            sp.energy_string());
        return false;
    }

    if (!sp.has_flag(spell_flag::NO_HANDS) && has_effect(effect_stunned)) {
        add_msg(game_message_params{ m_bad, gmf_bypass_cooldown },
            _("You can't focus enough to cast spell."));
        return false;
    }

    if (sp.energy_source() == magic_energy_type::hp && !has_quality(qual_CUT)) {
        add_msg(game_message_params{ m_bad, gmf_bypass_cooldown },
            _("You cannot cast Blood Magic without a cutting implement."));
        return false;
    }

    player_activity spell_act(ACT_SPELLCASTING, sp.casting_time(*this));
    // [0] this is used as a spell level override for items casting spells
    if (fake_spell) {
        spell_act.values.emplace_back(sp.get_level());
    }
    else {
        spell_act.values.emplace_back(-1);
    }
    // [1] if this value is 1, the spell never fails
    spell_act.values.emplace_back(0);
    // [2] this value overrides the mana cost if set to 0
    spell_act.values.emplace_back(1);
    spell_act.name = sp.id().c_str();
    if (magic->casting_ignore) {
        const std::vector<distraction_type> ignored_distractions = {
            distraction_type::noise,
            distraction_type::pain,
            distraction_type::attacked,
            distraction_type::hostile_spotted_near,
            distraction_type::hostile_spotted_far,
            distraction_type::talked_to,
            distraction_type::asthma,
            distraction_type::motion_alarm,
            distraction_type::weather_change,
            distraction_type::mutation
        };
        for (const distraction_type ignored : ignored_distractions) {
            spell_act.ignore_distraction(ignored);
        }
    }
    if (target) {
        spell_act.coords.emplace_back(get_map().getabs(*target));
    }
    assign_activity(spell_act, false);
    return true;
}

// this is here because it shares some things in common with cast_spell
bool bionic::activate_spell(Character& caster) const
{
    if (!caster.is_avatar() || !id->spell_on_activate) {
        // the return value tells us if the spell fails. if it has no spell it can't fail
        return true;
    }
    spell sp = id->spell_on_activate->get_spell();
    return caster.cast_spell(sp, true);
}

void game::open_consume_item_menu()
{
    uilist as_m;

    as_m.text = _("What do you want to consume?");

    as_m.entries.emplace_back(0, true, 'f', _("Food"));
    as_m.entries.emplace_back(1, true, 'd', _("Drink"));
    as_m.entries.emplace_back(2, true, 'm', _("Medication"));
    as_m.query();

    avatar& player_character = get_avatar();
    switch (as_m.ret) {
    case 0:
        avatar_action::eat(player_character, game_menus::inv::consume_food(player_character));
        break;
    case 1:
        avatar_action::eat(player_character, game_menus::inv::consume_drink(player_character));
        break;
    case 2:
        avatar_action::eat(player_character, game_menus::inv::consume_meds(player_character));
        break;
    default:
        break;
    }
}

static void handle_debug_mode()
{
    auto debug_mode_setup = [](uilist_entry& entry) -> void {
        entry.txt = string_format(_("Debug Mode (%1$s)"), debug_mode ? _("ON") : _("OFF"));
        entry.text_color = debug_mode ? c_green : c_light_gray;
    };

    // returns if entry became active
    auto debugmode_entry_setup = [](uilist_entry& entry, bool active) -> void {
        if (active)
        {
            entry.extratxt.txt = _("A");
            entry.extratxt.color = c_white_green;
            entry.text_color = c_green;
        }
        else
        {
            entry.extratxt.txt = " ";
            entry.extratxt.color = c_unset;
            entry.text_color = c_light_gray;
        }
    };

    static bool first_time = true;
    if (first_time) {
        first_time = false;
        debugmode::enabled_filters.clear();
        for (int i = 0; i < debugmode::DF_LAST; ++i) {
            debugmode::enabled_filters.emplace_back(static_cast<debugmode::debug_filter>(i));
        }
    }

    input_context ctxt("DEFAULTMODE");
    ctxt.register_action("debug_mode");

    uilist dbmenu;
    dbmenu.allow_anykey = true;
    dbmenu.title = _("Debug Mode Filters");
    dbmenu.text = string_format(_("Press [%1$s] to quickly toggle debug mode."),
        ctxt.get_desc("debug_mode"));

    dbmenu.entries.reserve(1 + debugmode::DF_LAST);

    dbmenu.addentry(0, true, 'd', " ");
    debug_mode_setup(dbmenu.entries[0]);

    dbmenu.addentry(1, true, 't', _("Toggle all filters"));
    bool toggle_value = true;

    for (int i = 0; i < debugmode::DF_LAST; ++i) {
        uilist_entry entry(i + 2, true, 0,
            debugmode::filter_name(static_cast<debugmode::debug_filter>(i)));

        entry.extratxt.left = 1;

        const bool active = std::find(
            debugmode::enabled_filters.begin(), debugmode::enabled_filters.end(),
            static_cast<debugmode::debug_filter>(i)) != debugmode::enabled_filters.end();

        if (toggle_value && active) {
            toggle_value = false;
        }

        debugmode_entry_setup(entry, active);
        dbmenu.entries.push_back(entry);
    }

    do {
        dbmenu.query();
        if (ctxt.input_to_action(dbmenu.ret_evt) == "debug_mode") {
            debug_mode = !debug_mode;
            if (debug_mode) {
                add_msg(m_info, _("Debug mode ON!"));
            }
            else {
                add_msg(m_info, _("Debug mode OFF!"));
            }
            break;
        }

        if (dbmenu.ret == 0) {
            debug_mode = !debug_mode;
            debug_mode_setup(dbmenu.entries[0]);

        }
        else if (dbmenu.ret == 1) {
            debugmode::enabled_filters.clear();

            for (int i = 0; i < debugmode::DF_LAST; ++i) {
                debugmode_entry_setup(dbmenu.entries[i + 2], toggle_value);

                if (toggle_value) {
                    debugmode::enabled_filters.emplace_back(static_cast<debugmode::debug_filter>(i));
                }
            }

            toggle_value = !toggle_value;

        }
        else if (dbmenu.ret > 1) {
            uilist_entry& entry = dbmenu.entries[dbmenu.ret];

            const auto filter_iter = std::find(
                debugmode::enabled_filters.begin(), debugmode::enabled_filters.end(),
                static_cast<debugmode::debug_filter>(dbmenu.ret - 2));

            const bool active = filter_iter != debugmode::enabled_filters.end();

            debugmode_entry_setup(entry, !active);

            if (active) {
                debugmode::enabled_filters.erase(filter_iter);
            }
            else {
                debugmode::enabled_filters.push_back(
                    static_cast<debugmode::debug_filter>(dbmenu.ret - 2));
            }
        }
    } while (dbmenu.ret != UILIST_CANCEL);
}

static bool has_vehicle_control(avatar& player_character)
{
    if (player_character.is_dead_state()) {
        return false;
    }
    const optional_vpart_position vp = get_map().veh_at(player_character.pos());
    if (vp && vp->vehicle().player_in_control(player_character)) {
        return true;
    }
    return g->remoteveh() != nullptr;
}

static void do_deathcam_action(const action_id& act, avatar& player_character)
{
    switch (act) {
    case ACTION_TOGGLE_MAP_MEMORY:
        player_character.toggle_map_memory();
        break;

    case ACTION_CENTER:
        player_character.view_offset.x = g->driving_view_offset.x;
        player_character.view_offset.y = g->driving_view_offset.y;
        break;

    case ACTION_SHIFT_N:
    case ACTION_SHIFT_NE:
    case ACTION_SHIFT_E:
    case ACTION_SHIFT_SE:
    case ACTION_SHIFT_S:
    case ACTION_SHIFT_SW:
    case ACTION_SHIFT_W:
    case ACTION_SHIFT_NW: {
        static const std::map<action_id, std::pair<point, point>> shift_delta = {
            { ACTION_SHIFT_N, { point_north, point_north_east } },
            { ACTION_SHIFT_NE, { point_north_east, point_east } },
            { ACTION_SHIFT_E, { point_east, point_south_east } },
            { ACTION_SHIFT_SE, { point_south_east, point_south } },
            { ACTION_SHIFT_S, { point_south, point_south_west } },
            { ACTION_SHIFT_SW, { point_south_west, point_west } },
            { ACTION_SHIFT_W, { point_west, point_north_west } },
            { ACTION_SHIFT_NW, { point_north_west, point_north } },
        };
        int soffset = get_option<int>("MOVE_VIEW_OFFSET");
        player_character.view_offset += g->is_tileset_isometric()
            ? shift_delta.at(act).second * soffset
            : shift_delta.at(act).first * soffset;
    }
                        break;

    case ACTION_LOOK:
        g->look_around();
        break;

    case ACTION_KEYBINDINGS: // already handled by input context
    default:
        break;
    }
}

// 检查绳梯路径上是否存在障碍物
// 检查内容包括：生物（中途+目标点）、不可通过地形（中途+目标点）、可登车载具部件（仅中途）
// 返回 true 表示存在障碍物阻止攀爬
static bool check_ladder_path_obstacles(const tripoint_bub_ms& start, int dist, bool going_up) {
    map& here = get_map();
    creature_tracker& creatures = get_creature_tracker();

    for (int i = 1; i <= dist; i++) {
        tripoint_bub_ms pt = start;
        if (going_up) {
            pt.z() += i;
        }
        else {
            pt.z() -= i;
        }

        bool is_destination = (i == dist);

        // 检查生物（中途和目标点，不包括起点）
        if (creatures.creature_at(pt, false)) {
            add_msg(m_warning, "绳梯路径上有生物阻挡！");
            return true;
        }

        // 检查不可通过地形（中途和目标点，不包括起点）
        if (here.impassable_ter_furn(pt.raw())) {
            add_msg(m_warning, "绳梯路径上有无法通过的地形！");
            return true;
        }

        // 检查可登车的载具部件（仅中途，不包括目标点）
        if (!is_destination) {
            const optional_vpart_position vp = here.veh_at(pt);
            if (vp && vp->part_with_feature(VPFLAG_BOARDABLE, true)) {
                add_msg(m_warning, "绳梯路径上有载具部件阻挡！");
                return true;
            }
        }
    }
    return false;
}

// 向下攀爬绳梯的路径与着陆点计算
// 逐层向下寻找第一个"可着陆点"作为目标：
//   - 地形非空气且可通过 → 可着陆
//   - 地形为空气但有可登车且可通过的载具部件 → 可着陆（踩到载具甲板）
// 中途遇生物或不可通过地形则阻挡。返回 nullopt 表示被阻挡（已输出警告）。
struct ladder_descent_result {
    int dist;               // 向下移动的层数
    bool dest_open_air;     // 目标格地形是否为空气
    bool dest_has_support;  // 目标格是否有可登车部件支撑
};
static std::optional<ladder_descent_result> check_ladder_descent(
    const tripoint_bub_ms& start, int ladder_len )
{
    map& here = get_map();
    creature_tracker& creatures = get_creature_tracker();
    const int max_dist = ladder_len - 1;
    if( max_dist <= 0 ) {
        return ladder_descent_result{ 0, false, false };
    }
    for( int i = 1; i <= max_dist; i++ ) {
        tripoint_bub_ms pt = start;
        pt.z() -= i;
        const bool is_last = ( i == max_dist );

        // 生物阻挡（中途+目标点）
        if( creatures.creature_at( pt, false ) ) {
            add_msg(m_warning, "绳梯路径上有生物阻挡！");
            return std::nullopt;
        }

        const bool open_air = here.ter( pt ).id().str() == "t_open_air";
        if( !open_air ) {
            // 非空气地形：不可通过则阻挡，可通过则为着陆点
            if( here.impassable_ter_furn( pt.raw() ) ) {
                add_msg(m_warning, "绳梯路径上有无法通过的地形！");
                return std::nullopt;
            }
            return ladder_descent_result{ i, false, false };
        }

        // 空气地形：检查是否有可登车且可通过的载具部件作为着陆点
        const optional_vpart_position vp = here.veh_at( pt );
        const bool boardable = vp && vp->part_with_feature( VPFLAG_BOARDABLE, true );
        if( boardable && here.passable( pt ) ) {
            return ladder_descent_result{ i, true, true };
        }

        // 空气且无可着陆部件：继续往下；若是最后一层则目标无支撑
        if( is_last ) {
            return ladder_descent_result{ i, true, boardable && here.passable( pt ) };
        }
    }
    // 理论不可达
    return ladder_descent_result{ max_dist, true, false };
}

bool game::do_regular_action(action_id& act, avatar& player_character,
    const std::optional<tripoint>& mouse_target)
{
    item_location weapon = player_character.get_wielded_item();
    bool in_shell = player_character.has_active_mutation(trait_SHELL2) ||
        player_character.has_active_mutation(trait_SHELL3);
    switch (act) {
    case ACTION_NULL: // dummy entry
    case NUM_ACTIONS: // dummy entry
    case ACTION_ACTIONMENU: // handled above
    case ACTION_MAIN_MENU:
    case ACTION_KEYBINDINGS:
        break;

    case ACTION_TIMEOUT:
        if (check_safe_mode_allowed(false)) {
            player_character.pause();
        }
        break;

    case ACTION_PAUSE:
        if (check_safe_mode_allowed()) {
            player_character.pause();
        }
        break;

    case ACTION_CYCLE_MOVE:
        player_character.cycle_move_mode();
        break;

    case ACTION_CYCLE_MOVE_REVERSE:
        player_character.cycle_move_mode_reverse();
        break;

    case ACTION_RESET_MOVE:
        player_character.reset_move_mode();
        break;

    case ACTION_TOGGLE_RUN:
        player_character.toggle_run_mode();
        break;

    case ACTION_TOGGLE_CROUCH:
        player_character.toggle_crouch_mode();
        break;

    case ACTION_TOGGLE_PRONE:
        player_character.toggle_prone_mode();
        break;

    case ACTION_OPEN_MOVEMENT:
        open_movement_mode_menu();
        break;

    case ACTION_MOVE_FORTH:
    case ACTION_MOVE_FORTH_RIGHT:
    case ACTION_MOVE_RIGHT:
    case ACTION_MOVE_BACK_RIGHT:
    case ACTION_MOVE_BACK:
    case ACTION_MOVE_BACK_LEFT:
    case ACTION_MOVE_LEFT:
    case ACTION_MOVE_FORTH_LEFT:
        if (!player_character.get_value("remote_controlling").empty() && 
            (player_character.has_active_item(itype_radiocontrol) ||
                player_character.has_active_bionic(bio_remote))) {
            rcdrive(get_delta_from_movement_action(act, iso_rotate::yes));
        }
        else if (has_vehicle_control(player_character)) {
            // vehicle control uses x for steering and y for ac/deceleration,
            // so no rotation needed
            pldrive(get_delta_from_movement_action(act, iso_rotate::no));
        }
        else if(g->get_now_controlled_monster() && player_character.has_active_item(itype_monster_controller_d)) {
            shared_ptr_fast<monster> m = g->get_now_controlled_monster();
            point dest_delta = get_delta_from_movement_action(act, iso_rotate::yes);
            tripoint_abs_ms goal = m->get_location() + dest_delta;
            m->set_dest(goal);
            m->remove_value("command_dirty");        
            player_character.moves = 0;
        }
        else {
            point dest_delta = get_delta_from_movement_action(act, iso_rotate::yes);
            if (auto_travel_mode && !player_character.is_auto_moving()) {
                for (int i = 0; i < SEEX; i++) {
                    tripoint_bub_ms auto_travel_destination =
                        player_character.pos_bub() + dest_delta * (SEEX - i);
                    destination_preview =
                        m.route(player_character.pos_bub(), auto_travel_destination,
                            player_character.get_pathfinding_settings(),
                            player_character.get_path_avoid());
                    if (!destination_preview.empty()) {
                        destination_preview.erase(
                            destination_preview.begin() + 1, destination_preview.end());
                        player_character.set_destination(destination_preview);
                        break;
                    }
                }
                act = player_character.get_next_auto_move_direction();
                const point dest_next = get_delta_from_movement_action(act, iso_rotate::yes);
                if (dest_next == point_zero) {
                    player_character.clear_destination();
                }
                dest_delta = dest_next;
            }
            // Capture the automatic-move state and action cost before stepping.  This lets
            // follower pacing limit the avatar's effective travel speed instead of only reacting
            // after a follower has already fallen behind.
            const bool auto_move_step = player_character.is_auto_moving();
            const int moves_before_step = player_character.moves;
            if( !auto_move_step ) {
                player_character.auto_travel_follower_wait_notified = false;
            }

            if (!avatar_action::move(player_character, m, dest_delta)) {
                // auto-move should be canceled due to a failed move or obstacle
                player_character.clear_destination();
            }

            if( auto_move_step && player_character.is_auto_moving() && !player_character.in_vehicle ) {
                const int player_speed = std::max( player_character.get_speed(), 1 );
                int slowest_speed = player_speed;
                npc *lagging_follower = nullptr;
                int worst_excess = 0;
                int lagging_distance = 0;

                for( npc *guy : g->get_npcs_if( []( const npc &n ) {
                return n.is_walking_with() && n.is_following() && !n.in_vehicle &&
                       !n.is_stationary( false );
                } ) ) {
                    const int follower_speed = std::max( guy->get_speed(), 1 );
                    slowest_speed = std::min( slowest_speed, follower_speed );

                    // Distance fallback also watches followers whose raw speed is not lower.
                    // Doors, corners, terrain and pathfinding can still make those NPCs lose ground.
                    const int lag = rl_dist( guy->pos(), player_character.pos() );
                    const int desired_radius = guy->rules.has_flag( ally_rule::follow_close ) ?
                                               guy->follow_distance() : 6;
                    const int threshold = std::max( desired_radius + 1, 4 );
                    const int excess = lag - threshold;
                    if( excess > worst_excess ) {
                        worst_excess = excess;
                        lagging_follower = guy;
                        lagging_distance = lag;
                    }
                }

                // Creature::process_turn() grants movement points equal to get_speed().  Scale the
                // cost of this automatic step by player_speed / slowest_speed so the avatar's
                // baseline travel rate cannot exceed the slowest active follower's rate.
                const int move_cost = std::max( 0, moves_before_step - player_character.moves );
                if( slowest_speed < player_speed && move_cost > 0 ) {
                    const int paced_cost = static_cast<int>( std::ceil(
                                               static_cast<double>( move_cost ) * player_speed /
                                               slowest_speed ) );
                    const int extra_cost = paced_cost - move_cost;
                    if( extra_cost > 0 ) {
                        player_character.mod_moves( -extra_cost );
                    }
                }

                if( lagging_follower != nullptr ) {
                    // Never silently abandon a follower after the old 20-tile safety cutoff.
                    // If the NPC is this far away, stop auto travel and let the player resolve it.
                    if( lagging_distance >= 20 ) {
                        if( player_character.moves > 0 ) {
                            player_character.moves = 0;
                        }
                        player_character.clear_destination();
                        add_msg( m_warning, _( "%s已经掉队，自动旅行已停止。" ),
                                 lagging_follower->get_name() );
                        player_character.auto_travel_follower_wait_notified = false;
                    } else {
                        // Spend only the remaining positive moves.  Keep negative move debt intact,
                        // otherwise the speed limiter above would be partially refunded.
                        if( player_character.moves > 0 ) {
                            player_character.moves = 0;
                        }
                        if( !player_character.auto_travel_follower_wait_notified ) {
                            add_msg( m_info, _( "你放慢脚步，等待%s跟上。" ),
                                     lagging_follower->get_name() );
                            player_character.auto_travel_follower_wait_notified = true;
                        }
                    }
                }
            }

            // Reaching the destination or canceling the route ends the notification scope.
            if( auto_move_step && !player_character.is_auto_moving() ) {
                player_character.auto_travel_follower_wait_notified = false;
            }

            if (get_option<bool>("AUTO_FEATURES") && get_option<bool>("AUTO_MOPPING") &&
                weapon && weapon->has_flag(json_flag_MOP)) {
                map& here = get_map();
                const bool is_blind = player_character.is_blind();
                for (const tripoint_bub_ms& point : here.points_in_radius(player_character.pos_bub(), 1)) {
                    bool did_mop = false;
                    if (is_blind) {
                        // blind character have a 1/3 chance of actually mopping
                        if (one_in(3)) {
                            did_mop = here.mop_spills(point);
                        }
                        else {
                            did_mop = here.terrain_moppable(point);
                        }
                    }
                    else {
                        did_mop = here.mop_spills(point);
                    }
                    // iuse::mop costs 15 moves per use
                    if (did_mop) {
                        player_character.mod_moves(-15);
                    }
                }
            }
        }
        break;
    case ACTION_MOVE_DOWN: {
        map& here = get_map();
        if (player_character.is_mounted() && get_option<bool>("骑乘状态可以上下楼") == false) {
            auto* mon = player_character.mounted_creature.get();
            if (!mon->has_flag(MF_RIDEABLE_MECH)) {
                add_msg(m_info, _("You can't go down stairs while you're riding."));
                break;
            }
        }
        if (g->get_now_controlled_monster() && player_character.has_active_item(itype_monster_controller_d)) {
            shared_ptr_fast<monster> m = g->get_now_controlled_monster();
            tripoint dest_delta(0, 0, -1);
            tripoint_abs_ms goal = m->get_location() + dest_delta;
            m->set_dest(goal);
            m->remove_value("command_dirty");
            player_character.moves = 0;
        }else if (player_character.in_vehicle) {
            if (has_vehicle_control(player_character)) {
                const optional_vpart_position vp = get_map().veh_at(player_character.pos());
                if (vp && vp->vehicle().is_rotorcraft()) {
                    pldrive(tripoint_below);
                    break;
                }
            }
            auto [found, rope_pos] = here.has_rope_at(player_character.pos_bub(), false);
            if (found) {
                const optional_vpart_position vp = here.veh_at(rope_pos);
                if (vp.has_value()) {
                    const int idx = vp->vehicle().part_with_feature(vp->part_index(), VPFLAG_LADDER, true);
                    if (idx != -1) {
                        const vpart_info& info = vp->vehicle().part(idx).info();
                        auto result = check_ladder_descent(player_character.pos_bub(), info.ladder_length());
                        if (!result) {
                            break;
                        }
                        bool confirm_unsupported = false;
                        if (result->dest_open_air && !result->dest_has_support) {
                            if (!query_yn(_("目标地点没有支撑物，确定要前往吗？"))) {
                                break;
                            }
                            confirm_unsupported = true;
                        }
                        here.unboard_vehicle(player_character.pos());
                        for (int i = 0; i < result->dist; i++) {
                            vertical_move(-1, true, false, confirm_unsupported && i == result->dist - 1);
                        }
                        // 登上目标载具（如到达B载具甲板）
                        if (here.veh_at(player_character.pos()).part_with_feature(VPFLAG_BOARDABLE, true)) {
                            here.board_vehicle(player_character.pos(), &player_character);
                        }
                        break;
                    }
                }
            }
        }
        else if (!player_character.in_vehicle) {
            // Check for vehicle rope ladder above before climbing down other ways
            if (auto [found, rope_pos] = here.has_rope_at(player_character.pos_bub(), false); found) {
                const optional_vpart_position vp = here.veh_at(rope_pos);
                if (vp.has_value()) {
                    const int idx = vp->vehicle().part_with_feature(vp->part_index(), VPFLAG_LADDER, true);
                    if (idx != -1) {
                        const vpart_info& info = vp->vehicle().part(idx).info();
                        auto result = check_ladder_descent(player_character.pos_bub(), info.ladder_length());
                        if (!result) {
                            break;
                        }
                        bool confirm_unsupported = false;
                        if (result->dest_open_air && !result->dest_has_support) {
                            if (!query_yn(_("目标地点没有支撑物，确定要前往吗？"))) {
                                break;
                            }
                            confirm_unsupported = true;
                        }
                        for (int i = 0; i < result->dist; i++) {
                            vertical_move(-1, true, false, confirm_unsupported && i == result->dist - 1);
                        }
                        // 登上目标载具（如到达B载具甲板）
                        if (here.veh_at(player_character.pos()).part_with_feature(VPFLAG_BOARDABLE, true)) {
                            here.board_vehicle(player_character.pos(), &player_character);
                        }
                        break;
                    }
                }
            }
            vertical_move(-1, false);
        }
        else if (has_vehicle_control(player_character)) {
            const optional_vpart_position vp = get_map().veh_at(player_character.pos());
            if (vp->vehicle().is_rotorcraft()) {
                pldrive(tripoint_below);
            }
        }
        break;
    }
    case ACTION_MOVE_UP: {
        map& here = get_map();
        if (player_character.is_mounted() && get_option<bool>("骑乘状态可以上下楼") == false) {
            auto* mon = player_character.mounted_creature.get();
            if (!mon->has_flag(MF_RIDEABLE_MECH)) {
                add_msg(m_info, _("You can't go up stairs while you're riding."));
                break;
            }
        }
        if (g->get_now_controlled_monster() && player_character.has_active_item(itype_monster_controller_d)) {
            shared_ptr_fast<monster> m = g->get_now_controlled_monster();
            tripoint dest_delta(0, 0, 1);
            tripoint_abs_ms goal = m->get_location() + dest_delta;
            m->set_dest(goal);
            m->remove_value("command_dirty");
            player_character.moves = 0;
        }
        else if (auto [found, rope_pos] = here.has_rope_at(player_character.pos_bub(), true); found) {
            const auto& veh_pair = here.get_rope_at(rope_pos);
            vehicle* veh = veh_pair.first;
            int veh_part = veh_pair.second;
            tripoint_bub_ms pt = player_character.pos_bub();
            const int ladder_len = veh->part(veh_part).info().ladder_length();
            int dist = rope_pos.z() - pt.z();
            if (dist >= ladder_len) {
                dist = ladder_len - 1;
            }
            if (check_ladder_path_obstacles(player_character.pos_bub(), dist, true)) {
                break;
            }
            // 离开当前载具（如在B载具上爬上到A载具）
            if (player_character.in_vehicle) {
                here.unboard_vehicle(player_character.pos());
            }
            for (int i = 0; i < dist; i++) {
                vertical_move(1, true, false, false);
            }
            // 登上目标载具（如到达A载具甲板）
            if (here.veh_at(player_character.pos()).part_with_feature(VPFLAG_BOARDABLE, true)) {
                here.board_vehicle(player_character.pos(), &player_character);
            }
            break;
        }
        else if (!player_character.in_vehicle) {
            vertical_move(1, false);
        }
        else if (has_vehicle_control(player_character)) {
            const optional_vpart_position vp = get_map().veh_at(player_character.pos());
            if (vp->vehicle().is_rotorcraft()) {
                pldrive(tripoint_above);
            }
        }
        break;
    }
    case ACTION_OPEN:
        if (in_shell) {
            add_msg(m_info, _("You can't open things while you're in your shell."));
        }
        else if (u.has_effect(effect_incorporeal)) {
            add_msg(m_info, _("You lack the substance to affect anything."));
        }
        else {
            open();
        }
        break;

    case ACTION_CLOSE:
        if (in_shell) {
            add_msg(m_info, _("You can't close things while you're in your shell."));
        }
        else if (player_character.has_effect(effect_incorporeal)) {
            add_msg(m_info, _("You lack the substance to affect anything."));
        }
        else if (mouse_target) {
            if( player_character.is_mounted() &&
                !is_mounted_passage_openable( m, *mouse_target ) ) {
                add_msg( m_info, _( "骑乘时只能关门或关窗。" ) );
            } else {
                doors::close_door( m, player_character, *mouse_target );
            }
        }
        else {
            close();
        }
        break;

    case ACTION_SMASH:
        if (has_vehicle_control(player_character)) {
            handbrake();
        }
        else if (in_shell) {
            add_msg(m_info, _("You can't smash things while you're in your shell."));
        }
        else if (u.has_effect(effect_incorporeal)) {
            add_msg(m_info, _("You lack the substance to affect anything."));
        }
        else {
            smash();
        }
        break;

    case ACTION_EXAMINE:
    case ACTION_EXAMINE_AND_PICKUP:
        if (in_shell) {
            add_msg(m_info, _("You can't examine your surroundings while you're in your shell."));
        }
        else if (mouse_target) {
            // Examine including item pickup if ACTION_EXAMINE_AND_PICKUP is used
            examine(*mouse_target, act == ACTION_EXAMINE_AND_PICKUP);
        }
        else {
            examine(act == ACTION_EXAMINE_AND_PICKUP);
        }
        break;

    case ACTION_ADVANCEDINV:
        if (in_shell) {
            add_msg(m_info, _("You can't move mass quantities while you're in your shell."));
        }
        else if (player_character.is_mounted()) {
            add_msg(m_info, _("You can't move mass quantities while you're riding."));
        }
        else if (u.has_effect(effect_incorporeal)) {
            add_msg(m_info, _("You lack the substance to affect anything."));
        }
        else {
            create_advanced_inv();
        }
        break;

    case ACTION_PICKUP:
    case ACTION_PICKUP_ALL:
        if (in_shell) {
            add_msg(m_info, _("You can't pick anything up while you're in your shell."));
        }
        else if (player_character.is_mounted()) {
            add_msg(m_info, _("You can't pick anything up while you're riding."));
        }
        else if (u.has_effect(effect_incorporeal)) {
            add_msg(m_info, _("You lack the substance to affect anything."));
        }
        else if (mouse_target) {
            pickup(*mouse_target);
        }
        else {
            if (act == ACTION_PICKUP_ALL) {
                pickup_all();
            }
            else {
                pickup();
            }
        }
        break;

    case ACTION_GRAB:
        if (in_shell) {
            add_msg(m_info, _("You can't grab things while you're in your shell."));
        }
        else if (player_character.is_mounted()) {
            add_msg(m_info, _("You can't grab things while you're riding."));
        }
        else if (u.has_effect(effect_incorporeal)) {
            add_msg(m_info, _("You lack the substance to affect anything."));
        }
        else {
            grab();
        }
        break;

    case ACTION_HAUL:
        if (in_shell) {
            add_msg(m_info, _("You can't haul things while you're in your shell."));
        }
        else if (player_character.is_mounted()) {
            add_msg(m_info, _("You can't haul things while you're riding."));
        }
        else if (u.has_effect(effect_incorporeal)) {
            add_msg(m_info, _("You lack the substance to affect anything."));
        }
        else {
            haul();
        }
        break;

    case ACTION_BUTCHER:
        if (in_shell) {
            add_msg(m_info, _("You can't butcher while you're in your shell."));
        }
        else if (player_character.is_mounted()) {
            add_msg(m_info, _("You can't butcher while you're riding."));
        }
        else if (u.has_effect(effect_incorporeal)) {
            add_msg(m_info, _("You lack the substance to affect anything."));
        }
        else {
            butcher();
        }
        break;

    case ACTION_CHAT:
        chat();
        break;

    case ACTION_PEEK:
        if (in_shell) {
            add_msg(m_info, _("You can't peek around corners while you're in your shell."));
        }
        else if (player_character.is_mounted()) {
            add_msg(m_info, _("You can't peek around corners while you're riding."));
        }
        else {
            peek();
        }
        break;

    case ACTION_LIST_ITEMS:
        list_items_monsters();
        break;

    case ACTION_ZONES:
        zones_manager();
        break;

    case ACTION_LOOT:
        loot();
        break;

    case ACTION_INVENTORY:
        game_menus::inv::common(player_character);
        break;

    case ACTION_COMPARE:
        game_menus::inv::compare(player_character, std::nullopt);
        break;

    case ACTION_ORGANIZE:
        game_menus::inv::swap_letters(player_character);
        break;

    case ACTION_USE:
        // Shell-users are presumed to be able to mess with their inventories, etc
        // while in the shell.  Eating, gear-changing, and item use are OK.
        avatar_action::use_item(player_character);
        break;

    case ACTION_USE_WIELDED:
        player_character.use_wielded();
        break;

    case ACTION_WEAR:
        wear();
        break;

    case ACTION_TAKE_OFF:
        takeoff();
        break;

    case ACTION_EAT:
        if ( (player_character.is_driving() && !query_yn(_("你正处于驾驶状态，确定要进食/消耗物品吗？"))) || !avatar_action::eat_here(player_character)) {
            avatar_action::eat(player_character, game_menus::inv::consume(player_character));
        }
        break;

    case ACTION_OPEN_CONSUME:
        if (!avatar_action::eat_here(player_character)) {
            open_consume_item_menu();
        }
        break;

    case ACTION_READ:
        // Shell-users are presumed to have the book just at an opening and read it that way
        read();
        break;

    case ACTION_WIELD:
        wield();
        break;

    case ACTION_PICK_STYLE:
        player_character.martial_arts_data->pick_style(player_character);
        break;

    case ACTION_RELOAD_ITEM:
        reload_item();
        break;

    case ACTION_RELOAD_WEAPON:
        reload_weapon();
        break;

    case ACTION_RELOAD_WIELDED:
        reload_wielded();
        break;

    case ACTION_UNLOAD:
        avatar_action::unload(player_character);
        break;

    case ACTION_MEND:
        avatar_action::mend(player_character, item_location());
        break;

    case ACTION_THROW: {
        if( !push_controlled_creature( player_character ) &&
            !throw_grabbed_creature( player_character ) &&
            !throw_grabbed_furniture( player_character ) &&
            !throw_grabbed_vehicle( player_character ) ) {
            item_location loc;
            avatar_action::plthrow( player_character, loc );
        }
        break;
    }

    case ACTION_FIRE:
        fire();
        break;

    case ACTION_CAST_SPELL:
        cast_spell();
        break;

    case ACTION_CAST_BEFORE_SELECT_SPELL: {
        
        if (!player_character.has_value("before_select_spell_id")) {
            break;
        }

        int spell_id = std::stoi(player_character.get_value("before_select_spell_id"));
        
        if (spell_id >= player_character.magic->get_spells().size()) {
            break;
        }

        spell& sp = *player_character.magic->get_spells()[spell_id];
        player_character.cast_spell(sp, false, std::nullopt);
        break;

    }

    case ACTION_FIRE_BURST: {
        if (weapon) {
            gun_mode_id original_mode = weapon->gun_get_mode_id();
            if (weapon->gun_set_mode(gun_mode_AUTO)) {
                avatar_action::fire_wielded_weapon(player_character);
                weapon->gun_set_mode(original_mode);
            }
        }
        break;
    }

    case ACTION_SELECT_FIRE_MODE:
        if (weapon) {
            if (weapon->is_gun() && !weapon->is_gunmod() &&
                weapon->gun_all_modes().size() > 1) {
                weapon->gun_cycle_mode();
            }
            else if (weapon->has_flag(flag_RELOAD_ONE) ||
                weapon->has_flag(flag_RELOAD_AND_SHOOT)) {
                item::reload_option opt = player_character.select_ammo(weapon, false);
                if (!opt) {
                    break;
                }
                else if (player_character.ammo_location && opt.ammo == player_character.ammo_location) {
                    player_character.ammo_location = item_location();
                }
                else {
                    player_character.ammo_location = opt.ammo;
                }
            }
        }
        break;

    case ACTION_UNLOAD_CONTAINER:
        // You CAN drop things to your own tile while in the shell.
        unload_container();
        break;

    case ACTION_DROP:
        drop_in_direction(player_character.pos());
        break;
    case ACTION_DIR_DROP:
        if (const std::optional<tripoint> pnt = choose_adjacent(_("Drop where?"))) {
            if (*pnt != player_character.pos() &&
                in_shell) {
                add_msg(m_info, _("You can't drop things to another tile while you're in your shell."));
            }
            else {
                drop_in_direction(*pnt);
            }
        }
        break;
    case ACTION_BIONICS:
        player_character.power_bionics();
        break;
    case ACTION_MUTATIONS:
        player_character.power_mutations();
        break;

    case ACTION_SORT_ARMOR:
        player_character.worn.sort_armor(player_character);
        break;

    case ACTION_WAIT:
        wait();
        break;

    case ACTION_CRAFT:
        if (in_shell) {
            add_msg(m_info, _("You can't craft while you're in your shell."));
        }
        else if (player_character.has_effect(effect_incorporeal)) {
            add_msg(m_info, _("You lack the substance to affect anything."));
        }
        else if (player_character.is_mounted()) {
            add_msg(m_info, _("You can't craft while you're riding."));
        }
        else {
            player_character.craft();
        }
        break;

    case ACTION_RECRAFT:
        if (in_shell) {
            add_msg(m_info, _("You can't craft while you're in your shell."));
        }
        else if (player_character.has_effect(effect_incorporeal)) {
            add_msg(m_info, _("You lack the substance to affect anything."));
        }
        else if (player_character.is_mounted()) {
            add_msg(m_info, _("You can't craft while you're riding."));
        }
        else {
            player_character.recraft();
        }
        break;

    case ACTION_LONGCRAFT:
        if (in_shell) {
            add_msg(m_info, _("You can't craft while you're in your shell."));
        }
        else if (player_character.is_mounted()) {
            add_msg(m_info, _("You can't craft while you're riding."));
        }
        else if (u.has_effect(effect_incorporeal)) {
            add_msg(m_info, _("You lack the substance to affect anything."));
        }
        else {
            player_character.long_craft();
        }
        break;

    case ACTION_DISASSEMBLE:
        if (in_shell) {
            add_msg(m_info, _("You can't disassemble while you're in your shell."));
        }
        else if (player_character.controlling_vehicle) {
            add_msg(m_info, _("You can't disassemble items while driving."));
        }
        else if (player_character.is_mounted()) {
            add_msg(m_info, _("You can't disassemble items while you're riding."));
        }
        else if (u.has_effect(effect_incorporeal)) {
            add_msg(m_info, _("You lack the substance to affect anything."));
        }
        else {
            player_character.disassemble();
        }
        break;

    case ACTION_CONSTRUCT:
        if (player_character.in_vehicle) {
            add_msg(m_info, _("You can't construct while in a vehicle."));
        }
        else if (in_shell) {
            add_msg(m_info, _("You can't construct while you're in your shell."));
        }
        else if (player_character.is_mounted()) {
            add_msg(m_info, _("You can't construct while you're riding."));
        }
        else if (u.has_effect(effect_incorporeal)) {
            add_msg(m_info, _("You lack the substance to affect anything."));
        }
        else {
            construction_menu(false);
        }
        break;

    case ACTION_SLEEP:
        if (has_vehicle_control(player_character)) {
            add_msg(m_info, _("Vehicle control has moved, %s"),
                press_x(ACTION_CONTROL_VEHICLE, _("new binding is "),
                    _("new default binding is '^'.")));
        }
        else {
            sleep();
        }
        break;

    case ACTION_CONTROL_VEHICLE:
        if (in_shell) {
            add_msg(m_info, _("You can't operate a vehicle while you're in your shell."));
        }
        else if (player_character.is_mounted()) {
            player_character.dismount();
        }
        else if (player_character.has_trait(trait_WAYFARER)) {
            add_msg(m_info, _("You refuse to take control of this vehicle."));
        }
        else if (u.has_effect(effect_incorporeal)) {
            add_msg(m_info, _("You lack the substance to affect anything."));
        }
        else {
            control_vehicle();
        }
        break;

    case ACTION_TOGGLE_AUTO_TRAVEL_MODE:
        auto_travel_mode = !auto_travel_mode;
        add_msg(m_info, auto_travel_mode ? _("Auto travel mode ON!") : _("Auto travel mode OFF!"));
        break;

    case ACTION_TOGGLE_SAFEMODE:
        if (safe_mode == SAFE_MODE_OFF) {
            set_safe_mode(SAFE_MODE_ON);
            mostseen = 0;
            add_msg(m_info, _("Safe mode ON!"));
        }
        else {
            turnssincelastmon = 0_turns;
            set_safe_mode(SAFE_MODE_OFF);
            add_msg(m_info, get_option<bool>("AUTOSAFEMODE")
                ? _("Safe mode OFF!  (Auto safe mode still enabled!)") : _("Safe mode OFF!"));
        }
        if (player_character.has_effect(effect_laserlocked)) {
            player_character.remove_effect(effect_laserlocked);
            safe_mode_warning_logged = false;
        }
        break;

    case ACTION_TOGGLE_AUTOSAFE: {
        options_manager::cOpt& autosafemode_option = get_options().get_option("AUTOSAFEMODE");
        add_msg(m_info, autosafemode_option.value_as<bool>()
            ? _("Auto safe mode OFF!") : _("Auto safe mode ON!"));
        autosafemode_option.setNext();
        break;
    }

    case ACTION_IGNORE_ENEMY:
        if (safe_mode == SAFE_MODE_STOP) {
            add_msg(m_info, _("Ignoring enemy!"));
            for (auto& elem : player_character.get_mon_visible().new_seen_mon) {
                monster& critter = *elem;
                critter.ignoring = rl_dist(player_character.pos(), critter.pos());
            }
            set_safe_mode(SAFE_MODE_ON);
        }
        else if (player_character.has_effect(effect_laserlocked)) {
            if (player_character.has_trait(trait_PROF_CHURL)) {
                add_msg(m_warning, _("You make the sign of the cross."));
            }
            else {
                add_msg(m_info, _("Ignoring laser targeting!"));
            }
            player_character.remove_effect(effect_laserlocked);
            safe_mode_warning_logged = false;
        }
        break;

    case ACTION_WHITELIST_ENEMY:
        if (safe_mode == SAFE_MODE_STOP && !get_safemode().empty()) {
            get_safemode().add_rule(get_safemode().lastmon_whitelist, Creature::Attitude::ANY, 0,
                rule_state::WHITELISTED);
            add_msg(m_info, _("Creature whitelisted: %s"), get_safemode().lastmon_whitelist);
            set_safe_mode(SAFE_MODE_ON);
            mostseen = 0;
        }
        else {
            get_safemode().show();
        }
        break;

    case ACTION_WORKOUT:
        if (query_yn(_("Start workout?"))) {
            player_character.assign_activity(player_activity(workout_activity_actor(
                player_character.pos())));
        }
        break;

    case ACTION_SUICIDE:
        if (query_yn(_("Abandon this character?"))) {
            if (query_yn(_("This will kill your character.  Continue?"))) {
                player_character.moves = 0;
                player_character.place_corpse();
                uquit = QUIT_SUICIDE;
            }
        }
        break;

    case ACTION_SAVE:
        if (query_yn(_("真的要退出到游戏开始界面吗?"))) {

            player_character.moves = 0;
            uquit = QUIT_SAVED;

        }
        break;

    case ACTION_QUICKSAVE:
        quicksave();
        return false;

    case ACTION_QUICKLOAD:
        quickload();
        return false;

    case ACTION_PL_INFO:
        player_character.disp_info(true);
        break;

    case ACTION_MAP:
        if (!m.is_outside(player_character.pos())) {
            uistate.overmap_visible_weather = false;
        }
        if (!get_timed_events().get(timed_event_type::OVERRIDE_PLACE)) {
            ui::omap::display();
        }
        else {
            add_msg(m_info, _("You have no idea where you are."));
        }
        break;

    case ACTION_SKY:
        if (m.is_outside(player_character.pos())) {
            ui::omap::display_visible_weather();
        }
        else {
            add_msg(m_info, _("You can't see the sky from here."));
        }
        break;

    case ACTION_MISSIONS:
        list_missions();
        break;

    case ACTION_DIARY:
        diary::show_diary_ui(u.get_avatar_diary());
        break;
    case ACTION_显示当前职业情况:

        show_profession_status();

        break;
    case ACTION_命令视野中的我方丧尸全部等待: {
    
    
        for (Creature* c : u.get_visible_creatures(MAPSIZE_X)) {


            monster* m = dynamic_cast<monster*>(c);

            if (  m != nullptr) {

                monster& m_ref = *m;

                if (m_ref.in_species(species_ZOMBIE) && m_ref.has_effect(effect_pet) && ! m_ref.has_effect(effect_wait_here)) {
                       
       
                    m_ref.add_effect(effect_wait_here,1_turns,true);

    
                }           
            
            }


        }
    
        

        break;
    
    
    }

        
    case ACTION_结束视野中的我方全部丧尸的等待状态: {
      
        for (Creature* c : u.get_visible_creatures(MAPSIZE_X)) {


            monster* m = dynamic_cast<monster*>(c);

            if (m != nullptr) {

                monster& m_ref = *m;

                if (m_ref.in_species(species_ZOMBIE) && m_ref.has_effect(effect_pet) && m_ref.has_effect(effect_wait_here)) {


                    m_ref.remove_effect(effect_wait_here);


                }

            }


        }



        break;


    }
    
    case ACTION_fuse_corpses_that_can_revive : {
        

        if (get_player_character().get_stamina() - 9000 + 1000 * get_avatar().dominator_of_zombies_lv < 0) {


            add_msg(m_info, _("你的耐力不够。"));


            break;


        }



       // num 用于记录可以复活的丧尸数量，我们需要30具可以复活的丧尸尸体融合成一个 放荡吞噬者
        int num = 0;

        map& here = get_map();

        for (const tripoint& p : here.points_in_radius(get_player_character().pos(), 1)) {
           
            map_stack items = here.i_at(p);

            for (item &i : items) {

                if ( i.can_revive() && i.get_mtype()->in_species(species_ZOMBIE) ) {
                    num++;                  
                }
            
            
            }
            
               
        }

         
        
        if ( num >= 30) {


            const std::optional<tripoint> pnt = choose_adjacent(string_format(
                _("选择一个方向来进行 融合可以复活的丧尸尸体 的流程")));


            if (!pnt) {

                break;

            }

            if ( *pnt == get_player_character().pos()) {
                
                add_msg(m_info, _("你不能选择这个方向。"));
                break;

            }

            if ( get_map().impassable(*pnt) ) {
            
                add_msg(m_info, _("你不能选择这个方向。"));
                break;
            
            }


            creature_tracker& creatures = get_creature_tracker();

            if (creatures.creature_at<Character>(*pnt)) {


                add_msg(m_info, _("你不能选择这个方向。"));
                break;

            }
            else if (monster* const mon = creatures.creature_at<monster>(*pnt, true)) {

                add_msg(m_info, _("你不能选择这个方向。"));
                break;

            }


            int count = 0;

            for (const tripoint& p : here.points_in_radius(get_player_character().pos(), 1)) {

                map_stack items = here.i_at(p);

                for (item_stack::iterator iter = items.begin(); iter != items.end();) {

                    if (iter->can_revive() && iter->get_mtype()->in_species(species_ZOMBIE)) {
                                                
                        iter = items.erase(iter);

                        count++;
              
                        if ( count==30 ) {

                            break;
                        
                        }

                    }
                    else {
                                       
                        iter++;
                                        
                    }
                    
                }

                if (count == 30) {

                    break;

                }

            }

            shared_ptr_fast<monster> newmon_ptr = make_shared_fast<monster>(mon_devourer);
            monster& newmon = *newmon_ptr;
            newmon.friendly = -1;
            newmon.add_effect(effect_pet, 1_turns, true);
            newmon.no_extra_death_drops = true;
            
            g->place_critter_at(newmon_ptr,*pnt);

            add_msg(m_good, _("你将这些可以复活的丧尸尸体融合成了一个 放荡吞噬者。"));
            
            get_player_character().moves = get_player_character().moves - 100;

            get_player_character().set_stamina( get_player_character().get_stamina() - 9000 + 1000 * get_avatar().dominator_of_zombies_lv );
        
        }
        else {
           
  
            add_msg(m_info,_("为了融合成 放荡吞噬者，还需要 %s具 可以复活的丧尸尸体。"), 30-num );
        
        }





        
        
        
        
        break;
    
    
    }
              
    case ACTION_SCORES:
        show_scores_ui(*achievements_tracker_ptr, stats(), get_kill_tracker());
        break;

    case ACTION_FACTIONS:
        faction_manager_ptr->display();
        break;

    case ACTION_MORALE:
        player_character.disp_morale();
        break;

    case ACTION_MEDICAL:
        player_character.disp_medical();
        break;

    case ACTION_BODYSTATUS:
        display_bodygraph(get_player_character());
        break;

    case ACTION_MESSAGES:
        Messages::display_messages();
        break;

    case ACTION_HELP:
        get_help().display_help();
        break;

    case ACTION_OPTIONS:
        get_options().show(true);
        break;

    case ACTION_AUTOPICKUP:
        get_auto_pickup().show();
        break;

    case ACTION_AUTONOTES:
        get_auto_notes_settings().show_gui();
        break;

    case ACTION_SAFEMODE:
        get_safemode().show();
        break;

    case ACTION_DISTRACTION_MANAGER:
        get_distraction_manager().show();
        break;

    case ACTION_COLOR:
        all_colors.show_gui();
        break;

    case ACTION_WORLD_MODS:
        world_generator->show_active_world_mods(world_generator->active_world->active_mod_order);
        break;

    case ACTION_DEBUG:
        if (MAP_SHARING::isCompetitive() && !MAP_SHARING::isDebugger()) {
            break;    //don't do anything when sharing and not debugger
        }
        debug_menu::debug();
        break;

    case ACTION_TOGGLE_FULLSCREEN:
        toggle_fullscreen();
        break;

    case ACTION_TOGGLE_PIXEL_MINIMAP:
        toggle_pixel_minimap();
        break;

    case ACTION_TOGGLE_PANEL_ADM:
        panel_manager::get_manager().show_adm();
        break;

    case ACTION_RELOAD_TILESET:
        reload_tileset();
        break;

    case ACTION_TOGGLE_AUTO_FEATURES:
        get_options().get_option("AUTO_FEATURES").setNext();
        get_options().save();
        //~ Auto Features are now ON/OFF
        add_msg(_("%s are now %s."),
            get_options().get_option("AUTO_FEATURES").getMenuText(),
            get_option<bool>("AUTO_FEATURES") ? _("ON") : _("OFF"));
        break;

    case ACTION_TOGGLE_AUTO_PULP_BUTCHER:
        get_options().get_option("AUTO_PULP_BUTCHER").setNext();
        get_options().save();
        //~ Auto Pulp/Pulp Adjacent/Butcher is now set to x
        add_msg(_("%s is now set to %s."),
            get_options().get_option("AUTO_PULP_BUTCHER").getMenuText(),
            get_options().get_option("AUTO_PULP_BUTCHER").getValueName());
        break;

    case ACTION_TOGGLE_AUTO_MINING:
        get_options().get_option("AUTO_MINING").setNext();
        get_options().save();
        //~ Auto Mining is now ON/OFF
        add_msg(_("%s is now %s."),
            get_options().get_option("AUTO_MINING").getMenuText(),
            get_option<bool>("AUTO_MINING") ? _("ON") : _("OFF"));
        break;

    case ACTION_TOGGLE_AUTO_CONSUME:
        get_options().get_option("AUTO_CONSUME").setNext();
        get_options().save();
        add_msg("%s已%s。",
            get_options().get_option("AUTO_CONSUME").getMenuText(),
            get_option<bool>("AUTO_CONSUME") ? "开启" : "关闭");
        break;

    case ACTION_TOGGLE_SHOW_CREATURE_VIEW_LINE:
        get_options().get_option("显示生物视线").setNext();
        get_options().save();
        //~ Auto Mining is now ON/OFF
        add_msg(_("%s is now %s."),
            get_options().get_option("显示生物视线").getMenuText(),
            get_option<bool>("显示生物视线") ? _("ON") : _("OFF"));
        break;

    case ACTION_TOGGLE_THIEF_MODE:
        if (player_character.get_value("THIEF_MODE") == "THIEF_ASK") {
            player_character.set_value("THIEF_MODE", "THIEF_HONEST");
            player_character.set_value("THIEF_MODE_KEEP", "YES");
            //~ Thief mode cycled between THIEF_ASK/THIEF_HONEST/THIEF_STEAL
            add_msg(_("You will not pick up other peoples belongings."));
        }
        else if (player_character.get_value("THIEF_MODE") == "THIEF_HONEST") {
            player_character.set_value("THIEF_MODE", "THIEF_STEAL");
            player_character.set_value("THIEF_MODE_KEEP", "YES");
            //~ Thief mode cycled between THIEF_ASK/THIEF_HONEST/THIEF_STEAL
            add_msg(_("You will pick up also those things that belong to others!"));
        }
        else if (player_character.get_value("THIEF_MODE") == "THIEF_STEAL") {
            player_character.set_value("THIEF_MODE", "THIEF_ASK");
            player_character.set_value("THIEF_MODE_KEEP", "NO");
            //~ Thief mode cycled between THIEF_ASK/THIEF_HONEST/THIEF_STEAL
            add_msg(_("You will be reminded not to steal."));
        }
        else {
            // ERROR
            add_msg(_("THIEF_MODE CONTAINED BAD VALUE [ %s ]!"),
                player_character.get_value("THIEF_MODE"));
        }
        break;

    case ACTION_TOGGLE_AUTO_FORAGING:
        get_options().get_option("AUTO_FORAGING").setNext();
        get_options().save();
        //~ Auto Foraging is now set to x
        add_msg(_("%s is now set to %s."),
            get_options().get_option("AUTO_FORAGING").getMenuText(),
            get_options().get_option("AUTO_FORAGING").getValueName());
        break;

    case ACTION_TOGGLE_AUTO_PICKUP:
        get_options().get_option("AUTO_PICKUP").setNext();
        get_options().save();
        //~ Auto pickup is now set to x
        add_msg(_("%s is now set to %s."),
            get_options().get_option("AUTO_PICKUP").getMenuText(),
            get_options().get_option("AUTO_PICKUP").getValueName());
        break;

    case ACTION_DISPLAY_SCENT:
    case ACTION_DISPLAY_SCENT_TYPE:
        if (MAP_SHARING::isCompetitive() && !MAP_SHARING::isDebugger()) {
            break;    //don't do anything when sharing and not debugger
        }
        display_scent();
        break;

    case ACTION_DISPLAY_TEMPERATURE:
        if (MAP_SHARING::isCompetitive() && !MAP_SHARING::isDebugger()) {
            break;    //don't do anything when sharing and not debugger
        }
        display_temperature();
        break;
    case ACTION_DISPLAY_VEHICLE_AI:
        if (MAP_SHARING::isCompetitive() && !MAP_SHARING::isDebugger()) {
            break;    //don't do anything when sharing and not debugger
        }
        display_vehicle_ai();
        break;
    case ACTION_DISPLAY_VISIBILITY:
        if (MAP_SHARING::isCompetitive() && !MAP_SHARING::isDebugger()) {
            break;    //don't do anything when sharing and not debugger
        }
        display_visibility();
        break;

    case ACTION_DISPLAY_LIGHTING:
        if (MAP_SHARING::isCompetitive() && !MAP_SHARING::isDebugger()) {
            break;    //don't do anything when sharing and not debugger
        }
        display_lighting();
        break;

    case ACTION_DISPLAY_RADIATION:
        if (MAP_SHARING::isCompetitive() && !MAP_SHARING::isDebugger()) {
            break;    //don't do anything when sharing and not debugger
        }
        display_radiation();
        break;

    case ACTION_TOGGLE_HOUR_TIMER:
        toggle_debug_hour_timer();
        break;

    case ACTION_DISPLAY_TRANSPARENCY:
        if (MAP_SHARING::isCompetitive() && !MAP_SHARING::isDebugger()) {
            break;    //don't do anything when sharing and not debugger
        }
        display_transparency();
        break;

    case ACTION_DISPLAY_REACHABILITY_ZONES:
        if (MAP_SHARING::isCompetitive() && !MAP_SHARING::isDebugger()) {
            break;    //don't do anything when sharing and not debugger
        }
        display_reachability_zones();
        break;

    case ACTION_TOGGLE_DEBUG_MODE:
        if (MAP_SHARING::isCompetitive() && !MAP_SHARING::isDebugger()) {
            break;    //don't do anything when sharing and not debugger
        }
        handle_debug_mode();
        break;

    case ACTION_DISPLAY_ISO_WALLS:
        get_options().get_option("RETRACT_ISO_WALLS").setNext();
        get_options().save();
        break;

    case ACTION_ZOOM_IN:
        zoom_in();
        mark_main_ui_adaptor_resize();
        break;

    case ACTION_ZOOM_OUT:
        zoom_out();
        mark_main_ui_adaptor_resize();
        break;

    case ACTION_ITEMACTION:
        item_action_menu();
        break;

    case ACTION_AUTOATTACK:
        avatar_action::autoattack(player_character, m);
        break;



    case ACTION_数据检索:

        handle_action_data_retrieval();


        break;
#if defined(__ANDROID__)
    case ACTION_管理扩展按键:

        manage_extra_buttons();
        
        break;
#endif
    case ACTION_网络功能:

        handle_action_network();


        break;
    


        // 标记 添加备份世界的case
    case actionCreateWorldBackup: {

        if (query_yn("确定要备份当前世界吗?")) {

            std::string world_name =
                world_generator->active_world->world_name;  // 获取角色当前的世界的名称
            const std::string world_path = PATH_INFO::savedir() + "/" + world_name + "_备份";

            std::string playing_world_path = PATH_INFO::savedir() + "/" + world_name;


            if (dir_exist(
                world_path)) { // 首先判断save文件夹里是否已经有一个备份的世界了，如果有则删除


                world_generator->remove_world(world_name + "_备份");

                save();

                std::filesystem::remove_all(world_path);
             
                
                std::filesystem::copy(playing_world_path, world_path, std::filesystem::copy_options::recursive);

                add_msg(m_good, _("备份当前世界成功"));

                world_generator->make_new_world(world_name + "_备份",
                    world_generator->active_world->active_mod_order);

            }
            else {
       
                save();
                
                std::filesystem::copy(playing_world_path, world_path, std::filesystem::copy_options::recursive);

                add_msg(m_good, _("备份当前世界成功"));

                world_generator->make_new_world(world_name + "_备份",
                    world_generator->active_world->active_mod_order);

            }

        }



        break;


    }

    default:
        break;
    }

    return true;
}

bool game::handle_action()
{
    std::string action;
    input_context ctxt;
    action_id act = ACTION_NULL;
    user_turn current_turn;
    avatar& player_character = get_avatar();
    if( !player_character.has_destination() ) {
        player_character.auto_travel_follower_wait_notified = false;
    }
    // Check if we have an auto-move destination
    if (player_character.has_destination()) {
        act = player_character.get_next_auto_move_direction();
        if (act == ACTION_NULL) {
            add_msg(m_info, _("Auto-move canceled"));
            player_character.clear_destination();
            return false;
        }
        handle_key_blocking_activity();
    }
    else if (player_character.has_destination_activity()) {
        // starts destination activity after the player successfully reached his destination
        player_character.start_destination_activity();
        return false;
    }
    else {
        // No auto-move, ask player for input
        ctxt = get_player_input(action);
    }

    bool veh_ctrl = has_vehicle_control(player_character);

    // If performing an action with right mouse button, co-ordinates
    // of location clicked.
    std::optional<tripoint> mouse_target;

    if (uquit == QUIT_WATCH && action == "QUIT") {
        uquit = QUIT_DIED;
        return false;
    }

    if (act == ACTION_NULL) {
        act = look_up_action(action);

        if (act == ACTION_KEYBINDINGS) {
            // already handled by input context
            return false;
        }

        if (act == ACTION_MAIN_MENU) {
            if (uquit == QUIT_WATCH) {
                return false;
            }
            // No auto-move actions have or can be set at this point.
            player_character.clear_destination();
            destination_preview.clear();
            act = handle_main_menu();
            if (act == ACTION_NULL) {
                return false;
            }
        }

        if (act == ACTION_ACTIONMENU) {
            if (uquit == QUIT_WATCH) {
                return false;
            }
            // No auto-move actions have or can be set at this point.
            player_character.clear_destination();
            destination_preview.clear();
            act = handle_action_menu();
            if (act == ACTION_NULL) {
                return false;
            }
#if defined(__ANDROID__)
            if (get_option<bool>("ANDROID_ACTIONMENU_AUTOADD") && ctxt.get_category() == "DEFAULTMODE") {
                add_best_key_for_action_to_quick_shortcuts(act, ctxt.get_category(), false);
            }
#endif
        }

        if (act == ACTION_KEYBINDINGS) {
            player_character.clear_destination();
            destination_preview.clear();
            act = ctxt.display_menu(true);
            if (act == ACTION_NULL) {
                return false;
            }
        }

        if (can_action_change_worldstate(act)) {
            user_action_counter += 1;
        }

        if (act == ACTION_CLICK_AND_DRAG) {
            // Need to return false to avoid disrupting actions like character mouse movement that require two clicks
            return false;
        }

        if (act == ACTION_SELECT || act == ACTION_SEC_SELECT) {
            // Mouse button click
            if (veh_ctrl) {
                // No mouse use in vehicle
                return false;
            }

            if (player_character.is_dead_state()) {
                // do not allow mouse actions while dead
                return false;
            }

            const std::optional<tripoint> mouse_pos = ctxt.get_coordinates(w_terrain, ter_view_p.xy(), true);
            if (!mouse_pos) {
                return false;
            }
            if (!player_character.sees(*mouse_pos)) {
                // Not clicked in visible terrain
                return false;
            }
            mouse_target = mouse_pos;

            if (act == ACTION_SELECT) {
                // Note: The following has the potential side effect of
                // setting auto-move destination state in addition to setting
                // act.
                // TODO: fix point types
                if (!try_get_left_click_action(act, tripoint_bub_ms(*mouse_target))) {
                    return false;
                }
            }
            else if (act == ACTION_SEC_SELECT) {
                // TODO: fix point types
                if (!try_get_right_click_action(act, tripoint_bub_ms(*mouse_target))) {
                    return false;
                }
            }
        }
        else if (act != ACTION_TIMEOUT) {
            // act has not been set for an auto-move, so clearing possible
            // auto-move destinations. Since initializing an auto-move with
            // the mouse may span across multiple actions, we do not clear the
            // auto-move destination if the action is only a timeout, as this
            // would require the user to double click quicker than the
            // timeout delay.
            player_character.clear_destination();
            destination_preview.clear();
        }
            }

    if (act == ACTION_NULL) {
        const input_event&& evt = ctxt.get_raw_input();
        if (!evt.sequence.empty()) {
            const int ch = evt.get_first_input();
            if (!get_option<bool>("NO_UNKNOWN_COMMAND_MSG")) {
                std::string msg = string_format(_("Unknown command: \"%s\" (%ld)"), evt.long_description(), ch);
                if (const std::optional<std::string> hint =
                    press_x_if_bound(ACTION_KEYBINDINGS)) {
                    msg = string_format("%s\n%s", msg,
                        string_format(_("%s at any time to see and edit keybindings relevant to "
                            "the current context."),
                            *hint));
                }
                add_msg(m_info, msg);
            }
        }
        return false;
    }

    // This has no action unless we're in a special game mode.
    gamemode->pre_action(act);

    int before_action_moves = player_character.moves;

    // These actions are allowed while deathcam is active. Registered in game::get_player_input
    if (uquit == QUIT_WATCH || !player_character.is_dead_state()) {
        do_deathcam_action(act, player_character);
    }

    // actions allowed only while alive
    if (!player_character.is_dead_state()) {
        if (!do_regular_action(act, player_character, mouse_target)) {
            return false;
        }
    }
    if (act != ACTION_TIMEOUT) {
        player_character.mod_moves(-current_turn.moves_elapsed());
    }
    gamemode->post_action(act);

    player_character.movecounter = (!player_character.is_dead_state() ? (before_action_moves -
        player_character.moves) : 0);
    dbg(D_INFO) << string_format("%s: [%d] %d - %d = %d", action_ident(act),
        to_turn<int>(calendar::turn), before_action_moves, player_character.movecounter,
        player_character.moves);
    return !player_character.is_dead_state();
        }
