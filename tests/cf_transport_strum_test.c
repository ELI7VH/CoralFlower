/* SPDX-License-Identifier: GPL-3.0-only
 * Actual CoralFlower transport-phase, deferred strum MIDI and legacy SHAYPE import. */
#define CF_TEST_EMBED 1
#include "cf_backup_test.c"
static cf_chord_out_t chord;
static void reset(int mode, int sync)
{
    cfa_at=&cfa_k;cfa_stop();memset(&cfa_k,0,sizeof cfa_k);cfa_k.part=0xFF;cfa.on=1;
    cfsh_defaults(&cfsh);cfp_shape=cfsh;cfp_shape.sync=(uint8_t)sync;
    cfp_cfg.arp.mode=(uint8_t)mode;cfp_cfg.arp.oct=1;cfp_cfg.arp.rate=4;
    cfp_cfg.arp.gate=100;cfp_cfg.arp.swing=50;cfp_cfg.arp.latch=0;cfp_cfg.arp.strum_ms=40;
    usb.config=1;cfp_cfg.vel=100;cfp_cfg.midi_out=1;cfp_cfg.ch_chord=0;cfp_cfg.bpm=120;cfa.mode=(uint8_t)mode;
    cf_grid.on=1;cf_grid.transport=0;cf_grid.g96=96;cf_grid.gen++;
    memset(&chord,0,sizeof chord);chord.sounding=1;chord.n=4;
    chord.note[0]=60;chord.note[1]=64;chord.note[2]=67;chord.note[3]=71;
    mo_r=mo_w;
}
static int ons(int *notes)
{
    int count=0;
    while(mo_r!=mo_w){uint32_t p=midi_out_q[mo_r++%MQ];
        if(((p>>8)&0xFFu)==0x90u && ((p>>24)&127u)){if(notes)notes[count]=(p>>16)&127u;count++;}}
    return count;
}
static int active(void)
{
    int n=0;for(unsigned i=0;i<ARP_ROWS;i++)n+=!!cfa.shleft[i];return n;
}
int main(void)
{
    emu_flash_open();memset(emu_flash,0xFF,sizeof emu_flash);cr_bank_boot();cf_ui_init();
    reset(CF_ARP_SHAYPE,1);cfa_chord(&chord,4);cfa_tick(32);
    CHECK(!active() && !ons(0),"transport-locked chord waits while stopped, even with the metronome grid on");
    cf_grid.transport=1;cfa_tick(1);
    CHECK(cfa.idx==1 && ons(0)==2,"transport start fires global step zero once");
    cf_grid.transport=0;cfa_tick(1);
    CHECK(!active(),"transport stop clears all active gates");
    cf_grid.transport=1;cf_grid.gen++;cf_grid.g96=96;cfa_tick(1);
    CHECK(cfa.idx==1 && ons(0)==2,"transport restart resets to its downbeat, with the chord retained");
    reset(CF_ARP_SHAYPE,1);cf_grid.transport=1;cf_grid.g96=96+53;cfa_chord(&chord,4);cfa_tick(1);
    CHECK(!active() && !ons(0),"late chord does not attack off-grid");
    cf_grid.g96=96+96;cfa_tick(1);int notes[16];int n=ons(notes);
    CHECK(cfa.idx==3 && n==2 && notes[0]==67 && notes[1]==71,"late chord joins transport step two, not pattern zero");
    reset(CF_ARP_SHAYPE,1);cf_grid.transport=1;cf_grid.g96=96+192;cfp_shape.dir=AD_UP;
    cfa_chord(&chord,4);cfa_tick(1);n=ons(notes);
    CHECK(n==2 && notes[0]==64 && notes[1]==71,"late join uses the transport pass offset too");
    reset(CF_ARP_SHAYPE,1);cfa_chord(&chord,4);cfa_tick(1);cfp_shape.sync=0;cfa_tick(1);
    CHECK(ons(0)==2,"switching to Free while holding a stopped chord starts it");
    reset(CF_ARP_SHAYPE,1);cf_grid.transport=1;cfp_cfg.arp.latch=1;cfa_chord(&chord,4);cfa_tick(1);ons(0);
    cf_grid.transport=0;cfa_tick(1);cfa_chord(&chord,0);
    CHECK(cfa.n==4 && cfa.latched && !active(),"latch keeps a silent chord across stopped transport");
    cf_grid.transport=1;cf_grid.gen++;cf_grid.g96=96;cfa_tick(1);
    CHECK(ons(0)==2,"retained latched chord resumes on the restarted grid");
    for(unsigned rate=0;rate<CF_ARP_NRATES;rate++)for(unsigned sw=50;sw<=75;sw+=25){
        reset(CF_ARP_SHAYPE,1);cf_grid.transport=1;cfp_cfg.arp.rate=(uint8_t)rate;cfp_cfg.arp.swing=(uint8_t)sw;
        cfa_chord(&chord,4);unsigned u=CF_ARP_UNITS[rate],second=(2*u*sw+50)/100;
        for(unsigned pair=0;pair<32;pair++)for(unsigned half=0;half<2;half++){
            cf_grid.g96=96+2*pair*u+(half?second:0);cfa_tick(1);ons(0);
            CHECK(cfa.idx==2*pair+half+1,"transport cycle position stays exact across every rate/swing");
        }
    }
    for(int mode=CF_ARP_SHAPE_UP;mode<=CF_ARP_SHAPE_DOWN;mode++){
        reset(mode,1);cf_grid.transport=1;cfa_chord(&chord,4);cfa_tick(1);n=ons(notes);
        CHECK(n==1 && notes[0]==(mode==CF_ARP_SHAPE_UP?60:67),"SHAYPE strum starts at the requested pitch end");
        CHECK(!cfp_holds(0,mode==CF_ARP_SHAPE_UP?67:60),"pending strum pitch is not reported as sounding");
        cfa_tick(FS*40u/1000u-1u);CHECK(!ons(0),"strum's second note waits for GAP");
        cfa_tick(1);n=ons(notes);
        CHECK(n==1 && notes[0]==(mode==CF_ARP_SHAPE_UP?67:60),"second SHAYPE pitch follows in the requested direction");
    }
    reset(CF_ARP_SHAPE_UP,1);cf_grid.transport=1;cfa_chord(&chord,4);cfa_tick(1);ons(0);
    cf_grid.transport=0;cfa_tick(FS);CHECK(!ons(0) && !active(),"Stop cancels pending strum attacks");
    reset(CF_ARP_SHAPE_UP,1);cf_grid.transport=1;cfp_shape.attach=AA_NOW;cfa_chord(&chord,4);cfa_tick(1);ons(0);
    cf_grid.g96=97;chord.note[0]=62;cfa_chord(&chord,4);cfa_tick(FS/10);
    CHECK(!ons(0),"NOW cancels an old pending strum but cannot retrigger off the transport grid");
    reset(CF_ARP_SHAPE_UP,1);cf_grid.transport=1;cfp_cfg.arp.rate=13;cfp_cfg.arp.strum_ms=250;
    cfa_chord(&chord,4);cfa_tick(1);
    unsigned max_delay=0;for(unsigned i=0;i<ARP_ROWS;i++)if(cfa.shdelay[i]>max_delay)max_delay=cfa.shdelay[i];
    CHECK(max_delay<FS/48u,"64T strum spread stays within its source step");
    cfa_stop();cfa_tick(FS);CHECK(!active(),"panic/stop cannot leave pending gates behind");
    struct {uint32_t magic,check;uint8_t bytes[CFSH_OLD_BYTES];} old;
    cf_shape_t migrated;cfsh_defaults(&cfsh);cfsh.steps=64;cfsh.pat.note[0]=ARP_NOTE(63,7,64);
    old.magic=CFSH_OLD_MAGIC;memcpy(old.bytes,&cfsh,CFSH_OLD_BYTES);old.check=cfs_fnv(old.bytes,sizeof old.bytes);
    CHECK(cfsh_import(&migrated,&old,sizeof old)==2 && !migrated.sync && !memcmp(&migrated,&cfsh,CFSH_OLD_BYTES),"WHS1 pattern/settings import unchanged with Free as the compatibility default");
    old.check^=1;CHECK(!cfsh_import(&migrated,&old,sizeof old),"corrupt WHS1 checksum is refused");old.check^=1;
    CHECK(t_put(68,(const uint8_t*)&old,sizeof old,st_crc32(&old,sizeof old))==0,"restore still accepts a valid WHS1 backup");
    cfsh_restore_lock=0;cfsh_load();CHECK(cfsh.steps==64 && !cfsh.sync && cfsh_saved==0,"old record schedules quiet migration after loading");
    CHECK(!cfsh_write(),"quiet migration writes WHS2");cfsh_rec_t rec;
    CHECK(st_load(OBJ_CFSHAPE,&rec,sizeof rec)==sizeof rec && cfsh_valid(&rec),"migrated WHS2 record validates at its new size");
    printf("Transport/strum: %d checks, %d failures\n",checks,fails);return fails!=0;
}
