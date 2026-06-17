10 rem readybasic sound design: READYBASIC_SOUND_COMMAND_DESIGN.md
20 print chr$(147);"rbsnd02 voice state"
30 print "manual sid voice setup."
40 print "listen for a pulse note, then changed"
50 print "pulse width and envelope."
60 SIDCLR():VOL(15)
70 print:print "wide pulse, soft release"
80 ADSR(1,0,9,12,6):PULSE(1,3072):FREQ(1,4455)
90 WAVE(1,64):GATE(1,1):ZPAUSE(70):GATE(1,0):ZPAUSE(50)
100 print "narrow pulse, snappier release"
110 ADSR(1,0,3,15,2):PULSE(1,512):FREQ(1,5612)
120 WAVE(1,64):GATE(1,1):ZPAUSE(70):GATE(1,0):ZPAUSE(50)
130 SILENCE()
140 print:print "rbsnd02 done"
