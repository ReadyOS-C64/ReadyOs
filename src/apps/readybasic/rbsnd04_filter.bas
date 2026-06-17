10 rem readybasic sound design: sound design md
20 print chr$(147);"rbsnd04 filter"
30 print "saw wave through sid filter."
40 print "listen for low-pass, band-pass,"
50 print "then high-pass color changes."
60 sidrst():vol(15):adsr(1,0,9,15,4)
70 frq(1,2230):wave(1,33)
80 print:print "low-pass sweep-ish steps"
90 for c=150 to 950 step 200:filter(c,8,1,1):zpause(35):next c
100 print "band-pass"
110 filter(700,12,1,2):zpause(90)
120 print "high-pass"
130 filter(700,12,1,4):zpause(90)
140 gate(1,0):sidoff()
150 print:print "rbsnd04 done"
