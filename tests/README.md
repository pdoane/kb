# Tests

One regression test per branch. Each test file is named after its branch
(`<branch>` -> `tests/test_<branch with underscores>.c`) and prints
`test_<name>: OK` when it passes.

## Branches and their tests

Two branches carry these tests:

- `tests` is this commit on upstream (2.28e). Every test fails there.
- `tests-with-fixes` is this commit on `all-changes`. Every test passes there.

| Branch | Test | What it checks |
| --- | --- | --- |
| feature-stage-limit | `test_feature_stage_limit.c` | A language system listing more than 32 features keeps all of them in the shape config, so an override can turn on any of them. Synthetic GSUB with 43 features. |
| config-eviction | `test_config_eviction.c` | `kbts_ShapeForgetFont` drops a caller-owned font's cached shape configs, and popping a context-owned font (`kbts_ShapePushFontFromMemory`) drops its configs, so a font loaded at the same address does not get the old font's config. Synthetic GSUBs. |
| pop-font | `test_pop_font.c` | `kbts_ShapePopFont` pops the top font, repeatedly and across a font block boundary, without crashing or corrupting the stack. |
| config-cache-hash | `test_config_cache_hash.c` | A cached shape config is keyed by its font info's hash, not by a pointer into the scratch arena, so font B shaped two `ShapeBegin`s after font A does not get A's config. |
| script-tag-runs | `test_script_tag_runs.c` | Scripts sharing an OpenType tag (hiragana and katakana, both `kana`) stay in one run; scripts with different tags still split. |
| feature-stack-order | `test_feature_stack_order.c` | The feature stack applies the latest push of a tag. |
| user-feature-filter | `test_user_feature_filter.c` | A `frac` pushed by the caller reaches every glyph its lookups cover, not only the glyphs the shaper flagged around U+2044. |
| delta-set-index-map-alignment | `test_delta_set_index_map_alignment.c` | A delta set index map at an odd offset (Noto Sans's HVAR) has its entry count read unaligned. Always built with `-fsanitize=alignment`, so the misaligned read fails the test; it also checks wght=900 shapes wider than the default. |
| gsub-gpos-bounds | `test_gsub_gpos_bounds.c` | A GSUB or GPOS missing any one of its script, feature or lookup lists loads as empty, without the byteswap following the zero offset and writing past the blob. Synthetic tables, checked with a guard pattern after the blob. |
| dflt-fallback | `test_dflt_fallback.c` | Language system selection takes the requested script, then DFLT, then latn, and none when the font has none of these, never the first script in the list. Synthetic GSUBs. |

## Running

```sh
make test     # build and run every test
make asan     # rebuild and run under -fsanitize=address
make ubsan    # rebuild and run under -fsanitize=undefined,alignment
```

The fonts are not bundled. Put them at the repo root, or point
`NOTOSANS_FONT` / `ROBOTOFLEX_FONT` at them:

- `NotoSans[wdth,wght].ttf`: https://fonts.google.com/noto/specimen/Noto+Sans
- `RobotoFlex-VariableFont_GRAD,XOPQ,XTRA,YOPQ,YTAS,YTDE,YTFI,YTLC,YTUC,opsz,slnt,wdth,wght.ttf`:
  https://fonts.google.com/specimen/Roboto+Flex

Tests on synthetic fonts take no arguments. `config_cache_hash` takes both
fonts; the other font tests take Noto Sans.

Against a header without `kbts_ShapeForgetFont`, the Makefile builds
`test_config_eviction.c` with `-DTEST_WITHOUT_FORGET_FONT`: the caller-owned
half runs without the call and fails on the freed font's config, and the
context-owned half, which needs a working `kbts_ShapePopFont`, is left out.
