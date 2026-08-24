---
name: document-and-test
description: Add Doxygen doc comments and a GoogleTest unit-test suite for a class/function in this repo, following its existing conventions. Use when asked to "document X", "add unit tests for X", or "document and test X".
---

Two separable jobs — do both unless the user asked for only one. Doc comments
follow the project-wide rule in `CLAUDE.md`; everything below is what that
rule doesn't spell out, plus this repo's test-suite conventions, learned by
building and running them (see the "verify" section — never skip it).

## Doc comments

- One short Doxygen block per function/method **on its declaration** (header),
  never repeated on an out-of-line `.cpp` definition. `/** @brief ... */`,
  4-6 lines max. Skip `@param`/`@return` unless the signature alone doesn't
  make them obvious (e.g. `Mount(str::StringCRef inMntPoint, str::StringCRef
  inRealPath)` doesn't need it; a bool-returning method with a non-obvious
  "what does true mean" case might).
- This applies to **every** function, including private nested types/structs
  (a one-liner is enough, e.g. `/** @brief One virtual-root-to-real-directory
  binding in the mount table. */`) and free helper functions declared only in
  a `.cpp`'s anonymous namespace (their only declaration site *is* the `.cpp`,
  so they get the doc comment there — see `NormalizeVirtualPath` in
  `VirtualFileSystem.cpp`).
- Plain struct/class data members get a short trailing `//` comment instead of
  a Doxygen block, one per line (see `Image.hpp`'s `m_Width`/`m_Height`, or
  `VirtualFileSystem.hpp`'s `MountInfo` fields) — don't Doxygen-block a field.
- Don't invent behavior the code doesn't have. If you're not sure what a
  branch does, read the `.cpp` before writing the comment — a doc comment
  that's wrong about actual behavior (e.g. claiming an order/priority the
  implementation doesn't provide) is worse than none.

## Unit tests

### Where things go

- `tests/unit/<Category>/` — one folder per subsystem (`Configuration`,
  `Filesystem`, `ECS`, `Memory`, ...), mirroring `src/ASGE/Core/<Category>/`.
- Each category folder has its own `CMakeLists.txt` defining one
  `ASGE_<Category>Tests` executable covering every `*.cpp` in that folder —
  don't create a second executable for one more class in an existing
  category, add the `.cpp` to the existing `add_executable(...)` list.
- A **new** category needs `add_subdirectory(<Category>)` added to
  `tests/unit/CMakeLists.txt` (or `tests/integration/CMakeLists.txt` for
  integration-tier tests).
- Copy an existing category's `CMakeLists.txt` verbatim and rename
  (`ASGE_ConfigurationTests` → `ASGE_FilesystemTests`, etc.) — same
  `target_link_libraries` (`ASGE`, `GTest::gtest_main`), same
  `target_compile_options` (`-Wall -Wextra -Wpedantic` / `/W4 /permissive-`),
  same `SDL3::SDL3` post-build copy on `WIN32`, same
  `gtest_discover_tests(...)` at the end.

### File shape

- One file per class: `<ClassName>Tests.cpp`, an anonymous namespace, a
  `using namespace <the class's namespace>;` plus `using` for the specific
  error enum(s) under test (e.g. `using asge::errors::VfsError;`).
- A `TEST_F` fixture named `<ClassName>Test : public ::testing::Test`.
  `SetUp()`/`TearDown()` create/remove an isolated temp path per test via
  `std::filesystem::temp_directory_path() / ("asge_<thing>_test_" +
  std::to_string(reinterpret_cast<std::uintptr_t>(this)))` — the `this`
  pointer keeps parallel test instances from colliding. `TearDown()` cleans
  up with the `std::error_code` overload (`remove`/`remove_all(path, ec)`),
  never the throwing one.
- Group tests with banner comments between logical sections, one per
  method/concern under test:
  `// ─── Resolve ──────────────────────────────────────────`
  (see `ConfigurationManagerTests.cpp` or `VirtualFileSystemTests.cpp`).
- Test names: `TEST_F(<ClassName>Test, <Method>_<Scenario>_<Outcome>)` in
  PascalCase-joined clauses, e.g. `Resolve_FileMissingFromMountedDirectory
  ReturnsNotMountedError`, `Load_WrongExtensionReturnsError`.

### What to cover

For every method returning this repo's `Result<T>`/`BoolResult`: the success
path, **and one test per distinct error enum value** it can produce — not
just a generic "returns an error" check. Assert the specific error:
```cpp
auto result = obj.DoThing(...);
ASSERT_FALSE(result.IsOk());
EXPECT_EQ(result.Code(), make_error_code(SomeError::ThatValue));
```
Use `ASSERT_TRUE(...)` for setup calls that must succeed for the rest of the
test to make sense (a failed precondition should stop the test, not cascade
into a confusing second failure); `EXPECT_*` for the actual assertion(s)
under test. Also cover: normalization/equivalence edge cases the
implementation explicitly handles (e.g. this repo's VFS normalizing
`\`-vs-`/` and leading/trailing slashes), and ordering/precedence behavior
if the class documents any (verify what the code *actually* does before
asserting it — see the gotcha below).

## Verify — don't skip this

Writing tests that merely compile is not the job; **build and run them**.
Use the `run-asge` skill's build commands:
```
cmake --preset windows
cmake --build --preset windows-debug --target ASGE_<Category>Tests
& "build\tests\unit\<Category>\Debug\ASGE_<Category>Tests.exe"
ctest --preset windows-debug          # full suite, confirm no regressions
```
Two things this project has already hit that only show up at this step:
- **A new `.cpp` implementation file must be added to
  `src/ASGE/CMakeLists.txt`'s source list**, or the test target compiles but
  fails to *link* (`LNK2019 unresolved external symbol`) against the class
  under test.
- **Reconfigure (`cmake --preset windows`) after adding a new test
  subdirectory or category `CMakeLists.txt`** — CMake won't pick it up on a
  build-only invocation.

If a test fails, that's the point of writing it — **don't loosen the
assertion to make it pass**. Read the implementation, find the actual defect,
fix it with a minimal change, and say plainly in your summary what was wrong
and why (see the dangling-`string_view` and off-by-one `substr()` bugs the
`VirtualFileSystem` test suite caught — both were genuine UB/crash bugs, not
test mistakes).
