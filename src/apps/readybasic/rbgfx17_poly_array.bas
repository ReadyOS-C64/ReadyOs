10 rem see graphics design md
20 rem demo 17
30 dim p%(7),q%(9)
40 gfxmode("hires"):gfxclear(0)
50 p%(0)=50:p%(1)=35:p%(2)=140:p%(3)=35
60 p%(4)=170:p%(5)=105:p%(6)=85:p%(7)=145
70 poly(p%(0),4,1)
80 q%(0)=190:q%(1)=30:q%(2)=235:q%(3)=55
90 q%(4)=245:q%(5)=125:q%(6)=220:q%(7)=155
100 q%(8)=175:q%(9)=95
110 poly(q%(0),5,1)
120 line(20,175,250,175,1)
130 zpause(30)
140 get a$:if a$="" then 130
150 gfxtext():print "phase3 done 17"
