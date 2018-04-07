# Xi2p I2P Router Windows Installers #

Copyright (c) 2017-2018, The Xi2p Project

## Introduction ##

This is a pair of *Inno Setup* scripts `Xi2p64.iss` and
`Xi2p32.iss` plus some related files that allow you to build
standalone Windows installers (.exe) for the
Xi2p I2P Router software.

This turns Xi2p into a more or less standard Windows program,
by default installed into a subdirectory of `C:\Program Files`,
a program group with some icons in the *Start* menu, and automatic
uninstall support.

The Inno Setup scripts have to refer to files and directories of the
software to install by name; therefore the addition of files and/or
directories in the future may require modifications of the scripts. 

## License ##

See [LICENSE](LICENSE).

## Building ##

You can only build on Windows, and the results are always
Windows .exe files that can act as standalone installers for Xi2p.

The build steps in detail:

1. Ensure that [Xi2p](https://github.com/incognito-currency/xi2p) is cloned and built (see building instructions for details)
2. Install *Inno Setup*. You can get it from [here](http://www.jrsoftware.org/isdl.php)
3. Start Inno Setup then load and compile either `Xi2p64.iss` or `Xi2p32.iss` (depending on your architecture) in the `pkg\installers\windows` directory (Inno Setup scripts plus related files are all in this directory). Optional: for a command-line build, run `ISCC.exe` in the Inno Setup Program Files directory
4. The results i.e. the finished installers will be `Xi2pSetup64.exe` or `Xi2pSetup32.exe` in the repo's root `build` directory
