#include "network.h"
#include "options.h"
#include "filesystem.h"
#include "json.h"
#include "catacharset.h"
#include <fstream>
#include <chrono>
#include <curl/curl.h>
#include <map>
#include <vector>
#include <memory>
#include <sstream>
#include <iomanip>
#include "messages.h"

namespace
{

std::string json_escape( const std::string &value )
{
    std::string result;
    for( char c : value ) {
        switch( c ) {
            case '\"': result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\b': result += "\\b"; break;
            case '\f': result += "\\f"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default: result += c; break;
        }
    }
    return result;
}

std::string url_encode( const std::string &value )
{
    char *encoded = curl_easy_escape( nullptr, value.c_str(), static_cast<int>( value.length() ) );
    std::string result;
    result = encoded;
    curl_free( encoded );
    return result;
}

struct Request
{
    network::RequestId id;
    network::RequestStatus status;
    CURL *handle;
    curl_slist *headers;
    std::string response_body;
    std::string error_message;
    long http_code;
    bool is_post;
    std::string post_data;
    std::chrono::steady_clock::time_point start_time;
};

CURLM *g_multi_handle = nullptr;
network::RequestId g_next_id = 1;
std::map<network::RequestId, std::unique_ptr<Request>> g_requests;

size_t write_callback( void *contents, size_t size, size_t nmemb, std::string *s )
{
    size_t new_length = size * nmemb;
    try {
        s->append( static_cast<char *>( contents ), new_length );
    } catch( std::bad_alloc & ) {
        return 0;
    }
    return new_length;
}

curl_slist *build_curl_headers( const network::Headers &headers )
{
    curl_slist *list = nullptr;
    for( const auto &pair : headers ) {
        std::string header = pair.first + ": " + pair.second;
        list = curl_slist_append( list, header.c_str() );
    }
    return list;
}

Request *find_request( CURL *handle )
{
    for( auto &pair : g_requests ) {
        if( pair.second->handle == handle ) {
            return pair.second.get();
        }
    }
    return nullptr;
}

Request *find_request( network::RequestId id )
{
    auto it = g_requests.find( id );
    if( it != g_requests.end() ) {
        return it->second.get();
    }
    return nullptr;
}

} // anonymous namespace

