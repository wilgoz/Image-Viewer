# Image Viewer
My first "toy" SDL program, rolls images (.png) through arrow keys
> Requires a C++17 capable compiler (>= GCC 8 / Clang 5)

## Features
*   Left arrow key to roll backward, right to roll forward
*   Supports file/directory drops
*   Adaptive image clamping on window resizes

## Installing prerequisites
*   sudo apt-get install libsdl2-dev
*   sudo apt-get install libsdl2-image-dev

## Running
*   `make` to compile (g++)
*   `./img-viewer data/*.png` to test
