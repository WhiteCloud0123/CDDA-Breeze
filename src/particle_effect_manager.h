#pragma once
#ifndef CATA_SRC_PARTICLE_EFFECT_MANAGER_H
#define CATA_SRC_PARTICLE_EFFECT_MANAGER_H

#include <map>
#include <string>
#include <vector>
#include "particle_system.h"
#include "json.h"

struct ParticleEffectConfig {
    std::string id;
    std::string name;
    std::string description;
    Particle_Activity::Mode mode;
    int total_particles;
    float duration;
    Vec2 gravity;
    float speed;
    float speed_var;
    float radial_accel;
    float radial_accel_var;
    float tangential_accel;
    float tangential_accel_var;
    float angle;
    float angle_var;
    float life;
    float life_var;
    float start_size;
    float start_size_var;
    float end_size;
    float end_size_var;
    Color4F start_color;
    Color4F start_color_var;
    Color4F end_color;
    Color4F end_color_var;
    Vec2 pos_var;
    float start_spin;
    float start_spin_var;
    float end_spin;
    float end_spin_var;
};

class ParticleEffectManager {
public:
    static ParticleEffectManager& get_instance();
    
    bool load_config(const std::string& filename);
    
    const ParticleEffectConfig* get_effect_config(const std::string& id) const;
    
    Particle_Activity* create_effect(const std::string& effect_id, const tripoint& pos);
    
    void update();
    void draw();
    
    void destroy_effect(Particle_Activity* effect);
    
    void clear_all_effects();
    
    int get_active_effect_count() const;
    
private:
    ParticleEffectManager();
    ~ParticleEffectManager();
    
    ParticleEffectManager(const ParticleEffectManager&) = delete;
    ParticleEffectManager& operator=(const ParticleEffectManager&) = delete;
    
    bool parse_effect_config(const JsonObject& obj, ParticleEffectConfig& config);
    Particle_Activity::Mode parse_effect_mode(const std::string& mode_str);
    
    std::map<std::string, ParticleEffectConfig> effect_configs;
    std::vector<Particle_Activity*> active_effects;
    
    bool is_initialized = false;
};

#endif // CATA_SRC_PARTICLE_EFFECT_MANAGER_H
