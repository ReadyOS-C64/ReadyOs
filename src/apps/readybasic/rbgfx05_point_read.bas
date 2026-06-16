10 rem see readybasic_graphics_command_design.md
20 print chr$(147);"rbgfx05 point read"
30 gfxmode("hires"):gfxclear(0)
40 plot(40,40,1)
50 pnt(40,40,a%)
60 pnt(41,40,b%)
70 print "point 40,40";a%
80 print "point 41,40";b%
90 plot(40,40,0)
100 pnt(40,40,c%)
110 print "after clear";c%
