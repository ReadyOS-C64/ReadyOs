10 rem readybasic sound design: sound design md
20 print chr$(147);"rbsnd05 voice batch"
30 print "voice uses packed sid envelope bytes:"
40 print "voice(v,f,wave,ad,sr)."
50 print "ad=$09 and sr=$c3 in decimal."
60 sidrst():vol(15):pulse(1,2048)
70 print:print "one batch command starts the voice"
80 voice(1,4455,65,9,195):zpause(80)
90 print "now raw ctrl turns gate off"
100 ctrl(1,64):zpause(50)
110 print "same voice, higher frquency"
120 voice(1,6672,65,9,195):zpause(80)
130 gate(1,0):sidoff()
140 print:print "rbsnd05 done"
