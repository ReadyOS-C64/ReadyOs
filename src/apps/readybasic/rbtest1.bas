5 ldmod("rbm.sample1",m%)
10 echo1(p%)
20 print "readybasic";p%
30 for i=1 to 3
40 add16(i,10,a%)
50 print "loop";a%
60 next i
70 print "expradd";add16(5,6)
80 b=add16(8,9):print "exprass";b
90 s$="ready":t$=upper(s$):print "exprstr";t$
