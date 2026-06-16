10 rem see readybasic_graphics_command_design.md
20 print chr$(147);"rbgfx12 showcase"
30 gfxmode("hires"):gfxclear(0)
40 rect(4,4,315,195,1)
50 for y=16 to 184 step 16
60 line(8,y,311,199-y,1)
70 next y
80 frect(136,72,184,120,1)
90 sprset(0,1,6,2):sprmove(0,160,100)
100 pnt(160,100,p%)
110 print "center point";p%
120 print "showcase done"
