#ifndef CATA_SRC_CACHED_OPTIONS_H
#define CATA_SRC_CACHED_OPTIONS_H

// A collection of options which are accessed frequently enough that we don't
// want to pay the overhead of a string lookup each time one is tested.
// They should be updated when the corresponding option is changed (in
// options.cpp).

extern bool fov_3d;
extern int fov_3d_z_range;
extern bool keycode_mode;
extern bool log_from_top;
extern int message_ttl;
extern int message_cooldown;
extern int tile_retracted;
extern float tile_retract_dist_min;
extern float tile_retract_dist_max;
extern bool use_tiles;
extern bool use_far_tiles;
extern bool use_pinyin_search;
extern bool use_tiles_overmap;
extern bool pixel_minimap_option;
extern int pixel_minimap_r;
extern int pixel_minimap_g;
extern int pixel_minimap_b;
extern int pixel_minimap_a;
extern bool use_particle_system;
extern bool use_show_creature_hp_bar;
extern bool use_show_player_move_point;
extern bool use_animation;
extern int terminal_x;
extern int terminal_y;

// 安卓
#if defined(__ANDROID__)

extern int android_virtual_joystick_opacity;
extern int android_initial_delay;
extern int android_shortcut_remove_turns;
extern int android_repeat_delay_min;
extern int android_repeat_delay_max;
extern int android_shortcut_screen_percentage;
extern int android_shortcut_opacity_bg;
extern int android_shortcut_opacity_shadow;

extern float android_deadzone_range;
extern float android_repeat_delay_range;
extern float android_sensitivity_power;

extern bool android_hide_holds;
extern bool android_show_virtual_joystick;
extern bool android_shortcut_move_front;
extern bool android_virtual_joystick_follow;

extern std::string android_shortcut_position;
extern std::string android_shortcut_defaults;

#endif

// test_mode is not a regular game option; it's true when we are running unit
// tests.
extern bool test_mode;
enum class test_mode_spilling_action_t {
    spill_all,
    cancel_spill,
};
extern test_mode_spilling_action_t test_mode_spilling_action;

extern bool direct3d_mode;

enum class error_log_format_t {
    human_readable,
    // Output error messages in github action command format (currently json only)
    // See https://docs.github.com/en/free-pro-team@latest/actions/reference/workflow-commands-for-github-actions#setting-an-error-message
    github_action,
};
#ifndef CATA_IN_TOOL
extern error_log_format_t error_log_format;
#else
constexpr error_log_format_t error_log_format = error_log_format_t::human_readable;
#endif

enum class check_plural_t {
    none,
    certain, // report strings that certainly have a non-regular plural form, such as those ending in "s"
    possible, // report strings that may or may not have a non-regular plural form, such as those containing the word "of"
};
#ifndef CATA_IN_TOOL
extern check_plural_t check_plural;
#else
constexpr check_plural_t check_plural = check_plural_t::none;
#endif

#endif // CATA_SRC_CACHED_OPTIONS_H
