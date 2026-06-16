10 rem see readybasic_graphics_command_design.md
20 print chr$(147);"rbgfx04 rects"
30 gfxmode("hires"):gfxclear(0)
40 rect(8,8,311,191,1)
50 rect(32,24,288,176,1)
60 frect(64,48,128,96,1)
70 frect(192,96,256,160,1)
80 line(0,0,319,199,1)
90 line(0,199,319,0,1)
100 print "outline and filled rects"
