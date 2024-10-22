#include "particle_system.h"
#include <algorithm>
#include <assert.h>
#include <string>
#include "output.h"
#include"options.h"
#include "sdltiles.h"
#include "messages.h"

SDL_Texture* Particle_Activity::_texture = nullptr;
SDL_Renderer* Particle_Activity::_renderer = nullptr;
std::list<Particle_Activity*> Particle_Activity::particle_activity_list;

inline float Deg2Rad(float a)
{
    return a * 0.01745329252f;
}

inline float Rad2Deg(float a)
{
    return a * 57.29577951f;
}

inline float clampf(float value, float min_inclusive, float max_inclusive)
{
    if (min_inclusive > max_inclusive)
    {
        std::swap(min_inclusive, max_inclusive);
    }
    return value < min_inclusive ? min_inclusive : value < max_inclusive ? value : max_inclusive;
}

inline void normalize_point(float x, float y, Pointf* out)
{
    float n = x * x + y * y;
    // Already normalized.
    if (n == 1.0f)
    {
        return;
    }

    n = sqrt(n);
    // Too close to zero.
    if (n < 1e-5)
    {
        return;
    }

    n = 1.0f / n;
    out->x = x * n;
    out->y = y * n;
}

/**
A more effect random number getter function, get from ejoy2d.
*/
inline static float RANDOM_M11(unsigned int* seed)
{
    *seed = *seed * 134775813 + 1;
    union
    {
        uint32_t d;
        float f;
    } u;
    u.d = (((uint32_t)(*seed) & 0x7fff) << 8) | 0x40000000;
    return u.f - 3.0f;
}



// implementation Particle_Activity

bool Particle_Activity::initWithTotalParticles(int numberOfParticles)
{
    _totalParticles = numberOfParticles;
    _isActive = true;
    _emitterMode = Mode::GRAVITY;
    _isAutoRemoveOnFinish = false;
    _transformSystemDirty = false;

    resetTotalParticles(numberOfParticles);

    return true;
}

void Particle_Activity::resetTotalParticles(int numberOfParticles)
{
    if (particle_data_.size() < numberOfParticles)
    {
        particle_data_.resize(numberOfParticles);
    }
}

void Particle_Activity::init_weather_content() {
    w_t_i_str = "null";
    support_weather_id = { "light_drizzle","drizzle" ,"rain","snowing" };
}

bool Particle_Activity::is_support_weather(const std::string& id) {
    
    return support_weather_id.find(id) != support_weather_id.end();

}

void Particle_Activity::init_texture(SDL_Texture* texture) {

    _texture = texture;

}

void Particle_Activity::init_renderer(SDL_Renderer* renderer) {

    _renderer = renderer;

}

