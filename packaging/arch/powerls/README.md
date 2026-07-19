# AUR submission notes

To submit powerls to the Arch User Repository:

1. Create an AUR package directory:
   ```bash
   mkdir -p powerls-aur
   cd powerls-aur
   cp ../PKGBUILD .
   cp ../.SRCINFO .
   ```

2. Generate `.SRCINFO`:
   ```bash
   makepkg --printsrcinfo > .SRCINFO
   ```

3. Create the AUR package via `git`:
   ```bash
   git init
   git add PKGBUILD .SRCINFO
   git commit -m "Initial package"
   git remote add origin ssh://aur@aur.archlinux.org/powerls.git
   git push origin master
   ```

4. After the AUR package is approved, users can install it with:
   ```bash
   yay -S powerls
   ```
