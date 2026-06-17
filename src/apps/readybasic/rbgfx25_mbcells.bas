10 rem see readybasic_graphics_command_design.md
20 print chr$(147);"rbgfx25 mbitmap cells"
30 gfxmode("mbitmap"):gfxclear(0):mcbg(0)
40 frect(8,16,31,47,17)
50 frect(40,16,63,47,34)
60 frect(72,16,95,47,51)
70 mcell(2,2,6,10,2)
80 mcell(10,2,4,12,7)
90 mcell(18,2,1,14,5)
100 line(4,120,150,170,49)
110 rect(20,72,140,112,50)
120 get a$:if a$="" then 120
130 gfxtext():print "phase4 mbitmap cells done"