void Particle_Activity::addParticles(int count)
{
    if (_paused)
    {
        return;
    }
    uint32_t RANDSEED = rand();

    int start = _particleCount;
    _particleCount += count;

    //life
    for (int i = start; i < _particleCount; ++i)
    {
        float theLife = _life + _lifeVar * RANDOM_M11(&RANDSEED);
        particle_data_[i].timeToLive = (std::max)(0.0f, theLife);
    }

    //position
    for (int i = start; i < _particleCount; ++i)
    {
        particle_data_[i].posx = _sourcePosition.x + _posVar.x * RANDOM_M11(&RANDSEED);
    }

    for (int i = start; i < _particleCount; ++i)
    {
        particle_data_[i].posy = _sourcePosition.y + _posVar.y * RANDOM_M11(&RANDSEED);
    }

    //color
#define SET_COLOR(c, b, v)                                                 \
    for (int i = start; i < _particleCount; ++i)                           \
    {                                                                      \
        particle_data_[i].c = clampf(b + v * RANDOM_M11(&RANDSEED), 0, 1); \
    }

    SET_COLOR(colorR, _startColor.r, _startColorVar.r);
    SET_COLOR(colorG, _startColor.g, _startColorVar.g);
    SET_COLOR(colorB, _startColor.b, _startColorVar.b);
    SET_COLOR(colorA, _startColor.a, _startColorVar.a);

    SET_COLOR(deltaColorR, _endColor.r, _endColorVar.r);
    SET_COLOR(deltaColorG, _endColor.g, _endColorVar.g);
    SET_COLOR(deltaColorB, _endColor.b, _endColorVar.b);
    SET_COLOR(deltaColorA, _endColor.a, _endColorVar.a);

#define SET_DELTA_COLOR(c, dc)                                                                              \
    for (int i = start; i < _particleCount; ++i)                                                            \
    {                                                                                                       \
        particle_data_[i].dc = (particle_data_[i].dc - particle_data_[i].c) / particle_data_[i].timeToLive; \
    }

    SET_DELTA_COLOR(colorR, deltaColorR);
    SET_DELTA_COLOR(colorG, deltaColorG);
    SET_DELTA_COLOR(colorB, deltaColorB);
    SET_DELTA_COLOR(colorA, deltaColorA);

    //size
    for (int i = start; i < _particleCount; ++i)
    {
        particle_data_[i].size = _startSize + _startSizeVar * RANDOM_M11(&RANDSEED);
        particle_data_[i].size = (std::max)(0.0f, particle_data_[i].size);
    }

    if (_endSize != START_SIZE_EQUAL_TO_END_SIZE)
    {
        for (int i = start; i < _particleCount; ++i)
        {
            float endSize = _endSize + _endSizeVar * RANDOM_M11(&RANDSEED);
            endSize = (std::max)(0.0f, endSize);
            particle_data_[i].deltaSize = (endSize - particle_data_[i].size) / particle_data_[i].timeToLive;
        }
    }
    else
    {
        for (int i = start; i < _particleCount; ++i)
        {
            particle_data_[i].deltaSize = 0.0f;
        }
    }

    // rotation
    for (int i = start; i < _particleCount; ++i)
    {
        particle_data_[i].rotation = _startSpin + _startSpinVar * RANDOM_M11(&RANDSEED);
    }
    for (int i = start; i < _particleCount; ++i)
    {
        float endA = _endSpin + _endSpinVar * RANDOM_M11(&RANDSEED);
        particle_data_[i].deltaRotation = (endA - particle_data_[i].rotation) / particle_data_[i].timeToLive;
    }

    // position
    Vec2 pos;
    pos.x = x_;
    pos.y = y_;

    for (int i = start; i < _particleCount; ++i)
    {
        particle_data_[i].startPosX = pos.x;
    }
    for (int i = start; i < _particleCount; ++i)
    {
        particle_data_[i].startPosY = pos.y;
    }

    // Mode Gravity: A
    if (_emitterMode == Mode::GRAVITY)
    {

        // radial accel
        for (int i = start; i < _particleCount; ++i)
        {
            particle_data_[i].modeA.radialAccel = modeA.radialAccel + modeA.radialAccelVar * RANDOM_M11(&RANDSEED);
        }

        // tangential accel
        for (int i = start; i < _particleCount; ++i)
        {
            particle_data_[i].modeA.tangentialAccel = modeA.tangentialAccel + modeA.tangentialAccelVar * RANDOM_M11(&RANDSEED);
        }

        // rotation is dir
        if (modeA.rotationIsDir)
        {
            for (int i = start; i < _particleCount; ++i)
            {
                float a = Deg2Rad(_angle + _angleVar * RANDOM_M11(&RANDSEED));
                Vec2 v(cosf(a), sinf(a));
                float s = modeA.speed + modeA.speedVar * RANDOM_M11(&RANDSEED);
                Vec2 dir = v * s;
                particle_data_[i].modeA.dirX = dir.x;    //v * s ;
                particle_data_[i].modeA.dirY = dir.y;
                particle_data_[i].rotation = -Rad2Deg(dir.getAngle());
            }
        }
        else
        {
            for (int i = start; i < _particleCount; ++i)
            {
                float a = Deg2Rad(_angle + _angleVar * RANDOM_M11(&RANDSEED));
                Vec2 v(cosf(a), sinf(a));
                float s = modeA.speed + modeA.speedVar * RANDOM_M11(&RANDSEED);
                Vec2 dir = v * s;
                particle_data_[i].modeA.dirX = dir.x;    //v * s ;
                particle_data_[i].modeA.dirY = dir.y;
            }
        }
    }

    // Mode Radius: B
    else
    {
        for (int i = start; i < _particleCount; ++i)
        {
            particle_data_[i].modeB.radius = modeB.startRadius + modeB.startRadiusVar * RANDOM_M11(&RANDSEED);
        }

        for (int i = start; i < _particleCount; ++i)
        {
            particle_data_[i].modeB.angle = Deg2Rad(_angle + _angleVar * RANDOM_M11(&RANDSEED));
        }

        for (int i = start; i < _particleCount; ++i)
        {
            particle_data_[i].modeB.degreesPerSecond = Deg2Rad(modeB.rotatePerSecond + modeB.rotatePerSecondVar * RANDOM_M11(&RANDSEED));
        }

        if (modeB.endRadius == START_RADIUS_EQUAL_TO_END_RADIUS)
        {
            for (int i = start; i < _particleCount; ++i)
            {
                particle_data_[i].modeB.deltaRadius = 0.0f;
            }
        }
        else
        {
            for (int i = start; i < _particleCount; ++i)
            {
                float endRadius = modeB.endRadius + modeB.endRadiusVar * RANDOM_M11(&RANDSEED);
                particle_data_[i].modeB.deltaRadius = (endRadius - particle_data_[i].modeB.radius) / particle_data_[i].timeToLive;
            }
        }
    }
}

