10 rem see readybasic_graphics_command_design.md
20 print chr$(147);"rbgfx16 sprite controls"
30 gfxmode("tile"):gfxclear(0)
40 for y=7 to 12
50 for x=13 to 24
60 tile(x,y,160,6)
70 next x
80 next y
90 sprset(0,1,2,0):sprmove(0,120,86)
100 sprrow(0,0,0,60,0):sprrow(0,1,0,126,0)
110 sprrow(0,2,0,255,0):sprrow(0,3,1,255,128)
120 sprrow(0,4,3,255,192):sprrow(0,5,7,255,224)
130 sprrow(0,6,15,255,240):sprrow(0,7,31,255,248)
140 sprrow(0,8,63,255,252):sprrow(0,9,127,255,254)
150 sprrow(0,10,255,255,255):sprrow(0,11,127,255,254)
160 sprrow(0,12,63,255,252):sprrow(0,13,31,255,248)
170 sprrow(0,14,15,255,240):sprrow(0,15,7,255,224)
180 sprrow(0,16,3,255,192):sprrow(0,17,1,255,128)
190 sprrow(0,18,0,255,0):sprrow(0,19,0,126,0)
200 sprrow(0,20,0,60,0)
210 zpause(30)
220 get a$:if a$="" then 210
230 sprsize(0,1,1):sprmove(0,134,92)
240 zpause(30)
250 get a$:if a$="" then 240
260 sprmco(5,14)
270 sprmul(0,1)
280 sprpri(0,1)
290 sprcol(0,3)
300 zpause(30)
310 get a$:if a$="" then 300
320 gfxtext():print "phase2 sprites done"
