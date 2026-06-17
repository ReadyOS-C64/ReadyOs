10 rem see graphics design md
20 print chr$(147);"rbgfx14 phase2 prims"
30 gfxmode("hires"):gfxclear(0)
40 circle(82,82,38,1)
50 fcircle(210,92,26,1)
60 rect(180,58,240,126,1)
70 line(20,170,300,170,1)
80 zpause(30)
90 get a$:if a$="" then 80
100 gfxtext():print "phase2 prims done"
