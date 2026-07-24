#ifndef GAMESTATE_H
#define GAMESTATE_H

class Game;  // forward declaration

class GameState
{
public:
    virtual ~GameState() = default;

    virtual void Enter(Game& game) = 0;
    virtual void Exit(Game& game) = 0;
    virtual void Update(Game& game) = 0;
    virtual void Draw(Game& game) = 0;
};

#endif // GAMESTATE_H
