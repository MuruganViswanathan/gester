# gester
Gester is a gesture interpreting program for  touchscreens that works directly on Linux input. Refer to https://www.kernel.org/doc/Documentation/input/input.txt
It has however been shown that this can run on touchscreens, touchpads and even a Nintendo Switch!
The gesture interpretation is being done directly in the program, not relying on other programs, which does allow for easy addition of gestures.

# configuration and installation
Everything that is supposed to be configured by the user can be found in the config.h file. 
I took this idea from the [suckless](https://suckless.org) tools, which I really like.
When everything is configured correctly one can just run the commands ```make``` and ```sudo make install``` which will install the compiled binary into /usr/local/bin/ .

### Update (4/21/2020): Added CMakeFile to support cmake builds. Do:
- `mkdir build`
- `cd build`
- `cmake ..` 
- `make`

# supported gestures
Currently the following gestures have been implemented:
* 1-5 finger swipes from top, bottom, left and right edges of the screen
* 2-5 finger swipes up, down, left and right
* 2 finger pinch in and pinch out
* 2-5 finger rotations clockwise and counterclockwise

# screen orientation
The accelerometer data will by default only be read once after startup to avoid too much file I/O, however if the program receives the SIGUSR1 signal, the accelerometer data will be reread.
I took this idea from using [polybar](https://github.com/polybar/polybar), which also reloads in this way. I use [thinkpad-l380-yoga-scripts](https://github.com/ffejery/thinkpad-l380-yoga-scripts)
to have my screen rotate, and just added a system call within that script to send the SIGUSR1 signal to gester.

# plans for the future
I do plan to add some more gestures in the future and am open to suggestions.
