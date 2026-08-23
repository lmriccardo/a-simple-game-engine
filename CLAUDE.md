# Project notes for Claude

## Code documentation

Every function — public API or internal `_internal`/SDL-translation helper —
gets a short doc comment on its **declaration** (the header, not a repeated
one on an out-of-line `.cpp` definition): a Doxygen `/** @brief ... */`, or a
single-line `/** ... */` for something trivial. **4-6 lines max** 
— one or two sentences describing what it does, not an essay. Skip
`@param`/`@return`/`@tparam` breakdowns unless the signature alone doesn't
make them obvious.
