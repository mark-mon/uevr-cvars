This project allows you to load cvars and exec commands into your UEVR game. This way UEVR profiles can have the cvars distributed with them rather than require the user to make engine.ini changes.
This requires a cvars.txt file to be next to cvars.dll. The format of that files is:

<code>
# This lets you script cvar and console commands form a file. The commands must go in this text file which must be
# present in the same plugins folder with cvars.dll.
#
# Lines that start with #, empty, or spaces are ignored.
# There should be no extra white space. command=value not command = value
# CVars must exist in the game already in order to work. You can use the UEVR console to dump all the cvars and adjust them
# here.
#
# Commands supported are:
#   cvar=value
#   exec=command
#   delay=millseconds <-- use only if you need a delay before a command or cvar is issued. Not common.
#
# exec=stat fps  <-- this enables unreal engine FPS counter.
# r.PostProcessAAQuality=1  <-- this is how you would set a cvar.
#
# For borderlands 3 you can uncomment these
#r.LandscapeLODBias=999
#r.SkeletalMeshLODBias=-999
exec=stat fps
</code>
