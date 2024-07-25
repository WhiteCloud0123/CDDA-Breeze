#include "generic_factory.h"
#include "text_style_check.h"

text_style_check_reader::text_style_check_reader( const allow_object object_allowed )
    : object_allowed( object_allowed )
{
}

std::string text_style_check_reader::get_next( const JsonValue &jv ) const
{
    const JsonValue &jsin = jv;
    std::string raw;
    if( object_allowed == allow_object::yes && jsin.test_object() ) {
        JsonObject jsobj = jsin.get_object();
        raw = jsobj.get_string( "str" );
        if( jsobj.has_member( "//NOLINT(cata-text-style)" ) ) {
        }
    } else {
        raw = jsin.get_string();
    }
    return raw;
}