namespace network
{

bool init()
{
    if( g_multi_handle ) {
        return true;
    }
    CURLcode res = curl_global_init( CURL_GLOBAL_DEFAULT );
    if( res != CURLE_OK ) {
        return false;
    }
    g_multi_handle = curl_multi_init();
    return g_multi_handle != nullptr;
}

void cleanup()
{
    for( auto &pair : g_requests ) {
        if( pair.second->handle ) {
            curl_multi_remove_handle( g_multi_handle, pair.second->handle );
            curl_easy_cleanup( pair.second->handle );
        }
        if( pair.second->headers ) {
            curl_slist_free_all( pair.second->headers );
        }
    }
    g_requests.clear();

    if( g_multi_handle ) {
        curl_multi_cleanup( g_multi_handle );
        g_multi_handle = nullptr;
    }
    curl_global_cleanup();
}

RequestId start_get( const std::string &url )
{
    return start_get( url, Headers{} );
}

RequestId start_get( const std::string &url, const Headers &headers )
{
    CURL *handle = curl_easy_init();
    if( !handle ) {
        return 0;
    }

    RequestId id = g_next_id++;
    auto req = std::make_unique<Request>();
    req->id = id;
    req->status = RequestStatus::Pending;
    req->handle = handle;
    req->http_code = 0;
    req->is_post = false;
    req->headers = build_curl_headers( headers );
    req->start_time = std::chrono::steady_clock::now();

    curl_easy_setopt( handle, CURLOPT_URL, url.c_str() );
    curl_easy_setopt( handle, CURLOPT_WRITEFUNCTION, write_callback );
    curl_easy_setopt( handle, CURLOPT_WRITEDATA, &req->response_body );
    curl_easy_setopt( handle, CURLOPT_FOLLOWLOCATION, 1L );
    curl_easy_setopt( handle, CURLOPT_SSL_VERIFYPEER, 0L );
    curl_easy_setopt( handle, CURLOPT_SSL_VERIFYHOST, 0L );
    if( req->headers ) {
        curl_easy_setopt( handle, CURLOPT_HTTPHEADER, req->headers );
    }
    curl_easy_setopt( handle, CURLOPT_PRIVATE, req.get() );

    curl_multi_add_handle( g_multi_handle, handle );
    req->status = RequestStatus::InProgress;

    g_requests[id] = std::move( req );
    return id;
}

RequestId start_post( const std::string &url, const std::string &data )
{
    return start_post( url, data, Headers{} );
}

RequestId start_post( const std::string &url, const std::string &data, const Headers &headers )
{
    CURL *handle = curl_easy_init();
    if( !handle ) {
        return 0;
    }

    RequestId id = g_next_id++;
    auto req = std::make_unique<Request>();
    req->id = id;
    req->status = RequestStatus::Pending;
    req->handle = handle;
    req->http_code = 0;
    req->is_post = true;
    req->post_data = data;
    req->headers = build_curl_headers( headers );
    req->start_time = std::chrono::steady_clock::now();

    curl_easy_setopt( handle, CURLOPT_URL, url.c_str() );
    curl_easy_setopt( handle, CURLOPT_POSTFIELDS, req->post_data.c_str() );
    curl_easy_setopt( handle, CURLOPT_POSTFIELDSIZE, req->post_data.size() );
    curl_easy_setopt( handle, CURLOPT_WRITEFUNCTION, write_callback );
    curl_easy_setopt( handle, CURLOPT_WRITEDATA, &req->response_body );
    curl_easy_setopt( handle, CURLOPT_FOLLOWLOCATION, 1L );
    curl_easy_setopt( handle, CURLOPT_SSL_VERIFYPEER, 0L );
    curl_easy_setopt( handle, CURLOPT_SSL_VERIFYHOST, 0L );
    if( req->headers ) {
        curl_easy_setopt( handle, CURLOPT_HTTPHEADER, req->headers );
    }
    curl_easy_setopt( handle, CURLOPT_PRIVATE, req.get() );

    curl_multi_add_handle( g_multi_handle, handle );
    req->status = RequestStatus::InProgress;

    g_requests[id] = std::move( req );
    return id;
}

void process()
{
    if( !g_multi_handle ) {
        return;
    }

    int running_handles = 0;
    CURLMcode mc;

    do {
        mc = curl_multi_perform( g_multi_handle, &running_handles );
    } while( mc == CURLM_CALL_MULTI_PERFORM );

    int msgs_left;
    CURLMsg *msg;
    while( ( msg = curl_multi_info_read( g_multi_handle, &msgs_left ) ) != nullptr ) {
        if( msg->msg == CURLMSG_DONE ) {
            CURL *handle = msg->easy_handle;
            Request *req = find_request( handle );
            if( req ) {
                if( msg->data.result == CURLE_OK ) {
                    curl_easy_getinfo( handle, CURLINFO_RESPONSE_CODE, &req->http_code );
                    req->status = RequestStatus::Completed;
                } else {
                    req->status = RequestStatus::Failed;
                    req->error_message = curl_easy_strerror( msg->data.result );
                    if( req->response_body.empty() ) {
                        req->response_body = "Error: " + req->error_message;
                    }
                }
                curl_multi_remove_handle( g_multi_handle, handle );
            }
        }
    }
    
    // 检查超时请求
    auto now = std::chrono::steady_clock::now();
    for( auto &pair : g_requests ) {
        Request *req = pair.second.get();
        if( req->status == RequestStatus::InProgress ) {
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>( now - req->start_time ).count();
            if( elapsed >= 40 ) {
                req->status = RequestStatus::Failed;
                req->error_message = "Request timed out after 40 seconds";
                req->response_body = "Error: Request timed out after 40 seconds";
                if( req->handle ) {
                    curl_multi_remove_handle( g_multi_handle, req->handle );
                }
            }
        }
    }
}

RequestStatus get_status( RequestId id )
{
    Request *req = find_request( id );
    if( req ) {
        return req->status;
    }
    return RequestStatus::Failed;
}

RequestResult get_result( RequestId id )
{
    RequestResult result;
    Request *req = find_request( id );
    if( req ) {
        result.status = req->status;
        result.http_code = req->http_code;
        result.response_body = req->response_body;
        result.error_message = req->error_message;
    } else {
        result.status = RequestStatus::Failed;
        result.error_message = "Request not found";
    }
    return result;
}

void cancel_request( RequestId id )
{
    auto it = g_requests.find( id );
    if( it != g_requests.end() ) {
        Request *req = it->second.get();
        if( req->handle ) {
            curl_multi_remove_handle( g_multi_handle, req->handle );
            curl_easy_cleanup( req->handle );
            req->handle = nullptr;
        }
        if( req->headers ) {
            curl_slist_free_all( req->headers );
            req->headers = nullptr;
        }
        req->status = RequestStatus::Failed;
    }
}

void clear_completed()
{
    for( auto it = g_requests.begin(); it != g_requests.end(); ) {
        Request *req = it->second.get();
        if( req->status == RequestStatus::Completed || req->status == RequestStatus::Failed ) {
            if( req->handle ) {
                curl_easy_cleanup( req->handle );
            }
            if( req->headers ) {
                curl_slist_free_all( req->headers );
            }
            it = g_requests.erase( it );
        } else {
            ++it;
        }
    }
}

std::vector<RequestId> get_all_requests()
{
    std::vector<RequestId> ids;
    for( const auto &pair : g_requests ) {
        ids.push_back( pair.first );
    }
    return ids;
}

RequestId start_pollinations_request( const std::string &system_prompt,const std::string& user_prompt )
{
    std::string api_key = get_option<std::string>( "密钥" );
    std::string model = get_option<std::string>( "文本模型名称" );
    std::string temperature = std::to_string(get_option<float>("温度"));
    
    Headers headers;
    headers["Content-Type"] = "application/json";
    headers["Accept"] = "application/json";
    if( !api_key.empty() ) {
        headers["Authorization"] = "Bearer " + api_key;
    }
    
    std::string json_body = "{\"messages\":[{\"content\":\"" + json_escape( system_prompt ) + 
        "\",\"role\":\"system\"},{\"role\":\"user\",\"content\":\""+json_escape(user_prompt)+"\"}], \"model\":\"" +
        json_escape(model.empty() ? "openai" : model ) + 
        "\",\"stream\":false,\"temperature\":"+temperature+"}";
    
    std::string url = "https://gen.pollinations.ai/v1/chat/completions";
    
    // std::ofstream log_file( "pollinations_request_debug.txt", std::ios::out | std::ios::trunc );
    // if( log_file.is_open() ) {
    //     log_file << "URL:\n";
    //     log_file << url << "\n";
    //     log_file << "\nHeaders:\n";
    //     for( const auto &header : headers ) {
    //         log_file << header.first << ": " << header.second << "\n";
    //     }
    //     log_file << "\nBody:\n";
    //     log_file << json_body << "\n";
    //     log_file << "\nSystem Prompt:\n";
    //     log_file << system_prompt << "\n";
    //     log_file << "\nModel:\n";
    //     log_file << model << "\n";
    //     log_file << "\nTemperature:\n";
    //     log_file << temperature << "\n";
    //     log_file.close();
    // }
    
    return start_post( url, json_body, headers );
}

std::string parse_pollinations_response( const std::string &json_response, bool include_token_usage )
{
    try {
        std::istringstream ss( json_response );
        TextJsonIn jsin( ss );
        TextJsonObject jo = jsin.get_object();
        jo.allow_omitted_members();

        if( !jo.has_member( "choices" ) ) {
            return "";
        }

        TextJsonArray choices = jo.get_array( "choices" );
        if( choices.empty() ) {
            return "";
        }

        TextJsonObject choice = choices.get_object( 0 );
        choice.allow_omitted_members();
        if( !choice.has_member( "message" ) ) {
            return "";
        }

        TextJsonObject message = choice.get_object( "message" );
        message.allow_omitted_members();
        if( !message.has_member( "content" ) ) {
            return "";
        }

        std::string content = message.get_string( "content" );

        if( include_token_usage && jo.has_member( "usage" ) ) {
            TextJsonObject usage = jo.get_object( "usage" );
            usage.allow_omitted_members();
            int prompt_tokens = 0;
            int completion_tokens = 0;
            int total_tokens = 0;

            if( usage.has_member( "prompt_tokens" ) ) {
                prompt_tokens = usage.get_int( "prompt_tokens" );
            }
            if( usage.has_member( "completion_tokens" ) ) {
                completion_tokens = usage.get_int( "completion_tokens" );
            }
            if( usage.has_member( "total_tokens" ) ) {
                total_tokens = usage.get_int( "total_tokens" );
            }

            content += "\n\nToken消耗情况:\n";
            content += "输入消耗：" + std::to_string( prompt_tokens ) + "\n";
            content += "输出消耗：" + std::to_string( completion_tokens ) + "\n";
            content += "总消耗  ：" + std::to_string( total_tokens );
        }

        return content;
    } catch( ... ) {
        return json_response;
    }
}

RequestId start_pollinations_image_request( const std::string &prompt )
{
    std::string api_key = get_option<std::string>( "密钥" );
    std::string model = get_option<std::string>( "生图模型名称" );

    Headers headers;
    headers["Content-Type"] = "application/json";
    headers["Accept"] = "application/json";
    if( !api_key.empty() ) {
        headers["Authorization"] = "Bearer " + api_key;
    }

    std::string json_body = "{\"prompt\":\"" + json_escape( prompt ) + "\",\"model\":\"" + json_escape( model.empty() ? "flux" : model ) + "\",\"size\":\"381x522\",\"n\":1,\"response_format\":\"b64_json\"}";

    std::string url = "https://gen.pollinations.ai/v1/images/generations";

    return start_post( url, json_body, headers );
}

static std::string standard_base64_decode( const std::string &encoded_string )
{
    static const std::string base64_chars =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789+/";

    auto is_base64 = []( unsigned char c ) {
        return ( isalnum( c ) || ( c == '+' ) || ( c == '/' ) );
    };

    int in_len = encoded_string.size();
    int i = 0;
    int j = 0;
    int in_ = 0;
    unsigned char char_array_4[4], char_array_3[3];
    std::string ret;

    while( in_len-- && ( encoded_string[in_] != '=' ) && is_base64( encoded_string[in_] ) ) {
        char_array_4[i++] = encoded_string[in_];
        in_++;
        if( i == 4 ) {
            for( i = 0; i < 4; i++ ) {
                char_array_4[i] = base64_chars.find( char_array_4[i] );
            }

            char_array_3[0] = ( char_array_4[0] << 2 ) + ( ( char_array_4[1] & 0x30 ) >> 4 );
            char_array_3[1] = ( ( char_array_4[1] & 0xF ) << 4 ) + ( ( char_array_4[2] & 0x3C ) >> 2 );
            char_array_3[2] = ( ( char_array_4[2] & 0x3 ) << 6 ) + char_array_4[3];

            for( i = 0; ( i < 3 ); i++ ) {
                ret += char_array_3[i];
            }
            i = 0;
        }
    }

    if( i ) {
        for( j = i; j < 4; j++ ) {
            char_array_4[j] = 0;
        }

        for( j = 0; j < 4; j++ ) {
            char_array_4[j] = base64_chars.find( char_array_4[j] );
        }

        char_array_3[0] = ( char_array_4[0] << 2 ) + ( ( char_array_4[1] & 0x30 ) >> 4 );
        char_array_3[1] = ( ( char_array_4[1] & 0xF ) << 4 ) + ( ( char_array_4[2] & 0x3C ) >> 2 );
        char_array_3[2] = ( ( char_array_4[2] & 0x3 ) << 6 ) + char_array_4[3];

        for( j = 0; ( j < i - 1 ); j++ ) {
            ret += char_array_3[j];
        }
    }

    return ret;
}

bool parse_pollinations_image_response( const std::string &json_response, const std::string &save_path )
{
    try {
        // 调试日志：保存原始响应到文件
        {
            std::ofstream debug_log( "debug_image_response.txt", std::ios::out | std::ios::trunc );
            if( debug_log.is_open() ) {
                debug_log << "=== 图片生成响应 ===\n";
                debug_log << "保存路径: " << save_path << "\n";
                debug_log << "响应长度: " << json_response.size() << " 字节\n";
                debug_log << "响应内容:\n" << json_response << "\n";
                debug_log.close();
            }
        }

        std::istringstream ss( json_response );
        TextJsonIn jsin( ss );
        TextJsonObject jo = jsin.get_object();
        jo.allow_omitted_members();

        if( !jo.has_member( "data" ) ) {
            return false;
        }

        TextJsonArray data = jo.get_array( "data" );
        if( data.empty() ) {
            return false;
        }

        TextJsonObject image_data = data.get_object( 0 );
        image_data.allow_omitted_members();
        if( image_data.has_member( "b64_json" ) ) {
            std::string b64_data = image_data.get_string( "b64_json" );
            std::string decoded_data = standard_base64_decode( b64_data );

            // 调试日志
            {
                std::ofstream debug_log( "debug_image_response.txt", std::ios::out | std::ios::app );
                if( debug_log.is_open() ) {
                    debug_log << "\n=== 解码信息 ===\n";
                    debug_log << "Base64数据长度: " << b64_data.size() << " 字节\n";
                    debug_log << "解码后数据长度: " << decoded_data.size() << " 字节\n";
                    debug_log.close();
                }
            }

            std::ofstream out_file( save_path, std::ios::binary );
            if( out_file.is_open() ) {
                out_file.write( decoded_data.c_str(), decoded_data.size() );
                out_file.close();
                
                // 验证文件是否成功写入
                std::ifstream check_file( save_path, std::ios::binary | std::ios::ate );
                if( check_file.is_open() ) {
                    std::streamsize file_size = check_file.tellg();
                    {
                        std::ofstream debug_log( "debug_image_response.txt", std::ios::out | std::ios::app );
                        if( debug_log.is_open() ) {
                            debug_log << "文件写入成功: " << save_path << "\n";
                            debug_log << "文件大小: " << file_size << " 字节\n";
                            debug_log.close();
                        }
                    }
                    check_file.close();
                }
                return true;
            } else {
                // 调试日志：无法打开文件
                {
                    std::ofstream debug_log( "debug_image_response.txt", std::ios::out | std::ios::app );
                    if( debug_log.is_open() ) {
                        debug_log << "错误: 无法打开文件进行写入: " << save_path << "\n";
                        debug_log.close();
                    }
                }
            }
        }

        return false;
    } catch( const std::exception &e ) {
        // 调试日志：异常
        {
            std::ofstream debug_log( "debug_image_response.txt", std::ios::out | std::ios::app );
            if( debug_log.is_open() ) {
                debug_log << "异常错误: " << e.what() << "\n";
                debug_log.close();
            }
        }
        return false;
    } catch( ... ) {
        // 调试日志：未知异常
        {
            std::ofstream debug_log( "debug_image_response.txt", std::ios::out | std::ios::app );
            if( debug_log.is_open() ) {
                debug_log << "未知异常错误\n";
                debug_log.close();
            }
        }
        return false;
    }
}

} // namespace network
