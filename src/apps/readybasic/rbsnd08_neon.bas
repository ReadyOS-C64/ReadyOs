10 rem =====================================
20 rem rbsnd08 - ready / orbital echoes
30 rem art: luis zuno / ansimuz - cc0
40 rem opengameart.org/content/space-background-3
50 rem adapted: layers, exposure, c64 palette
60 rem music: summer vacation - shiru 2010
70 rem shiru.untergrund.net/music.shtml
80 rem cc by 3.0 - sidreloc $9200 / zp $fc-fd
90 rem ready sprite lettering: original art

100 rem --- setup: all disk reads happen here ---
110 memcap(36864)
120 bc=peek(53280) and 15
130 print chr$(147);"ready / orbital echoes"
140 print "space art: ansimuz / cc0"
150 print "summer vacation: shiru / cc by 3.0"
160 print "preparing picture, sprites and music"
170 print "q quits / image renews every 30 sec"
180 ldmod("rbm.media",m%)
190 dim s%(255)
200 for i=0 to 255
210 s%(i)=int(100*sin(i*0.0245436926))
220 next i
225 print "orbital show running"
230 h%=gfxsurf("mbitmap")
240 exec scene
250 mustune("rb.summer")
260 musplay(1)
270 tb=ti:rt=ti:rc=0:c%=1

300 rem --- show: clock-driven, additive trails ---
310 repeat
320   ft=ti
330   tm=ti-tb:tm=(tm-5184000*(tm<0))/60
335   tm=tm-int(tm/8)*8
340   p%=int(tm*32) and 255
350   exec glyphs
360   exec weave
370   c%=c%+1:if c%=4 then c%=1
375   ag=ti-rt:ag=(ag-5184000*(ag<0))/60
380   if ag<30 then 390
385   exec clean
390   get a$
400   rem cap fast machines at 30 updates/sec
410   repeat
420   until elapsed(ft)>=0.0333333
430 until a$="q"
440 exec finish
450 clr:memcap(40960)
460 print "basic memory returned:";fre(0)
470 end

1000 rem --- initialize the bank-d scene ---
1010 proc scene()
1020   gfxmode("mbitmap"):gfxclear(0):border(0)
1030   for i=0 to 4
1040     sprset(i,1,3,0)
1050     sprmul(i,1):sprsize(i,1,1):sprpri(i,0)
1055     sprmove(i,56+i*52,78)
1060   next i
1070   sprmco(1,6)
1080   rem sprset makes patterns: replace them now
1090   sprfile("rb.ready")
1100   mcfile("rb.neon")
1110   rem cache pixels, screen and color in reu
1120   gfxtgt(h%):gfxsync():gfxtgt(0)
1130 endp

1300 rem --- five independent sine-wave letters ---
1310 proc glyphs()
1320   for i=0 to 4
1330     ph%=(p%+i*20) and 255
1340     yy=78+s%(ph%)/10
1350     sprmove(i,56+i*52,yy)
1360   next i
1370 endp

1500 rem --- kinetic line art, not palette writes ---
1510 proc weave()
1520   x1=80+s%(p%)*0.75
1530   ph%=(p%*2) and 255
1540   y1=112+s%(ph%)*0.70
1550   ph%=(p%*3+64) and 255
1560   x2=80+s%(ph%)*0.75
1570   ph%=(p%*5+96) and 255
1580   y2=112+s%(ph%)*0.70
1590   rem slots 1-3 keep each 4x8 cell's colors
1600   mcline(x1,y1,x2,y2,c%)
1610   mcline(159-x1,199-y1,159-x2,199-y2,c%)
1620 endp

1800 rem --- a clean image, with no disk access ---
1810 proc clean()
1820   gfxblit(h%)
1830   rt=ti:rc=rc+1
1840 endp

2000 rem --- leave the machine as we found it ---
2010 proc finish()
2020   musdrop()
2030   for i=0 to 4:sprset(i,0,0,0):next i
2040   gfxtext():border(bc):bufdrop(h%)
2050   print chr$(147);"orbital echoes complete"
2060   print "image refreshes:";rc
2090 endp

2300 rem --- elapsed seconds, including midnight ---
2310 func elapsed(mark)
2320   dt=ti-mark
2330   ret (dt-5184000*(dt<0))/60
2340 endp