void Particle_Activity::stopSystem()
{
    _isActive = false;
    _elapsed = _duration;
    _emitCounter = 0;
}

void Particle_Activity::resetSystem()
{
    _isActive = true;
    _elapsed = 0;
    for (int i = 0; i < _particleCount; ++i)
    {
        //particle_data_[i].timeToLive = 0.0f;
    }
}

bool Particle_Activity::isFull()
{
    return (_particleCount == _totalParticles);
}

// Particle_Activity - MainLoop
void Particle_Activity::update()
{
    float dt = 1.0 / 25;
    if (_isActive && _emissionRate)
    {
        float rate = 1.0f / _emissionRate;
        int totalParticles = _totalParticles;

        //issue #1201, prevent bursts of particles, due to too high emitCounter
        if (_particleCount < totalParticles)
        {
            _emitCounter += dt;
            if (_emitCounter < 0.f)
            {
                _emitCounter = 0.f;
            }
        }

        int emitCount = (std::min)(1.0f * (totalParticles - _particleCount), _emitCounter / rate);
        addParticles(emitCount);
        _emitCounter -= rate * emitCount;

        _elapsed += dt;
        if (_elapsed < 0.f)
        {
            _elapsed = 0.f;
        }
        if (_duration != DURATION_INFINITY && _duration < _elapsed)
        {
            this->stopSystem();
        }
    }

    for (int i = 0; i < _particleCount; ++i)
    {
        particle_data_[i].timeToLive -= dt;
    }

    // rebirth
    for (int i = 0; i < _particleCount; ++i)
    {
        if (particle_data_[i].timeToLive <= 0.0f)
        {
            int j = _particleCount - 1;
            //while (j > 0 && particle_data_[i].timeToLive <= 0)
            //{
            //    _particleCount--;
            //    j--;
            //}
            particle_data_[i] = particle_data_[_particleCount - 1];
            --_particleCount;
        }
    }

    if (_emitterMode == Mode::GRAVITY)
    {
        for (int i = 0; i < _particleCount; ++i)
        {
            Pointf tmp, radial = { 0.0f, 0.0f }, tangential;

            // radial acceleration
            if (particle_data_[i].posx || particle_data_[i].posy)
            {
                normalize_point(particle_data_[i].posx, particle_data_[i].posy, &radial);
            }
            tangential = radial;
            radial.x *= particle_data_[i].modeA.radialAccel;
            radial.y *= particle_data_[i].modeA.radialAccel;

            // tangential acceleration
            std::swap(tangential.x, tangential.y);
            tangential.x *= -particle_data_[i].modeA.tangentialAccel;
            tangential.y *= particle_data_[i].modeA.tangentialAccel;

            // (gravity + radial + tangential) * dt
            tmp.x = radial.x + tangential.x + modeA.gravity.x;
            tmp.y = radial.y + tangential.y + modeA.gravity.y;
            tmp.x *= dt;
            tmp.y *= dt;

            particle_data_[i].modeA.dirX += tmp.x;
            particle_data_[i].modeA.dirY += tmp.y;

            // this is cocos2d-x v3.0
            // if (_configName.length()>0 && _yCoordFlipped != -1)

            // this is cocos2d-x v3.0
            tmp.x = particle_data_[i].modeA.dirX * dt * _yCoordFlipped;
            tmp.y = particle_data_[i].modeA.dirY * dt * _yCoordFlipped;
            particle_data_[i].posx += tmp.x;
            particle_data_[i].posy += tmp.y;
        }
    }
    else
    {
        for (int i = 0; i < _particleCount; ++i)
        {
            particle_data_[i].modeB.angle += particle_data_[i].modeB.degreesPerSecond * dt;
            particle_data_[i].modeB.radius += particle_data_[i].modeB.deltaRadius * dt;
            particle_data_[i].posx = -cosf(particle_data_[i].modeB.angle) * particle_data_[i].modeB.radius;
            particle_data_[i].posy = -sinf(particle_data_[i].modeB.angle) * particle_data_[i].modeB.radius * _yCoordFlipped;
        }
    }

    //color, size, rotation
    for (int i = 0; i < _particleCount; ++i)
    {
        particle_data_[i].colorR += particle_data_[i].deltaColorR * dt;
        particle_data_[i].colorG += particle_data_[i].deltaColorG * dt;
        particle_data_[i].colorB += particle_data_[i].deltaColorB * dt;
        particle_data_[i].colorA += particle_data_[i].deltaColorA * dt;
        particle_data_[i].size += (particle_data_[i].deltaSize * dt);
        particle_data_[i].size = (std::max)(0.0f, particle_data_[i].size);
        particle_data_[i].rotation += particle_data_[i].deltaRotation * dt;
    }
}

