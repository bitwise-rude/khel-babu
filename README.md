# khel-babu

This is my attempt to completely create a game boy emulator (hardware and software) using esp32

I will post updates below, including what I am working on, TODOs, bugs, etc while this is during the 
development phase.

You can contribute by contacting me through my FB profile in github
or through e-mail: dareludum@gmail.com
> Thank You

## Checkpoint #1
The CPU is almost done, I haven't added support for every Opcode yet but for most of the non-prefixed and prefixed ones are added and more will be added later.
I am taking a break because CPU took a awful lot of time. I will be refactoring the cpu code througout this week and maybe will start workig on the PPU
I have implemented DEBUG outputs and loggings which can be turned on or off as needed.
CPU isn't fully optimized but 100000000 took 6 seconsd with -O3 flag so i think its okay
I couldva used switch case ladder instead of function pointer but i am living with this.
This much for today meet you at PPU.


# References That I am currently Using
https://gbdev.io/pandocs/Memory_Map.html
https://meganesu.github.io/generate-gb-opcodes/
https://github.com/robert/gameboy-doctor
http://blog.kevtris.org/blogfiles/Nitty%20Gritty%20Gameboy%20VRAM%20Timing.txt
https://github.com/Ashiepaws/GBEDG/blob/master/timers/index.md