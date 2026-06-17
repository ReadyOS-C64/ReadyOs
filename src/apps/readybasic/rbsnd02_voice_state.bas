10 rem readybasic sound design: sound design md
20 print chr$(147);"rbsnd02 voice state"
30 print "manual sid voice setup."
40 print "listen for a pulse pitch, then changed"
50 print "pulse width and envelope."
60 sidrst():vol(15)
70 print:print "wide pulse, soft release"
80 adsr(1,0,9,12,6):pulse(1,3072):frq(1,4455)
90 wave(1,64):gate(1,1):zpause(70):gate(1,0):zpause(50)
100 print "narrow pulse, snappier release"
110 adsr(1,0,3,15,2):pulse(1,512):frq(1,5612)
120 wave(1,64):gate(1,1):zpause(70):gate(1,0):zpause(50)
130 sidoff()
140 print:print "rbsnd02 done"
