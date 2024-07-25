#include "translation.h"

#include <regex>

#include "cached_options.h"
#include "cata_utility.h"
#include "debug.h"
#include "generic_factory.h"
#include "json.h"
#include "localized_comparator.h"

translation::translation( const plural_tag ) : raw_pl( cata::make_value<std::string>() ) {}

translation::translation( const std::string &ctxt, const std::string &raw )
    : ctxt( cata::make_value<std::string>( ctxt ) ), raw( raw ), needs_translation( true )
{
}

translation::translation( const std::string &raw )
    : raw( raw ), needs_translation( true )
{
}

translation::translation( const std::string &raw, const std::string &raw_pl,
                          const plural_tag )
    : raw( raw ), raw_pl( cata::make_value<std::string>( raw_pl ) ), needs_translation( true )
{
}

translation::translation( const std::string &ctxt, const std::string &raw,
                          const std::string &raw_pl, const plural_tag )
    : ctxt( cata::make_value<std::string>( ctxt ) ),
      raw( raw ), raw_pl( cata::make_value<std::string>( raw_pl ) ), needs_translation( true )
{
}

translation::translation( const std::string &str, const no_translation_tag ) : raw( str ) {}

translation translation::to_translation( const std::string &raw )
{
    return translation{ raw };
}

translation translation::to_translation( const std::string &ctxt, const std::string &raw )
{
    return { ctxt, raw };
}

translation translation::pl_translation( const std::string &raw, const std::string &raw_pl )
{
    return { raw, raw_pl, plural_tag() };
}

translation translation::pl_translation( const std::string &ctxt, const std::string &raw,
        const std::string &raw_pl )
{
    return { ctxt, raw, raw_pl, plural_tag() };
}

translation translation::no_translation( const std::string &str )
{
    return { str, no_translation_tag() };
}

void translation::make_plural()
{
    if( needs_translation ) {
        // if plural form has not been enabled yet
        if( !raw_pl ) {
            // copy the singular string without appending "s" to preserve the original behavior
            raw_pl = cata::make_value<std::string>( raw );
        }
    } else if( !raw_pl ) {
        // just mark plural form as enabled
        raw_pl = cata::make_value<std::string>();
    }
    // reset the cache
    cached_language_version = INVALID_LANGUAGE_VERSION;
    cached_translation = nullptr;
}

// return { true, suggested plural } if no irregular form is detected,
// { false, suggested plural } otherwise. do have false positive/negatives.
static std::pair<bool, std::string> possible_plural_of( const std::string &raw )
{
    const std::string plural = raw + "s";
    return { true, plural };
}

void translation::deserialize( const JsonValue &jsin )
{
    // reset the cache
    cached_language_version = INVALID_LANGUAGE_VERSION;
    cached_num = 0;
    cached_translation = nullptr;

    if( jsin.test_string() ) {
        ctxt = nullptr;
        // strings with plural forms are currently only simple names, and
        // need no text style check.
        raw = jsin.get_string();
        // if plural form is enabled
        if( raw_pl ) {
            const std::pair<bool, std::string> suggested_pl = possible_plural_of( raw );
            raw_pl = cata::make_value<std::string>( suggested_pl.second );
        }
        needs_translation = true;
    } else {
        JsonObject jsobj = jsin.get_object();
        if( jsobj.has_member( "ctxt" ) ) {
            ctxt = cata::make_value<std::string>( jsobj.get_string( "ctxt" ) );
        } else {
            ctxt = nullptr;
        }
        if( jsobj.has_member( "str_sp" ) ) {
            // same singular and plural forms
            // strings with plural forms are currently only simple names, and
            // need no text style check.
            raw = jsobj.get_string( "str_sp" );
            // if plural form is enabled
            if( raw_pl ) {
                raw_pl = cata::make_value<std::string>( raw );
            } else {
                try {
                    jsobj.throw_error_at( "str_sp", "str_sp not supported here" );
                } catch( const JsonError &e ) {
                    debugmsg( "(json-error)\n%s", e.what() );
                }
            }
        } else {
            // strings with plural forms are currently only simple names, and
            // need no text style check.
            raw = jsobj.get_string( "str" );            
            // if plural form is enabled
            if( raw_pl ) {
                if( jsobj.has_member( "str_pl" ) ) {
                    raw_pl = cata::make_value<std::string>( jsobj.get_string( "str_pl" ) );
                } else {
                    const std::pair<bool, std::string> suggested_pl = possible_plural_of( raw );
                    raw_pl = cata::make_value<std::string>( suggested_pl.second );
                }
            } else if( jsobj.has_member( "str_pl" ) ) {
                try {
                    jsobj.throw_error_at( "str_pl", "str_pl not supported here" );
                } catch( const JsonError &e ) {
                    debugmsg( "(json-error)\n%s", e.what() );
                }
            }
        }
        needs_translation = true;
    }

    // Reset the underlying jsonin stream because errors leave it in an undefined state.
    // This will be removed once everything is migrated off JsonIn.
    if( jsin.test_string() ) {
        jsin.get_string();

    } else if( jsin.test_object() ) {
        jsin.get_object().allow_omitted_members();
    }
}

