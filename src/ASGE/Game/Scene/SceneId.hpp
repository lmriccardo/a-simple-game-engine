#pragma once

#include <string>

namespace asge::game::scene
{

/**
 * @brief Marks which scene (by virtual path) an entity belongs to.
 *        SceneManager stamps this on entities it loads, to tell scenes
 *        apart within its one shared Registry — pure bookkeeping, not
 *        gameplay data, so it never round-trips through a scene file.
 */
struct SceneId
{
    str::String m_Path;

    friend bool operator==(SceneId const&, SceneId const&) = default;
};

}
