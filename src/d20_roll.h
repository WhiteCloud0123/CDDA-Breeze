#pragma once
#ifndef CATA_SRC_D20_ROLL_H
#define CATA_SRC_D20_ROLL_H

#include <algorithm>
#include <string>
#include <vector>

#include "rng.h"

class Character;
class Creature;

/**
 * 二十面骰的投掷状态。
 * 优势投两次取较高的一次，劣势投两次取较低的一次。
 * 摔投/抓取与弓箭隐蔽射击共用这套状态与投骰实现。
 */
enum class d20_roll_state {
    normal,
    advantage,
    disadvantage
};

// 普通投一次 d20；优势取两次中较高的一次，劣势取较低的一次。
inline int roll_d20( const d20_roll_state state )
{
    const int first = rng( 1, 20 );
    if( state == d20_roll_state::normal ) {
        return first;
    }
    const int second = rng( 1, 20 );
    return state == d20_roll_state::advantage ? std::max( first, second ) :
           std::min( first, second );
}

// 依据攻击者与目标的状态判定本次投骰是普通、优势还是劣势（定义见 creature.cpp）。
// unseen_bonus 表示额外的一个优势来源（例如弓手未被目标发现）。
// 优势与劣势都只按“有/无”计：多个优势不会叠加，任意一个劣势即可抵消全部优势，
// 优势与劣势同时存在时互相抵消为普通。
d20_roll_state get_d20_roll_state( const Character &you, const Creature &target,
                                   bool unseen_bonus = false );

// 优势与劣势的来源描述，供瞄准界面显示（定义见 creature.cpp）。
std::vector<std::string> get_roll_advantage_reasons( const Character &you, const Creature &target,
        bool unseen_bonus );
std::vector<std::string> get_roll_disadvantage_reasons( const Character &you );

#endif // CATA_SRC_D20_ROLL_H
