10 rem see graphics design md
20 print chr$(147);"rbgfx09 sprites"
30 gfxmode("tile"):gfxclear(0)
40 sprset(0,1,2,0)
50 sprset(1,1,7,1)
60 sprmove(0,80,80)
70 sprmove(1,180,110)
80 sprcol(0,5)
90 for t=1 to 60000:next t
100 gfxtext():print "two hardware sprites"
