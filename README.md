# Text Editor based on Intel ISIS-II aedit

Text editor based on interface from Intel ISIS-II aedit, which in turn was based on its big brother alter.

This was originally written in the mid 80s on QNX running on an 8088, with a CBM-64 acting as the console.

I have used this on every UNIX system I have touched. A couple of things have not been implemented, 

- other - a feature to hold two files in memory 
- error on saving detected, the original ISIS-II would reboot if you attempted to write to a read-only disk, so this part just never happened in the C version - sin of omission.

It has even run on UNIX Release 7 on a PDP-II emulator.

## Features

- Written in ANSI C
- Highly portable
- No curses or other libraries required
- Single source file
- Block highlighting.
- Clipboard.
- Window resizing.
- Uses temporary file to deal with large files.
- Works over SSH, including on Windows

## Origins
Back in the day, aedit was an ISIS-II 8080 port of the 8086 alter text editor, that in turn ran on the Intellec MDS 8086 co-processor.

PC-DOS 1.1 only came with edlin. I wrote an 8086 assembler version of aedit for PC-DOS, it was written using edlin and masm.

I later wrote a CP/M version, in Z80 assembler to run on a 380Z.

This version here is a very portable C implementation, originally written on QNX running on an 8088.

## Building

This is not intended to be a _configure/make/make install_ type of project. The output of the build process is a package that can be installed.

### Make

On `*bsd`, `haiku`, `linux`, `osx`, `qnx` or `solaris` make can be used. The output should be a package if supported by the build system. The `osx` build process should build both an `arm` and `x86_64` suitable package.

### Host build system

On `alpine`, `archlinux`, `gentoo` or `solus` then the host build system should be used.

### Visual Studio on Windows

On `Windows` the `PowerShell` script should be used. This will enumerate through the architectures setting up the build environment and creating

- signed EXE executable
- signed MSI package
- signed MSIX package
- signed MSIX bundle
- zip containing the EXE for each architecture

Visual Studio Community edition is required.

## Summary

| os | tool | script | output
| -- | ----- | ------ | ------
| GNU Hurd | make | [Makefile](Makefile) | `deb`
| Haiku | make | [Makefile](Makefile) | `hpkg`
| Linux | make | [Makefile](Makefile) | `deb`, `ipk`, `rpm`, `tgz`
| macOS | make | [Makefile](Makefile) | `pkg` with `lipo` executable containing `arm64` and `x86_64`
| DragonFly, NetBSD, FreeBSD, OpenBSD | make | [Makefile](Makefile) | native package
| QNX | make | [Makefile](Makefile) | `qpr`
| Solaris | make | [Makefile](Makefile) | `pkg`
| Arch Linux | makepkg | [PKGBUILD](https://sourceforge.net/p/aedit/code/HEAD/tree/branches/pacman/PKGBUILD) | `pkg.tar.zst`
| Alpine Linux | abuild | [APKBUILD](https://sourceforge.net/p/aedit/code/HEAD/tree/branches/alpine/APKBUILD) | `apk`
| Gentoo Linux | emerge | [aedit-1.1.82.ebuild](https://sourceforge.net/p/aedit/code/HEAD/tree/branches/gentoo/app-editors/aedit/aedit-1.1.82.ebuild) | installed program |
| Solus | solbuild | [package.yaml](https://sourceforge.net/p/aedit/code/HEAD/tree/branches/solus/package.yml) | `eopkg`
| Windows | pwsh | [package.ps1](package.ps1) | `exe`, `msi`, `msix`, `msixbundle`, `zip`

## Usage

Main usage is

```
$ aedit filename.txt
```

The editor uses modes while showing the menu of options directly on screen.

While the main menu is showing you can use the cursor keys to navigate the file.

### Main Menu

| menu | key | function |
| ---- | ----| ---------|
| Again | `a` | repeat the last menu item |
| Block | `b` | enter block selection mode |
| Delete | `d` | enter block selection mode |
| Find | `f` | look forward for a string |
| -find | `-` | look backwards for a string |
| Get | `g` | read a text file in at insertion point, or clipboard if no filename given |
| Insert | `i` | enter text insertion mode, press `end` to exit |
| Jump | `j` | jump to a specific line |
| Length | `l` | show length of file |
| Quit | `q` | go to the quit options |
| Replace | `r` | replace one string with another |
| ?replace | `?` | optionally replace a string |
| Shell | `s` | run command prompt |
| View | `v` | redraw the screen |
| Xchange | `x` | overwrite text entry mode, exit with `end` |
| --more-- | `tab` | press tab key to see more options |

### Block Menu

| menu | key | function |
| ---- | ----| ---------|
| Buffer | `b` | copy data to internal buffer |
| Copy | `c` | copy data to a file |
| Delete | `d` | remove the text |
| Put | `p` | write text to file and remove text |

### Quit Menu

| menu | key | function |
| ---- | ----| ---------|
| Abort | `a` | exit without saving |
| Exit | `e` | exit with file save |
| Init | `i`| abort current session and edit new file |
| Update | `u` | write the file but do not exit |
| Write | `w` | write all contents to another file |

The `end` key will exit the current mode or menu apart from the top-level menu.

### Prebuilt binaries

Prebuilt binaries can be found either on the [releases page on github](https://github.com/rhubarb-geek-nz/aedit/releases) or as a [file on sourceforge](https://sourceforge.net/projects/aedit/files/). If you have an obsolete/legacy system there may still be a suitable existing package.
