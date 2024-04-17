#pragma once

#include "cxxpool/cxxpool.h"
#include <filesystem>

namespace breeze {

    namespace thread_pool {

        extern cxxpool::thread_pool pool;

    }

    namespace filesyetm {

        int copy_(std::filesystem::path& path);

        int delete_(std::filesystem::path& path);

    }

    

}
