# Showroom Star Explorer

A new ship paint, **Showroom Silver**, and a new game that starts with the showroom Star Explorer: Split Nose, Hi-Lift Wings, Triple Boost Acceleration Engine, Fusion Laser Cannons, Multi-Torpedo Launcher, Mega-Mine Launcher and Showroom Silver.

At Slim Cognito's Ship Shack, Blargian Red is the first paint and needs no skill points, so you can switch back to it as soon as you get there; Showroom Silver comes right after it, also free. Showroom Silver is the game's finished but unused 18th paint slot, so every other paint is untouched. (A save made with 0.4 or earlier that had Showroom Silver fitted shows Blargian Red; buy Showroom Silver again at the Ship Shack.)

By Slim Cognito. For Ratchet & Clank: Going Commando in OpenRAC, on every disc it supports: NTSC-U v1.01, NTSC-U v2.00 (Greatest Hits) and PAL.

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
