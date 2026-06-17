10 rem see readybasic_graphics_command_design.md
20 print chr$(147);"rbgfx22 mbitmap point"
30 gfxmode("mbitmap"):gfxclear(0)
40 plot(20,40,17):plot(24,40,34):plot(28,40,51)
50 pnt(20,40,a%):pnt(24,40,b%):pnt(28,40,c%)
60 line(12,80,148,80,17)
70 line(12,92,148,124,34)
80 line(12,150,148,108,51)
90 zpause(30)
100 get a$:if a$="" then 90
110 gfxtext():print chr$(147);"mbitmap point:";a%;b%;c%
120 if a%<>1 or b%<>2 or c%<>3 then print "?mbitmap point fail":stop
130 print "mbitmap point done"
