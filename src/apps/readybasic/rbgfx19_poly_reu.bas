10 rem see graphics design md
20 rem demo 19
30 gfxmode("hires"):gfxclear(0)
40 pbmake(5,h%)
50 pbufset(h%,0,70,40):pbufset(h%,1,155,45)
60 pbufset(h%,2,180,115):pbufset(h%,3,105,155)
70 pbufset(h%,4,45,95)
80 polyh(h%,5,1)
90 pbmake(3,g%)
100 pbufset(g%,0,215,45):pbufset(g%,1,245,135)
110 pbufset(g%,2,190,150)
120 polyh(g%,3,1)
130 rem demo
140 zpause(30)
150 get a$:if a$="" then 140
160 gfxtext():print "phase3 done 19"
