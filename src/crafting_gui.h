#pragma once
#ifndef CATA_SRC_CRAFTING_GUI_H
#define CATA_SRC_CRAFTING_GUI_H

#include <iosfwd>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "point.h"
#include "type_id.h"

class Character;
class JsonObject;
class recipe;

struct crafting_selection {
    Character *crafter = nullptr;
    const recipe *rec = nullptr;
    std::optional<tripoint> workplace;
};

/**
 * Open the crafting menu and choose the recipe, nearby allied crafter and
 * workplace.  A null workplace keeps the original automatic behavior.
 */
crafting_selection select_crafter_and_crafting_recipe(
    int &batch_size_out, const recipe_id &goto_recipe, Character &initial_crafter,
    const std::optional<tripoint> &initial_workplace );

void load_recipe_category( const JsonObject &jsobj );
void reset_recipe_categories();

// Returns nullptr if the category does not exist, or a pointer to its vector
// of subcategories it the category does exist
const std::vector<std::string> *subcategories_for_category( const std::string &category );

#endif // CATA_SRC_CRAFTING_GUI_H
