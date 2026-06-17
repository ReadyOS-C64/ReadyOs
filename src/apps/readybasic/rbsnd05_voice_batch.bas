10 rem readybasic sound design: READYBASIC_SOUND_COMMAND_DESIGN.md
20 print chr$(147);"rbsnd05 voice batch"
30 print "VOICE uses packed sid envelope bytes:"
40 print "voice(v,f,wave,ad,sr)."
50 print "ad=$09 and sr=$c3 in decimal."
60 SIDCLR():VOL(15):PULSE(1,2048)
70 print:print "one batch command starts the voice"
80 VOICE(1,4455,65,9,195):ZPAUSE(80)
90 print "now raw CTRL turns gate off"
100 CTRL(1,64):ZPAUSE(50)
110 print "same voice, higher frequency"
120 VOICE(1,6672,65,9,195):ZPAUSE(80)
130 GATE(1,0):SILENCE()
140 print:print "rbsnd05 done"
