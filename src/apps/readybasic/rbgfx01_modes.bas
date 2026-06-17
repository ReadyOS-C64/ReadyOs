10 rem see graphics design md
20 print chr$(147);"rbgfx01 modes"
30 gfxmode("hires"):m%=gfxmode():print "hires mode";m%
40 gfxclear(0)
50 gfxmode("mbitmap"):m%=gfxmode():print "mbitmap mode";m%
60 gfxclear(1)
70 gfxmode("tile"):m%=gfxmode():print "tile mode";m%
80 gfxclear(2)
90 gfxmode("mtile"):m%=gfxmode():print "mtile mode";m%
100 gfxclear(3)
110 gfxtext()
120 print "back to text"
