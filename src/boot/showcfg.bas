5 print chr$(147)
10 print "readyos apps.cfg inspector"
20 print "drive 8, seq read"
30 print
40 open 2,8,2,"apps.cfg,s,r"
50 if st<>0 then print "open error st=";st:goto 900
60 dim b(24)
70 ln=1:tb=0:bc=0:ll=0:pg=0:nlf=0:l$=""
80 get#2,a$
90 if st=64 then gosub 500:goto 900
100 if a$="" then 80
110 tb=tb+1:c=asc(a$)
120 if c=13 or c=10 then if nlf=1 then nlf=0:goto 80
130 if c=13 or c=10 then gosub 500:nlf=1:goto 80
140 nlf=0
150 if bc<24 then bc=bc+1:b(bc)=c
160 if ll<38 then if c>=65 and c<=90 then l$=l$+chr$(c):ll=ll+1:goto 80
165 if ll<38 then if c>=97 and c<=122 then l$=l$+chr$(c-32):ll=ll+1:goto 80
170 if ll<38 then if c<32 or c>126 then l$=l$+".":ll=ll+1:goto 80
175 if ll<38 then l$=l$+a$:ll=ll+1
180 goto 80
500 if bc=0 and ll=0 then return
510 print ln;":";l$
520 print "bytes:";
530 for i=1 to bc:print b(i);:next
540 print
550 ln=ln+1:bc=0:ll=0:l$="":pg=pg+1
560 if pg<8 then return
570 print "press key...";:get k$:if k$="" then 570
580 print
590 pg=0
600 return
900 close 2
910 print
920 print "total bytes";tb
930 print "total lines";ln-1
940 print "done"
