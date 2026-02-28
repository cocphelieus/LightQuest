#pragma once

#include <string>

// helper macro constants to build asset paths
static inline std::string assetPath(const std::string &sub)
{
    return std::string("assets/images/") + sub;
}

#define ASSET(sub) assetPath(sub).c_str()
