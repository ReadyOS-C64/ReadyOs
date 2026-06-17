10 rem see graphics design md
20 print chr$(147);"rbgfx27 target blit"
30 h%=gfxsurf("hires")
40 print "surf";h%;" err";errcode()
50 gfxtgt(h%):print "target surf err";errcode()
60 gfxsync():print "sync err";errcode()
70 gfxblit(h%):print "blit err";errcode()
80 gfxtgt(0):print "target visible err";errcode()
90 gfxmode("hires"):gfxclear(0)
100 line(0,0,319,199,1):rect(40,40,220,150,1)
110 get a$:if a$="" then 110
120 gfxtext():print "target blit done"
