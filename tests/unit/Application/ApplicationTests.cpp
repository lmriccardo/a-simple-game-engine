#include <ASGE/Application/Application.hpp>
#include <ASGE/Game/Game.hpp>

#include <gtest/gtest.h>

namespace
{

// Nothing currently instantiates Application<TGame> (every example still
// targets the pre-state-machine game::IGame API), so its template body goes
// otherwise unchecked by any build. A minimal Game<TStateId> subclass is
// enough to force the compiler to type-check every Application member.
class CompileCheckGame final : public asge::game::Game<int>
{
public:
    using Game::Game;

protected:
    [[nodiscard]] std::unique_ptr<StateType> CreateState( int ) override { return nullptr; }
};

}

// Must live outside the anonymous namespace above: [temp.explicit] requires
// an explicit instantiation to appear in a namespace enclosing the
// template's own namespace (asge) -- MSVC accepts it inside the anonymous
// namespace regardless, but GCC/Clang correctly reject it there.
template class asge::Application<CompileCheckGame>;

// Constructing/running Application<TGame> for real needs a live SDL window
// (VideoSystem::Initialize), which is out of scope for a unit test -- this
// suite only guards that the template itself still compiles.
TEST(ApplicationTest, TemplateInstantiatesForAGameSubclass)
{
    SUCCEED();
}
