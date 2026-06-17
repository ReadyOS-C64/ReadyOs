10 rem see readybasic_graphics_command_design.md
20 print chr$(147);"rbgfx28 tile visible"
30 gfxmode("tile"):gfxclear(0)
40 plot(4,4,5):plot(5,4,6):plot(6,4,7)
50 line(1,1,38,20,3)
60 rect(3,5,20,16,10):frect(24,8,34,18,12)
70 pnt(4,4,a%)
80 get a$:if a$="" then 80
90 gfxtext():print "tile visible";a%
