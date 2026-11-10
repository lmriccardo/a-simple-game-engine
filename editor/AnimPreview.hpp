#pragma once

#include <ASGE/Core/ECS/Registry.hpp>

/**
 * @brief Starts previewing animations in the editor.
 *
 * Saves every Animator's runtime state and its Sprite's source rect, then
 * resets the Animators so they start from the machine's initial state. Does
 * nothing if a preview is already running.
 */
void BeginAnimPreview( asge::ecs::Registry& inRegistry ) noexcept;

/**
 * @brief Ends the preview and puts back what BeginAnimPreview saved.
 *
 * Entities that were deleted in the meantime are skipped, so the scene
 * ends up exactly as it was before the preview. Does nothing if no preview is running.
 */
void EndAnimPreview( asge::ecs::Registry& inRegistry ) noexcept;

/** @brief True between BeginAnimPreview and EndAnimPreview. */
[[nodiscard]] bool IsAnimPreviewing() noexcept;
