		mame2009-libretro (0.135U4)


		WIP libretro port of mame0.135U4

-----------------------------------------------------------------------------------------

Mainly design for wii U , but sould work on linux/pi. 

Remember,Everything not working well,
It's a debug release , so expect to more bug.
(use at your own risk)

-----------------------------------------------------------------------------------------


full build Wii U: 
make -f Makefile PTR64=0 TINY=0 platform=wiiu -j10

tiny build Wii U:
make -f Makefile PTR64=0 TINY=1 platform=wiiu -j10


