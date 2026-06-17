10 rem see graphics design md
20 print chr$(147);"rbgfx32 convex poly"
30 gfxmode("hires"):gfxclear(0)
40 dim a%(7)
50 a%(0)=32:a%(1)=32:a%(2)=146:a%(3)=24
60 a%(4)=184:a%(5)=120:a%(6)=64:a%(7)=170
70 poly(a%(0),4,1)
80 dim b%(9)
90 b%(0)=196:b%(1)=36:b%(2)=278:b%(3)=58
100 b%(4)=292:b%(5)=132:b%(6)=238:b%(7)=178
110 b%(8)=178:b%(9)=108
120 fpoly(b%(0),5,1)
130 get a$:if a$="" then 130
140 gfxtext():print "convex poly done"
