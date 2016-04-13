# tsb-c #

So I went looking for a bootloader for ATtiny88, and ran across a
nice, small, serial-based bootloader for the whole tiny and mega AVR
line.  Looked great.  This bootloader was called TinySafeBoot and
is available [here] (http://jtxp.org/tech/tinysafeboot_en.htm)

Unfortunately, the tools for working with the firmware (uploads,
downloads, eeprom, etc) were written in some dialect of basic, and I
don't have that basic compiler available on the platforms I'm
interested in.

So these are the start of an alternative set of firmware management
utilities in C.

This is not to take anything away from the original author (Julien
Thomas)
