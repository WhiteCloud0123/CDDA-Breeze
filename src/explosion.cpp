#include "explosion.h" // IWYU pragma: associated
#include "fragment_cloud.h" // IWYU pragma: associated

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <iosfwd>
#include <limits>
#include <map>
#include <memory>
#include <queue>
#include <random>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <variant>

#include "bodypart.h"
#include "ballistics.h"
#include "calendar.h"
#include "cata_utility.h"
#include "character.h"
#include "colony.h"
#include "color.h"
#include "creature.h"
#include "creature_tracker.h"
#include "damage.h"
#include "debug.h"
#include "enums.h"
#include "field_type.h"
#include "flag.h"
#include "game.h"
#include "game_constants.h"
#include "item.h"
#include "item_factory.h"
#include "itype.h"
#include "json.h"
#include "line.h"
#include "make_static.h"
#include "map.h"
#include "map_iterator.h"
#include "mapdata.h"
#include "math_defines.h"
#include "messages.h"
#include "mongroup.h"
#include "monster.h"
#include "mtype.h"
#include "npc.h"
#include "optional.h"
#include "options.h"
#include "output.h"
#include "point.h"
#include "projectile.h"
#include "rng.h"
#include "shadowcasting.h"
#include "sounds.h"
#include "string_formatter.h"
#include "translations.h"
#include "trap.h"
#include "type_id.h"
#include "units.h"
#include "vehicle.h"
#include "vpart_position.h"
#include"posix_time.h"
#include"ui_manager.h"










static const efftype_id effect_blind( "blind" );
static const efftype_id effect_deaf( "deaf" );
static const efftype_id effect_emp( "emp" );
static const efftype_id effect_stunned( "stunned" );
static const efftype_id effect_teleglow( "teleglow" );

static const flag_id json_flag_ACTIVATE_ON_PLACE( "ACTIVATE_ON_PLACE" );

static const furn_str_id furn_f_machinery_electronic( "f_machinery_electronic" );

static const itype_id fuel_type_none( "null" );
static const itype_id itype_e_handcuffs( "e_handcuffs" );
static const itype_id itype_mininuke_act( "mininuke_act" );
static const itype_id itype_rm13_armor_on( "rm13_armor_on" );

static const json_character_flag json_flag_GLARE_RESIST( "GLARE_RESIST" );

static const mongroup_id GROUP_NETHER( "GROUP_NETHER" );

static const species_id species_ROBOT( "ROBOT" );

static const trait_id trait_LEG_TENT_BRACE( "LEG_TENT_BRACE" );
static const trait_id trait_PER_SLIME( "PER_SLIME" );
static const trait_id trait_PER_SLIME_OK( "PER_SLIME_OK" );








// Global to smuggle data into shrapnel_calc() function without replicating it across entire map.
// Mass in kg
static float fragment_mass = 0.0001f;
// Cross-sectional area in cm^2
static float fragment_area = 0.00001f;
// Minimum velocity resulting in skin perforation according to https://www.ncbi.nlg->m.nih.gov/pubmed/7304523
static constexpr float MIN_EFFECTIVE_VELOCITY = 70.0f;
// Pretty arbitrary minimum density.  1/1,000 change of a fragment passing through the given square.
static constexpr float MIN_FRAGMENT_DENSITY = 0.0001f;




static float obstacle_blast_percentage(float range, float distance)
{
    return distance > range ? 0.0f : distance > range / 2 ? 0.5f : 1.0f;
}

static float item_blast_percentage(float range, float distance)
{
    const float radius_reduction = 1.0f - distance / range;
    return radius_reduction;
}

static float critter_blast_percentage(Creature* c, float range, float distance)
{
    const float radius_reduction = distance > range ? 0.0f : distance > range / 2 ? 0.5f : 1.0f;

    switch (c->get_size()) {
    case(creature_size::tiny):
        return 0.5 * radius_reduction;
    case(creature_size::small):
        return 0.8 * radius_reduction;
    case(creature_size::medium):
        return 1.0 * radius_reduction;
    case(creature_size::large):
        return 1.5 * radius_reduction;
    case(creature_size::huge):
        return 2.0 * radius_reduction;
    default:
        return 1.0 * radius_reduction;
    }
}



// Programmer-friendly numbers to tweak
namespace ExplosionConstants
{
    // Assumed distance between z-levels.
    // Affects depths of craters and such.
    constexpr int Z_LEVEL_DIST = 4;

    // Shrapnel hitting terrain and vehicle parts should not inflict full damage
    // This constant specifies by how much the damage of shrapnel is multiplied
    //   on terrain/vehicle hits.
    constexpr float SHRAPNEL_OBSTACLE_REDUCTION = 0.25;

    // To destroy terrain consistently, we bash it several times
    //   at linearly dissipating strength.
    // This determines how many times.
    constexpr int MULTIBASH_COUNT = 5;

    // Even though bash the terrain several times, we want to limit
    //   the total amount of damage inflicted
    // For terrain and furniture this doesn't matter, but it does
    //   for vehicles
    // This coeff specifies total amount of damage inflicted after every bash
    constexpr float VEHICLE_DAMAGE_MULT = 2.0;

    // We use this upper bound for the linear interpolation of terfurn strength
    // The intent is to make explosions consistent, but have jagged edges
    constexpr float BASH_RANDOM_FACTOR = 0.3;

    // Flinging entities
    // Whenever an entity is struck by the blast wave
    //   it is given velocity
    // Velocity is the range the object will fly

    // Refer to MOB_FLING_FACTOR & ITEM_FLING_FACTOR
    constexpr int EXPLOSION_CALIBRATION_POWER = 100;

    // This is the amount of grams a mob staying at the
    //   blast center has to weight
    //   in order to fly exactly one blast radius away when struck by
    //   an explosion of strength EXPLOSION_CALIBRATION_STRENGTH
    constexpr int MOB_FLING_FACTOR = 40750;

    // This is the amount of grams an item staying at the
    //   blast center has to weight
    //   in order to fly exactly one blast radius away when struck by
    //   an explosion of strength EXPLOSION_CALIBRATION_STRENGTH
    constexpr int ITEM_FLING_FACTOR = 1500;

    // To make items propagate a bit more interestingly, we add a random amount to effective distance
    //   as rng_float(-ITEM_FLING_RANDOM_FACTOR, +ITEM_FLING_RANDOM_FACTOR);
    constexpr float ITEM_FLING_RANDOM_FACTOR = 2.5;

    // If the entity strikes an obstacle, we multiply its velocity by this factor
    //   and reflect it back
    constexpr float RESTITUTION_COEFF = 0.3;

    // If a flung entity would get more than this speed,
    //   it instead will choose a random value between
    //   FLING_THRESHOLD and FLING_MAX_RANGE
    constexpr float FLING_THRESHOLD = 30.0;

    // See above
    constexpr float FLING_MAX_RANGE = 50.0;
}; // namespace ExplosionConstants



class ExplosionEvent
{
public:
    struct PropelledEntity {
        std::variant<Creature*, safe_reference<item>> target;

        rl_vec2d position;
        float angle;
        float velocity;

        int cur_time;
    };
    struct FieldToAdd {
        field_type_id field;
        int intensity;
        bool hit_player;
    };
    using target_types =
        std::variant<std::monostate, PropelledEntity, FieldToAdd, field_type_id, int>;

    enum class Kind { ITEM_MOVEMENT, MOB_MOVEMENT, BLAST, SHRAPNEL, FIELD_ADDITION, FIELD_REMOVAL } kind;
    target_types target;
    tripoint position;

    static ExplosionEvent mob_movement(const tripoint position, Creature* mob, float angle,
        float velocity, int cur_time) {
        return ExplosionEvent(Kind::MOB_MOVEMENT, position,
            PropelledEntity{
                mob,
                rl_vec2d(position.x + 0.5, position.y + 0.5),
                angle,
                velocity,
                cur_time
            });
    }
    static ExplosionEvent item_movement(const tripoint position, safe_reference<item> item,
        float angle, float velocity, int cur_time) {
        return ExplosionEvent(Kind::ITEM_MOVEMENT, position,
            PropelledEntity{
                item,
                rl_vec2d(position.x + 0.5, position.y + 0.5),
                angle,
                velocity,
                cur_time
            });
    }
    static ExplosionEvent tile_blast(const tripoint position, const int distance) {
        return ExplosionEvent(Kind::BLAST, position, distance);
    }
    static ExplosionEvent tile_shrapnel(const tripoint position) {
        return ExplosionEvent(Kind::SHRAPNEL, position);
    }
    static ExplosionEvent field_addition(
        const tripoint position, field_type_id target,
        const int intensity = INT_MAX, const bool hit_player = false
    ) {
        return ExplosionEvent(Kind::FIELD_ADDITION, position, FieldToAdd{ target, intensity, hit_player });
    }
    static ExplosionEvent field_removal(const tripoint position, field_type_id target) {
        return ExplosionEvent(Kind::FIELD_REMOVAL, position, target);
    }

private:
    ExplosionEvent(Kind kind, const tripoint position) :
        kind(kind), position(position) {};
    ExplosionEvent(Kind kind, const tripoint position, target_types target) :
        kind(kind), target(target), position(position) {};
};



class ExplosionProcess
{
public:
    // Where did the explosion originate from?
    const tripoint center;

    // Explosion damage.
    const int blast_power;

    // Explosion radius, 0 to disable
    const int blast_radius;

    // Is the fire created by the explosion actually left behind?
    const bool is_fiery;

    // Shrapnel data, nullopt to disable
    const std::optional<projectile> shrapnel;

    // Who do we attribute the explosion & shrapnel to? nullopt to disable
    const std::optional<Creature*> emitter;
private:
    using dist_point_pair = std::pair<float, tripoint>;
    using time_event_pair = std::pair<int, ExplosionEvent>;

    std::vector<dist_point_pair> blast_map;
    std::vector<dist_point_pair> shrapnel_map;
    std::priority_queue<time_event_pair, std::vector<time_event_pair>, pair_greater_cmp_first>
        event_queue;

    std::optional<avatar*> player_flung;
    std::map<const Creature*, int> mobs_blasted;
    std::map<const Creature*, int> mobs_shrapneled;
    std::set<const Creature*> flung_set;
    std::vector<tripoint> recombination_targets;

