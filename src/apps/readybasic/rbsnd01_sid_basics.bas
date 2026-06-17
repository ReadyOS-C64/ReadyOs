10 rem readybasic sound design: READYBASIC_SOUND_COMMAND_DESIGN.md
20 print chr$(147);"rbsnd01 sid basics"
30 print "you should hear triangle, saw, pulse,"
40 print "then noise. each tone is separate."
50 SIDCLR():VOL(15):ADSR(1,0,5,12,3)
60 print:print "triangle tone"
70 SOUND(1,4455,45,16):ZPAUSE(20)
80 print "saw tone"
90 SOUND(1,4455,45,32):ZPAUSE(20)
100 print "pulse tone"
110 PULSE(1,2048):SOUND(1,4455,45,64):ZPAUSE(20)
120 print "noise burst"
130 SOUND(1,4455,45,128):ZPAUSE(20)
140 SILENCE()
150 print:print "rbsnd01 done"
