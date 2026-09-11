10 rem =====================================
20 rem rbugfxsnddemo - orbital echoes / ultimate
30 rem art: luis zuno / ansimuz - cc0
40 rem opengameart.org/content/space-background-3
50 rem adapted: layers, exposure, c64 palette
60 rem music: summer vacation - shiru 2010
70 rem shiru.untergrund.net/music.shtml
80 rem cc by 3.0 - sidreloc $9200 / zp $fc-fd
90 rem ready lettering: original pixel art

95 rem requires c64u turbo registers, not manual mode
100 rem --- safe setup, before disk reads ---
105 uspeed(1)
110 if peek(49663)<>0 then :musdrop()
120 memcap(36864)
130 bc=peek(53280) and 15:bg=peek(53281) and 15
140 fc=peek(646)
150 print chr$(147);"ready / orbital echoes"
160 print "q: stop all   m: leave music playing"
170 print "image renews every 15 seconds"
180 print "precalculating motion - please wait"
190 dim s%(255),sy%(335)
200 dim lx%(511),ly%(511),rx%(511),ry%(511)
210 exec prep
220 zmodld("rbm.media",m%):h%=gfxsurf("mbitmap")
225 print "orbital show running"
230 exec scene
240 mustune("rb.summer"):musplay(1)
260 uspeed(16)
270 pp%=256:rt=ti:lt=rt:c%=1:mi%=0:rc=0

300 rem --- sprites follow time, lines run less often ---
310 repeat
315   repeat
320     p%=phase(peek(162))
325   until p%<>pp%
330   pp%=p%:exec glyphs
340   tt=ti:if tt<rt then rt=tt:lt=tt
350   if tt-rt<900 then 360
355   exec clean
360   if tt-lt<4 then 390
370   exec weave
375   lt=ti
390   get a$
400 until a$="q" or a$="m"
410 exec finish
420 if a$="m" then 500
430 musdrop():clr:memcap(40960)
440 print "all stopped - basic memory:";fre(0)
450 end

500 rem music still owns its protected memory
510 clr
520 print "music continues - memory stays reserved"
530 print "to release it:":print "musdrop():clr:memcap(40960)"
540 end

1000 rem --- all sine and coordinate work happens once ---
1010 proc prep()
1020   for i=0 to 255
1030     s%(i)=int(100*sin(i*0.0245436926))
1040   next i
1050   for i=0 to 255
1060     sy%(i)=78+int(s%(i)*0.16+0.5)
1070     lx%(i)=80+int(s%(i)*0.75)
1080     j=(i*2) and 255:ly%(i)=112+int(s%(j)*0.70)
1090     j=(i*3+64) and 255:rx%(i)=80+int(s%(j)*0.75)
1100     j=(i*5+96) and 255:ry%(i)=112+int(s%(j)*0.70)
1110     j=i+256
1120     lx%(j)=159-lx%(i):ly%(j)=199-ly%(i)
1130     rx%(j)=159-rx%(i):ry%(j)=199-ry%(i)
1140   next i
1150   rem extra samples eliminate wrapping per letter
1160   for i=0 to 79:sy%(i+256)=sy%(i):next i
1170 endp

1300 rem --- disk setup, then cache the complete scene ---
1310 proc scene()
1320   gfxmode("mbitmap"):gfxclear(0):border(0)
1330   for i=0 to 4
1340     sprset(i,1,3,0):sprmul(i,1)
1350     sprsize(i,1,1):sprpri(i,0)
1360     sprmove(i,56+i*52,78)
1370   next i
1380   sprmco(1,6):sprfile("rb.ready")
1390   mcfile("rb.neon")
1400   gfxtgt(h%):gfxsync():gfxtgt(0)
1410 endp

1600 rem --- no for loop or sine math in the hot path ---
1610 proc glyphs()
1620   sprmove(0,56,sy%(p%))
1630   sprmove(1,108,sy%(p%+20))
1640   sprmove(2,160,sy%(p%+40))
1650   sprmove(3,212,sy%(p%+60))
1660   sprmove(4,264,sy%(p%+80))
1670 endp

1900 rem --- one alternating line; preserve cell palettes ---
1910 proc weave()
1920   wi%=p%+mi%*256
1930   mcline(lx%(wi%),ly%(wi%),rx%(wi%),ry%(wi%),c%)
1940   mi%=1-mi%:c%=c%+1:if c%=4 then c%=1
1950 endp

2200 rem --- instant refresh from the owned reu surface ---
2210 proc clean()
2220   gfxblit(h%):rt=ti:rc=rc+1
2230 endp

2500 rem --- both exits restore the original text colors ---
2510 proc finish()
2520 uspeed(1)
2530   for i=0 to 4:sprset(i,0,0,0):next i
2540   bufdrop(h%):gfxtext():mcbg(bg):border(bc)
2550   poke 646,fc:print chr$(147);"orbital echoes complete"
2560   print "image refreshes:";rc
2570 endp

2800 rem jiffy low byte: 2.13-second wave, safe at midnight
2810 func phase(tk%)
2820   ret% (tk%*2) and 255
2830 endp
