10 rem see readybasic_graphics_command_design.md
20 print chr$(147);"rbgfx07 mbitmap"
30 gfxmode("mbitmap"):gfxclear(0)
40 for y=10 to 190 step 12
50 line(0,y,159,199-y,7)
60 next y
70 for x=16 to 144 step 16
80 plot(x,100,55)
90 next x
100 print "multicolor bitmap register path"
