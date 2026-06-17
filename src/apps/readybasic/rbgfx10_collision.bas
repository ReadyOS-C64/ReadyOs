10 rem see graphics design md
20 print chr$(147);"rbgfx10 collision"
30 gfxmode("tile"):gfxclear(0)
40 sprset(0,1,2,0)
50 sprset(1,1,3,0)
60 sprmove(0,120,100)
70 sprmove(1,124,104)
80 for t=1 to 60000:next t
90 sprcoll(0,c%):gfxtext()
100 print "sprite collision";c%
110 sprscan()
