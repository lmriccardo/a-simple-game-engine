#include <iostream>
#include <ASGE/Core/Application.hpp>
#include <ASGE/Game/Game.hpp>

class Game final: public asge::game::IGame
{
public:
    void Update(float inDeltaTime) {};
    void Render(asge::graphics::IRenderer& inRenderer)
    {
        inRenderer.Clear({255, 0, 0, 255});
    }
};

int main(int, char**)
{
    Game game;
    asge::Application app(game, asge::ApplicationConfig{});
    app.Run();
    return 0;
}
