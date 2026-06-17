10 rem see readybasic_graphics_command_design.md
20 print chr$(147);"rbgfx30 mbitmap dlist"
30 gfxmode("mbitmap"):gfxclear(0):mcbg(0)
40 dlmake(12,h%)
50 dlplot(h%,20,28,17)
60 dlline(h%,4,38,155,66)
70 dlrect(h%,12,82,72,144)
80 dlfrect(h%,88,86,148,148)
90 dldraw(h%)
100 get a$:if a$="" then 100
110 gfxtext():print "mbitmap dlist done"
