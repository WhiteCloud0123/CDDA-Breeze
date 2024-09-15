/* Entry point and main loop for Cataclysm
 */

// IWYU pragma: no_include <sys/signal.h>
#include <clocale>
#include <algorithm>
#include <array>
#include <clocale>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <exception>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#if defined(_WIN32)
#include "platform_win.h"
#else
#include <csignal>
#endif

#include <flatbuffers/util.h>

#include "cached_options.h"
#include "cata_path.h"
#include "color.h"
#include "compatibility.h"
#include "crash.h"
#include "cursesdef.h"
#include "debug.h"
#include "do_turn.h"
#include "filesystem.h"
#include "game.h"
#include "game_constants.h"
#include "game_ui.h"
#include "get_version.h"
#include "input.h"
#include "loading_ui.h"
#include "main_menu.h"
#include "mapsharing.h"
#include "memory_fast.h"
#include "options.h"
#include "output.h"
#include "help.h"
#include "ordered_static_globals.h"
#include "path_info.h"
#include "rng.h"
#include "sdltiles.h"
#include "system_locale.h"
#include "translations.h"
#include "type_id.h"
#include "ui_manager.h"

#if defined(PREFIX)
#   undef PREFIX
#   include "prefix.h"
#endif

#if defined(__ANDROID__)
#include <jni.h>
#endif

class ui_adaptor;


#if defined(_MSC_VER) && defined(USE_VCPKG)
#include <SDL2/SDL_version.h>
#else
#include <SDL_version.h>
#endif


#if defined(__ANDROID__)
#include <SDL_filesystem.h>
#include <SDL_keyboard.h>
#include <SDL_system.h>
#include <android/log.h>
#include <unistd.h>

// Taken from: https://codelab.wordpress.com/2014/11/03/how-to-use-standard-output-streams-for-logging-in-android-apps/
// Force Android standard output to adb logcat output

static int pfd[2];
static pthread_t thr;
static const char *tag = "cdda";

static void *thread_func( void * )
{
    ssize_t rdsz;
    char buf[128];
    for( ;; ) {
        if( ( ( rdsz = read( pfd[0], buf, sizeof buf - 1 ) ) > 0 ) ) {
            if( buf[rdsz - 1] == '\n' ) {
                --rdsz;
            }
            buf[rdsz] = 0;  /* add null-terminator */
            __android_log_write( ANDROID_LOG_DEBUG, tag, buf );
        }
    }
    return 0;
}

int start_logger( const char *app_name )
{
    tag = app_name;

    /* make stdout line-buffered and stderr unbuffered */
    setvbuf( stdout, 0, _IOLBF, 0 );
    setvbuf( stderr, 0, _IONBF, 0 );

    /* create the pipe and redirect stdout and stderr */
    pipe( pfd );
    dup2( pfd[1], 1 );
    dup2( pfd[1], 2 );

    /* spawn the logging thread */
    if( pthread_create( &thr, 0, thread_func, 0 ) == -1 ) {
        return -1;
    }
    pthread_detach( thr );
    return 0;
}

#endif //__ANDROID__

namespace
{

#if defined(_WIN32)
// Used only if AttachConsole() works
FILE *CONOUT;
#endif
void exit_handler( int s )
{
    const int old_timeout = inp_mngr.get_timeout();
    inp_mngr.reset_timeout();
    if( s != 2 || query_yn( _( "Really Quit?  All unsaved changes will be lost." ) ) ) {
        deinitDebug();

        int exit_status = 0;
        g.reset();

        catacurses::endwin();

#if defined(__ANDROID__)
        // Avoid capturing SIGABRT on exit on Android in crash report
        // Can be removed once the SIGABRT on exit problem is fixed
        signal( SIGABRT, SIG_DFL );
#endif

        exit( exit_status );
    }
    inp_mngr.set_timeout( old_timeout );
    ui_manager::redraw_invalidated();
    catacurses::doupdate();
}

struct arg_handler {
    //! Handler function to be invoked when this argument is encountered. The handler will be
    //! called with the number of parameters after the flag was encountered, along with the array
    //! of following parameters. It must return an integer indicating how many parameters were
    //! consumed by the call or -1 to indicate that a required argument was missing.
    using handler_method = std::function<int ( int, const char ** )>;

    const char *flag;  //!< The commandline parameter to handle (e.g., "--seed").
    const char *param_documentation;  //!< Human readable description of this arguments parameter.
    const char *documentation;  //!< Human readable documentation for this argument.
    const char *help_group; //!< Section of the help message in which to include this argument.
    int num_args; //!< How many further arguments are expected for this parameter (usually 0 or 1).
    handler_method handler;  //!< The callback to be invoked when this argument is encountered.
};

bool assure_essential_dirs_exist()
{
    using namespace PATH_INFO;
    std::vector<std::string> essential_paths{
        config_dir(),
        savedir(),
        templatedir(),
        user_font(),
        user_sound().get_unrelative_path().u8string(),
        user_gfx().get_unrelative_path().u8string()
    };
    for( const std::string &path : essential_paths ) {
        if( !assure_dir_exist( path ) ) {
            popup( _( "Unable to make directory \"%s\".  Check permissions." ), path );
            return false;
        }
    }
    return true;
}

}  // namespace

