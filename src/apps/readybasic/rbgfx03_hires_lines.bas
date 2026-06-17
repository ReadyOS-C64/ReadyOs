10 rem see graphics design md
20 print chr$(147);"rbgfx03 hires lines"
30 gfxmode("hires"):gfxclear(0)
40 for y=0 to 199 step 20
50 line(0,0,319,y,1)
60 next y
70 for x=0 to 319 step 32
80 line(319,199,x,0,1)
90 next x
100 line(0,199,319,0,1)
110 print "fan lines"
