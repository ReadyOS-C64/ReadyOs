10 rem see readybasic_graphics_command_design.md
20 rem demo 20
30 gfxmode("hires"):gfxclear(0)
40 pbufnew(4,h%)
50 pbufset(h%,0,42,58):pbufset(h%,1,118,35)
60 pbufset(h%,2,168,120):pbufset(h%,3,72,156)
70 fpolyh(h%,4,1)
80 pbufnew(5,g%)
90 pbufset(g%,0,185,42):pbufset(g%,1,244,55)
100 pbufset(g%,2,248,112):pbufset(g%,3,226,158)
110 pbufset(g%,4,170,106)
120 fpolyh(g%,5,1)
130 line(15,182,250,182,1):rect(176,24,252,170,1)
140 rem demo
150 zpause(30)
160 get a$:if a$="" then 150
170 gfxtext():print "phase3 done 20"
