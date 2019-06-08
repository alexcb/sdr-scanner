# sdr-scanner
An ncurses-based fork of rtl_fm.c which sends audio directly to pulseaudio (withing needing to pipe stdout to play/aplay).

A file of frequencies to scan is passed as the first argument.

## Example Usage:

    make && ./scaner ham-all.txt

## Example output:

    442.360 MHz                     [                       unknown]; signal: 0020; scan: 33% [manual]

    keys: j/k +/- 5kHz; [s]kip; spacebar: scanner pause; [q]uit

