# Contributing

I very much welcome a pull request from anyone wanting to contribute! Be it a small spelling mistake, a bug fix or otherwise.

## Getting started

First of all, devkitpro and ctrulib are required to set up for 3DS homebrew development. Ctrmus additionally requires a number of libraries to successfully compile, and these libraries can be compiled by using [3ds_portlibs](https://github.com/deltabeard/3ds_portlibs).

Note that libopus currently requires a small modification to make ctrmus compile:
* Change opusfile.h (located on my machine at "/opt/devkitpro/portlibs/armv6k/include/opus/opusfile.h") at line 110 to "# include \<opus/opus_multistream.h\>".

## Adding features
Create an issue before you start work so that others know that your considering working on adding a feature or fixing a bug.

## Pull requests
Please consider the following when making a pull request.
* Commit messages should be clear.
* Code style should be consistent.

If you're unsure whether to make a pull request, just make it. :smiley:
