# Showroom Star Explorer

Blargian Red becomes Showroom Silver, and Playthrough 1 starts with the showroom Star Explorer: Split Nose, Hi-Lift Wings, Triple Boost Acceleration Engine, Fusion Laser Cannons, Multi-Torpedo Launcher and Mega-Mine Launcher.

By Slim Cognito. For Ratchet & Clank: Going Commando (NTSC-U v1.01) in OpenRAC.

## Building

```sh
python3 tools/fetch_sdk.py          # the OpenRAC mod SDK sdk.lock pins, into .sdk/
cmake -S . -B build -G Ninja
cmake --build build
python3 .sdk/openrac-mod-sdk-*/tools/package_mod.py build/package/showroom -o dist
```

`lib/windows-x86_64/` holds a Windows build made without the C runtime
(`clang --target=x86_64-pc-windows-msvc -ffreestanding -DOPENRAC_FREESTANDING`,
`lld-link /dll /noentry /nodefaultlib`), so the folder can be added to the
launcher as it is.

## Releasing

`.github/workflows/build.yml` is the OpenRAC mod SDK's template workflow (as in
openrac-mods). Every push and pull request checks and packages the mod; a tag
`v<version>` (the manifest's version) publishes the archives and `SHA256SUMS`
on that release, which the launcher installs from when a player pastes the
repository's address.
