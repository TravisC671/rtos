# TODO
1. Set up color pallete to load into vga driver [here](./src/doomgeneric/i_video.c#87)
2. Set up the init function to configure the vga driver
3. Set up video so that it writes to the vga buffer's address space [here](./src/doomgeneric/doomgeneric.c#21)
4. Configure the command line arguments properly. most likely leave them blank
5. get M_MakeDirectory running [here](./src/doomgeneric/m_misc.c#55)
6. move to FreeRTOS fs. shouldn't be too bad
7. create a loader project that can call the executable

# OPTIONAL
1. Convert [code](./src/doomgeneric/i_video.c#424) to vhdl block
2. Create a simple keyboard that can connect to the Nexys PMOD headers

Current model:
Loader freeRTOS: initializes freertos project & critical systems, then loads doom exe from SD card
doom freertos: initializes itself in ddr2 and & then takes over running