#include "particle_effect_manager.h"
#include "json.h"
#include "json_loader.h"
#include "output.h"
#include "game.h"
#include "map.h"

ParticleEffectManager::ParticleEffectManager() {

}

ParticleEffectManager::~ParticleEffectManager() {
    clear_all_effects();
}

ParticleEffectManager& ParticleEffectManager::get_instance() {
    static ParticleEffectManager instance;
    return instance;
}

bool ParticleEffectManager::load_config(const std::string& filename) {
    try {
        cata_path config_path( cata_path::root_path::data, "particle_effects.json" );
        JsonValue root = json_loader::from_path(config_path);
        JsonObject root_obj = root.get_object();
        
        if (!root_obj.has_array("particle_effects")) {
            debugmsg("Particle effects config file missing 'particle_effects' array: %s", filename.c_str());
            return false;
        }
        
        JsonArray effects_array = root_obj.get_array("particle_effects");
        for (const JsonValue& effect_val : effects_array) {
            JsonObject effect_obj = effect_val.get_object();
            ParticleEffectConfig config;
            
            if (parse_effect_config(effect_obj, config)) {
                effect_configs[config.id] = config;
            } else {
                debugmsg("Failed to parse particle effect config from %s", filename.c_str());
            }
        }
        
        is_initialized = true;
        return true;
    } catch (const std::exception& e) {
        debugmsg("Error loading particle effects config from %s: %s", filename.c_str(), e.what());
        return false;
    }
}

const ParticleEffectConfig* ParticleEffectManager::get_effect_config(const std::string& id) const {
    auto it = effect_configs.find(id);
    if (it != effect_configs.end()) {
        return &(it->second);
    }
    return nullptr;
}

Particle_Activity* ParticleEffectManager::create_effect(const std::string& effect_id, const tripoint& pos) {
    const ParticleEffectConfig* config = get_effect_config(effect_id);
    if (!config) {
        debugmsg("Attempt to create unknown particle effect: %s", effect_id.c_str());
        return nullptr;
    }
    
    Particle_Activity* effect = new Particle_Activity();
    
    // 初始化粒子效果
    effect->initWithTotalParticles(config->total_particles);
    effect->setEmitterMode(config->mode);
    effect->setDuration(config->duration);
    effect->setGravity(config->gravity);
    effect->setSpeed(config->speed);
    effect->setSpeedVar(config->speed_var);
    effect->setRadialAccel(config->radial_accel);
    effect->setRadialAccelVar(config->radial_accel_var);
    effect->setTangentialAccel(config->tangential_accel);
    effect->setTangentialAccelVar(config->tangential_accel_var);
    
    effect->set_style(effect_id);
    effect->set_position(pos);
    
    // 设置角度
    effect->setAngle(config->angle);
    effect->setAngleVar(config->angle_var);
    
    // 设置生命周期
    effect->setLife(config->life);
    effect->setLifeVar(config->life_var);
    
    // 设置大小
    effect->setStartSize(config->start_size);
    effect->setStartSizeVar(config->start_size_var);
    effect->setEndSize(config->end_size);
    effect->setEndSizeVar(config->end_size_var);
    
    // 设置颜色
    effect->setStartColor(config->start_color);
    effect->setStartColorVar(config->start_color_var);
    effect->setEndColor(config->end_color);
    effect->setEndColorVar(config->end_color_var);
    
    // 设置位置方差
    effect->setPosVar(config->pos_var);
    
    // 设置旋转
    effect->setStartSpin(config->start_spin);
    effect->setStartSpinVar(config->start_spin_var);
    effect->setEndSpin(config->end_spin);
    effect->setEndSpinVar(config->end_spin_var);
    
    // 设置发射率
    effect->setEmissionRate(effect->getTotalParticles() / effect->getLife());
    
    active_effects.push_back(effect);
    return effect;
}

void ParticleEffectManager::update() {
    if (!is_initialized) {
        return;
    }
    
    // 更新所有活跃粒子效果
    for (auto it = active_effects.begin(); it != active_effects.end();) {
        Particle_Activity* effect = *it;
        effect->update();
        
        // 如果效果已完成且设置了自动移除，或者效果不再活跃且没有粒子
        if ((effect->isAutoRemoveOnFinish() && !effect->isActive()) || 
            (!effect->isActive() && effect->getParticleCount() == 0)) {
            delete effect;
            it = active_effects.erase(it);
        } else {
            ++it;
        }
    }
}

void ParticleEffectManager::draw() {
    if (!is_initialized) {
        return;
    }
    for (Particle_Activity* effect : active_effects) {
        effect->draw();
    }
}

void ParticleEffectManager::destroy_effect(Particle_Activity* effect) {
    if (!effect) {
        return;
    }
    
    for (auto it = active_effects.begin(); it != active_effects.end(); ++it) {
        if (*it == effect) {
            delete effect;
            active_effects.erase(it);
            break;
        }
    }
}