    const int radius_step_delay;
    int cur_time;
    bool request_redraw;
public:
    void run();

    std::map<const Creature*, int> get_blasted() {
        return mobs_blasted;
    };
    std::map<const Creature*, int> get_shrapneled() {
        return mobs_shrapneled;
    };

    ExplosionProcess(
        const tripoint blast_center,
        const int blast_power,
        const int blast_radius,
        const std::optional<projectile> proj = std::nullopt,
        const bool is_fiery = false,
        const std::optional<Creature*> responsible = std::nullopt
    ) : center(blast_center),
        blast_power(blast_power),
        blast_radius(blast_radius),
        is_fiery(is_fiery),
        shrapnel(proj),
        emitter(responsible),
        player_flung(std::nullopt),
        // We want to ensure the delay is at least some small value at least to make sure propagation occurs correctly
        radius_step_delay(std::max(get_option<int>("ANIMATION_DELAY"), 5)),
        cur_time(0),
        request_redraw(false) {}
private:
    static bool dist_comparator(dist_point_pair a, dist_point_pair b) {
        return a.first < b.first;
    };
    static bool time_comparator(time_event_pair a, time_event_pair b) {
        return a.first < b.first;
    };

    void fill_maps();
    void init_event_queue();
    float generate_fling_angle(const tripoint from, const tripoint to);
    bool is_occluded(const tripoint from, const tripoint to);
    void add_event(const int delay, const ExplosionEvent event) {
        cata_assert(delay >= 0);
        event_queue.push({ cur_time + 1 + delay, event });
    }

    bool process_next();
    void blast_tile(const tripoint position, const int rl_distance);
    void project_shrapnel(const tripoint position);
    void add_field(const tripoint position, const field_type_id field,
        const int intensity, const bool hit_player);
    void remove_field(const tripoint position, const field_type_id target);
    void move_entity(const tripoint position, const ExplosionEvent::PropelledEntity& datum,
        bool is_mob);

    // How long should it take for an entity to travel 1 unit of distance at `velocity`?
    int one_tile_at_vel(float velocity) {
        cata_assert(velocity > 0);
        return radius_step_delay / velocity;
    };
};

void ExplosionProcess::fill_maps()
{
    map& here = get_map();

    const int shrapnel_range = shrapnel.has_value() ? shrapnel.value().range : 0;
    const int aoe_radius = std::max(blast_radius, shrapnel_range);
    const int z_levels_affected = aoe_radius / ExplosionConstants::Z_LEVEL_DIST;
    const tripoint_range<tripoint> affected_block(
        center + tripoint(-aoe_radius, -aoe_radius, -z_levels_affected),
        center + tripoint(aoe_radius, aoe_radius, z_levels_affected)
    );

    for (const tripoint& target : affected_block) {
        if (!here.inbounds(target)) {
            continue;
        }

        // Uses this ternany check instead of rl_dist because it converts trig_dist's distance to int implicitly
        const float distance = (
            trigdist ?
            trig_dist(center, target) :
            square_dist(center, target)
            );
        const float z_distance = abs(target.z - center.z);
        const float z_aware_distance = distance + (ExplosionConstants::Z_LEVEL_DIST - 1) * z_distance;
        // We static_cast<int> in order to keep parity with legacy blasts using rl_dist for distance
        //   which, as stated above, converts trig_dist into int implicitly
        if (blast_radius > 0 && static_cast<int>(z_aware_distance) <= blast_radius) {
            blast_map.push_back({ z_aware_distance, target });
        }

        if (shrapnel && static_cast<int>(distance) <= shrapnel_range && target.z == center.z &&
            !is_occluded(center, target)) {
            shrapnel_map.push_back({ distance, target });
        }
    }

    std::stable_sort(blast_map.begin(), blast_map.end(), dist_comparator);
    std::stable_sort(shrapnel_map.begin(), shrapnel_map.end(), dist_comparator);
};
void ExplosionProcess::init_event_queue()
{
    // Start with shrapnel first
    // In how many blast steps should the animation for shrapnel complete?
    const float SHRAPNEL_EQUIV = 3.0;
    const float shrapnel_delay = shrapnel.has_value() ? radius_step_delay * SHRAPNEL_EQUIV : 0.0;

    for (const auto& [distance, position] : shrapnel_map) {
        const float random_factor = rng_float(-0.2, 0.2);
        const float range_percent = distance / shrapnel.value().range;
        const float timing = std::min(std::max(range_percent + random_factor, 0.0f), 1.0f);
        const int time_taken = static_cast<int>(SHRAPNEL_EQUIV * radius_step_delay * timing);

        add_event(time_taken, ExplosionEvent::tile_shrapnel(position));
    }

    // Then apply blasting
    for (const auto& [distance, position] : blast_map) {
        const int time_taken = static_cast<int>(shrapnel_delay + distance * radius_step_delay);
        // We static_cast<int> in order to keep parity with legacy blasts using rl_dist for distance
        //   which, as stated before, converts trig_dist into int implicitly
        add_event(time_taken, ExplosionEvent::tile_blast(position, static_cast<int>(distance)));
    }
};
bool ExplosionProcess::is_occluded(const tripoint from, const tripoint to)
{
    if (from == to) {
        return false;
    }
    
    map& here = get_map();
    tripoint last_position = from;

    std::vector<tripoint> line_of_movement = line_to(from, to);
    // Annoyingly, line_to does not include the origin point
    //   so it has to be added manually
    line_of_movement.insert(line_of_movement.begin(), from);
    for (const auto& position : line_of_movement) {
        // position != to necessary because we do want to strike the
        //   target obstacle
        if (position != to && here.impassable(position)) {
            return true;
        }
        

        // 待定
        // position != to is unneeded here though because we want to
        //   make sure stuff does not fly into vehicles
        /*if (here.obstructed_by_vehicle_rotation(last_position, position)) {
            return true;
        }*/



        last_position = position;
    }
    return false;
};

float ExplosionProcess::generate_fling_angle(const tripoint from, const tripoint to)
{
    if (from != to) {
        // -+ 0.95 added to add a half-arc
        // It should be noted that this mathematically has a bias towards diagonal directions
        //   but this is the shortest way to get good enough results
        return units::atan2(
            to.y - from.y + rng_float(-0.95, 0.95),
            to.x - from.x + rng_float(-0.95, 0.95)
        ).value();
    }
    else {
        return rng_float(-M_PI, M_PI);
    }
}

bool ExplosionProcess::process_next()
{
    if (event_queue.empty()) {
        return false;
    }
    const int time = event_queue.top().first;

    // We don't need to wait in testing mode
    if (!test_mode) {
        const long int anim_delay = (time - cur_time) * 1000000L;
        const timespec delay = timespec{ 0, anim_delay };
        nanosleep(&delay, nullptr);
    }

    // You can change this to adjust the step size
    //   to make the animation have fewer rerenders
    cur_time = time;

    while (!event_queue.empty() && event_queue.top().first <= cur_time) {
        const auto& event = event_queue.top().second;

        switch (event.kind) {
        case ExplosionEvent::Kind::SHRAPNEL:
            project_shrapnel(event.position);
            break;
        case ExplosionEvent::Kind::BLAST:
            blast_tile(event.position, std::get<int>(event.target));
            break;
        case ExplosionEvent::Kind::FIELD_ADDITION: {
            const auto& [field, intensity,
                hit_player] = std::get<ExplosionEvent::FieldToAdd>(event.target);
            add_field(event.position, field, intensity, hit_player);
            break;
        }
        case ExplosionEvent::Kind::FIELD_REMOVAL:
            remove_field(event.position, std::get<field_type_id>(event.target));
            break;
        case ExplosionEvent::Kind::ITEM_MOVEMENT:
        case ExplosionEvent::Kind::MOB_MOVEMENT:
            move_entity(
                event.position,
                std::get<ExplosionEvent::PropelledEntity>(event.target),
                event.kind == ExplosionEvent::Kind::MOB_MOVEMENT
            );
            break;
        };
        event_queue.pop();
    }

    return true;
}


void ExplosionProcess::project_shrapnel(const tripoint position)
{
    map& here = get_map();
    creature_tracker& c_t = get_creature_tracker();
    cata_assert(shrapnel);

    if (is_occluded(center, position)) {
        return;
    }

    projectile fragment = shrapnel.value();
    
    // 待定
    //fragment.add_effect(ammo_effect_NULL_SOURCE);
   



    auto critter = c_t.creature_at(position);
    if (critter && !critter->is_dead_state()) {
        int damage_taken = 0;
        // 变通 const auto bps = critter->get_all_body_parts(true);
        const auto bps = critter->get_all_body_parts();
        // Humans get hit in all body parts
        if (critter->is_avatar()) {
            for (bodypart_id bp : bps) {
                
                // 待定
                /*if (Character::bp_to_hp(bp->token) == num_hp_parts) {
                    continue;
                }*/
        
                // TODO: Apply projectile effects
                // TODO: Penalize low coverage armor
                // Halve damage to be closer to what monsters take
                damage_instance half_impact = fragment.impact;
                half_impact.mult_damage(0.5f);
                dealt_damage_instance dealt = critter->deal_damage(emitter.value_or(nullptr), bp,
                    fragment.impact);
                if (dealt.total_damage() > 0) {
                    damage_taken += dealt.total_damage();
                }
            }
        }
        else {
            dealt_damage_instance dealt = critter->deal_damage(emitter.value_or(nullptr), bps[0],
                fragment.impact);
            if (dealt.total_damage() > 0) {
                damage_taken += dealt.total_damage();
            }
            critter->check_dead_state();
        }
        mobs_shrapneled[critter] = damage_taken;
    }

    if (here.impassable(position)) {
        const int damage = fragment.impact.total_damage() * ExplosionConstants::SHRAPNEL_OBSTACLE_REDUCTION;
        if (optional_vpart_position vp = here.veh_at(position)) {
            vp->vehicle().damage(here,vp->part_index(), damage);
        }
        else {
            // Terrain should be affected by shrapnel less
            here.bash(position, damage, true);
        }
    }

    if (!test_mode) {
        std::vector<tripoint> buf = line_to(position, center);
        buf.resize(2);
        g->draw_line(position, buf);
    }
    request_redraw |= true;
}

