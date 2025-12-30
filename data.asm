.include "hdr.asm"

.section ".rodata1" superfree

; -= game background =-
back:
.incbin "back.pic"
back_end:

map:
.incbin "back.map"

palette:
.incbin "back.pal"

; -= Splash Screen =-
patterns_splash:
.incbin "splash.pic"
patterns_splash_end:

map_splash:
.incbin "splash.map"
map_splash_end:

palette_splash:
.incbin "splash.pal"
palette_splash_end:

; -= Ingame font =-
tilfont:
.incbin "pvsneslibfont.pic"

palfont:
.incbin "pvsneslibfont.pal"

gfxplayer:
.incbin "player.pic"
gfxplayer_end:

palplayer:
.incbin "player.pal"
palplayer_end:

gfxlaser:
.incbin "laser.pic"
gfxlaser_end:

pallaser:
.incbin "laser.pal"
pallaser_end:

.ends

.section ".rodata2" superfree

gfxalien:
.incbin "alien.pic"
gfxalien_end:

palalien:
.incbin "alien.pal"
palalien_end:

soundbrr:
.incbin "laserwav.brr"
soundbrrend:


.ends
