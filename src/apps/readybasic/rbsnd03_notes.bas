10 rem readybasic sound design: sound design md
20 print chr$(147);"rbsnd03 pitchs"
30 print "chromatic pitch command."
40 print "you should hear c through b, then c"
50 print "one octave higher."
60 sidrst():vol(15):adsr(1,0,5,12,3):wave(1,32)
70 for n=0 to 11
80 pitch(1,n,4):gate(1,1):zpause(22):gate(1,0):zpause(6)
90 next n
100 pitch(1,0,5):gate(1,1):zpause(45):gate(1,0)
110 sidoff()
120 print:print "rbsnd03 done"
