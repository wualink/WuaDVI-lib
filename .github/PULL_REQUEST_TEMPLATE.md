<!--
Thanks for contributing to WuaDVI-lib! Please fill this in so we can review
quickly. See CONTRIBUTING.md for the full guidelines.
-->

## Summary

<!-- What does this PR do, and why? -->

Closes #

## Type of change

- [ ] Bug fix (non-breaking change that fixes an issue)
- [ ] New feature (non-breaking change that adds functionality)
- [ ] Breaking API change
- [ ] Performance / stability
- [ ] Documentation only
- [ ] Build / CI / tooling

## How it was tested

<!--
CI proves it compiles, not that the picture is right. For anything touching the
board, the transport or the widgets, test on hardware and say how.
-->

- [ ] Examples build locally
- Hardware tested: <!-- WuaDVI board / not tested (docs-only) -->
- Resolution(s) tested: <!-- 320x240 / 400x240 / 640x480x1 / 800x600x1 / 1280x720x1 -->
- Relevant serial log:

```
paste here
```

## Checklist

- [ ] The change is focused (one logical change)
- [ ] `clang-format --dry-run --Werror src/*.h src/*.cpp` passes
- [ ] Code follows CONTRIBUTING.md (English comments, `@brief/@param/@return`)
- [ ] I did not break anything in
      [Things that are load-bearing](../CONTRIBUTING.md#things-that-are-load-bearing)
- [ ] **Wire protocols:** if I changed a protocol header, the matching change is
      landed in [WuaDVI-rp-lite](https://github.com/wualink/WuaDVI-rp-lite) —
      these must stay byte-identical
- [ ] Public headers stay light (no generated payloads or large arrays)
- [ ] Added a line under **[Unreleased]** in [`CHANGELOG.md`](../CHANGELOG.md)
- [ ] I did **not** bump the version (maintainers cut releases)
- [ ] I agree my contribution is licensed under the project's MIT License
