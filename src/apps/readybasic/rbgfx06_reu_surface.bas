10 rem see graphics design md
20 print chr$(147);"rbgfx06 reu surface"
30 h%=gfxsurf("hires")
40 print "surface handle";h%
50 gfxmode("hires"):gfxclear(0)
60 line(0,0,319,199,1)
70 gfxblit(h%)
80 print "blit validates handle in phase 1"
90 gfxtext()