#if defined(USE_WINMAIN)
int APIENTRY WinMain( _In_ HINSTANCE /* hInstance */, _In_opt_ HINSTANCE /* hPrevInstance */,
                      _In_ LPSTR /* lpCmdLine */, _In_ int /* nCmdShow */ )
{
    int argc = __argc;
    char **argv = __argv;
#elif defined(__ANDROID__)
extern "C" int SDL_main( int argc, char **argv ) {
#else
int main( int argc, const char *argv[] )
{
#endif
    ordered_static_globals();
    init_crash_handlers();
    reset_floating_point_mode();
    flatbuffers::ClassicLocale::Get();

#if defined(_WIN32) and defined(TILES)
    const HANDLE std_output { GetStdHandle( STD_OUTPUT_HANDLE ) }, std_error { GetStdHandle( STD_ERROR_HANDLE ) };
    if( std_output != INVALID_HANDLE_VALUE and std_error != INVALID_HANDLE_VALUE ) {
        if( AttachConsole( ATTACH_PARENT_PROCESS ) ) {
            if( std_output == nullptr ) {
                freopen_s( &CONOUT, "CONOUT$", "w", stdout );
            }
            if( std_error == nullptr ) {
                freopen_s( &CONOUT, "CONOUT$", "w", stderr );
            }
        }
    }
#endif
#if defined(__ANDROID__)

    // Start the standard output logging redirector
    start_logger( "cdda" );

    // On Android first launch, we copy all data files from the APK into the app's writeable folder so std::io stuff works.
    // Use the external storage so it's publicly modifiable data (so users can mess with installed data, save games etc.)
    std::string external_storage_path( SDL_AndroidGetExternalStoragePath() );
    if( external_storage_path.back() != '/' ) {
        external_storage_path += '/';
    }

    PATH_INFO::init_base_path( external_storage_path );
#else
    // Set default file paths
#if defined(PREFIX)
    PATH_INFO::init_base_path( std::string( PREFIX ) );
#else
    PATH_INFO::init_base_path( "" );
#endif
#endif

#if defined(__ANDROID__)
    PATH_INFO::init_user_dir( external_storage_path );
#else
#   if defined(USE_HOME_DIR) || defined(USE_XDG_DIR)
    PATH_INFO::init_user_dir( "" );
#   else
    PATH_INFO::init_user_dir( "./" );
#   endif
#endif
    PATH_INFO::set_standard_filenames();

    MAP_SHARING::setDefaults();

    if( !dir_exist( PATH_INFO::datadir() ) ) {
        printf( "Fatal: Can't find data directory \"%s\"\nPlease ensure the current working directory is correct or specify data directory with --datadir.  Perhaps you meant to start \"cataclysm-launcher\"?\n",
                PATH_INFO::datadir().c_str() );
        exit( 1 );
    }

    if( !assure_dir_exist( PATH_INFO::user_dir() ) ) {
        printf( "Can't open or create %s. Check permissions.\n",
                PATH_INFO::user_dir().c_str() );
        exit( 1 );
    }

    setupDebug( DebugOutput::file );

    /**
     * OS X does not populate locale env vars correctly (they usually default to
     * "C") so don't bother trying to set the locale based on them.
     */
#if !defined(MACOSX)
    if( setlocale( LC_ALL, "" ) == nullptr ) {
        DebugLog( D_WARNING, D_MAIN ) << "Error while setlocale(LC_ALL, '').";
    } else {
#endif
        try {
            std::locale::global( std::locale( "" ) );
        } catch( const std::exception & ) {
            // if user default locale retrieval isn't implemented by system
            try {
                // default to basic C locale
                std::locale::global( std::locale::classic() );
            } catch( const std::exception &err ) {
                debugmsg( "%s", err.what() );
                exit_handler( -999 );
            }
        }
#if !defined(MACOSX)
    }
#endif

    DebugLog( D_INFO, DC_ALL ) << "[main] C locale set to " << setlocale( LC_ALL, nullptr );
    DebugLog( D_INFO, DC_ALL ) << "[main] C++ locale set to " << std::locale().name();


    SDL_version compiled;
    SDL_VERSION( &compiled );
    DebugLog( D_INFO, DC_ALL ) << "SDL version used during compile is "
                               << static_cast<int>( compiled.major ) << "."
                               << static_cast<int>( compiled.minor ) << "."
                               << static_cast<int>( compiled.patch );

    SDL_version linked;
    SDL_GetVersion( &linked );
    DebugLog( D_INFO, DC_ALL ) << "SDL version used during linking and in runtime is "
                               << static_cast<int>( linked.major ) << "."
                               << static_cast<int>( linked.minor ) << "."
                               << static_cast<int>( linked.patch );


#if defined(__ANDROID__)

    jni_env = (JNIEnv*)SDL_AndroidGetJNIEnv();

    jobject temp_activity = (jobject)SDL_AndroidGetActivity();
    j_activity = jni_env->NewGlobalRef(temp_activity);

    jclass temp_class(jni_env->GetObjectClass(j_activity));
    j_class = (jclass)jni_env->NewGlobalRef(temp_class);

    method_id_setExtraButtonVisibility = jni_env->GetMethodID(j_class, "setExtraButtonVisibility", "(Z)V");
    method_id_addExtraButton = jni_env->GetMethodID(j_class, "addExtraButton", "(Ljava/lang/String;)V");
    method_id_getDisplayDensity = jni_env->GetMethodID(j_class, "getDisplayDensity", "()F");
    method_id_isHardwareKeyboardAvailable = jni_env->GetMethodID(j_class, "isHardwareKeyboardAvailable", "()Z");
    method_id_vibrate = jni_env->GetMethodID(j_class, "vibrate", "(I)V");
    method_id_show_sdl_surface = jni_env->GetMethodID(j_class, "show_sdl_surface", "()V");
    method_id_toast = jni_env->GetMethodID(j_class,"toast","(Ljava/lang/String;)V");
    method_id_showToastMessage = jni_env->GetMethodID(j_class, "showToastMessage", "(Ljava/lang/String;)V");
    method_id_getDefaultSetting = jni_env->GetMethodID(j_class, "getDefaultSetting", "(Ljava/lang/String;Z)Z");
    method_id_getSystemLang = jni_env->GetMethodID(j_class, "getSystemLang", "()Ljava/lang/String;");
    method_id_set_force_full_screen = jni_env->GetMethodID(j_class, "set_force_full_screen", "(Z)V");
    method_id_set_hide_status_bar = jni_env->GetMethodID(j_class, "set_hide_status_bar", "(Z)V");
    jni_env->DeleteLocalRef(temp_activity);
    jni_env->DeleteLocalRef(temp_class);


#endif

    particle_system_weather.init_weather_content();


    try {
            // set minimum FULL_SCREEN sizes
        FULL_SCREEN_WIDTH = EVEN_MINIMUM_TERM_WIDTH;
        FULL_SCREEN_HEIGHT = EVEN_MINIMUM_TERM_HEIGHT;
        catacurses::init_interface();
    } catch( const std::exception &err ) {
        // can't use any curses function as it has not been initialized
        std::cerr << "Error while initializing the interface: " << err.what() << std::endl;
        DebugLog( D_ERROR, DC_ALL ) << "Error while initializing the interface: " << err.what() << "\n";
        return 1;
    }
    
    set_language();


    game_ui::init_ui();

    g = std::make_unique<game>();

    // First load and initialize everything that does not
    // depend on the mods.
    try {
        g->load_static_data();
    } catch( const std::exception &err ) {
        debugmsg( "%s", err.what() );
        exit_handler( -999 );
    }


    // Now we do the actual game.

    // I have no clue what this comment is on about
    // Any value works well enough for debugging at least
    catacurses::curs_set( 0 ); // Invisible cursor here, because MAPBUFFER.load() is crash-prone

#if !defined(_WIN32)
    struct sigaction sigIntHandler;
    sigIntHandler.sa_handler = exit_handler;
    sigemptyset( &sigIntHandler.sa_mask );
    sigIntHandler.sa_flags = 0;
    sigaction( SIGINT, &sigIntHandler, nullptr );
#endif

#if defined(LOCALIZE)
    if( get_option<std::string>( "USE_LANG" ).empty() && !SystemLocale::Language().has_value() ) {
        select_language();
        set_language();
    }
#endif
    replay_buffered_debugmsg_prompts();

    if( !assure_essential_dirs_exist() ) {
        exit_handler( -999 );
        return 0;
    }


    get_help().load();


#if defined(__ANDROID__)

    jni_env->CallVoidMethod(j_activity, method_id_set_hide_status_bar, ::get_option<bool>("非全屏模式下隐藏状态栏"));
    jni_env->CallVoidMethod(j_activity, method_id_set_force_full_screen, ::get_option<bool>("强制全屏"));
    jstring extra_button_str = jni_env->NewStringUTF(get_option<std::string>("默认扩展按键").c_str());
    jni_env->CallVoidMethod(j_activity, method_id_addExtraButton, extra_button_str);
    jni_env->CallVoidMethod(j_activity, method_id_setExtraButtonVisibility, ::get_option<bool>("启用扩展按键"));
    jni_env->DeleteLocalRef(extra_button_str);

#endif

    while( true ) {
        main_menu menu;
        if( !menu.opening_screen() ) {
            break;
        }

        shared_ptr_fast<ui_adaptor> ui = g->create_or_get_main_ui_adaptor();
        while( !do_turn() );
    }

    exit_handler( -999 );
    return 0;
}
