10 rem see graphics design md
20 print chr$(147);"rbgfx08 tile"
30 gfxmode("tile"):gfxclear(0)
40 for y=0 to 23 step 2
50 for x=0 to 38 step 2
60 plot(x,y,1):plot(x+1,y+1,1)
70 next x
80 next y
90 rect(2,2,37,22,5)
100 pnt(3,3,a%)
110 print "tile cells use plot";a%