// Particle_Activity - Texture protocol
void Particle_Activity::setTexture(SDL_Texture* var)
{
    if (_texture != var)
    {
        _texture = var;
    }
}

void Particle_Activity::draw()
{
    if (_texture == nullptr)
    {
        add_msg("texture为空");
        return;
    }

    if (style != "") {
        for (int i = 0; i < _particleCount; i++)
        {
            auto& p = particle_data_[i];
            if (p.size <= 0 || p.colorA <= 0)
            {
                continue;
            }
            SDL_Rect r = { int(p.posx + p.startPosX - p.size / 2), int(p.posy + p.startPosY - p.size / 2), int(p.size), int(p.size) };
            SDL_Color c = { Uint8(p.colorR * 255), Uint8(p.colorG * 255), Uint8(p.colorB * 255), Uint8(p.colorA * 255) };
            SDL_SetTextureColorMod(_texture, c.r, c.g, c.b);
            SDL_SetTextureAlphaMod(_texture, c.a);
            SDL_SetTextureBlendMode(_texture, SDL_BLENDMODE_BLEND);
            SDL_RenderCopyEx(_renderer, _texture, nullptr, &r, p.rotation, nullptr, SDL_FLIP_NONE);
        }
        update();
    }

}

SDL_Texture* Particle_Activity::getTexture()
{
    return _texture;
}

// Particle_Activity - Properties of Gravity Mode
void Particle_Activity::setTangentialAccel(float t)
{
    modeA.tangentialAccel = t;
}

float Particle_Activity::getTangentialAccel() const
{
    return modeA.tangentialAccel;
}

void Particle_Activity::setTangentialAccelVar(float t)
{
    modeA.tangentialAccelVar = t;
}

float Particle_Activity::getTangentialAccelVar() const
{
    return modeA.tangentialAccelVar;
}

void Particle_Activity::setRadialAccel(float t)
{
    modeA.radialAccel = t;
}

float Particle_Activity::getRadialAccel() const
{
    return modeA.radialAccel;
}

void Particle_Activity::setRadialAccelVar(float t)
{
    modeA.radialAccelVar = t;
}

float Particle_Activity::getRadialAccelVar() const
{
    return modeA.radialAccelVar;
}

void Particle_Activity::setRotationIsDir(bool t)
{
    modeA.rotationIsDir = t;
}

bool Particle_Activity::getRotationIsDir() const
{
    return modeA.rotationIsDir;
}

void Particle_Activity::setGravity(const Vec2& g)
{
    modeA.gravity = g;
}


void Particle_Activity::setSpeed(float speed)
{
    modeA.speed = speed;
}

float Particle_Activity::getSpeed() const
{
    return modeA.speed;
}

void Particle_Activity::setSpeedVar(float speedVar)
{

    modeA.speedVar = speedVar;
}

