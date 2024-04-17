#include "breeze.h"


namespace breeze {

    namespace thread_pool {

        cxxpool::thread_pool pool{4};

    }

    namespace filesyetm {

        int copy_(std::filesystem::path& path) {

            return 1;

        }

        int delete_(std::filesystem::path& path) {

            std::filesystem::remove_all(path);

            return 1;

        }

    }



}
