# Arch Linux: local development package

This directory contains a local, fork-based replacement for the AUR
`umbriel-git` package. It is **not published on AUR** and is not a package
called `umbriel-git`; it provides the same `umbriel` capability.

The package source is
`riccardomarotti/umbriel#feature/realtime-effect-input`. A rebuild fetches
the latest committed version on that branch.

## Install

From a checked-out copy of that branch:

~~~fish
cd ~/workspace/umbriel
git switch feature/realtime-effect-input
git pull --ff-only
cd packaging/arch
makepkg -si
~~~

`makepkg -si` builds and installs a **local** PKGBUILD and resolves missing
dependencies from configured pacman repositories. For dependencies available
only from AUR, install them separately with yay first. A regular `yay -S umbriel-orametra-git` will not work unless
the package is published to AUR or a configured repository.

On first installation pacman will ask to replace the installed
`umbriel-git` because the development package conflicts with it.
It installs the same `/usr/bin/umbriel`, session launcher, and service files;
do not try to install both versions simultaneously.

Review PKGBUILD before installation. Do not run `makepkg` with sudo.
Back up your Umbriel configuration before testing a modified compositor.

## Update

After committing code to the tracked feature branch:

~~~fish
cd ~/workspace/umbriel
git pull --ff-only
cd packaging/arch
makepkg -si
~~~

If the source branch has changed, `makepkg` will recalculate the VCS version.
To force fetching and recompiling from scratch, use `makepkg -Csi` from this
directory instead. The package manager owns the installed files.

For *uncommitted* local changes, use Meson directly; the VCS PKGBUILD clones
from GitHub and does **not** automatically include working-tree modifications.

A compositor update becomes active **only on the next graphical login**.
Close the existing desktop session normally and log back in, rather than
replacing a compositor process while running.

## Revert to upstream

~~~fish
yay -S umbriel-git
~~~

That package conflicts with `umbriel-orametra-git`, so pacman should offer
to replace the development variant. Check the prompt before confirming.
The personal configuration under `~/.config/umbriel` is not packaged and is
left untouched.

## Build without installing

For quick checks directly in the fork:

~~~fish
cd ~/workspace/umbriel
just configure debug
just debug
~~~

The resulting `build-debug/umbriel` can be used for nested compositor
experiments without replacing the system package. Debug builds enable the
test harness, which is disabled in the release PKGBUILD.