float Particle_Activity::getSpeedVar() const
{

    return modeA.speedVar;
}

// Particle_Activity - Properties of Radius Mode
void Particle_Activity::setStartRadius(float startRadius)
{
    modeB.startRadius = startRadius;
}

float Particle_Activity::getStartRadius() const
{
    return modeB.startRadius;
}

void Particle_Activity::setStartRadiusVar(float startRadiusVar)
{
    modeB.startRadiusVar = startRadiusVar;
}

float Particle_Activity::getStartRadiusVar() const
{
    return modeB.startRadiusVar;
}

void Particle_Activity::setEndRadius(float endRadius)
{
    modeB.endRadius = endRadius;
}

float Particle_Activity::getEndRadius() const
{
    return modeB.endRadius;
}

void Particle_Activity::setEndRadiusVar(float endRadiusVar)
{
    modeB.endRadiusVar = endRadiusVar;
}

float Particle_Activity::getEndRadiusVar() const
{

    return modeB.endRadiusVar;
}

void Particle_Activity::setRotatePerSecond(float degrees)
{
    modeB.rotatePerSecond = degrees;
}

float Particle_Activity::getRotatePerSecond() const
{
    return modeB.rotatePerSecond;
}

void Particle_Activity::setRotatePerSecondVar(float degrees)
{
    modeB.rotatePerSecondVar = degrees;
}

float Particle_Activity::getRotatePerSecondVar() const
{
    return modeB.rotatePerSecondVar;
}

bool Particle_Activity::isActive() const
{
    return _isActive;
}

int Particle_Activity::getTotalParticles() const
{
    return _totalParticles;
}

void Particle_Activity::setTotalParticles(int var)
{
    _totalParticles = var;
}

bool Particle_Activity::isAutoRemoveOnFinish() const
{
    return _isAutoRemoveOnFinish;
}

void Particle_Activity::setAutoRemoveOnFinish(bool var)
{
    _isAutoRemoveOnFinish = var;
}

////don't use a transform matrix, this is faster
//void Particle_Activity::setScale(float s)
//{
//    _transformSystemDirty = true;
//    Node::setScale(s);
//}
//
//void Particle_Activity::setRotation(float newRotation)
//{
//    _transformSystemDirty = true;
//    Node::setRotation(newRotation);
//}
//
//void Particle_Activity::setScaleX(float newScaleX)
//{
//    _transformSystemDirty = true;
//    Node::setScaleX(newScaleX);
//}
//
//void Particle_Activity::setScaleY(float newScaleY)
//{
//    _transformSystemDirty = true;
//    Node::setScaleY(newScaleY);
//}

bool Particle_Activity::isPaused() const
{
    return _paused;
}

void Particle_Activity::pauseEmissions()
{
    _paused = true;
}

void Particle_Activity::resumeEmissions()
{
    _paused = false;
}