void ExplosionProcess::blast_tile(const tripoint position, const int rl_distance)
{
    cata_assert(blast_radius > 0);
    // Verify we have view of the center
    if (is_occluded(center, position)) {
        return;
    }

    map& here = get_map();
    

    // Item damage comes first in order to prevent dropped loot from being destroyed immediately.
    {
        const std::string cause = _("force of the explosion");
        const int smash_force = blast_power * item_blast_percentage(blast_radius, rl_distance);
        here.smash_trap(position, smash_force, cause);
        here.smash_items(position, smash_force, cause);
        // Don't forget to mark them as explosion smashed already
        for (auto& item : here.i_at(position)) {
            item.set_flag(flag_EXPLOSION_SMASHED);
        }
    }

    // Damage creatures. Done first to reduce the amount of flung enemies.
    {
        Creature* critter = get_creature_tracker().creature_at(position);

        if (critter != nullptr && !mobs_blasted.count(critter)) {
            const int blast_damage = blast_power * critter_blast_percentage(critter, blast_radius,
                rl_distance);
            const auto shockwave_dmg = damage_instance::physical(blast_damage, 0, 0, 0.4f);

            avatar* player_ptr = critter->as_avatar();
            if (player_ptr != nullptr) {
                player_ptr->add_msg_if_player(m_bad, _("You're caught in the explosion!"));

                struct blastable_part {
                    bodypart_id bp;
                    float low_mul;
                    float high_mul;
                    float armor_mul;
                };

                static const std::array<blastable_part, 6> blast_parts = { {
                        { bodypart_id("torso"), 0.5f, 1.0f, 0.5f },
                        { bodypart_id("head"),  0.5f, 1.0f, 0.5f },
                        { bodypart_id("leg_l"), 0.75f, 1.25f, 0.4f },
                        { bodypart_id("leg_r"), 0.75f, 1.25f, 0.4f },
                        { bodypart_id("arm_l"), 0.75f, 1.25f, 0.4f },
                        { bodypart_id("arm_r"), 0.75f, 1.25f, 0.4f },
                    }
                };

                for (const auto& blast_part : blast_parts) {
                    const int part_dam = rng(blast_damage * blast_part.low_mul, blast_damage * blast_part.high_mul);
                    const std::string hit_part_name = body_part_name_accusative(blast_part.bp);
                    const auto dmg_instance = damage_instance(damage_type::BASH, part_dam, 0, blast_part.armor_mul);
                    const auto result = player_ptr->deal_damage(emitter.value_or(nullptr), blast_part.bp,
                        dmg_instance);
                    const int res_dmg = result.total_damage();

                    if (res_dmg > 0) {
                        mobs_blasted[critter] = res_dmg;
                    }
                }
            }
            else {
                critter->deal_damage(emitter.value_or(nullptr), bodypart_id("torso"), shockwave_dmg);
                critter->check_dead_state();
                mobs_blasted[critter] = blast_damage;
            }
        }
    }

    // Fling creatures
    {
        Creature* critter = get_creature_tracker().creature_at(position);
        avatar* player_ptr = dynamic_cast<avatar*>(critter);

        if (critter != nullptr && !flung_set.count(critter)) {
            const int push_strength = (blast_radius - rl_distance) * blast_power;
            const float move_power = ExplosionConstants::MOB_FLING_FACTOR * push_strength;

            const int weight = to_gram(critter->get_weight());
            const int inertia = std::max(weight, 1) * ExplosionConstants::EXPLOSION_CALIBRATION_POWER;
            const float real_velocity = move_power / inertia;
            const float velocity = real_velocity > ExplosionConstants::FLING_THRESHOLD ?
                rng_float(ExplosionConstants::FLING_THRESHOLD, ExplosionConstants::FLING_MAX_RANGE) :
                real_velocity;

            if (velocity >= 1.0) {
                if (player_ptr != nullptr) {
                    player_flung = std::make_optional(player_ptr);
                }

                add_event(one_tile_at_vel(velocity),
                    ExplosionEvent::mob_movement(
                        position, critter,
                        generate_fling_angle(center, position), velocity, cur_time
                    )
                );
                flung_set.insert(critter);
            }
        }
    }

    {
        // This reduces the randomness factor in terrain bash significantly
        // Which makes explosions have a more well-defined shape.
        const float offset_distance = std::max(rl_distance - 1.0, 0.0);
        const float terrain_random_factor = rng_float(0.0, ExplosionConstants::BASH_RANDOM_FACTOR);
        const float terrain_factor = std::min(std::max(offset_distance / blast_radius -
            terrain_random_factor, 0.0f), 1.0f);
        float terrain_blast_force = blast_power * obstacle_blast_percentage(blast_radius, rl_distance);

        // Multibash is done by bashing the tile with decaying force.
        // The reason for this existing is because a number of tiles undergo multiple bashed states
        // Things like doors and wall -> floor -> ground.

        const float blast_force_decay = (ExplosionConstants::VEHICLE_DAMAGE_MULT - 1.0) *
            blast_power / ExplosionConstants::MULTIBASH_COUNT;
        cata_assert(blast_force_decay > 0);
        while (terrain_blast_force > 0) {
            bash_params bash{
                static_cast<int>(terrain_blast_force),
                true,
                false,
                here.passable(position + tripoint_below),
                terrain_factor,
                center.z > position.z,
                true
            };
            // Despite what you might expect, this is NOT the same as smash_items

            here.bash_items_new(position, bash);
            here.bash_field_new(position, bash);
            here.bash_vehicle_new(position, bash);
            here.bash_ter_furn_new(position, bash);
            terrain_blast_force -= blast_force_decay;
        }
    }

    {
        // Split items here into stacks
        std::vector<item> stacks;

        for (auto& it : here.i_at(position)) {
            while (true) {
                const int amt = it.count();
                const int split_cnt = rng(1, amt - 1);
                const bool is_final = amt <= 1;

                // If the item is already propelled, ignore it
                if (!is_final && !it.has_flag(flag_EXPLOSION_PROPELLED)) {
                    stacks.push_back(it.split(split_cnt));
                }
                else {
                    stacks.push_back(item(it));
                    break;
                }
            }
        }
        here.i_clear(position);

        for (const auto& it : stacks) {
            here.add_item(position, it);
        }
        recombination_targets.push_back(position);
    }

    // Now give items velocity
    for (auto& it : here.i_at(position)) {
        // If the item is already propelled, ignore it
        if (it.has_flag(flag_EXPLOSION_PROPELLED)) {
            continue;
        };

        const float random_factor = rng_float(-ExplosionConstants::ITEM_FLING_RANDOM_FACTOR,
            ExplosionConstants::ITEM_FLING_RANDOM_FACTOR);
        const float push_strength = std::max(blast_radius - rl_distance + random_factor,
            0.0f) * blast_power;
        const float move_power = ExplosionConstants::ITEM_FLING_FACTOR * push_strength;

        const int weight = to_gram(it.weight());
        const float inertia = std::max(weight, 1) * ExplosionConstants::EXPLOSION_CALIBRATION_POWER;
        const float real_velocity = move_power / inertia;
        const float velocity = real_velocity > ExplosionConstants::FLING_THRESHOLD ?
            rng_float(ExplosionConstants::FLING_THRESHOLD, ExplosionConstants::FLING_MAX_RANGE) :
            real_velocity;

        if (velocity < 1.0) {
            continue;
        }

        it.set_flag(flag_EXPLOSION_PROPELLED);

        add_event(
            one_tile_at_vel(velocity),
            ExplosionEvent::item_movement(
                position, it.get_safe_reference(),
                generate_fling_angle(center, position), velocity, cur_time)
        );
    }

    // Finally, add fields if we can
    if (here.passable(position)) {
        const float radius_percent = static_cast<float>(rl_distance) / blast_radius;
        // Create fresh smoke
        // 50% of the radius is guaranteed to be covered in thin smoke afterwards
        if (here.get_field(position, fd_smoke) == nullptr) {
            const float radius_offset = 2 * radius_percent - 1;
            add_event(0, ExplosionEvent::field_addition(position, fd_smoke));
            if (radius_offset > 0) {
                const int delay = static_cast<int>(blast_radius * radius_step_delay *
                    rng_float(0.0, 1.1 - radius_offset));
                add_event(delay, ExplosionEvent::field_removal(position, fd_smoke));
            }
        }

        // Create fresh fire fields
        if (here.get_field(position, fd_fire) == nullptr) {
            add_event(
                radius_step_delay,
                ExplosionEvent::field_addition(position, fd_fire,
                    1 + (blast_power > 10) + (blast_power > 30),
                    is_fiery
                )
            );
            if (!is_fiery) {
                // Remove at an accelerating pace
                const int delay = static_cast<int>(rng_float(2.0, 4.0) *
                    radius_step_delay * (1.1 - radius_percent));
                add_event(delay + radius_step_delay, ExplosionEvent::field_removal(position, fd_fire));
            }
        }
    }
    request_redraw |= position.z == get_avatar().posz();
};

void ExplosionProcess::add_field(const tripoint position,
    const field_type_id field,
    const int intensity,
    const bool hit_player)
{
    map& here = get_map();
    here.add_field(position, field, intensity, 0_turns, hit_player);
    request_redraw |= position.z == get_avatar().posz();
};

void ExplosionProcess::remove_field(const tripoint position, field_type_id target)
{
    map& here = get_map();
    here.remove_field(position, target);
    request_redraw |= position.z == get_avatar().posz();
};

