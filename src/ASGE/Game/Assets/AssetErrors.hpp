#pragma once

#include <ASGE/Core/Errors.hpp>

namespace asge::errors
{

// ---------------------------------------------------------------------------------------------
// ASSET LOADING ERRORS
// ---------------------------------------------------------------------------------------------

enum class AssetError
{
    ScenePrefab_AtLeastOneEntity,
    ScenePrefab_TooManyRoots,
    ScenePrefab_MissingRootEntity
};

inline str::String ToErrorString(AssetError e) noexcept
{
    switch (e)
    {
    case AssetError::ScenePrefab_AtLeastOneEntity: return "at least one entity must be defined in the prefab";
    case AssetError::ScenePrefab_TooManyRoots: return "too many root entities";
    case AssetError::ScenePrefab_MissingRootEntity: return "missing root entity in the scene prefab";
    }
    return "uknown asset error";
}

}

REGISTER_ASGE_ERROR(asge::errors::AssetError, "asge.asset")