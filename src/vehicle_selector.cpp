#include "vehicle_selector.h"

#include "map.h"
#include <optional>
#include "point.h"
#include "vpart_position.h"

vehicle_selector::vehicle_selector( const tripoint &pos, int radius, bool accessible,
                                    bool visibility_only )
{
    map &here = get_map();
    for( const tripoint &e : closest_points_first( pos, radius ) ) {
        if( !accessible ||
            ( visibility_only ? here.sees( pos, e, radius ) : here.clear_path( pos, e, radius, 1, 100 ) ) ) {
            if( const optional_vpart_position vp = here.veh_at( e ) ) {
                data.emplace_back( vp->vehicle(), vp->part_index() );
            }
        }
    }
}

vehicle_selector::vehicle_selector( const tripoint &pos, int radius, bool accessible,
                                    const vehicle &ignore )
{
    map &here = get_map();
    for( const tripoint &e : closest_points_first( pos, radius ) ) {
        if( !accessible || here.clear_path( pos, e, radius, 1, 100 ) ) {
            if( const optional_vpart_position vp = here.veh_at( e ) ) {
                data.emplace_back( vp->vehicle(), vp->part_index(), &vp->vehicle() == &ignore );
            }
        }
    }
}