void ExplosionProcess::move_entity(const tripoint position,
    const ExplosionEvent::PropelledEntity& datum,
    const bool is_mob)
{
    if (datum.velocity < 1) {
        return;
    }

    std::variant<Creature*, safe_reference<item>> cur_target = datum.target;

    if (!is_mob && !std::get<safe_reference<item>>(cur_target)) {
        return;
    }

    map& here = get_map();


    const int time_delta = cur_time - datum.cur_time;
    const float distance_to_travel = std::min(
        datum.velocity * time_delta / radius_step_delay,
        datum.velocity
    );

    tripoint new_position = position;
    float new_velocity = datum.velocity;
    float new_angle = datum.angle;

    // Sometimes items fly more than one tile at once
    //   so we want to make sure we do not hit any obstacles on the way
    // Hence this complication
    {
        const int intermediate_steps = ceil(1.5 * distance_to_travel);

        for (int step = 0; step <= intermediate_steps; step++) {
            const float progress = static_cast<float>(step) / static_cast<float>(intermediate_steps);
            const float cur_distance_travelled = distance_to_travel * progress;
            rl_vec2d new_position_vec = datum.position +
                rl_vec2d(cur_distance_travelled, 0.0).rotated(datum.angle);
            tripoint maybe_new_position = tripoint(static_cast<int>(new_position_vec.x),
                static_cast<int>(new_position_vec.y),
                position.z);
            if (!here.inbounds(maybe_new_position) ||
                here.impassable(maybe_new_position) ||
                (is_mob && maybe_new_position != position && get_creature_tracker().creature_at(maybe_new_position)) /* 待定 ||
                here.obstructed_by_vehicle_rotation(position, maybe_new_position)*/) {
                // TODO: Bash the obstacle with whatever is flung?

                new_angle += M_PI;
                new_velocity = datum.velocity - cur_distance_travelled;
                new_velocity *= ExplosionConstants::RESTITUTION_COEFF;
                break;
            }
            new_position = maybe_new_position;
            new_velocity = datum.velocity - cur_distance_travelled;
        }
    }

    bool do_next = new_velocity >= 1;

    if (new_position != position) {
        if (is_mob) {
            std::get<Creature*>(cur_target)->setpos(new_position);
        }
        else {
            item* target = std::get<safe_reference<item>>(cur_target).get();
            item copy = *target;

            if (!copy.is_null()) {
                here.i_rem(position, target);

                item* new_item = &here.add_item(new_position, copy);
                recombination_targets.push_back(position);
                recombination_targets.push_back(new_position);

                // add_item may in fact change the item in a number of ways
                //   so we _have_ to check if it's in the location we expect
                // It's slow, but what can you do?
                new_item->set_flag(flag_IS_EXPLOSION_PROPELLED);
                bool is_safe = false;
                for (auto& it : here.i_at(new_position)) {
                    if (it.has_flag(flag_IS_EXPLOSION_PROPELLED)) {
                        is_safe = true;
                        break;
                    }
                }
                new_item->unset_flag(flag_IS_EXPLOSION_PROPELLED);
                if (!is_safe) {
                    do_next = false;
                }
                else {
                    cur_target = new_item->get_safe_reference();
                }
            }
        }
        request_redraw |= position.z == get_avatar().posz();
        request_redraw |= new_position.z == get_avatar().posz();
    }

    if (do_next) {
        add_event(
            one_tile_at_vel(new_velocity),
            is_mob ?
            ExplosionEvent::mob_movement(new_position,
                std::get<Creature*>(cur_target), new_angle, new_velocity, cur_time) :
            ExplosionEvent::item_movement(new_position,
                std::get<safe_reference<item>>(cur_target), new_angle, new_velocity, cur_time)
        );
    }
};


void ExplosionProcess::run()
{
    fill_maps();
    init_event_queue();

    // We need to temporary disable it because
    //   larger explosions may end up filling
    //   the texture pool, causing a crash
    bool disable_minimap = !test_mode && pixel_minimap_option;
    if (disable_minimap) {
        g->toggle_pixel_minimap();
    }

    map& here = get_map();
    while (process_next()) {
        // No need to redraw in testing mode
        if (!test_mode && request_redraw) {
            ui_manager::redraw();
            refresh_display();
            request_redraw = false;
        }
    };

    // Reenable disabled options
    if (disable_minimap) {
        g->toggle_pixel_minimap();
    }

    // Remove temporary flags
    for (int z = -OVERMAP_DEPTH; z <= OVERMAP_HEIGHT; z++) {
        for (const auto& pos : here.points_on_zlevel(z)) {
            for (auto& it : here.i_at(pos)) {
                it.unset_flag(flag_EXPLOSION_SMASHED);
                it.unset_flag(flag_EXPLOSION_PROPELLED);
            }
        }
    }

    // Make sure the map is centered around the player
    // 待定
    /*if (player_flung.has_value()) {
        g->update_map(*player_flung.value());
    }*/

    // Finally, recombine thrown items into full stacks again
    std::sort(recombination_targets.begin(), recombination_targets.end());
    auto end = std::unique(recombination_targets.begin(), recombination_targets.end());
    recombination_targets.erase(end, recombination_targets.end());

    for (const auto& position : recombination_targets) {
        std::vector<item> storage;
        for (item& it : here.i_at(position)) {
            storage.push_back(it);
        }
        here.i_clear(position);
        for (item& it : storage) {
            here.add_item_or_charges(position, it);
        }
    }
}










explosion_data load_explosion_data( const JsonObject &jo )
{
    explosion_data ret;
    // Power is mandatory
    jo.read( "power", ret.power );
    // Rest isn't
    ret.distance_factor = jo.get_float( "distance_factor", 0.75f );
    ret.max_noise = jo.get_int( "max_noise", 90000000 );
    ret.fire = jo.get_bool( "fire", false );
    if( jo.has_int( "shrapnel" ) ) {
        ret.shrapnel.casing_mass = jo.get_int( "shrapnel" );
        ret.shrapnel.recovery = 0;
        ret.shrapnel.drop = fuel_type_none;
    } else if( jo.has_object( "shrapnel" ) ) {
        JsonObject shr = jo.get_object( "shrapnel" );
        ret.shrapnel = load_shrapnel_data( shr );
    }

    return ret;
}

shrapnel_data load_shrapnel_data( const JsonObject &jo )
{
    shrapnel_data ret;
    // Casing mass is mandatory
    jo.read( "casing_mass", ret.casing_mass );
    // Rest isn't
    ret.fragment_mass = jo.get_float( "fragment_mass", 0.08 );
    ret.recovery = jo.get_int( "recovery", 0 );
    ret.drop = itype_id( jo.get_string( "drop", "null" ) );
    return ret;
}





