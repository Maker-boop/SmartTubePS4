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
No YouTube playback, network transport, ad blocking, SponsorBlock integration, or sponsor skipping is implemented in this native milestone. Those remain later development work.
