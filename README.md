# SmartTube PS4 v2.9 — cloud build

This repository contains the v2.8 development source archive and the v2.9 native PS4 build starter.

## v2.9 scope
- OpenOrbis-targeted native C entry point.
- SDL2 basic 1080p screen.
- DualShock 4 initialization and button polling via libScePad.
- GitHub Actions workflow to compile and package a PKG artifact.

This is a development milestone, not a verified release. The build must pass in GitHub Actions, and the resulting PKG must still be installed and tested on the target jailbroken PS4. A successful compile alone does not prove compatibility with firmware 14.02 / GoldHEN 2.4b18.14.

## Build
Open **Actions** → **Build SmartTube PS4 v2.9 PKG** → **Run workflow**. Download the `SmartTubePS4-v2.9-PKG` artifact from the completed run.

## Not included in v2.9
No YouTube playback, ad blocking, SponsorBlock integration, or sponsor skipping is implemented in this native milestone. SDL2's static library references PS4 audio output symbols, so the audio output stub is linked to satisfy SDL2; this milestone does not implement app audio playback.


## v2.9 native PS4 milestone

The v2.9 target is a minimal native PS4 application: an SDL2-rendered status screen plus direct OpenOrbis DualShock 4 polling through `libScePad` and `libSceUserService`. It does not include YouTube playback, SponsorBlock integration, networking, or video decoding.

The cloud workflow uses the OpenOrbis toolchain Docker image for the PS4 compile/link/package steps and builds OpenOrbis `create-fself` v1.3 as a static Linux helper for the final EBOOT conversion. The PS4 package's required app files are `eboot.bin`, `sce_sys/param.sfo`, and `sce_sys/icon0.png`; optional sample LiveArea assets are not treated as required dependencies.

A successful CI package build is not proof of boot compatibility. The resulting PKG must still be installed and tested on the target PS4 firmware/GoldHEN setup.