namespace explosion_handler
{

    
    

    




static int ballistic_damage( float velocity, float mass )
{
    // Damage is square root of Joules, dividing by 2000 because it's dividing by 2 and
    // converting mass from grams to kg. The initial term is simply a scaling factor.
    return 4.0 * std::sqrt( ( velocity * velocity * mass ) / 2000.0 );
}
// Calculate cross-sectional area of a steel sphere in cm^2 based on mass of fragment.
static float mass_to_area( const float mass )
{
    // Density of steel in g/cm^3
    constexpr float steel_density = 7.85f;
    float fragment_volume = ( mass / 1000.0 ) / steel_density;
    float fragment_radius = std::cbrt( ( fragment_volume * 3.0f ) / ( 4.0f * M_PI ) );
    return fragment_radius * fragment_radius * M_PI;
}

// Approximate Gurney constant for Composition B and C (in m/s instead of the usual km/s).
// Source: https://en.wikipedia.org/wiki/Gurney_equations#Gurney_constant_and_detonation_velocity
static constexpr double TYPICAL_GURNEY_CONSTANT = 2700.0;
static float gurney_spherical( const double charge, const double mass )
{
    return static_cast<float>( std::pow( ( mass / charge ) + ( 3.0 / 5.0 ),
                                         -0.5 ) * TYPICAL_GURNEY_CONSTANT );
}

// (C1001) Compiler Internal Error on Visual Studio 2015 with Update 2
static void do_blast( const Creature *source, const tripoint &p, const float power,
                      const float distance_factor, const bool fire )
{
    const float tile_dist = 1.0f;
    const float diag_dist = trigdist ? M_SQRT2 * tile_dist : 1.0f * tile_dist;
    const float zlev_dist = 2.0f; // Penalty for going up/down
    // 7 3 5
    // 1 . 2
    // 6 4 8
    // 9 and 10 are up and down
    static constexpr std::array<int, 10> x_offset = { -1, 1,  0, 0,  1, -1, -1, 1, 0, 0 };
    static constexpr std::array<int, 10> y_offset = { 0, 0, -1, 1, -1,  1, -1, 1, 0, 0 };
    static constexpr std::array<int, 10> z_offset = { 0, 0,  0, 0,  0,  0,  0, 0, 1, -1 };
    map &here = get_map();
    const size_t max_index = 10;

    here.bash( p, fire ? power : ( 2 * power ), true, false, false );

    std::priority_queue< std::pair<float, tripoint>, std::vector< std::pair<float, tripoint> >, pair_greater_cmp_first >
    open;
    std::set<tripoint> closed;
    std::set<tripoint> bashed{ p };
    std::map<tripoint, float> dist_map;
    open.push( std::make_pair( 0.0f, p ) );
    dist_map[p] = 0.0f;
    // Find all points to blast
    while( !open.empty() ) {
        // Add some random factor to effective distance to make it look cooler
        const float distance = open.top().first * rng_float( 1.0f, 1.2f );
        const tripoint pt = open.top().second;
        open.pop();

        if( closed.count( pt ) != 0 ) {
            continue;
        }

        closed.insert( pt );

        const float force = power * obstacle_blast_percentage(distance_factor, distance);;
        if( force <= 1.0f ) {
            continue;
        }

        if( here.impassable( pt ) && pt != p ) {
            // Don't propagate further
            continue;
        }

        // Those will be used for making "shaped charges"
        // Don't check up/down (for now) - this will make 2D/3D balancing easier
        int empty_neighbors = 0;
        for( size_t i = 0; i < 8; i++ ) {
            tripoint dest( pt + tripoint( x_offset[i], y_offset[i], z_offset[i] ) );
            if( closed.count( dest ) == 0 && here.valid_move( pt, dest, false, true ) ) {
                empty_neighbors++;
            }
        }

        empty_neighbors = std::max( 1, empty_neighbors );
        // Iterate over all neighbors. Bash all of them, propagate to some
        for( size_t i = 0; i < max_index; i++ ) {
            tripoint dest( pt + tripoint( x_offset[i], y_offset[i], z_offset[i] ) );
            if( closed.count( dest ) != 0 || !here.inbounds( dest ) ) {
                continue;
            }

            if( bashed.count( dest ) == 0 ) {
                bashed.insert( dest );
                // Up to 200% bonus for shaped charge
                // But not if the explosion is fiery, then only half the force and no bonus
                const float bash_force = !fire ?
                                         force + ( 2 * force / empty_neighbors ) :
                                         force / 2;
                if( z_offset[i] == 0 ) {
                    // Horizontal - no floor bashing
                    here.bash( dest, bash_force, true, false, false );
                } else if( z_offset[i] > 0 ) {
                    // Should actually bash through the floor first, but that's not really possible yet
                    here.bash( dest, bash_force, true, false, true );
                } else if( !here.valid_move( pt, dest, false, true ) ) {
                    // Only bash through floor if it doesn't exist
                    // Bash the current tile's floor, not the one's below
                    here.bash( pt, bash_force, true, false, true );
                }
            }

            float next_dist = distance;
            next_dist += ( x_offset[i] == 0 || y_offset[i] == 0 ) ? tile_dist : diag_dist;
            if( z_offset[i] != 0 ) {
                if( !here.valid_move( pt, dest, false, true ) ) {
                    continue;
                }

                next_dist += zlev_dist;
            }

            if( dist_map.count( dest ) == 0 || dist_map[dest] > next_dist ) {
                open.push( std::make_pair( next_dist, dest ) );
                dist_map[dest] = next_dist;
            }
        }
    }

    // Draw the explosion
    std::map<tripoint, nc_color> explosion_colors;
    for( const tripoint &pt : closed ) {
        if( here.impassable( pt ) ) {
            continue;
        }

        const float force = power * obstacle_blast_percentage(distance_factor, dist_map.at(pt));
        nc_color col = c_red;
        if( force < 10 ) {
            col = c_white;
        } else if( force < 30 ) {
            col = c_yellow;
        }

        explosion_colors[pt] = col;
    }

    draw_custom_explosion( get_player_character().pos(), explosion_colors );

    creature_tracker &creatures = get_creature_tracker();
    Creature *mutable_source = source == nullptr ? nullptr : creatures.creature_at( source->pos() );
    for( const tripoint &pt : closed ) {
        const float force = power * obstacle_blast_percentage(distance_factor, dist_map.at(pt));
        if( force < 1.0f ) {
            // Too weak to matter
            continue;
        }

        if( here.has_items( pt ) ) {
            here.smash_items( pt, force, _( "force of the explosion" ) );
        }

        if( fire ) {
            int intensity = ( force > 50.0f ) + ( force > 100.0f );
            if( force > 10.0f || x_in_y( force, 10.0f ) ) {
                intensity++;
            }
            here.add_field( pt, fd_fire, intensity );
        }

        if( const optional_vpart_position vp = here.veh_at( pt ) ) {
            // TODO: Make this weird unit used by vehicle::damage more sensible
            vp->vehicle().damage( here, vp->part_index(), force,
                                  fire ? damage_type::HEAT : damage_type::BASH, false );
        }

        Creature *critter = creatures.creature_at( pt, true );
        if( critter == nullptr ) {
            continue;
        }

        add_msg_debug( debugmode::DF_EXPLOSION, "Blast hits %s with force %.1f", critter->disp_name(),
                       force );

        Character *pl = critter->as_character();
        if( pl == nullptr ) {
            const double dmg = std::max( force - critter->get_armor_bash( bodypart_id( "torso" ) ) / 2.0, 0.0 );
            const int actual_dmg = rng_float( dmg * 2, dmg * 3 );
            critter->apply_damage( mutable_source, bodypart_id( "torso" ), actual_dmg );
            critter->check_dead_state();
            add_msg_debug( debugmode::DF_EXPLOSION, "Blast hits %s for %d damage", critter->disp_name(),
                           actual_dmg );
            continue;
        }

        // Print messages for all NPCs
        pl->add_msg_player_or_npc( m_bad, _( "You're caught in the explosion!" ),
                                   _( "<npcname> is caught in the explosion!" ) );

        struct blastable_part {
            bodypart_id bp;
            float low_mul = 0.0f;
            float high_mul = 0.0f;
            float armor_mul = 0.0f;
        };

        static const std::array<blastable_part, 6> blast_parts = { {
                { bodypart_id( "torso" ), 2.0f, 3.0f, 0.5f },
                { bodypart_id( "head" ),  2.0f, 3.0f, 0.5f },
                // Hit limbs harder so that it hurts more without being much more deadly
                { bodypart_id( "leg_l" ), 2.0f, 3.5f, 0.4f },
                { bodypart_id( "leg_r" ), 2.0f, 3.5f, 0.4f },
                { bodypart_id( "arm_l" ), 2.0f, 3.5f, 0.4f },
                { bodypart_id( "arm_r" ), 2.0f, 3.5f, 0.4f },
            }
        };

        for( const blastable_part &blp : blast_parts ) {
            const int part_dam = rng( force * blp.low_mul, force * blp.high_mul );
            const std::string hit_part_name = body_part_name_accusative( blp.bp );
            const damage_instance dmg_instance = damage_instance( damage_type::BASH, part_dam, 0,
                                                 blp.armor_mul );
            const dealt_damage_instance result = pl->deal_damage( mutable_source, blp.bp, dmg_instance );
            const int res_dmg = result.total_damage();

            add_msg_debug( debugmode::DF_EXPLOSION, "%s for %d raw, %d actual", hit_part_name, part_dam,
                           res_dmg );
            if( res_dmg > 0 ) {
                pl->add_msg_if_player( m_bad, _( "Your %s is hit for %d damage!" ), hit_part_name, res_dmg );
            }
        }
    }
}

static std::map<const Creature*, int> do_blast_new(const tripoint& blast_center,
    const float raw_blast_force,
    const float raw_blast_radius)
{
    /*
    Explosions are completed in 3 stages.
    1. Shrapnel
    The very first component: shrapnel is thrown all around.
    Impassable terrain around has not yet been broken down by the blast, so it will shield mobs.
    2. Blast wave
    This propagates the explosion outwards and:
        1. Damage all items in the tile. Done first to prevent loot from being destroyed too much.
        2. Bashes critters (unless they had already been bashed before) in the tile.
        The main bulk of damage is inflicted here.
        3. Flings still alive critters.
        This causes some extra damage depending on how the explosion is set up and may fling the same mob several times.
        This is effective inside buildings since this causes the mobs to be thrown against walls.
        4. Bashes terrain (and vehicles).
        All vehicle parts in the tile are bashed 2 times at full force,
        both to compensate for their tankiness and to make sure they get actually destroyed.
        Terrain is destroyed in a consistent manner.
    */
    using dist_point_pair = std::pair<float, tripoint>;

    // TODO: Move the consts outside the function and make static when this information becomes needed for UI

    // Distance between z-levels
    const int Z_LEVEL_DIST = 4;

    // Since terrain is bashed multiple times, the blast power needs to dissipate with each blast.
    // This factor determines by how much it is dissipated (thus determining multibash amt).
    const float TERRAIN_DISSIPATION_FACTOR = 0.15;

    // Terrain bashing uses relative distance from the epicenter to determine the force needed to break down a piece of furniture
    // By default this makes explosives predictable. To counteract it, a small random value is added
    // to add slightly more jaggedness. This factor determines the maximum value added.
    const float TERRAIN_RANDOM_FACTOR = 0.1;

    // Flinging creatures uses a different scale to determine its damage and range.
    // More specifically, FLING_POWER_FACTOR * FORCE * RADIUS determines the weight
    // in grams that a fling will move by one tile. Half of the weight will move 2 tiles and so on...

    // Calibrated to this value as regular zombies weigh 40750 grams and we want to throw them a bit more than one radius away per 100 blast damage.
    // With 100 blast damage coming from baseline dynamite explosion.
    const float FLING_POWER_FACTOR = 420.0;

    // Flinging light creatures causes them to fly the entire reality and inflicts insane damage
    // This constant limits the maximum fling distance (and indirectly damage) to FLING_HARD_CAP * BLAST RADIUS.
    const float FLING_HARD_CAP = 2.0;

    const int z_levels_affected = raw_blast_radius / Z_LEVEL_DIST;
    const tripoint_range<tripoint> affected_block(
        blast_center + tripoint(-raw_blast_radius, -raw_blast_radius, -z_levels_affected),
        blast_center + tripoint(raw_blast_radius, raw_blast_radius, z_levels_affected)
    );

    static std::vector<dist_point_pair> blast_map(MAPSIZE_X * MAPSIZE_Y);
    static std::map<tripoint, bool> blast_shield_map;
    static std::map<tripoint, nc_color> explosion_colors;
    blast_map.clear();
    explosion_colors.clear();
    blast_shield_map.clear();

    bool fling_creature_affected_player = false;

    for (const tripoint& target : affected_block) {
        
        

        if (!get_map().inbounds(target)) {
            continue;
        }

        // Uses this ternany check instead of rl_dist because it converts trig_dist's distance to int implicitly
        const float distance = (
            trigdist ?
            trig_dist(blast_center, target) :
            square_dist(blast_center, target)
            );
        const float z_distance = abs(target.z - blast_center.z);
        const float z_aware_distance = distance + (Z_LEVEL_DIST - 1) * z_distance;
        if (z_aware_distance <= raw_blast_radius) {
            blast_map.emplace_back(std::make_pair(z_aware_distance, target));
        }
    }

   


    std::stable_sort(blast_map.begin(), blast_map.end(), [](dist_point_pair pair1,
        dist_point_pair pair2) {
            return pair1.first <= pair2.first;
        });
    int animated_explosion_range = 0.0f;
    std::map<const Creature*, int> blasted;
    std::set<const Creature*> already_flung;
    

    creature_tracker& creatures = get_creature_tracker();
    
    for (const dist_point_pair& pair : blast_map) {
        
        
        
        float distance;
        tripoint position;
        std::tie(distance, position) = pair;

        const std::vector<tripoint> line_of_movement = line_to(blast_center, position);
        const bool has_obstacles = std::any_of(line_of_movement.begin(),
            line_of_movement.end(), [position](tripoint ray_position) {
                return ray_position != position && get_map().impassable(ray_position);
            });

        // Don't bother animating explosions that are on other levels
        const bool to_animate = get_player_character().posz() == position.z;

        // Animate the explosion by drawing the shock wave rather than the whole explosion
        if (to_animate && distance > animated_explosion_range) {
            //draw_custom_explosion(blast_center, explosion_colors, "explosion");
            draw_custom_explosion(blast_center, explosion_colors);
            explosion_colors.clear();
            animated_explosion_range++;
        }

        if (has_obstacles) {
            continue;
        }

        if (to_animate) {
            explosion_colors[position] = c_white;
        }

        // Item damage comes first in order to prevent dropped loot from being destroyed immediately.
        const int smash_force = raw_blast_force * item_blast_percentage(raw_blast_radius, distance);
        get_map().smash_items(position, smash_force, _("force of the explosion"));

        // Critter damage occurs next to reduce the amount of flung enemies, leading to much less predictable damage output
        if (Creature* critter = creatures.creature_at(position, true)) {

           

            if (blasted.count(critter)) {
                // Prevent multibashes to monsters due to flinging.
                continue;
            }

            const int blast_force = raw_blast_force * critter_blast_percentage(critter, raw_blast_radius,
                distance);
            const auto shockwave_dmg = damage_instance::physical(blast_force, 0, 0, 0.4f);

            if (critter->is_avatar()) {

                add_msg(m_bad, _("You're caught in the explosion!"));

                struct blastable_part {
                    bodypart_id bp;
                    float low_mul;
                    float high_mul;
                    float armor_mul;
                };

                static const std::array<blastable_part, 6> blast_parts = { {
                        { bodypart_id("torso"), 0.5f, 1.0f, 0.5f },
                        { bodypart_id("head"),  0.5f, 1.0f, 0.5f },
                        // Hit limbs harder so that it hurts more without being much more deadly
                        { bodypart_id("leg_l"), 0.75f, 1.25f, 0.4f },
                        { bodypart_id("leg_r"), 0.75f, 1.25f, 0.4f },
                        { bodypart_id("arm_l"), 0.75f, 1.25f, 0.4f },
                        { bodypart_id("arm_r"), 0.75f, 1.25f, 0.4f },
                    }
                };

                for (const auto& blast_part : blast_parts) {
                    const int part_dam = rng(blast_force * blast_part.low_mul, blast_force * blast_part.high_mul);
                    const std::string hit_part_name = body_part_name_accusative(blast_part.bp);
                    const auto dmg_instance = damage_instance(damage_type::BASH, part_dam, 0, blast_part.armor_mul);
                    const auto result = get_player_character().deal_damage(nullptr, blast_part.bp, dmg_instance);
                    const int res_dmg = result.total_damage();

                    if (res_dmg > 0) {
                        blasted[critter] += res_dmg;
                    }
                }
            }
            else {
                critter->deal_damage(nullptr, bodypart_id("torso"), shockwave_dmg);
                critter->check_dead_state();
                blasted[critter] = blast_force;
            }
        }

        // rng_float is needed to make sure critters at the center get thrown in a random direction.
        units::angle angle = units::atan2(position.y - blast_center.y + rng_float(-0.5f, 0.5f),
            position.x - blast_center.x + rng_float(-0.5f, 0.5f));

        // How many grams can the blast fling 1 tile away?
        // Multiplied by ten because this is coupled with fling_creature
        const float move_power = 10 * FLING_POWER_FACTOR * (raw_blast_radius - distance) *
            raw_blast_force;

        if (Creature* critter = creatures.creature_at(position, true)) {
            if (!already_flung.count(critter)) {
                

                

                const int weight = to_gram(critter->get_weight());
                const int fling_vel = std::min(move_power / std::max(weight, 1),
                    10 * FLING_HARD_CAP * raw_blast_radius);

                if ( !critter->is_avatar() ) {
                    g->fling_creature(critter, angle, fling_vel);
                }
                else {
                    fling_creature_affected_player = true;
                    g->fling_creature(critter, angle, fling_vel, false);
                }
                // Prevent multiflings
                already_flung.insert(critter);
            }
        }

        // Terrain bashes occur last to ensure enemies being bashed against walls if flung.

        // This reduces the randomness factor in terrain bash significantly
        // Which makes explosions have a more well-defined shape.
        const float terrain_factor = std::max(distance - 1.0, 0.0) / raw_blast_radius + rng_float(0.0,
            TERRAIN_RANDOM_FACTOR);
        const int terrain_blast_force = raw_blast_force * obstacle_blast_percentage(raw_blast_radius,
            distance);

        bash_params shockwave_bash{
            terrain_blast_force,
            false, // Bashing down terrain should not be silent
            false,
            !get_map().impassable(position + tripoint_below), // We will only try to break the floor down if there is nothing underneath
            terrain_factor,
            blast_center.z > position.z
        };

        if (const optional_vpart_position& vp = get_map().veh_at(position)) {
            // HP values of vehicle parts aren't really on the same scale

            // Would be better if explosives had a separate damage value for vehicle parts
            // But for now, this will suffice.

            vehicle& veh_ptr = vp->vehicle();

            const std::vector<int>& affected_part_nums = veh_ptr.parts_at_relative(vp->mount(), false);
            
            for (const auto part_indx : affected_part_nums) {
                // Double bash to bypass XX state if possible.
                veh_ptr.damage(get_map(), part_indx, shockwave_bash.strength, damage_type::BASH, true);
                veh_ptr.damage(get_map(), part_indx,shockwave_bash.strength, damage_type::BASH,true);
            }
        }
        else {
            // Multibash is done by bashing the tile with decaying force.
            // The reason for this existing is because a number of tiles undergo multiple bashed states
            // Things like doors and wall -> floor -> ground.
            while (shockwave_bash.strength > 0) {
                get_map().bash_ter_furn_new(position, shockwave_bash);
                shockwave_bash.strength = std::max(static_cast<int>(shockwave_bash.strength -
                    TERRAIN_DISSIPATION_FACTOR * raw_blast_force), 0);
            }
        }
    }

    
    // Final blast wave points
    draw_custom_explosion(blast_center, explosion_colors);

    /*if ( fling_creature_affected_player == true ) {
        g->update_map(get_player_character());

    }*/
    return blasted;
}



static std::vector<tripoint> shrapnel( const Creature *source, const tripoint &src, int power,
                                       int casing_mass, float per_fragment_mass, int range = -1 )
{
    // The gurney equation wants the total mass of the casing.
    const float fragment_velocity = gurney_spherical( power, casing_mass );
    fragment_mass = per_fragment_mass;
    fragment_area = mass_to_area( fragment_mass );
    int fragment_count = casing_mass / fragment_mass;

    // Contains all tiles reached by fragments.
    std::vector<tripoint> distrib;

    projectile proj;
    proj.speed = fragment_velocity;
    proj.range = range;
    proj.proj_effects.insert( "NULL_SOURCE" );

    struct local_caches {
        cata::mdarray<fragment_cloud, point_bub_ms> obstacle_cache;
        cata::mdarray<fragment_cloud, point_bub_ms> visited_cache;
    };

    std::unique_ptr<local_caches> caches = std::make_unique<local_caches>();
    cata::mdarray<fragment_cloud, point_bub_ms> &obstacle_cache = caches->obstacle_cache;
    cata::mdarray<fragment_cloud, point_bub_ms> &visited_cache = caches->visited_cache;

    map &here = get_map();
    // TODO: Calculate range based on max effective range for projectiles.
    // Basically bisect between 0 and map diameter using shrapnel_calc().
    // Need to update shadowcasting to support limiting range without adjusting initial distance.
    const tripoint_range<tripoint> area = here.points_on_zlevel( src.z );

    here.build_obstacle_cache( area.min(), area.max() + tripoint_south_east, obstacle_cache );

    // Shadowcasting normally ignores the origin square,
    // so apply it manually to catch monsters standing on the explosive.
    // This "blocks" some fragments, but does not apply deceleration.
    fragment_cloud initial_cloud = accumulate_fragment_cloud( obstacle_cache[src.x][src.y],
    { fragment_velocity, static_cast<float>( fragment_count ) }, 1 );
    visited_cache[src.x][src.y] = initial_cloud;

    castLightAll<fragment_cloud, fragment_cloud, shrapnel_calc, shrapnel_check,
                 update_fragment_cloud, accumulate_fragment_cloud>
                 ( visited_cache, obstacle_cache, src.xy(), 0, initial_cloud );

    creature_tracker &creatures = get_creature_tracker();
    Creature *mutable_source = source == nullptr ? nullptr : creatures.creature_at( source->pos() );
    // Now visited_caches are populated with density and velocity of fragments.
    for( const tripoint &target : area ) {
        fragment_cloud &cloud = visited_cache[target.x][target.y];
        if( cloud.density <= MIN_FRAGMENT_DENSITY ||
            cloud.velocity <= MIN_EFFECTIVE_VELOCITY ) {
            continue;
        }
        distrib.emplace_back( target );
        int damage = ballistic_damage( cloud.velocity, fragment_mass );
        Creature *critter = creatures.creature_at( target );
        if( damage > 0 && critter && !critter->is_dead_state() ) {
            std::poisson_distribution<> d( cloud.density );
            int hits = d( rng_get_engine() );
            dealt_projectile_attack frag;
            frag.proj = proj;
            frag.proj.speed = cloud.velocity;
            frag.proj.impact = damage_instance( damage_type::BULLET, damage );
            // dealt_dam.total_damage() == 0 means armor block
            // dealt_dam.total_damage() > 0 means took damage
            // Need to differentiate target among player, npc, and monster
            // Do we even print monster damage?
            int damage_taken = 0;
            int damaging_hits = 0;
            int non_damaging_hits = 0;
            for( int i = 0; i < hits; ++i ) {
                frag.missed_by = rng_float( 0.05, 1.0 / critter->ranged_target_size() );
                critter->deal_projectile_attack( mutable_source, frag, false );
                if( frag.dealt_dam.total_damage() > 0 ) {
                    damaging_hits++;
                    damage_taken += frag.dealt_dam.total_damage();
                } else {
                    non_damaging_hits++;
                }
                add_msg_debug( debugmode::DF_EXPLOSION, "Shrapnel hit %s at %d m/s at a distance of %d",
                               critter->disp_name(),
                               frag.proj.speed, rl_dist( src, target ) );
                add_msg_debug( debugmode::DF_EXPLOSION, "Shrapnel dealt %d damage", frag.dealt_dam.total_damage() );
                if( critter->is_dead_state() ) {
                    break;
                }
            }
            int total_hits = damaging_hits + non_damaging_hits;
            multi_projectile_hit_message( critter, total_hits, damage_taken, n_gettext( "bomb fragment",
                                          "bomb fragments", total_hits ) );
        }
        if( here.impassable( target ) ) {
            if( optional_vpart_position vp = here.veh_at( target ) ) {
                vp->vehicle().damage( here, vp->part_index(), damage / 10 );
            } else {
                here.bash( target, damage / 100, true );
            }
        }
    }

    return distrib;
}

void explosion( const Creature *source, const tripoint &p, float power, float factor, bool fire,
                int casing_mass, float frag_mass )
{
    explosion_data data;
    data.power = power;
    data.distance_factor = factor;
    data.fire = fire;
    data.shrapnel.casing_mass = casing_mass;
    data.shrapnel.fragment_mass = frag_mass;
    explosion( source, p, data );
}

void explosion( const Creature *source, const tripoint &p, const explosion_data &ex )
{
    _explosions.emplace_back( source, get_map().getglobal( p ), ex );
}

void _make_explosion( const Creature *source, const tripoint &p, const explosion_data &ex )
{
    int noise = ex.power * ( ex.fire ? 2 : 10 );
    noise = ( noise > ex.max_noise ) ? ex.max_noise : noise;
    if( noise >= 30 ) {
        sounds::sound( p, noise, sounds::sound_t::combat, _( "a huge explosion!" ), false, "explosion",
                       "huge" );
    } else if( noise >= 4 ) {
        sounds::sound( p, noise, sounds::sound_t::combat, _( "an explosion!" ), false, "explosion",
                       "default" );
    } else if( noise > 0 ) {
        sounds::sound( p, 3, sounds::sound_t::combat, _( "a loud pop!" ), false, "explosion", "small" );
    }

    if( ex.distance_factor >= 1.0f ) {
        debugmsg( "called game::explosion with factor >= 1.0 (infinite size)" );
    } 

    map &here = get_map();
    const shrapnel_data &shr = ex.shrapnel;
    if( shr.casing_mass > 0 ) {
        auto shrapnel_locations = shrapnel( source, p, ex.power, shr.casing_mass, shr.fragment_mass );

        // If explosion drops shrapnel...
        if( shr.recovery > 0 && !shr.drop.is_null() ) {

            // Extract only passable tiles affected by shrapnel
            std::vector<tripoint> tiles;
            for( const tripoint &e : shrapnel_locations ) {
                if( here.passable( e ) ) {
                    tiles.push_back( e );
                }
            }
            const itype *fragment_drop = item_controller->find_template( shr.drop );
            int qty = shr.casing_mass * std::min( 1.0, shr.recovery / 100.0 ) /
                      to_gram( fragment_drop->weight );
            // Truncate to a random selection
            std::shuffle( tiles.begin(), tiles.end(), rng_get_engine() );
            tiles.resize( std::min( static_cast<int>( tiles.size() ), qty ) );

            for( const tripoint &e : tiles ) {
                here.add_item_or_charges( e, item( shr.drop, calendar::turn, item::solitary_tag{} ) );
            }
        }
    }

    //if (ex.distance_factor > 0.0f && ex.power > 0.0f) {
    //    // Power rescaled to mean grams of TNT equivalent, this scales it roughly back to where
    //    // it was before until we re-do blasting power to be based on TNT-equivalent directly.
    //    //do_blast( source, p, ex.power, ex.distance_factor, ex.fire );

    //    do_blast_new(p, ex.power, ex.distance_factor * 10);
    //    

    //}

 
     ExplosionProcess process( p, ex.power, ex.distance_factor*10, std::nullopt, ex.fire);
     process.run();
 


}

void flashbang( const tripoint &p, bool player_immune )
{
    draw_explosion( p, 8, c_white );
    Character &player_character = get_player_character();
    int dist = rl_dist( player_character.pos(), p );
    map &here = get_map();
    if( dist <= 8 && !player_immune ) {
        if( !player_character.has_flag( STATIC( json_character_flag( "IMMUNE_HEARING_DAMAGE" ) ) ) &&
            !player_character.is_wearing( itype_rm13_armor_on ) ) {
            player_character.add_effect( effect_deaf, time_duration::from_turns( 40 - dist * 4 ) );
        }
        if( here.sees( player_character.pos(), p, 8 ) ) {
            int flash_mod = 0;
            if( player_character.has_trait( trait_PER_SLIME ) ) {
                if( one_in( 2 ) ) {
                    flash_mod = 3; // Yay, you weren't looking!
                }
            } else if( player_character.has_trait( trait_PER_SLIME_OK ) ) {
                flash_mod = 8; // Just retract those and extrude fresh eyes
            } else if( player_character.has_flag( json_flag_GLARE_RESIST ) ||
                       player_character.is_wearing( itype_rm13_armor_on ) ) {
                flash_mod = 6;
            } else if( player_character.worn_with_flag( STATIC( flag_id( "BLIND" ) ) ) ||
                       player_character.worn_with_flag( STATIC( flag_id( "FLASH_PROTECTION" ) ) ) ) {
                flash_mod = 3; // Not really proper flash protection, but better than nothing
            }
            player_character.add_env_effect( effect_blind, bodypart_id( "eyes" ), ( 12 - flash_mod - dist ) / 2,
                                             time_duration::from_turns( 10 - dist ) );
        }
    }
    for( monster &critter : g->all_monsters() ) {
        if( critter.type->in_species( species_ROBOT ) ) {
            continue;
        }
        // TODO: can the following code be called for all types of creatures
        dist = rl_dist( critter.pos(), p );
        if( dist <= 8 ) {
            if( dist <= 4 ) {
                critter.add_effect( effect_stunned, time_duration::from_turns( 10 - dist ) );
            }
            if( critter.has_flag( MF_SEES ) && here.sees( critter.pos(), p, 8 ) ) {
                critter.add_effect( effect_blind, time_duration::from_turns( 18 - dist ) );
            }
            if( critter.has_flag( MF_HEARS ) ) {
                critter.add_effect( effect_deaf, time_duration::from_turns( 60 - dist * 4 ) );
            }
        }
    }
    sounds::sound( p, 12, sounds::sound_t::combat, _( "a huge boom!" ), false, "misc", "flashbang" );
    // TODO: Blind/deafen NPC
}

void shockwave( const tripoint &p, int radius, int force, int stun, int dam_mult,
                bool ignore_player )
{
    draw_explosion( p, radius, c_blue );

    sounds::sound( p, force * force * dam_mult / 2, sounds::sound_t::combat, _( "Crack!" ), false,
                   "misc", "shockwave" );

    for( monster &critter : g->all_monsters() ) {
        if( critter.posz() != p.z ) {
            continue;
        }
        if( rl_dist( critter.pos(), p ) <= radius ) {
            add_msg( _( "%s is caught in the shockwave!" ), critter.name() );
            g->knockback( p, critter.pos(), force, stun, dam_mult );
        }
    }
    // TODO: combine the two loops and the case for avatar using all_creatures()
    for( npc &guy : g->all_npcs() ) {
        if( guy.posz() != p.z ) {
            continue;
        }
        if( rl_dist( guy.pos(), p ) <= radius ) {
            add_msg( _( "%s is caught in the shockwave!" ), guy.get_name() );
            g->knockback( p, guy.pos(), force, stun, dam_mult );
        }
    }
    Character &player_character = get_player_character();
    if( rl_dist( player_character.pos(), p ) <= radius && !ignore_player &&
        ( !player_character.has_trait( trait_LEG_TENT_BRACE ) ||
          !player_character.is_barefoot() ) ) {
        add_msg( m_bad, _( "You're caught in the shockwave!" ) );
        g->knockback( p, player_character.pos(), force, stun, dam_mult );
    }
}

void scrambler_blast( const tripoint &p )
{
    if( monster *const mon_ptr = get_creature_tracker().creature_at<monster>( p ) ) {
        monster &critter = *mon_ptr;
        if( critter.has_flag( MF_ELECTRONIC ) ) {
            critter.make_friendly();
        }
        add_msg( m_warning, _( "The %s sparks and begins searching for a target!" ),
                 critter.name() );
    }
}

void emp_blast( const tripoint &p )
{
    Character &player_character = get_player_character();
    const bool sight = player_character.sees( p );
    map &here = get_map();
    if( here.has_flag( ter_furn_flag::TFLAG_CONSOLE, p ) ) {
        if( sight ) {
            add_msg( _( "The %s is rendered non-functional!" ), here.tername( p ) );
        }
        here.furn_set( p, furn_f_machinery_electronic );
        return;
    }
    // TODO: More terrain effects.
    if( here.ter( p ) == t_card_science || here.ter( p ) == t_card_military ||
        here.ter( p ) == t_card_industrial ) {
        int rn = rng( 1, 100 );
        if( rn > 92 || rn < 40 ) {
            if( sight ) {
                add_msg( _( "The card reader is rendered non-functional." ) );
            }
            here.ter_set( p, t_card_reader_broken );
        }
        if( rn > 80 ) {
            if( sight ) {
                add_msg( _( "The nearby doors slide open!" ) );
            }
            for( int i = -3; i <= 3; i++ ) {
                for( int j = -3; j <= 3; j++ ) {
                    if( here.ter( p + tripoint( i, j, 0 ) ) == t_door_metal_locked ) {
                        here.ter_set( p + tripoint( i, j, 0 ), t_floor );
                    }
                }
            }
        }
        if( rn >= 40 && rn <= 80 ) {
            if( sight ) {
                add_msg( _( "Nothing happens." ) );
            }
        }
    }
    if( monster *const mon_ptr = get_creature_tracker().creature_at<monster>( p ) ) {
        monster &critter = *mon_ptr;
        if( critter.has_flag( MF_ELECTRONIC ) ) {
            int deact_chance = 0;
            const auto mon_item_id = critter.type->revert_to_itype;
            switch( critter.get_size() ) {
                case creature_size::tiny:
                    deact_chance = 6;
                    break;
                case creature_size::small:
                    deact_chance = 3;
                    break;
                default:
                    // Currently not used, I have no idea what chances bigger bots should have,
                    // Maybe export this to json?
                    break;
            }
            if( !mon_item_id.is_empty() && deact_chance != 0 && one_in( deact_chance ) ) {
                if( sight ) {
                    add_msg( _( "The %s beeps erratically and deactivates!" ), critter.name() );
                }
                here.add_item_or_charges( p, critter.to_item() );
                for( auto &ammodef : critter.ammo ) {
                    if( ammodef.second > 0 ) {
                        here.spawn_item( p, ammodef.first, 1, ammodef.second, calendar::turn );
                    }
                }
                g->remove_zombie( critter );
            } else {
                if( sight ) {
                    add_msg( _( "The EMP blast fries the %s!" ), critter.name() );
                }
                int dam = dice( 10, 10 );
                critter.apply_damage( nullptr, bodypart_id( "torso" ), dam );
                critter.check_dead_state();
                if( !critter.is_dead() && one_in( 6 ) ) {
                    critter.make_friendly();
                }
            }
        } else if( critter.has_flag( MF_ELECTRIC_FIELD ) ) {
            if( !critter.has_effect( effect_emp ) ) {
                if( sight ) {
                    add_msg( m_good, _( "The %s's electrical field momentarily goes out!" ), critter.name() );
                }
                critter.add_effect( effect_emp, 3_minutes );
            } else if( critter.has_effect( effect_emp ) ) {
                int dam = dice( 3, 5 );
                if( sight ) {
                    add_msg( m_good, _( "The %s's disabled electrical field reverses polarity!" ),
                             critter.name() );
                    add_msg( m_good, _( "It takes %d damage." ), dam );
                }
                critter.add_effect( effect_emp, 1_minutes );
                critter.apply_damage( nullptr, bodypart_id( "torso" ), dam );
                critter.check_dead_state();
            }
        } else if( sight ) {
            add_msg( _( "The %s is unaffected by the EMP blast." ), critter.name() );
        }
    }
    if( player_character.posx() == p.x && player_character.posy() == p.y &&
        player_character.posz() == p.z ) {
        if( player_character.get_power_level() > 0_kJ ) {
            add_msg( m_bad, _( "The EMP blast drains your power." ) );
            int max_drain = ( player_character.get_power_level() > 1000_kJ ? 1000 : units::to_kilojoule(
                                  player_character.get_power_level() ) );
            player_character.mod_power_level( units::from_kilojoule( -rng( 1 + max_drain / 3, max_drain ) ) );
        }
        // TODO: More effects?
        //e-handcuffs effects
        item_location weapon = player_character.get_wielded_item();
        if( weapon && weapon->typeId() == itype_e_handcuffs && weapon->charges > 0 ) {
            weapon->unset_flag( STATIC( flag_id( "NO_UNWIELD" ) ) );
            weapon->charges = 0;
            weapon->active = false;
            add_msg( m_good, _( "The %s on your wrists spark briefly, then release your hands!" ),
                     weapon->tname() );
        }

        for( item_location &it : player_character.all_items_loc() ) {
            // Render any electronic stuff in player's possession non-functional
            if( it->has_flag( flag_ELECTRONIC ) && !it->is_broken() &&
                get_option<bool>( "EMP_DISABLE_ELECTRONICS" ) ) {
                add_msg( m_bad, _( "The EMP blast fries your %s!" ), it->tname() );
                it->deactivate();
                it->set_flag( flag_ITEM_BROKEN );
            }
        }
    }

    for( item &it : here.i_at( p ) ) {
        // Render any electronic stuff on the ground non-functional
        if( it.has_flag( flag_ELECTRONIC ) && !it.is_broken() &&
            get_option<bool>( "EMP_DISABLE_ELECTRONICS" ) ) {
            if( sight ) {
                add_msg( _( "The EMP blast fries the %s!" ), it.tname() );
            }
            it.deactivate();
            it.set_flag( flag_ITEM_BROKEN );
        }
    }
    // TODO: Drain NPC energy reserves
}

void nuke( const tripoint_abs_omt &p )
{
    const tripoint_abs_sm pos_sm = project_to<coords::sm>( p );

    tinymap tmpmap;
    tmpmap.load( pos_sm, false );

    item mininuke( itype_mininuke_act );
    mininuke.set_flag( json_flag_ACTIVATE_ON_PLACE );
    tmpmap.add_item( { SEEX - 1, SEEY - 1, 0 }, mininuke );

    tmpmap.save();
}

void resonance_cascade( const tripoint &p )
{
    Character &player_character = get_player_character();
    const time_duration maxglow = time_duration::from_turns( 100 - 5 * trig_dist( p,
                                  player_character.pos() ) );
    if( maxglow > 0_turns ) {
        const time_duration minglow = std::max( 0_turns, time_duration::from_turns( 60 - 5 * trig_dist( p,
                                                player_character.pos() ) ) );
        player_character.add_effect( effect_teleglow, rng( minglow, maxglow ) * 100 );
    }
    int startx = p.x < 8 ? 0 : p.x - 8;
    int endx = p.x + 8 >= SEEX * 3 ? SEEX * 3 - 1 : p.x + 8;
    int starty = p.y < 8 ? 0 : p.y - 8;
    int endy = p.y + 8 >= SEEY * 3 ? SEEY * 3 - 1 : p.y + 8;
    tripoint dest( startx, starty, p.z );
    map &here = get_map();
    for( int &i = dest.x; i <= endx; i++ ) {
        for( int &j = dest.y; j <= endy; j++ ) {
            switch( rng( 1, 80 ) ) {
                case 1:
                case 2:
                    emp_blast( dest );
                    break;
                case 3:
                case 4:
                case 5:
                    for( int k = i - 1; k <= i + 1; k++ ) {
                        for( int l = j - 1; l <= j + 1; l++ ) {
                            field_type_id type = fd_null;
                            switch( rng( 1, 7 ) ) {
                                case 1:
                                    type = fd_blood;
                                    break;
                                case 2:
                                    type = fd_bile;
                                    break;
                                case 3:
                                case 4:
                                    type = fd_slime;
                                    break;
                                case 5:
                                    type = fd_fire;
                                    break;
                                case 6:
                                case 7:
                                    type = fd_nuke_gas;
                                    break;
                            }
                            if( !one_in( 3 ) ) {
                                // TODO: fix point types
                                here.add_field( tripoint_bub_ms{ k, l, p.z }, type, 3 );
                            }
                        }
                    }
                    break;
                case  6:
                case  7:
                case  8:
                case  9:
                case 10:
                    here.trap_set( dest, tr_portal );
                    break;
                case 11:
                case 12:
                    here.trap_set( dest, tr_goo );
                    break;
                case 13:
                case 14:
                case 15: {
                    std::vector<MonsterGroupResult> spawn_details =
                        MonsterGroupManager::GetResultFromGroup( GROUP_NETHER );
                    for( const MonsterGroupResult &mgr : spawn_details ) {
                        g->place_critter_at( mgr.name, dest );
                    }
                }
                break;
                case 16:
                case 17:
                case 18:
                    here.destroy( dest );
                    break;
                case 19:
                    explosion( &player_character, dest, rng( 1, 10 ), rng( 0, 1 ) * rng( 0, 6 ), one_in( 4 ) );
                    break;
                default:
                    break;
            }
        }
    }
}

void process_explosions()
{
    for( const queued_explosion &ex : _explosions ) {
        const tripoint p = get_map().getlocal( ex.pos );
        if( p.x < 0 || p.x >= MAPSIZE_X || p.y < 0 || p.y >= MAPSIZE_Y ) {
            debugmsg( "Explosion origin (%d, %d, %d) is out-of-bounds", p.x, p.y, p.z );
            continue;
        }
        _make_explosion( ex.source, p, ex.data );
    }
    _explosions.clear();
}

} // namespace explosion_handler

