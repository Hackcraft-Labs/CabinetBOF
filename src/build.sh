#!/bin/bash

i686-w64-mingw32-gcc -c CabinetBOF.c -o CabinetBOF.x86.o
x86_64-w64-mingw32-gcc -c CabinetBOF.c -o CabinetBOF.x64.o
