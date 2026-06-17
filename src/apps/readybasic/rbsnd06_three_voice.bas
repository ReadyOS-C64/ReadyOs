10 rem readybasic sound design: READYBASIC_SOUND_COMMAND_DESIGN.md
20 print chr$(147);"rbsnd06 three voice"
30 print "three sid voices: c major, then"
40 print "a lower pulse bass with a high saw."
50 SIDCLR():VOL(15)
60 ADSR(1,0,5,12,4):ADSR(2,0,5,12,4):ADSR(3,0,5,12,4)
70 print:print "c major chord"
80 NOTE(1,0,4):NOTE(2,4,4):NOTE(3,7,4)
90 WAVE(1,33):WAVE(2,33):WAVE(3,33):ZPAUSE(100)
100 GATE(1,0):GATE(2,0):GATE(3,0):ZPAUSE(45)
110 print "pulse bass plus high saw"
120 PULSE(1,1536):NOTE(1,7,2):NOTE(2,2,5)
130 WAVE(1,65):WAVE(2,33):ZPAUSE(110)
140 GATE(1,0):GATE(2,0):SILENCE()
150 print:print "rbsnd06 done"