// This is only ever used to zero the cloud values, which is what makes it work.
fragment_cloud &fragment_cloud::operator=( const float &value )
{
    velocity = value;
    density = value;

    return *this;
}

bool fragment_cloud::operator==( const fragment_cloud &that ) const
{
    return velocity == that.velocity && density == that.density;
}

bool operator<( const fragment_cloud &us, const fragment_cloud &them )
{
    return us.density < them.density && us.velocity < them.velocity;
}

// Projectile velocity in air. See https://fas.org/man/dod-101/navy/docs/es310/warheads/Warheads.htm
// for a writeup of this exact calculation.
fragment_cloud shrapnel_calc( const fragment_cloud &initial,
                              const fragment_cloud &cloud,
                              const int &distance )
{
    // SWAG coefficient of drag.
    constexpr float Cd = 0.5f;
    fragment_cloud new_cloud;
    new_cloud.velocity = initial.velocity * std::exp( -cloud.velocity * ( (
                             Cd * fragment_area * distance ) /
                         ( 2.0f * fragment_mass ) ) );
    // Two effects, the accumulated proportion of blocked fragments,
    // and the inverse-square dilution of fragments with distance.
    new_cloud.density = ( initial.density * cloud.density ) / ( distance * distance / 2.5 );
    return new_cloud;
}
bool shrapnel_check( const fragment_cloud &cloud, const fragment_cloud &intensity )
{
    return cloud.density > 0.0 && intensity.velocity > MIN_EFFECTIVE_VELOCITY &&
           intensity.density > MIN_FRAGMENT_DENSITY;
}

