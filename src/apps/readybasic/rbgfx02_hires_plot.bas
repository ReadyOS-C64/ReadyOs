10 rem see graphics design md
20 print chr$(147);"rbgfx02 hires plot"
30 gfxmode("hires"):gfxclear(0)
40 for x=0 to 319 step 16
50 plot(x,0,1):plot(x,199,1)
60 next x
70 for y=0 to 199 step 16
80 plot(0,y,1):plot(319,y,1)
90 next y
100 for i=0 to 199
110 plot(i,i,1)
120 next i
130 print "grid and diagonal"
