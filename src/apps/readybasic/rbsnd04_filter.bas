10 rem readybasic sound design: READYBASIC_SOUND_COMMAND_DESIGN.md
20 print chr$(147);"rbsnd04 filter"
30 print "saw wave through sid filter."
40 print "listen for low-pass, band-pass,"
50 print "then high-pass color changes."
60 SIDCLR():VOL(15):ADSR(1,0,9,15,4)
70 FREQ(1,2230):WAVE(1,33)
80 print:print "low-pass sweep-ish steps"
90 for c=150 to 950 step 200:FILTER(c,8,1,1):ZPAUSE(35):next c
100 print "band-pass"
110 FILTER(700,12,1,2):ZPAUSE(90)
120 print "high-pass"
130 FILTER(700,12,1,4):ZPAUSE(90)
140 GATE(1,0):SILENCE()
150 print:print "rbsnd04 done"
