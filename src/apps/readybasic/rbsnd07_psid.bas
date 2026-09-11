10 rem =====================================
20 rem rbsnd07 - music while basic works
30 rem summer vacation by shiru (2010)
40 rem https://shiru.untergrund.net/music.shtml
50 rem cc by 3.0: creativecommons.org/licenses/by/3.0/
60 rem modified: sidreloc to $9200; zp $fc-$fd
70 rem =====================================

100 rem --- main: read this as a story ---
110 rem reserve ram before making strings
120 memcap(36864)
130 bc=peek(53280) and 15
140 exec title
150 exec assets

200 rem play: basic animates the border
210 musplay(1)
220 print "playing - basic is still running"
230 exec animate(8)

300 rem halt keeps the song in reserved ram
310 mushalt()
320 print "halted - ram still reserved"
330 exec linger(2)

400 rem play again restarts the subtune
410 musplay(1)
420 print "restart from beginning"
430 exec animate(5)

500 rem detach before returning ram to basic
510 musdrop()
520 border(bc)
530 print "dropped - driver detached"
540 rem clr discards any dynamic strings
550 clr
560 memcap(40960)
570 print "memory returned:";fre(0)
580 print "rbsnd07 done"
590 end

1000 rem --- procedures: named actions ---
1010 proc title()
1020   print chr$(147);"rbsnd07 / summer vacation"
1030   print "music: shiru / cc by 3.0"
1040   print "proc / func / repeat / until"
1050   print
1060   print "reserved 4096 bytes: ";fre(0)
1070 endp

1200 proc assets()
1210   zmodld("rbm.media",m%)
1220   print "module commands:";m%
1230   rem palette: 16 bytes at $9e00
1240   rscfile("rb.colors")
1250   rem psid player and data at $9200
1260   mustune("rb.summer")
1270 endp

1400 proc animate(span)
1410   rem span is seconds; mk is start time
1420   mk=ti
1430   repeat
1440     p%=int(age(mk)*4) and 15
1450     border(shade(p%))
1460   until age(mk)>=span
1470 endp

1600 proc linger(span)
1610   rem the same clock, without animation
1620   mk=ti
1630   repeat
1640   until age(mk)>=span
1650 endp

2000 rem --- functions: return values ---
2010 func shade(ix%)
2020   rem look up a color in rb.colors
2030   ret% peek(40448+(ix% and 15))
2040 endp

2200 func age(mark)
2210   rem ti counts 60 ticks per second
2220   dt=ti-mark
2230   rem allow for midnight; true is -1
2240   ret (dt-5184000*(dt<0))/60
2250 endp
