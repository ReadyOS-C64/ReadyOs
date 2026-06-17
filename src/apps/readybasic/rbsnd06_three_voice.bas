10 rem readybasic sound design: sound design md
20 print chr$(147);"rbsnd06 three voice"
30 print "three sid voices: c major, then"
40 print "a lower pulse bass with a high saw."
50 sidrst():vol(15)
60 adsr(1,0,5,12,4):adsr(2,0,5,12,4):adsr(3,0,5,12,4)
70 print:print "c major chord"
80 pitch(1,0,4):pitch(2,4,4):pitch(3,7,4)
90 wave(1,33):wave(2,33):wave(3,33):zpause(100)
100 gate(1,0):gate(2,0):gate(3,0):zpause(45)
110 print "pulse bass plus high saw"
120 pulse(1,1536):pitch(1,7,2):pitch(2,2,5)
130 wave(1,65):wave(2,33):zpause(110)
140 gate(1,0):gate(2,0):sidoff()
150 print:print "rbsnd06 done"
