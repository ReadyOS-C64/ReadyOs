10 rem see readybasic_graphics_command_design.md
20 print chr$(147);"rbgfx23 dlist"
30 gfxmode("hires"):gfxclear(0)
40 dlmake(12,h%)
50 dlplot(h%,80,60,1)
60 dlline(h%,20,20,300,160)
70 dlrect(h%,40,50,140,130)
80 dlfrect(h%,180,70,260,140)
90 dldraw(h%)
100 get a$:if a$="" then 100
110 gfxtext():print "phase4 dlist done"
