10 rem see graphics design md
20 print chr$(147);"rbgfx21 mbitmap prims"
30 gfxmode("mbitmap"):gfxclear(0)
40 line(4,18,155,18,17)
50 line(4,22,155,70,34)
60 rect(8,82,72,144,20)
70 fbox(86,82,148,144,37)
80 circle(46,112,28,51)
90 fcircle(118,112,22,58)
100 for x=0 to 159 step 8
110 plot(x,170,16+(x/8)-int((x/8)/15)*15)
120 next x
130 zpause(30)
140 get a$:if a$="" then 130
150 gfxtext():print "mbitmap prims done"
