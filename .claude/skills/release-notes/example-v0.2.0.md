# Release Notes — v0.2.0 [2026-08-19]

Rendering primitives are complete. v0.1.1 brought shapes and v0.1.2 brought textures — v0.2.0 closes it out with text, so `IRenderer` can now put a shape, a sprite, and a text label on screen in a single frame.

## Highlights

- **Text rendering** — a new `Font` type bakes TTF files into a glyph atlas via `stb_truetype`, and `IRenderer::DrawString` draws strings against it using each glyph's own baked metrics. A new `text_demo` example bakes one font at three sizes, with a color-cycling headline.
- **Sub-region texture draws** — `DrawTexture(src, dst)` draws an arbitrary source rect into an arbitrary destination rect — the primitive text rendering (and future sprite-sheet work) is built on.
- **Texture color modulation** — `ITexture::SetColorMod`/`GetColorMod` tint a texture per draw call, so one shared glyph atlas can render text in any color.
- **Dependency vendoring extended** — `install-stb.sh`/`.ps1` now fetch `stb_truetype.h` alongside `stb_image.h`, behind the same unified install scripts.

## Testing

- **Font unit coverage** — load failures, baked atlas format, glyph metrics and lookup, move semantics.
- **Pixel-verified text rendering** — color-mod round-tripping and a pixel-readback proof that drawn glyphs land where their metrics say and carry the requested tint.
- **DrawTexture(src, dst) coverage** folded into the existing texture test suite, since text rendering is built on it.

## Contributors

- lmriccardo
