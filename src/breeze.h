#pragma once

#include "cxxpool/cxxpool.h"
#include <filesystem>
#if defined(__ANDROID__)
#include<jni.h>
#endif


namespace breeze {

    namespace thread_pool {

        extern cxxpool::thread_pool pool;

    }

    namespace filesystem {

        int copy_(std::filesystem::path& path);

        int delete_(std::filesystem::path& path);

    }

    namespace android {
    

    
    
    
    }


    

}
