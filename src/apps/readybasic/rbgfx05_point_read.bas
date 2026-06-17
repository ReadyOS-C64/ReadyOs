10 rem see graphics design md
20 print chr$(147);"rbgfx05 pnt read"
30 gfxmode("hires"):gfxclear(0)
40 plot(40,40,1)
50 pnt(40,40,a%)
60 pnt(41,40,b%)
70 print "pnt 40,40";a%
80 print "pnt 41,40";b%
90 plot(40,40,0)
100 pnt(40,40,c%)
110 print "after clear";c%
