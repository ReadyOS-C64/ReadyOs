10 rem see readybasic_graphics_command_design.md
20 print chr$(147);"rbgfx31 sync blit"
30 gfxmode("hires"):gfxclear(0)
40 line(0,0,319,199,1):rect(42,38,220,150,1)
50 h%=gfxsurf("hires")
60 gfxtgt(h%):gfxsync():gfxtgt(0)
70 get a$:if a$="" then 70
80 gfxclear(0)
90 get a$:if a$="" then 90
100 gfxblit(h%)
110 get a$:if a$="" then 110
120 gfxtext():print "sync blit done";errcode()
