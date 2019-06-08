# sdr-scanner
An ncurses-based fork of librtlsdr's rtl_fm.c which sends audio directly to pulseaudio (without needing to pipe stdout to play/aplay).

Hacks done by Alex Couture-Beil (VE7SCB); standing on the shoulders of giants who brought us librtlsdr.

A file of frequencies to scan is passed as the first argument.

## Example Usage:

    make && ./scaner ham-all.txt

## Example output:

    442.360 MHz                     [                       unknown]; signal: 0020; scan: 33% [manual]

    keys: j/k +/- 5kHz; [s]kip; spacebar: scanner pause; [q]uit

