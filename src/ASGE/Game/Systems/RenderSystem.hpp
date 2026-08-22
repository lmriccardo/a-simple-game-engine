#pragma once

#include <ASGE/Core/ECS/Registry.hpp>
#include <ASGE/Video/Graphics/Renderer.hpp>
#include <ASGE/Game/Components/Transform.hpp>
#include <ASGE/Game/Components/Sprite.hpp>

namespace asge::game::systems
{

/**
 * @brief Draws every entity that has both a Transform and a Sprite.
 *
 * Transform's position is the sprite's top-left corner; scale stretches
 * the texture's native size. Entities whose Sprite::m_Texture is null are
 * skipped. Rotation is not applied — IRenderer's Rect-based DrawTexture
 * overloads (the only ones that also support a source rect) don't take
 * one; use DrawTextureAffine directly for a rotating, unclipped sprite.
 */
void RenderSystem( ecs::Registry& inRegistry, video::IRenderer& inRenderer ) noexcept;

}
