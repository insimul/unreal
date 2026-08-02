# InsimulCoreLibrary — prebuilt libinsimulcore binaries

The platform binaries staged here are **build artifacts**, not checked into git.
They are copied from `insimul-native/dist/<platform>/` when the plugin is
packaged (see `scripts/release/build-plugin-zip.mjs`) so that the
`ThirdParty/InsimulCoreLibrary` module can link and stage them per
`Target.Platform`:

```
lib/
  Mac/libinsimulcore.dylib          # dist/macos-arm64 or macos-x64
  Linux/libinsimulcore.so           # dist/linux-x64
  Win64/insimulcore.dll  insimulcore.lib   # dist/windows-x64
```

The stable public header — `../include/insimulcore.h` — **is** committed (it is
the ABI contract `Private/Core/InsimulCoreBridge.cpp` compiles against) and is a
byte-for-byte copy of `insimul-native/corebridge/include/insimulcore.h`. A header
that forks between repositories is the exact failure the bridge's promotion to
`native/` was meant to prevent, so re-copy it rather than editing it here.

**Desktop only.** There is no console, iOS or Android build of libinsimulcore.
On those targets the module defines `INSIMUL_WITH_CORE=0` and the plugin falls
back to its pre-adoption behaviour — see `RUNTIME_CORE_ADOPTION.md` §4.7.2.

The host-side gate (`tools/verify-unreal`, target `radiant_bridge`) does **not**
use these staged binaries: it compiles libinsimulcore from an
`insimul-native` checkout so the gate tests the source of truth rather than a
copy. See `tools/verify-unreal/CMakeLists.txt`.
