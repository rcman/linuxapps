Just the apps I add to my default install. I really should be using Ajile.

The Linuxapps.sh installs all the SDL2 and Allegro Libraries along with other apps I use all the time. 

It also sets up other things. You can remove what you do want and only install what you needs.

The SConstruct file always resides in the base if the build directory. Make a src, release and include folder, put your src, inlude in the appropriate directories and place all your sound and pics in release and them just run scons.

You make have to change your code to point to SDL2/SDL.h instead of just SDL.h and all other SDL libraries

When compiling with addition includes make sure to check the SConstrct file as it has a section to include libraries on compile.


