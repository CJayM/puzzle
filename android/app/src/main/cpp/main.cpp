#include "core/Game.h"

int main()
{
    Game game;
    if (!game.Init())
    {
        return 1;
    }

    game.Run();

    return 0;
}
