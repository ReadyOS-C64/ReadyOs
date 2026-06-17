10 rem see graphics design md
20 print chr$(147);"rbgfx26 mode matrix"
30 gfxmode("hires"):gfxclear(0)
40 plot(20,20,1):line(4,36,300,180,1)
50 rect(32,48,138,112,1):fbox(178,72,260,140,1)
60 circle(82,150,28,1):pnt(20,20,a%)
70 get a$:if a$="" then 70
80 gfxmode("mbitmap"):gfxclear(0):mcbg(0)
90 plot(20,20,17):line(4,36,155,180,34)
100 rect(8,70,70,128,20):fbox(88,78,148,142,51)
110 circle(48,150,24,49):pnt(20,20,b%)
120 get a$:if a$="" then 120
130 gfxmode("tile"):gfxclear(0)
140 plot(4,4,5):line(1,1,38,20,6)
150 rect(3,5,20,16,7):fbox(24,8,34,18,2)
160 pnt(4,4,c%)
170 get a$:if a$="" then 170
180 gfxmode("mtile"):gfxclear(0)
190 plot(4,4,9):line(1,22,38,2,10)
200 rect(5,4,21,17,11):fbox(25,7,35,19,12)
210 pnt(4,4,d%)
220 get a$:if a$="" then 220
230 gfxtext():print "matrix";a%;b%;c%;d%
240 print "mode matrix done"
