#include "cached_options.h"
#include<string>
bool fov_3d;
int fov_3d_z_range;
bool keycode_mode;
bool log_from_top;
int message_ttl;
int message_cooldown;
bool test_mode;
int tile_retracted;
float tile_retract_dist_min;
float tile_retract_dist_max;
bool use_tiles;
bool use_far_tiles;
bool use_tiles_overmap;
bool use_pinyin_search;
test_mode_spilling_action_t test_mode_spilling_action = test_mode_spilling_action_t::spill_all;
bool direct3d_mode;
bool pixel_minimap_option;
int pixel_minimap_r;
int pixel_minimap_g;
int pixel_minimap_b;
int pixel_minimap_a;
bool use_particle_system;
bool use_show_creature_hp_bar;
bool use_show_player_move_point;
bool use_animation;
int terminal_x;
int terminal_y;
// 安卓
#if defined(__ANDROID__)

int android_virtual_joystick_opacity;
int android_initial_delay;
int android_shortcut_remove_turns;
int android_repeat_delay_min;
int android_repeat_delay_max;
int android_shortcut_screen_percentage;
int android_shortcut_opacity_bg;
int android_shortcut_opacity_shadow;
int android_shortcut_color;
int android_shortcut_opacity_fg;

float android_deadzone_range;
float android_repeat_delay_range;
float android_sensitivity_power;

bool android_hide_holds;
bool android_show_virtual_joystick;
bool android_shortcut_move_front;
bool android_virtual_joystick_follow;

std::string android_shortcut_position;
std::string android_shortcut_defaults;

#endif



#ifndef CATA_IN_TOOL
error_log_format_t error_log_format = error_log_format_t::human_readable;
check_plural_t check_plural = check_plural_t::certain;
#endif
