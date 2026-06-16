10 rem see readybasic_graphics_command_design.md
20 print chr$(147);"rbgfx15 phase2 tiles"
30 gfxtext():print chr$(147)
40 for y=4 to 16
50 for x=6 to 33
60 tile(x,y,160,1+(x+y)-int((x+y)/8)*8)
70 next x
80 next y
90 charat(10,7,160,2):charat(11,7,160,3)
100 charat(12,7,160,4):charat(13,7,160,5)
110 charat(10,9,160,6):charat(11,9,160,7)
120 charat(12,9,160,8):charat(13,9,160,9)
130 zpause(30)
140 get a$:if a$="" then 130
150 gfxtext():print "phase2 tiles done"
