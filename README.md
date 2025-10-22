<img align="right" alt="The ctrmus icon" src="meta/icon.png">

# ctrmus: a music player for the Nintendo 3DS

The latest 3DSX/CIA/3DS download can be found on the <a href="https://github.com/deltabeard/ctrmus/releases">releases</a> page, or by scanning <a href="https://zxing.org/w/chart?cht=qr&chs=350x350&chld=M&choe=UTF-8&chl=https%3A%2F%2Fgithub.com%2Fdeltabeard%2Fctrmus%2Freleases%2Fdownload%2Fv0.5.3%2Fctrmus.cia">this QR code</a>.

## Features
* Plays PCM WAV, AIFF, FLAC, Opus, Vorbis and MP3 files.
* Pause and play support.
* Plays music via headphones whilst system is closed.
* Ability to browse directories.

## Controls
L+R or L+Up: Pause

ZL/ZR or L/R: Previous/Next Song

L+Left: Show Controls

A: Play file or change to selected directory

B: Go up folder

Up & down = Move cursor

Left & right = Move cursor skipping 13 files at a time.

Start: Exit

# Contributing

I very much welcome a pull request from anyone wanting to contribute! Be it a small spelling mistake, a bug fix or otherwise.

## Adding features
Create an issue before you start work so that others know that your considering working on adding a feature or fixing a bug.

## Pull requests
Please consider the following when making a pull request.
* Commit messages should be clear.
* Code style should be consistent.

If you're unsure whether to make a pull request, just make it. :smiley:

## Compiling

Build dependencies:
- GNU Make
- [makerom](https://github.com/3DSGuy/Project_CTR)
- [bannertool](https://github.com/Steveice10/bannertool)
- [devkitARM](https://devkitpro.org/wiki/Getting_Started), with the following packages installed:
  - 3ds-libogg
  - 3ds-libopus
  - 3ds-libsidplay
  - 3ds-libvorbisidec
  - 3ds-mpg123
  - 3ds-opusfile
  - libctru

Installing packages in MSYS2:

```
pacman -S libctru 3ds-mpg123 3ds-libopus 3ds-opusfile 3ds-libogg 3ds-libvorbisidec 3ds-libsidplay
```

Make sure to put `C:\devkitPro\tools\bin` on your PATH.

To build, type `make` in the project folder.

### Planned features
* Playlist support.
* Repeat and shuffle support.
* Metadata support.

#### Notes
Due to limitations in [ctrulib](https://github.com/smealum/ctrulib/issues/328), only ASCII characters are displayed correctly. Other characters will appear garbled, but functionality is not affected.

[Ctrulib](https://github.com/smealum/ctrulib/issues/328)の制約でアスキー文字のみ正しく表示されます。それ以外の文字エンコードの場合文字化けはしますが、機能自体に影響はありません。

Banner music uses a modified version of ["Rad Adventure" by Scott Holmes](http://freemusicarchive.org/music/Scott_Holmes/).
