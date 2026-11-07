#include "core/Game.h"
#include "core/Config.h"

#include <cstring>

int main(int argc, char** argv)
{
    // --debug открывает разрушительные инструменты в интерфейсе
    for (int i = 1; i < argc; ++i)
    {
        if (std::strcmp(argv[i], "--debug") == 0)
            Config::Instance().debugMode = true;
    }

    Game game;
    if (!game.Init())
    {
        return 1;
    }

    game.Run();

    return 0;
}