void Particle_Activity::set_style_for_weather(const std::string &id_str , SDL_Renderer *renderer) {

    
    if (w_t_i_str == id_str) {
        return;
    }

    w_t_i_str = id_str;

    if ( w_t_i_str == "snowing" ) {
        

        setPosition(TERMX*fontwidth *0.35,0);

        setStartSpin(0);
        setStartSpinVar(90);
        setEndSpin(90);
        setStartSpinVar(90);

        initWithTotalParticles(700);

        // duration
        _duration = DURATION_INFINITY;

        // set gravity mode.
        setEmitterMode(Mode::GRAVITY);

        // Gravity Mode: gravity
        setGravity(Vec2(0, 1));

        // Gravity Mode: speed of particles
        setSpeed(-5);
        setSpeedVar(1);

        // Gravity Mode: radial
        setRadialAccel(0);
        setRadialAccelVar(1);

        // Gravity mode: tangential
        setTangentialAccel(0);
        setTangentialAccelVar(1);

        // angle
        _angle = -90;
        _angleVar = 5;

        // life of particles
        _life = 45;
        _lifeVar = 15;

        // size, in pixels
        _startSize = 10.0f;
        _startSizeVar = 5.0f;
        _endSize = START_SIZE_EQUAL_TO_END_SIZE;

        // emits per second
        _emissionRate = 10;

        // color of particles
        _startColor.r = 1.0f;
        _startColor.g = 1.0f;
        _startColor.b = 1.0f;
        _startColor.a = 1.0f;
        _startColorVar.r = 0.0f;
        _startColorVar.g = 0.0f;
        _startColorVar.b = 0.0f;
        _startColorVar.a = 0.0f;
        _endColor.r = 1.0f;
        _endColor.g = 1.0f;
        _endColor.b = 1.0f;
        _endColor.a = 0.0f;
        _endColorVar.r = 0.0f;
        _endColorVar.g = 0.0f;
        _endColorVar.b = 0.0f;
        _endColorVar.a = 0.0f;

        _posVar = { 1.0f * x_, 0.0f };


    }
    else if (w_t_i_str == "rain") {

        setPosition(TERMX * 0.35 * get_option<int>("FONT_WIDTH"), 0);              // set the position
#if defined(__ANDROID__)

        setPosition(WindowWidth * 0.38, 0);

#endif
        setStartSpin(0);
        setStartSpinVar(90);
        setEndSpin(90);
        setStartSpinVar(90);
        
        initWithTotalParticles(2000);

        // duration
        _duration = DURATION_INFINITY;

        setEmitterMode(Mode::GRAVITY);

        // Gravity Mode: gravity
        setGravity(Vec2(10, 10));

        // Gravity Mode: radial
        setRadialAccel(0);
        setRadialAccelVar(1);

        // Gravity Mode: tangential
        setTangentialAccel(0);
        setTangentialAccelVar(1);

        // Gravity Mode: speed of particles
        setSpeed(-130);
        setSpeedVar(30);

        // angle
        _angle = -90;
        _angleVar = 5;

        // life of particles
        _life = 4.5f;
        _lifeVar = 0;

        // size, in pixels
        _startSize = 6.0f;
        _startSizeVar = 2.0f;
        _endSize = START_SIZE_EQUAL_TO_END_SIZE;

        // emits per second
        _emissionRate = 20;

        // color of particles
        _startColor.r = 0.7f;
        _startColor.g = 0.8f;
        _startColor.b = 1.0f;
        _startColor.a = 1.0f;
        _startColorVar.r = 0.0f;
        _startColorVar.g = 0.0f;
        _startColorVar.b = 0.0f;
        _startColorVar.a = 0.0f;
        _endColor.r = 0.7f;
        _endColor.g = 0.8f;
        _endColor.b = 1.0f;
        _endColor.a = 0.5f;
        _endColorVar.r = 0.0f;
        _endColorVar.g = 0.0f;
        _endColorVar.b = 0.0f;
        _endColorVar.a = 0.0f;

        _posVar = { 1.0f * x_, 0.0f };

    
    }
    else if (w_t_i_str == "drizzle") {

        setPosition(TERMX * 0.35 * get_option<int>("FONT_WIDTH"), 0);              // set the position
#if defined(__ANDROID__)

        setPosition(WindowWidth * 0.38, 0);

#endif
        setStartSpin(0);
        setStartSpinVar(90);
        setEndSpin(90);
        setStartSpinVar(90);

        initWithTotalParticles(1000);

        // duration
        _duration = DURATION_INFINITY;

        setEmitterMode(Mode::GRAVITY);

        // Gravity Mode: gravity
        setGravity(Vec2(10, 10));

        // Gravity Mode: radial
        setRadialAccel(0);
        setRadialAccelVar(1);

        // Gravity Mode: tangential
        setTangentialAccel(0);
        setTangentialAccelVar(1);

        // Gravity Mode: speed of particles
        setSpeed(-130);
        setSpeedVar(30);

        // angle
        _angle = -90;
        _angleVar = 5;

        // life of particles
        _life = 4.5f;
        _lifeVar = 0;

        // size, in pixels
        _startSize = 4.0f;
        _startSizeVar = 2.0f;
        _endSize = START_SIZE_EQUAL_TO_END_SIZE;

        // emits per second
        _emissionRate = 20;

        // color of particles
        _startColor.r = 0.7f;
        _startColor.g = 0.8f;
        _startColor.b = 1.0f;
        _startColor.a = 1.0f;
        _startColorVar.r = 0.0f;
        _startColorVar.g = 0.0f;
        _startColorVar.b = 0.0f;
        _startColorVar.a = 0.0f;
        _endColor.r = 0.7f;
        _endColor.g = 0.8f;
        _endColor.b = 1.0f;
        _endColor.a = 0.5f;
        _endColorVar.r = 0.0f;
        _endColorVar.g = 0.0f;
        _endColorVar.b = 0.0f;
        _endColorVar.a = 0.0f;

        _posVar = { 1.0f * x_, 0.0f };
    
    
    
    
    }
    else if (w_t_i_str == "light_drizzle") {

        setPosition(TERMX * 0.35 * get_option<int>("FONT_WIDTH"), 0);              // set the position
#if defined(__ANDROID__)

        setPosition(WindowWidth * 0.38, 0);

#endif
        setStartSpin(0);
        setStartSpinVar(90);
        setEndSpin(90);
        setStartSpinVar(90);

        initWithTotalParticles(800);

        // duration
        _duration = DURATION_INFINITY;

        setEmitterMode(Mode::GRAVITY);

        // Gravity Mode: gravity
        setGravity(Vec2(10, 10));

        // Gravity Mode: radial
        setRadialAccel(0);
        setRadialAccelVar(1);

        // Gravity Mode: tangential
        setTangentialAccel(0);
        setTangentialAccelVar(1);

        // Gravity Mode: speed of particles
        setSpeed(-130);
        setSpeedVar(30);

        // angle
        _angle = -90;
        _angleVar = 5;

        // life of particles
        _life = 4.5f;
        _lifeVar = 0;

        // size, in pixels
        _startSize = 4.0f;
        _startSizeVar = 2.0f;
        _endSize = START_SIZE_EQUAL_TO_END_SIZE;

        // emits per second
        _emissionRate = 20;

        // color of particles
        _startColor.r = 0.7f;
        _startColor.g = 0.8f;
        _startColor.b = 1.0f;
        _startColor.a = 1.0f;
        _startColorVar.r = 0.0f;
        _startColorVar.g = 0.0f;
        _startColorVar.b = 0.0f;
        _startColorVar.a = 0.0f;
        _endColor.r = 0.7f;
        _endColor.g = 0.8f;
        _endColor.b = 1.0f;
        _endColor.a = 0.5f;
        _endColorVar.r = 0.0f;
        _endColorVar.g = 0.0f;
        _endColorVar.b = 0.0f;
        _endColorVar.a = 0.0f;

        _posVar = { 1.0f * x_, 0.0f };




        }


    else {
    
        //
        add_msg("测试信息");
        
    }




}


