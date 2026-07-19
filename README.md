# powerls
A more refined version of ls on linux

## Project structure

- [src/main.cpp](src/main.cpp): application entry point
- [CMakeLists.txt](CMakeLists.txt): build definition for the executable
- [packaging/](packaging/): Arch and Debian packaging metadata
- [scripts/](scripts/): helper scripts for packaging and builds

## Build from source

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

## Install on any Linux distro

```bash
mkdir -p ~/.local/bin
install -m 0755 build/powerls ~/.local/bin/powerls
export PATH="$HOME/.local/bin:$PATH"
```

## Arch Linux package build

From the repository root:

```bash
makepkg -si -p packaging/arch/PKGBUILD
```

This uses the repository's packaged Arch build definition under packaging/arch/.

To install the built package file with pacman or yay:

```bash
sudo pacman -U powerls-1.0-1-x86_64.pkg.tar.zst
yay -U powerls-1.0-1-x86_64.pkg.tar.zst
```

Once published to the AUR, the package can also be installed with:

```bash
yay -S powerls
```

## Packaging layout

Packaging metadata is now organized under [packaging/arch/PKGBUILD](packaging/arch/PKGBUILD) and [packaging/README.md](packaging/README.md) so the repository follows a clearer, more maintainable structure.

## Why we are not sending a separate powerls-debug package to AUR

AUR packages should primarily provide the user-facing runtime binary. A separate debug package adds extra maintenance overhead, duplicates packaging metadata, and is usually unnecessary unless you are explicitly targeting kernel or low-level library debugging workflows. For this project, the standard release package is the correct distribution artifact, and debug symbols are kept out of the AUR package by default.

## GitHub release workflow

A release workflow is included in [.github/workflows/release.yml](.github/workflows/release.yml) that builds a portable tar.gz archive for Linux x86_64 on tag pushes.

