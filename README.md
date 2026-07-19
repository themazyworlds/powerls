# powerls
A colorful terminal directory explorer written in C++20, providing a more refined and optimized alternative to the classic `ls` command on Linux.

## Project Structure

- [src/main.cpp](src/main.cpp): Application entry point.
- [CMakeLists.txt](CMakeLists.txt): Build definition for the executable.
- [packaging/](packaging/): Arch Linux and Debian packaging metadata blueprints.
- [scripts/](scripts/): Helper scripts for packaging and local builds.

## Installation

### 1. Arch Linux (AUR)
`powerls` is officially published on the Arch User Repository (AUR). You can install it seamlessly using an AUR helper like `yay` or `paru`:

```bash
yay -S powerls