10 rem see graphics design md
20 print chr$(147);"rbgfx11 input"
30 for i=1 to 30
40 joy(2,j%)
50 keyp(k%)
60 keylast(l%)
70 print "joy";j%;" key";k%;" last";l%
80 keyscan()
90 next i
100 print "polling only, no irq sampler"
