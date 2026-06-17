10 rem see graphics design md
20 print chr$(147);"rbgfx29 mtile visible"
30 gfxmode("mtile"):gfxclear(0)
40 plot(4,4,9):plot(5,4,10):plot(6,4,11)
50 line(1,22,38,2,6)
60 rect(5,4,21,17,13):fbox(25,7,35,19,15)
70 pnt(4,4,a%)
80 get a$:if a$="" then 80
90 gfxtext():print "mtile visible";a%
