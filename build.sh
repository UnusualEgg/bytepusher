#!/bin/env bash
gcc -lSDL3 -lm main.c SDL3_ttf/lib/libSDL3_ttf.a -lharfbuzz -lfreetype -o bytepusher