std::string translation::translated( const int num ) const
{
    if( !needs_translation || raw.empty() ) {
        return raw;
    }
    // Note1: `raw`, `raw_pl` and `ctxt` are effectively immutable for caching purposes:
    // in the places where they are changed, cache is explicitly invalidated
    // Note2: if `raw_pl` is defined, `num` becomes part of the "cache key"
    // otherwise `num` is ignored (for both translation and cache)
    if( cached_language_version != detail::get_current_language_version() ||
        ( raw_pl && cached_num != num ) || !cached_translation ) {
        cached_language_version = detail::get_current_language_version();
        cached_num = num;

        if( !ctxt ) {
            if( !raw_pl ) {
                cached_translation = cata::make_value<std::string>( detail::_translate_internal( raw ) );
            } else {
                cached_translation = cata::make_value<std::string>(
                                         n_gettext( raw.c_str(), raw_pl->c_str(), num ) );
            }
        } else {
            if( !raw_pl ) {
                cached_translation = cata::make_value<std::string>( pgettext( ctxt->c_str(), raw.c_str() ) );
            } else {
                cached_translation = cata::make_value<std::string>(
                                         npgettext( ctxt->c_str(), raw.c_str(), raw_pl->c_str(), num ) );
            }
        }
    }
    return *cached_translation;
}

bool translation::empty() const
{
    return raw.empty();
}

bool translation::translated_lt( const translation &that ) const
{
    return localized_compare( translated(), that.translated() );
}

bool translation::translated_gt( const translation &that ) const
{
    return that.translated_lt( *this );
}

bool translation::translated_le( const translation &that ) const
{
    return !that.translated_lt( *this );
}

bool translation::translated_ge( const translation &that ) const
{
    return !translated_lt( that );
}

bool translation::translated_eq( const translation &that ) const
{
    return translated() == that.translated();
}

bool translation::translated_ne( const translation &that ) const
{
    return !translated_eq( that );
}

bool translated_less::operator()( const translation &lhs,
                                  const translation &rhs ) const
{
    return lhs.translated_lt( rhs );
}

bool translated_greater::operator()( const translation &lhs,
                                     const translation &rhs ) const
{
    return lhs.translated_gt( rhs );
}

bool translated_less_equal::operator()( const translation &lhs,
                                        const translation &rhs ) const
{
    return lhs.translated_le( rhs );
}

bool translated_greater_equal::operator()( const translation &lhs,
        const translation &rhs ) const
{
    return lhs.translated_ge( rhs );
}

bool translated_equal_to::operator()( const translation &lhs,
                                      const translation &rhs ) const
{
    return lhs.translated_eq( rhs );
}

bool translated_not_equal_to::operator()( const translation &lhs,
        const translation &rhs ) const
{
    return lhs.translated_ne( rhs );
}

bool translation::operator==( const translation &that ) const
{
    return value_ptr_equals( ctxt, that.ctxt ) && raw == that.raw &&
           value_ptr_equals( raw_pl, that.raw_pl ) &&
           needs_translation == that.needs_translation;
}

bool translation::operator!=( const translation &that ) const
{
    return !operator==( that );
}

cata::optional<int> translation::legacy_hash() const
{
    if( needs_translation && !ctxt && !raw_pl ) {
        return djb2_hash( reinterpret_cast<const unsigned char *>( raw.c_str() ) );
    }
    // Otherwise the translation must have been added after snippets were changed
    // to use string ids only, so the translation doesn't have a legacy hash value.
    return cata::nullopt;
}

translation to_translation( const std::string &raw )
{
    return translation::to_translation( raw );
}

translation to_translation( const std::string &ctxt, const std::string &raw )
{
    return translation::to_translation( ctxt, raw );
}

translation pl_translation( const std::string &raw, const std::string &raw_pl )
{
    return translation::pl_translation( raw, raw_pl );
}

translation pl_translation( const std::string &ctxt, const std::string &raw,
                            const std::string &raw_pl )
{
    return translation::pl_translation( ctxt, raw, raw_pl );
}

translation no_translation( const std::string &str )
{
    return translation::no_translation( str );
}

std::ostream &operator<<( std::ostream &out, const translation &t )
{
    return out << t.translated();
}

std::string operator+( const translation &lhs, const std::string &rhs )
{
    return lhs.translated() + rhs;
}

std::string operator+( const std::string &lhs, const translation &rhs )
{
    return lhs + rhs.translated();
}

std::string operator+( const translation &lhs, const translation &rhs )
{
    return lhs.translated() + rhs.translated();
}