void ParticleEffectManager::clear_all_effects() {
    for (Particle_Activity* effect : active_effects) {
        delete effect;
    }
    active_effects.clear();
}

int ParticleEffectManager::get_active_effect_count() const {
    return active_effects.size();
}

bool ParticleEffectManager::parse_effect_config(const JsonObject& obj, ParticleEffectConfig& config) {
    try {
        // 基本属性
        config.id = obj.get_string("id");
        config.name = obj.get_string("name", "");
        config.description = obj.get_string("description", "");
        
        // 模式
        std::string mode_str = obj.get_string("mode");
        config.mode = parse_effect_mode(mode_str);
        
        // 基本参数
        config.total_particles = obj.get_int("total_particles");
        config.duration = obj.get_float("duration");
        
        // 重力
        JsonObject gravity_obj = obj.get_object("gravity");
        config.gravity.x = gravity_obj.get_float("x");
        config.gravity.y = gravity_obj.get_float("y");
        
        // 速度参数
        config.speed = obj.get_float("speed");
        config.speed_var = obj.get_float("speed_var");
        
        // 加速度参数
        config.radial_accel = obj.get_float("radial_accel");
        config.radial_accel_var = obj.get_float("radial_accel_var");
        config.tangential_accel = obj.get_float("tangential_accel");
        config.tangential_accel_var = obj.get_float("tangential_accel_var");
        
        // 角度参数
        config.angle = obj.get_float("angle");
        config.angle_var = obj.get_float("angle_var");
        
        // 生命周期
        config.life = obj.get_float("life");
        config.life_var = obj.get_float("life_var");
        
        // 大小参数
        config.start_size = obj.get_float("start_size");
        config.start_size_var = obj.get_float("start_size_var");
        
        if (obj.has_string("end_size")) {
            std::string end_size_str = obj.get_string("end_size");
            if (end_size_str == "equal") {
                config.end_size = Particle_Activity::START_SIZE_EQUAL_TO_END_SIZE;
                config.end_size_var = 0.0f;
            } else {
                config.end_size = std::stof(end_size_str);
                config.end_size_var = obj.get_float("end_size_var", 0.0f);
            }
        } else {
            config.end_size = obj.get_float("end_size");
            config.end_size_var = obj.get_float("end_size_var", 0.0f);
        }
        
        // 颜色参数
        JsonObject start_color_obj = obj.get_object("start_color");
        config.start_color.r = start_color_obj.get_float("r");
        config.start_color.g = start_color_obj.get_float("g");
        config.start_color.b = start_color_obj.get_float("b");
        config.start_color.a = start_color_obj.get_float("a");
        
        JsonObject start_color_var_obj = obj.get_object("start_color_var");
        config.start_color_var.r = start_color_var_obj.get_float("r");
        config.start_color_var.g = start_color_var_obj.get_float("g");
        config.start_color_var.b = start_color_var_obj.get_float("b");
        config.start_color_var.a = start_color_var_obj.get_float("a");
        
        JsonObject end_color_obj = obj.get_object("end_color");
        config.end_color.r = end_color_obj.get_float("r");
        config.end_color.g = end_color_obj.get_float("g");
        config.end_color.b = end_color_obj.get_float("b");
        config.end_color.a = end_color_obj.get_float("a");
        
        JsonObject end_color_var_obj = obj.get_object("end_color_var");
        config.end_color_var.r = end_color_var_obj.get_float("r");
        config.end_color_var.g = end_color_var_obj.get_float("g");
        config.end_color_var.b = end_color_var_obj.get_float("b");
        config.end_color_var.a = end_color_var_obj.get_float("a");
        
        // 位置方差
        JsonObject pos_var_obj = obj.get_object("pos_var");
        config.pos_var.x = pos_var_obj.get_float("x");
        config.pos_var.y = pos_var_obj.get_float("y");
        
        // 旋转参数
        config.start_spin = obj.get_float("start_spin", 0.0f);
        config.start_spin_var = obj.get_float("start_spin_var", 0.0f);
        config.end_spin = obj.get_float("end_spin", 0.0f);
        config.end_spin_var = obj.get_float("end_spin_var", 0.0f);
        
        return true;
    } catch (const std::exception& e) {
        debugmsg("Error parsing particle effect config: %s", e.what());
        return false;
    }
}

Particle_Activity::Mode ParticleEffectManager::parse_effect_mode(const std::string& mode_str) {
    if (mode_str == "GRAVITY") {
        return Particle_Activity::Mode::GRAVITY;
    } else if (mode_str == "RADIUS") {
        return Particle_Activity::Mode::RADIUS;
    } else {
        debugmsg("Unknown particle effect mode: %s, defaulting to GRAVITY", mode_str.c_str());
        return Particle_Activity::Mode::GRAVITY;
    }
}