void update_fragment_cloud( fragment_cloud &update, const fragment_cloud &new_value, quadrant )
{
    update = std::max( update, new_value );
}

fragment_cloud accumulate_fragment_cloud( const fragment_cloud &cumulative_cloud,
        const fragment_cloud &current_cloud, const int &distance )
{
    // Velocity is the cumulative and continuous decay of speed,
    // so it is accumulated the same way as light attenuation.
    // Density is the accumulation of discrete attenuation events encountered in the traversed squares,
    // so each term is added to the series via multiplication.
    return fragment_cloud( ( ( distance - 1 ) * cumulative_cloud.velocity + current_cloud.velocity ) /
                           distance,
                           cumulative_cloud.density * current_cloud.density );
}

float explosion_data::expected_range( float ratio ) const
{
    if( power <= 0.0f || distance_factor >= 1.0f || distance_factor <= 0.0f ) {
        return 0.0f;
    }

    // The 1.1 is because actual power drops at roughly that rate
    return std::log( ratio ) / std::log( distance_factor / 1.1f );
}

float explosion_data::power_at_range( float dist ) const
{
    if( power <= 0.0f || distance_factor >= 1.0f || distance_factor <= 0.0f ) {
        return 0.0f;
    }

    // The 1.1 is because actual power drops at roughly that rate
    return power * std::pow( distance_factor / 1.1f, dist );
}

int explosion_data::safe_range() const
{
    const float ratio = 1 / power / 2;
    return expected_range( ratio ) + 1;
}
