#pragma once
#ifndef CATA_SRC_CALENDAR_UI_H
#define CATA_SRC_CALENDAR_UI_H

#include <string>

#include "calendar.h"

namespace calendar_ui
{

enum class granularity : int {
    year,
    season,
    day,
    hour,
    minute,
    turn,
    last,
};

/**
 * Displays a UI element that allows selecting and returning a time point.
 */
time_point select_time_point( time_point initial_value, const std::string &title,
                              granularity granularity_level = granularity::turn );

} // namespace calendar_ui

#endif // CATA_SRC_CALENDAR_UI_H
