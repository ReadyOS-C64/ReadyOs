/* Device-8 persistent boot location. Disk operations finish before suspension. */
#include <cbm.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../lib/tui.h"
#include "config.h"
extern unsigned char bridge_boot_config[117];
static char device[3], partition[4], path[62], bootfile[17];
static char diskbuf[128], draft[62], message[40];
static TuiInput input;
static unsigned char field, key, valid, dirty;
static unsigned int n;
static char *fields[] = {device, partition, path, bootfile};
static const char *labels[] = {"Device (8-30)", "Partition (0: leave current)",
    "Directory (empty: leave current)", "Boot filename"};
static const unsigned char limits[] = {2,3,61,16};
static const TuiRect heading = {0,0,40,3};

static unsigned char number(const char *s, unsigned int max) {
    unsigned int value = 0;
    if (!*s) return 0;
    while (*s) {
        if (*s < '0' || *s > '9') return 0;
        value = value * 10 + *s++ - '0';
        if (value > max) return 0;
    }
    return 1;
}
static unsigned char safe_text(const char *s) {
    while (*s) {
        if ((unsigned char)*s < 32 || *s == ',' || *s == ':' ||
            *s == '*' || *s == '?' || *s == '@') return 0;
        ++s;
    }
    return 1;
}
static unsigned char check(void) {
    return number(device,30) && atoi(device)>=8 && number(partition,255)
        && *bootfile && safe_text(bootfile) && safe_text(path);
}
static void defaults(unsigned char ide) {
    strcpy(device, ide ? "12" : "8");
    strcpy(partition, ide ? "1" : "0");
    strcpy(path, ide ? "//os" : "");
    strcpy(bootfile,"booter");
}
void bridge_config_load(unsigned char ide) {
    char *p, *end;
    defaults(ide);
    valid=0;
    if (!cbm_open(2,8,2,"c64os.cfg,s,r")) {
        n=cbm_read(2,diskbuf,sizeof(diskbuf)-1);
        if (n < sizeof(diskbuf)) {
            diskbuf[n]=0;
            if (!strncmp(diskbuf,"rbg1",4) && (diskbuf[4]==13 || diskbuf[4]==10)) {
                p=diskbuf+5;
                for (field=0;field<4;++field) {
                    end=strchr(p,diskbuf[4]);
                    if (!end || end-p>limits[field]) break;
                    *end=0; strcpy(fields[field],p); p=end+1;
                }
                valid=field==4 && !*p && check();
            }
        }
    }
    cbm_close(2);
    if (!valid) defaults(ide);
    dirty=0;
}
void bridge_config_stage(void) {
    memset(bridge_boot_config,0,117);
    bridge_boot_config[0]=atoi(device);
    strcpy((char*)bridge_boot_config+4,bootfile);
    bridge_boot_config[3]=strlen(bootfile);
    if (atoi(partition)) {
        sprintf((char*)bridge_boot_config+21,"cp%s",partition);
        bridge_boot_config[1]=strlen((char*)bridge_boot_config+21);
    }
    if (*path) {
        sprintf((char*)bridge_boot_config+53,"cd%s",path);
        bridge_boot_config[2]=strlen((char*)bridge_boot_config+53);
    }
}
static unsigned char save(void) {
    int amount;
    /* cc65's PETSCII char map remaps newline escapes, even hex escapes.
     * Emit numeric CR through %c; accept older LF records when loading. */
    sprintf(diskbuf,"rbg1%c%s%c%s%c%s%c%s%c",13,device,13,partition,13,path,13,bootfile,13);
    n=strlen(diskbuf);
    if (cbm_open(2,8,2,"@0:c64os.cfg,s,w")) {cbm_close(2);return 0;}
    amount=cbm_write(2,diskbuf,n);
    cbm_close(2);
    if (amount!=n) return 0;
    /* Read back before reporting success; catch drive-full/write failures. */
    if (cbm_open(2,8,2,"c64os.cfg,s,r")) {cbm_close(2);return 0;}
    /* Compare in small chunks so cc65 needs no large stack buffer. */
    for (field=0;field<n;++field) {
        if (cbm_read(2,draft,1)!=1 || draft[0]!=diskbuf[field]) break;
    }
    cbm_close(2);
    return field==n;
}
static void draw(void) {
    tui_clear(TUI_THEME_BG);
    tui_window_title(&heading,"C64OS BOOT LOCATION",TUI_THEME_BORDER,TUI_THEME_TITLE);
    for (field=0;field<4;++field) {
        tui_puts(1,4+field*3,labels[field],TUI_COLOR_GRAY3);
        tui_puts_n(2,5+field*3,fields[field],36,TUI_COLOR_WHITE);
    }
    tui_puts(1,17,"1-4: edit   S: save   RETURN: back",TUI_COLOR_CYAN);
    tui_puts(1,19,"Saved on device 8 as c64os.cfg",TUI_COLOR_GRAY3);
    tui_puts(1,20,"Location applies to the next cold boot.",TUI_COLOR_GRAY3);
    tui_puts(1,21,"C64OS 1.09 + Bridge Setup cap required.",TUI_COLOR_GRAY3);
    tui_puts_n(0,24,message,40,TUI_COLOR_LIGHTGREEN);
}
void bridge_config_edit(void) {
    strcpy(message,valid ? "Saved location loaded" : "Defaults; save to keep this location");
    draw();
    for (;;) {
        key=tui_getkey();
        if (key==13 || key==3 || key==133) {
            if (check()) return;
            strcpy(message,"Invalid location; correct before leaving"); draw();
        }
        if (key=='s') {
            if (!check()) strcpy(message,"Invalid location; check fields");
            else if (save()) {valid=1;dirty=0;strcpy(message,"Saved and verified on device 8");}
            else strcpy(message,"Save failed; check device 8 disk");
            draw();
        } else if (key>='1' && key<='4') {
            field=key-'1';
            tui_input_init(&input,2,5+field*3,36,limits[field],draft,TUI_COLOR_WHITE);
            strcpy(draft,fields[field]); input.cursor=strlen(draft);
            for (;;) {
                tui_input_draw(&input); key=tui_getkey();
                if (key==3) break;
                if (tui_input_key(&input,key)) {
                    strcpy(fields[field],draft); dirty=1; break;
                }
            }
            strcpy(message,"Edits apply now; S saves for next boot");
            draw();
        }
    }
}
