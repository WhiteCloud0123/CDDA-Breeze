#pragma once
#ifndef CATA_SRC_NETWORK_H
#define CATA_SRC_NETWORK_H

#include <string>
#include <functional>
#include <vector>
#include <map>

namespace network
{

enum class RequestStatus
{
    Pending,
    InProgress,
    Completed,
    Failed
};

struct RequestResult
{
    RequestStatus status;
    int http_code;
    std::string response_body;
    std::string error_message;
};

using RequestId = size_t;
using Headers = std::map<std::string, std::string>;

bool init();
void cleanup();

RequestId start_get( const std::string &url );
RequestId start_get( const std::string &url, const Headers &headers );
RequestId start_post( const std::string &url, const std::string &data );
RequestId start_post( const std::string &url, const std::string &data, const Headers &headers );

void process();

RequestStatus get_status( RequestId id );
RequestResult get_result( RequestId id );
void cancel_request( RequestId id );
void clear_completed();

std::vector<RequestId> get_all_requests();

// 发送 pollinations API 请求
RequestId start_pollinations_request( const std::string &system_prompt,const std::string& user_prompt);

// 解析 pollinations API 响应，提取 content，可选追加token消耗信息
std::string parse_pollinations_response( const std::string &json_response, bool include_token_usage = false );

// 发送 pollinations AI 生图 API 请求
RequestId start_pollinations_image_request( const std::string &prompt );

// 解析 pollinations AI 生图 API 响应，保存图片到指定路径
bool parse_pollinations_image_response( const std::string &json_response, const std::string &save_path );

// 发送 pollinations AI 改图 API 请求（基于已有图片进行修改）
RequestId start_pollinations_image_edit_request( const std::string &prompt, const std::string &image_path );

} // namespace network

#endif // CATA_SRC_NETWORK_H
