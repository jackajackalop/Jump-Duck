# Game Information

Title: Jump Duck

Author: Yixin He (designed by clavika)

Design Document: [Jump Duck design document](http://graphics.cs.cmu.edu/courses/15-466-f18/game0-designs/clavicka/)

Screen Shot:

![Screen Shot](screenshot.png)

Difficulties Encountered:

I had lots of difficulties with everthing initially. Navigating the base code, moving things around the window, using blender were all things that made me suffer for extended periods of time. Things got easier as I went along though. 

At first, I had some difficulty adding the ability to reset my game, but that was fixed by changing the way I was going about it. 

Good Code:

I think I'm happy with the way the enemies move. I made it so that when the enemy geese collide, one would go in the opposite direction for a short period of time. The code is not particularly complex, but I think it really benefits the game by making the enemy movement more interesting.

## Base Code
The base code I used for this project can be found [here](https://github.com/ixchow/15-466-f18-base0).

## Runtime Build Instructions

The runtime code has been set up to be built with [FT Jam](https://www.freetype.org/jam/).

### Getting Jam

For more information on Jam, see the [Jam Documentation](https://www.perforce.com/documentation/jam-documentation) page at Perforce, which includes both reference documentation and a getting started guide.

On unixish OSs, Jam is available from your package manager:
```
	brew install ftjam #on OSX
	apt get ftjam #on Debian-ish Linux
```

On Windows, you can get a binary [from sourceforge](https://sourceforge.net/projects/freetype/files/ftjam/2.5.2/ftjam-2.5.2-win32.zip/download),
and put it somewhere in your `%PATH%`.
(Possibly: also set the `JAM_TOOLSET` variable to `VISUALC`.)

### Libraries

This code uses the [libSDL](https://www.libsdl.org/) library to create an OpenGL context, and the [glm](https://glm.g-truc.net) library for OpenGL-friendly matrix/vector types.
On MacOS and Linux, the code should work out-of-the-box if if you have these installed through your package manager.

If you are compiling on Windows or don't want to install these libraries globally there are pre-built library packages available in the
[kit-libs-linux](https://github.com/ixchow/kit-libs-linux),
[kit-libs-osx](https://github.com/ixchow/kit-libs-osx),
and [kit-libs-win](https://github.com/ixchow/kit-libs-win) repositories.
Simply clone into a subfolder and the build should work.

### Building

Open a terminal (or ```x64 Native Tools Command Prompt for VS 2017``` on Windows), change to the directory containing this code, and type:

```
jam
```

That's it. You can use ```jam -jN``` to run ```N``` parallel jobs if you'd like; ```jam -q``` to instruct jam to quit after the first error; ```jam -dx``` to show commands being executed; or ```jam main.o``` to build a specific file (in this case, main.cpp).  ```jam -h``` will print help on additional options.
