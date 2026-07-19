# powerls

A colorful terminal directory explorer written in C++20, providing a more refined and optimized alternative to the classic `ls` command on Linux.

## Project Structure

- [src/main.cpp](src/main.cpp) — Application entry point.
- [CMakeLists.txt](CMakeLists.txt) — Build definition for the executable.
- [packaging/](packaging/) — Arch Linux and Debian packaging metadata blueprints.
- [scripts/](scripts/) — Helper scripts for packaging and local builds.

## Installation

### 1. Arch Linux (AUR)

`powerls` is officially published on the Arch User Repository (AUR). You can install it seamlessly using an AUR helper like `yay` or `paru`:

```bash
yay -S powerls
```

### 2. Any Other Linux Distro (Manual Installation)

If you are installing the compiled binary manually or downloading it directly from the GitHub Releases page:

```bash
mkdir -p ~/.local/bin
install -m 0755 build/powerls ~/.local/bin/powerls
export PATH="$HOME/.local/bin:$PATH"
```

### Build from Source

To compile the application completely from scratch, ensure you have `cmake`, a C++20 compliant compiler (`g++`), and a build system (`make` or `ninja`) installed:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

### Local Arch Linux Package Build

If you want to manually test the Arch packaging blueprints locally from the repository root:

```bash
makepkg -si -p packaging/arch/PKGBUILD
```

## Packaging Layout

Packaging metadata is organized under [packaging/arch/PKGBUILD](packaging/arch/PKGBUILD) and [packaging/README.md](packaging/README.md) to ensure the repository follows a clear, maintainable, and standard multi-distro upstream layout.

### Debug Packages on the AUR

AUR packages primarily provide user-facing runtime binaries. A separate debug package adds unnecessary maintenance overhead and duplicates packaging metadata. For this project, the standard release package is the designated distribution artifact, and debug symbols are stripped out of the AUR package tracking pipeline by default.

## Automated Deployment Pipeline

A robust, automated CI/CD engine is configured inside [.github/workflows/release.yml](.github/workflows/release.yml). Whenever a new release version is generated, GitHub Actions dynamically fires up a secure execution runner to:

1. **Compile & Link** — Verify and build the C++ codebase using CMake and Ninja.
2. **Package Portability** — Generate a standard independent Linux x86_64 `tar.gz` archive.
3. **Debian Packaging** — Use `debhelper` natively to generate a production-ready `.deb` package.
4. **GitHub Deployment** — Create a GitHub Release and upload the tarball and Debian assets.
5. **AUR Sync** — Securely connect to the Arch Linux servers via SSH to commit and push updated `PKGBUILD` and `.SRCINFO` changes downstream automatically.