10 rem see readybasic_graphics_command_design.md
20 rem demo 18
30 dim p%(7),q%(7)
40 gfxmode("hires"):gfxclear(0)
50 p%(0)=45:p%(1)=48:p%(2)=130:p%(3)=34
60 p%(4)=165:p%(5)=118:p%(6)=70:p%(7)=150
70 fpoly(p%(0),4,1)
80 q%(0)=185:q%(1)=42:q%(2)=245:q%(3)=70
90 q%(4)=230:q%(5)=150:q%(6)=175:q%(7)=115
100 fpoly(q%(0),4,1)
110 rect(18,22,252,176,1)
120 zpause(30)
130 get a$:if a$="" then 120
140 gfxtext():print "phase3 done 18"
