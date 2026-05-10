; ROM image with the region number at the beginning of each region.
;
; Copyright 2026, EASilky
;
rgn0    .sect
        .byte   00h
        .word   05555h

rgn1    .sect
        .byte   01h
        .word   0555Ah

rgn2    .sect
        .byte   02h
        .word   055A5h

rgn3    .sect
        .byte   03h
        .word   05A55h

rgn4    .sect
        .byte   04h
        .word   0A555h

rgn5    .sect
        .byte   05h
        .word   0A55Ah

rgn6    .sect
        .byte   06h
        .word   0A5AAh

rgn7    .sect
        .byte   07h
        .word   0AAAAh

.end