void Particle_Activity::set_style(const std::string& str) {
    
    if (style != str) {
        style = str;
    }
    else {
        return;
    }
    
    if (str=="effect_pet") {
    

        initWithTotalParticles(10);
        // duration
        _duration = DURATION_INFINITY;

        // Gravity Mode
        setEmitterMode(Mode::GRAVITY);

        // Gravity Mode: gravity
        setGravity(Vec2(0, 0));

        // Gravity Mode: speed of particles
        setSpeed(-60);
        setSpeedVar(10);

        // Gravity Mode: radial
        setRadialAccel(-80);
        setRadialAccelVar(0);

        // Gravity Mode: tangential
        setTangentialAccel(80);
        setTangentialAccelVar(0);

        // angle
        _angle = 90;
        _angleVar = 360;

        // life of particles
        _life = 4;
        _lifeVar = 1;

        // size, in pixels
        _startSize = 37.0f;
        _startSizeVar = 10.0f;
        _endSize = START_SIZE_EQUAL_TO_END_SIZE;

        // emits per second
        _emissionRate = _totalParticles / _life;

        // color of particles
        _startColor.r = 1.00f; 
        _startColor.g = 0.90f; 
        _startColor.b = 0.05f; 
        _startColor.a = 1.0f;
        _startColorVar.r = 0.0f;
        _startColorVar.g = 0.0f;
        _startColorVar.b = 0.0f;
        _startColorVar.a = 0.0f;
        _endColor.r = 0.90f; 
        _endColor.g = 0.85f; 
        _endColor.b = 0.60f;
        _endColor.a = 0.2f;   
        _endColorVar.r = 0.0f;
        _endColorVar.g = 0.0f;
        _endColorVar.b = 0.0f;
        _endColorVar.a = 0.0f;

        _posVar = { 0, 0 };

    }







}






