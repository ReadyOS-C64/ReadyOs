10 rem readybasic sound design: READYBASIC_SOUND_COMMAND_DESIGN.md
20 print chr$(147);"rbsnd03 notes"
30 print "chromatic NOTE command."
40 print "you should hear c through b, then c"
50 print "one octave higher."
60 SIDCLR():VOL(15):ADSR(1,0,5,12,3):WAVE(1,32)
70 for n=0 to 11
80 NOTE(1,n,4):GATE(1,1):ZPAUSE(22):GATE(1,0):ZPAUSE(6)
90 next n
100 NOTE(1,0,5):GATE(1,1):ZPAUSE(45):GATE(1,0)
110 SILENCE()
120 print:print "rbsnd03 done"
