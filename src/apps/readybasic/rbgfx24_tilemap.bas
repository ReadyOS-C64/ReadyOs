10 rem see readybasic_graphics_command_design.md
20 print chr$(147);"rbgfx24 tilemap"
30 gfxmode("tile"):gfxclear(0)
40 chrmake(256,c%)
50 for r=0 to 7:chrrow(c%,1,r,255):next
60 chrrow(c%,2,0,255):chrrow(c%,2,1,129)
70 chrrow(c%,2,2,189):chrrow(c%,2,3,165)
80 chrrow(c%,2,4,165):chrrow(c%,2,5,189)
90 chrrow(c%,2,6,129):chrrow(c%,2,7,255)
100 chrrow(c%,3,0,24):chrrow(c%,3,1,60)
110 chrrow(c%,3,2,126):chrrow(c%,3,3,219)
120 chrrow(c%,3,4,24):chrrow(c%,3,5,24)
130 chrrow(c%,3,6,60):chrrow(c%,3,7,0)
140 for r=0 to 7:chrrow(c%,4,r,170):next
150 chruse(c%)
160 tsmake(64,t%):tmmake(1000,m%)
170 tsset(t%,0,32,0)
180 tsset(t%,1,1,2)
190 tsset(t%,2,2,5)
200 tsset(t%,3,3,7)
210 tsset(t%,4,4,1)
220 for x=0 to 39
230 tmset(m%,x,1,0):tmset(m%,960+x,1,0)
240 next
250 for y=0 to 24
260 i=y*40:tmset(m%,i,1,0):tmset(m%,i+39,1,0)
270 next
280 for x=6 to 33
290 tmset(m%,160+x,2,0):tmset(m%,320+x,3,0)
300 tmset(m%,480+x,4,0)
310 next
320 tmdraw(m%,t%)
330 get a$:if a$="" then 330
340 gfxtext():print "phase4 tilemap done"
