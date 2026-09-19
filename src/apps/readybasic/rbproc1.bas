5 ldmod("rbm.sample1",m%)
10 print "procfunc"
20 exec show0
30 exec showi(7)
40 exec shows("yo")
50 a%=addlate(4,5)
60 print "sum";a%
70 a$=greet("ready")
80 print a$
90 print "chain":exec showi(8):print "after"
100 if 1 then :a%=addi(1,2)
110 print "if";a%
120 a%=outer(3)
130 print "nested";a%
140 print "efn";addi(6,7)
150 b=addi(2,3):print "efass";b
160 t$=greet("expr"):print t$
170 a%=idi(9):print "ret%";a%
180 t$=ids("ok"):print "ret$ ";t$
190 print "cex";abs(add16(1,6)-10)
200 t$=upper("mix"):print "cs$ ";t$
210 print "fpex";addi(1,2+4)
220 a%=add16(3,10):print "fcmd";a%
230 t$=funcupper("yo"):print "fs$ ";t$
240 print "fminus";addi(1,6)-10
250 print "fnabs";abs(addi(1,6)-10)
260 t$=left$(greet("ready"),2):print "fnleft ";t$
270 print "fparen";addi(1,(2+4))
280 print "cparen";add16(1,(2+4))
290 print "dparen";addi((1+2),(3+4))
300 print "fadd";fadd(1.2,2.3)
310 f=fadd(1.5,fadd(2.25,3.25)):print "nfadd";f
320 s=scale(2.25):print "scale";s
330 print "naddi";addi(1,addi(2,3))
340 print "fabs";abs(fadd(1.2,2.3)-3)
350 fadd(1.2,2.3,q):print "sfadd";q
360 t$=left$(greet("ready")+"!",3):print "cat ";t$
370 t$=left$(upper(greet("ready")),2):print "ngs ";t$
380 end
1000 proc show0()
1010 print "proc0"
1020 endp
1100 proc showi(p%)
1110 print "proci";p%
1120 endp
1200 proc shows(s$)
1210 print "procs ";s$
1220 endp
1300 func addi(x%,y%)
1310 ret x%+y%
1320 endp
1400 func greet(n$)
1410 r$="hi "+n$
1420 ret r$
1430 endp
1500 func inner(x%)
1510 ret x%+10
1520 endp
1600 func outer(x%)
1610 r%=inner(x%)
1620 r%=r%+1
1630 ret r%
1640 endp
1700 func addlate(x%,y%)
1710 r%=x%+y%
1720 ret r%
1730 endp
1800 func idi(x%)
1810 ret% x%
1820 endp
1900 func ids(s$)
1910 ret$ s$
1920 endp
2100 func funcupper(n$)
2110 r$=upper(n$)
2120 ret r$
2130 endp
2200 func scale(x)
2210 ret x*1.5
2220 endp
