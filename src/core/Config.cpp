#include "core/Config.h"

Config& Config::Instance()
{
    static Config instance;
    return instance;
}
