/* SPDX-License-Identifier: GPL-3.0-only
 * WaveLoop SHAYPE integration: actual CoralFlower player, UI, storage and backup protocol. */
#define CF_TEST_EMBED 1
#include "cf_backup_test.c"

static void chord(void)
{
    cf_chord_out_t o = {0};
    o.sounding = 1; o.n = 4; o.note[0] = 60; o.note[1] = 64; o.note[2] = 67; o.note[3] = 71;
    cfa_chord(&o, o.n);
}
static void reset_shape(void)
{
    cfa_at = &cfa_k; cfa_stop();
    cfsh_defaults(&cfsh); cfp_shape = cfsh;
    cfp_cfg.arp.mode = CF_ARP_SHAYPE; cfp_cfg.arp.oct = 1; cfp_cfg.arp.gate = 100;
    cfp_cfg.arp.rate = 4; cfp_cfg.arp.swing = 50; cfp_cfg.arp.latch = 0;
    cfp_cfg.bpm = 120; cfa.on = 1; cf_grid.on = 0;
}
static int sounding(int note)
{
    for (unsigned i = 0; i < ARP_ROWS; i++) if (cfa.shleft[i] && cfa.shnote[i] == note) return 1;
    return 0;
}
int main(void)
{
    emu_flash_open(); memset(emu_flash, 0xFF, sizeof emu_flash); cr_bank_boot(); cf_ui_init();
    CHECK(CF_ARP_NRATES == 21, "straight, triplet and dotted divisions for seven note values");
    for (unsigned i = 0; i < CF_ARP_NRATES; i++) {
        CHECK(CF_ARP_RATE[i] && CF_ARP_UNITS[i], "rate %u has a label and nonzero period", i);
        if (i) CHECK(CF_ARP_UNITS[CF_ARP_ORDER_RATE[i-1]] > CF_ARP_UNITS[CF_ARP_ORDER_RATE[i]], "knob rates run slow to fast");
        CHECK(cfa_rate_turn(CF_ARP_ORDER_RATE[i], 0) == CF_ARP_ORDER_RATE[i], "stored ID maps to knob position");
        if (i+1 < CF_ARP_NRATES) CHECK(cfa_rate_turn(CF_ARP_ORDER_RATE[i], 1) == CF_ARP_ORDER_RATE[i+1], "knob reaches next rate");
    }
    CHECK(CF_ARP_UNITS[12] == 6 && CF_ARP_UNITS[13] == 4 && CF_ARP_UNITS[20] == 9, "64th periods: straight 6, triplet 4, dotted 9 ticks");
    reset_shape(); cfp_cfg.arp.rate = 12;
    CHECK(cfa_div64(cfa_step_len(0), 120u * 9600u) == FS / 32u, "64th gate duration at 120 BPM");
    cfp_cfg.arp.rate = 13;
    CHECK(cfa_div64(cfa_step_len(0), 120u * 9600u) == FS / 48u, "64th triplet gate duration at 120 BPM");
    cfp_cfg.arp.rate = 20;
    CHECK(cfa_div64(cfa_step_len(0), 120u * 9600u) == FS * 3u / 64u, "64th dotted gate duration at 120 BPM");
    /* 96 PPQN interpolation does not alter native 24 PPQN and stays monotonic through pulse jitter. */
    cfm.on = 0; cfm.running = 1; cfm.bpm = 120; cfx_in.active = 1; cfm_restart();
    cfm_tick(0, 0); uint32_t origin = cf_grid.g96, last = origin;
    for (unsigned pulse = 0; pulse < 48; pulse++) {
        for (unsigned block = 0; block < (pulse & 1 ? 9u : 7u); block++) {
            cfm_tick(CTL, 0);
            CHECK(cf_grid.g96 >= last, "interpolated MIDI phase advances monotonically"); last = cf_grid.g96;
            CHECK(cf_grid.g96 / 4u == cf_grid.g24, "interpolated phase stays within the native MIDI pulse");
        }
        cfm_tick(0, 1);
        CHECK(cf_grid.g96 == origin + 4u * (pulse+1u), "MIDI pulse anchors exactly four arp ticks"); last = cf_grid.g96;
    }
    cfx_in.active = 0; cfm_tick(CTL, 0);
    CHECK(cf_grid.g96 >= last, "fallback from external clock never runs arp phase backwards");
    cfm_restart(); cfm_tick(0,0); origin=cf_grid.g96;
    for (unsigned sample = 0; sample < FS / 2u; sample++) cfm_tick(1,0);
    CHECK(cf_grid.g96 == origin + 96u, "internal 120 BPM quarter is exactly 96 arp ticks");
    CHECK(cf_grid.g24 == cf_grid.g96 / 4u, "native sequencer remains on its 24 PPQN grid");
    cfm.running = 0; cf_grid.on = 0;
    reset_shape(); chord();
    CHECK(cfa.idx == 1 && sounding(60) && sounding(67), "default step starts two chord rows");
    cfa_step();
    CHECK(cfa.idx == 2 && sounding(64) && sounding(71), "second step plays its two rows");
    cfa_stop();
    CHECK(!sounding(60) && !sounding(64) && !sounding(67) && !sounding(71), "stop clears every SHAYPE gate");
    for (unsigned dir = 0; dir <= AD_SHYFT; dir++) for (unsigned factory = 0; factory < ARP_NFACTORY; factory++) {
        reset_shape(); ash_load(&cfp_shape.pat, factory); cfp_shape.steps = ARP_FACTORY[factory].steps;
        cfp_shape.bound = ARP_FACTORY[factory].bound; cfp_shape.dir = dir; chord();
        for (unsigned j = 0; j < 128; j++) { cfa_tick(FS / 4); cfa_step(); }
        cfa_stop();
        unsigned n = 0; for (unsigned j = 0; j < ARP_ROWS; j++) n += !!cfa.shleft[j];
        CHECK(!n, "factory %u direction %u releases all notes", factory, dir);
    }
    reset_shape(); chord();
    cfp_shape.attach = AA_CYCLE;
    cf_chord_out_t o = {0}; o.sounding = 1; o.n = 3; o.note[0] = 62; o.note[1] = 65; o.note[2] = 69;
    cfa_chord(&o, 3); cfa_step();
    CHECK(cfa.shheld[0] == 60, "CYCLE keeps the old chord for the current pass");
    while (cfa.idx <= 4) cfa_step();
    CHECK(cfa.shheld[0] == 62, "CYCLE attaches the next chord at the pass boundary");
    reset_shape(); chord(); cf_grid.on = 1; cf_grid.g96 = 148; cfa.locked = 1; cfa.gen = cf_grid.gen;
    cfa.rate = cfp_cfg.arp.rate; cfa.swing = 50; cfa.next96 = 192; cfa.idx = 7;
    cfp_cfg.arp.rate = 6; cfa_tick(1);
    CHECK(cfa.idx == 7 && cfa.next96 == 168, "rate change joins next grid slot without resetting the pattern");
    cf_grid.g96 = 168; cfa_tick(1);
    CHECK(cfa.idx == 8, "new rate fires at the joined slot");
    reset_shape(); chord(); cfp_shape.attach = AA_NOTE; cfa_chord(&o, 0);
    CHECK(sounding(60), "NOTE release lets the sounding gate finish");
    cfa_tick(FS * 8);
    CHECK(!cfa.n, "NOTE release stops after the last gate");
    reset_shape(); chord(); cfp_shape.attach = AA_NOW; cfa_chord(&o, 0);
    CHECK(!cfa.n && !sounding(60), "NOW release stops immediately");
    for (unsigned sw = 50; sw <= 75; sw += 25) for (unsigned old = 0; old < CF_ARP_NRATES; old++)
        for (unsigned rate = 0; rate < CF_ARP_NRATES; rate++) {
            reset_shape(); chord(); cf_grid.on = 1; cf_grid.g96 = 148;
            cfa.locked = 1; cfa.gen = cf_grid.gen; cfa.rate = old; cfa.swing = sw;
            cfp_cfg.arp.rate = rate; cfp_cfg.arp.swing = sw;
            cfa.next96 = 384; cfa.idx = 23; cfa_tick(1);
            CHECK(cfa.idx == 23, "rate %u -> %u swing %u retains cycle position", old, rate, sw);
        }
    reset_shape(); cfa_stop(); cfa_at = &cfa_sq[0]; cfa.on = 1; chord(); sqp.on = 1;
    CHECK(sounding(60) && sounding(67) && sqp_holds(0, 67), "sequencer chord SHAYPE owns every polyphonic pitch");
    cfp_cfg.arp.mode = CF_ARP_UP; cfa_tick(1);
    CHECK(!sounding(60) && !sounding(67) && cfa.cur, "sequencer mode switch clears all SHAYPE gates");
    cfa_stop(); cfa_at = &cfa_sq[1]; cfa.on = 1; cfp_cfg.arp.mode = CF_ARP_SHAYPE; chord();
    CHECK(sounding(60) && sounding(67), "sequencer layer also plays SHAYPE");
    cfa_stop(); cfa_at = &cfa_k; sqp.on = 0;
    /* Real panel knob callbacks edit, retain long notes, and send a coherent ISR snapshot. */
    cs.arp.mode = CF_ARP_SHAYPE; cu.scr = SCR_ARP; cu.page[SCR_ARP] = 3;
    cfsh_step = 63; cfsh_row = 7; cfsh_len = 64; cfsh.steps = 64;
    cfsh_knob(3, 1); int at = cfsh_cell();
    CHECK(at >= 0 && ARP_LEN(cfsh.pat.note[at]) == 64, "editor adds step64 row8 length64");
    cf_send_config();
    CHECK(cfp_shape.steps == 64 && cfp_shape.pat.n == cfsh.pat.n, "UI publishes full pattern to audio side");
    uint32_t before_lo = st_crc32(emu_flash, 0xA0000), before_hi = st_crc32(emu_flash + 0xA2000, sizeof emu_flash - 0xA2000);
    cfsh_poll(5000, 0); cfsh_poll(8000, 0);
    CHECK(cfsh_saved != cfs_fnv(&cfsh, sizeof cfsh), "SHAYPE autosave defers while audio is not quiet");
    cfsh_poll(10000, 1);
    CHECK(cfsh_saved == cfs_fnv(&cfsh, sizeof cfsh), "SHAYPE autosaves after edits settle and audio is quiet");
    CHECK(st_crc32(emu_flash, 0xA0000) == before_lo &&
          st_crc32(emu_flash + 0xA2000, sizeof emu_flash - 0xA2000) == before_hi,
          "SHAYPE save changes only its A0000/A1000 pair");
    CHECK(!cfsh_write(), "SHAYPE A/B storage write succeeds");
    cf_shape_t saved = cfsh; memset(&cfsh, 0, sizeof cfsh); cfsh_load();
    CHECK(!memcmp(&saved, &cfsh, sizeof saved), "SHAYPE reload keeps complete pattern/settings");
    cfs_restore_lock = 0; cfsh_restore_lock = 0;
    CHECK(t_list() == 0 && t_find(CFB_SHAPE) && t_find(CFB_SHAPE)->size == sizeof(cfsh_rec_t), "backup lists SHAYPE object68");
    obj_t shape = *t_find(CFB_SHAPE); CHECK(t_get_all(&shape) == 0, "SHAYPE GET CRC matches");
    CHECK(t_put(shape.id, shape.bytes, shape.size, shape.crc) == 0, "backup restores validated SHAYPE object");
    shape.bytes[0] ^= 1; shape.crc = st_crc32(shape.bytes, shape.size);
    CHECK(t_put(shape.id, shape.bytes, shape.size, shape.crc) == 2, "restore rejects another magic despite a matching wire CRC");
    printf("SHAYPE: %d checks, %d failures\n", checks, fails);
    return fails != 0;
}
