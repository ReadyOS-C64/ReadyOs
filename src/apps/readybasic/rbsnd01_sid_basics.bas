10 rem readybasic sound design: sound design md
20 print chr$(147);"rbsnd01 sid basics"
30 print "you should hear triangle, saw, pulse,"
40 print "then noise. each tone is separate."
50 sidrst():vol(15):adsr(1,0,5,12,3)
60 print:print "triangle tone"
70 sound(1,4455,45,16):zpause(20)
80 print "saw tone"
90 sound(1,4455,45,32):zpause(20)
100 print "pulse tone"
110 pulse(1,2048):sound(1,4455,45,64):zpause(20)
120 print "noise burst"
130 sound(1,4455,45,128):zpause(20)
140 sidoff()
150 print:print "rbsnd01 done"
