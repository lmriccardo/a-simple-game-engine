#pragma once

#include <ASGE/ASGE.hpp>

/**
 * @brief A car (Sprite + components::PathFollow) driving a closed, curved
 *        street loop -- the end-to-end showcase for PathFollow/
 *        PathFollowingSystem: waypoints resolved once into a
 *        math::CatmullRomSpline at spawn time, then walked every frame at a
 *        player-adjustable speed via systems::PathFollowingSystem, wrapping
 *        back to the start on every lap.
 *
 * The street itself is drawn straight from the car's own resolved
 * PathFollow::m_Path -- IRenderer has no curve/polygon-fill primitive, so
 * RenderRoad() paints it as a dense trail of overlapping filled circles
 * with a dashed centerline. UP/DOWN adjust the car's speed while held,
 * SPACE pauses/resumes, R resets it to the start of the loop.
 */
class PathFollowingDemoState final : public asge::game::state::IGameState<int>
{
    asge::ecs::Registry&             m_Registry;
    asge::game::asset::AssetManager& m_Assets;

    asge::ecs::Entity m_Car{ asge::ecs::Entity::Null() };

    float    m_Speed{ 0.0f }; // current units/second, UP/DOWN-adjustable -- set to kBaseSpeed by Reset()
    unsigned m_Laps{ 0 };
    bool     m_Paused{ false };

    void SpawnCar(asge::video::IRenderer& inRenderer);
    void UpdateCar(float inDeltaTime);
    void RecenterCarSprite();
    void Reset();
    void RenderRoad(asge::video::IRenderer& inRenderer) const;
    void RenderHud(asge::video::IRenderer& inRenderer) const;

public:
    PathFollowingDemoState(asge::ecs::Registry& inRegistry, asge::game::asset::AssetManager& inAssets,
        asge::video::IRenderer& inRenderer);

    [[nodiscard]] std::optional<asge::game::state::Transition<int>>
    Update(float inDeltaTime, asge::input::InputState const& inInput) override;
    void Render(asge::video::IRenderer& inRenderer) override;
    void OnSystemEvent(asge::event::SystemEvent const& inSysEvent) override;
};

class PathFollowingDemoGame final : public asge::game::Game<int>
{
public:
    explicit PathFollowingDemoGame(asge::video::IRenderer& inRenderer, asge::audio::AudioDevice& inAudioDev);

protected:
    [[nodiscard]] std::unique_ptr<StateType> CreateState(int inId) override;
};
