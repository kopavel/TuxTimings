#ifndef UI_H
#define UI_H

#include "types.h"
#include <gtk/gtk.h>

/* Holds all UI label widgets for live updates. */
typedef struct {
    GtkWidget *window;

    /* Header */
    GtkWidget *lbl_cpu_name;
    GtkWidget *lbl_codename;
    GtkWidget *lbl_board_info;
    GtkWidget *combo_modules;

    /* RAM tab — which channel's UMC registers are displayed (driven by the
     * selected module) */
    GtkWidget *lbl_channel_note;

    /* RAM tab — DIMM section */
    GtkWidget *lbl_speed, *lbl_mclk, *lbl_fclk, *lbl_uclk, *lbl_bclk;
    GtkWidget *lbl_gdm, *lbl_powerdown, *lbl_spd_temp;

    /* RAM tab — DIMM info */
    GtkWidget *lbl_capacity, *lbl_manufacturer, *lbl_part_number;
    GtkWidget *lbl_serial_number, *lbl_rank, *lbl_cmd2t, *lbl_dimm_channel;

    /* RAM tab — Voltages */
    GtkWidget *lbl_vsoc, *lbl_vddp, *lbl_vddg_ccd, *lbl_vddg_iod;
    GtkWidget *lbl_vdd_misc, *lbl_mem_vdd, *lbl_mem_vddq;
    GtkWidget *lbl_cpu_vddio, *lbl_mem_vpp, *lbl_vcore, *lbl_ppt;

    /* RAM tab — Primary timings */
    GtkWidget *lbl_tcl, *lbl_trcd_rd, *lbl_trcd_wr, *lbl_trp, *lbl_tras, *lbl_trc;
    GtkWidget *lbl_trrds, *lbl_trrdl, *lbl_tfaw, *lbl_twr, *lbl_tcwl;
    GtkWidget *lbl_trfc_ns, *lbl_rfc, *lbl_rfc2, *lbl_rfcsb;

    /* RAM tab — Secondary timings */
    GtkWidget *lbl_rtp, *lbl_wtrs, *lbl_wtrl, *lbl_rdwr, *lbl_wrrd;
    GtkWidget *lbl_rdrd_sc, *lbl_rdrd_sd, *lbl_rdrd_dd;
    GtkWidget *lbl_wrwr_sc, *lbl_wrwr_sd, *lbl_wrwr_dd;
    GtkWidget *lbl_refi, *lbl_trefi_ns, *lbl_wrpre, *lbl_rdpre;

    /* RAM tab — Tertiary timings */
    GtkWidget *lbl_rdrd_scl, *lbl_wrwr_scl, *lbl_cke, *lbl_xp;
    GtkWidget *lbl_trc_page, *lbl_mod, *lbl_mod_pda, *lbl_mrd, *lbl_mrd_pda;
    GtkWidget *lbl_stag, *lbl_stag_sb, *lbl_phy_wrl, *lbl_phy_rdl, *lbl_phy_wrd;

    /* RAM tab — Extended timings (new, per-channel) */
    GtkWidget *lbl_rp_pb, *lbl_rc_pb, *lbl_ppd, *lbl_mw;
    GtkWidget *lbl_rdpost, *lbl_wrpost, *lbl_rdrd_ban, *lbl_wrwr_ban;
    GtkWidget *lbl_dllk, *lbl_xs, *lbl_mrri, *lbl_mrr, *lbl_mrw, *lbl_ctrl_switch;
    GtkWidget *lbl_csh, *lbl_cacsh, *lbl_srx2srx, *lbl_stab;
    GtkWidget *lbl_zqcs, *lbl_zqoper, *lbl_zqcs_int;

    /* RAM tab — Controller config (new) */
    GtkWidget *lbl_refresh_mode, *lbl_fgr, *lbl_bgs, *lbl_bgs_alt;
    GtkWidget *lbl_bank_group_en, *lbl_ecc, *lbl_burst_length, *lbl_burst_ctrl;
    GtkWidget *lbl_pd_mode, *lbl_aggr_pd, *lbl_odts, *lbl_odts_int;
    GtkWidget *lbl_uclk_gt_fclk, *lbl_wck_ratio, *lbl_nitro, *lbl_nitro_mode;
    GtkWidget *lbl_channel_ranks, *lbl_memclk_ratio;

    /* RAM tab — Electrical / termination (new, AOD) */
    GtkWidget *lbl_proc_odt, *lbl_proc_odt_pu, *lbl_proc_odt_pd;
    GtkWidget *lbl_rtt_nom_wr, *lbl_rtt_nom_rd, *lbl_rtt_wr;
    GtkWidget *lbl_rtt_park, *lbl_rtt_park_dqs;
    GtkWidget *lbl_cadbus_ds, *lbl_proc_dq_ds, *lbl_proc_dq_ds_pu, *lbl_proc_dq_ds_pd;
    GtkWidget *lbl_proc_cs_ds, *lbl_proc_ck_ds;
    GtkWidget *lbl_dram_ds, *lbl_dram_dq_ds_pu, *lbl_dram_dq_ds_pd;

    /* RAM tab — Footer */
    GtkWidget *lbl_footer_type;

    /* CPU tab — VID & per-core voltages */
    GtkWidget *lbl_vid;
    GtkWidget *lbl_core_volt[MAX_CORES];
    GtkWidget *lbl_core_volt_lbl[MAX_CORES]; /* row label widgets for show/hide */
    /* merged core table extra columns: °C | OS load | MHz | W | C0% | CC6% */
    GtkWidget *lbl_core_t[MAX_CORES];        /* °C per core (kernel)          */
    GtkWidget *lbl_core_load[MAX_CORES];     /* OS load %                     */
    GtkWidget *lbl_core_freq[MAX_CORES];     /* MHz (cpufreq)                 */
    GtkWidget *lbl_core_pw[MAX_CORES];       /* W  (SMU PM table)             */
    GtkWidget *lbl_core_c0[MAX_CORES];       /* C0% (SMU PM table)            */
    GtkWidget *lbl_core_cc6[MAX_CORES];      /* CC6% (SMU PM table)           */
    int        cpu_core_volt_rows;

    /* CPU tab — Temperatures (lbl_tdie = single-line temps blob) */
    GtkWidget *lbl_tdie;

    /* CPU tab — Fans */
    GtkWidget *lbl_fans;

    /* CPU tab — PBO limits (current / limit pairs) */
    GtkWidget *lbl_pbo_ppt, *lbl_pbo_tdc, *lbl_pbo_edc, *lbl_pbo_thermal;

    /* Benchmark tab — RAM */
    GtkWidget *btn_bench_run;
    GtkWidget *lbl_bench_status;
    GtkWidget *lbl_bench_lat_l1;
    GtkWidget *lbl_bench_lat_l2;
    GtkWidget *lbl_bench_lat_l3;
    GtkWidget *lbl_bench_lat_dram;
    GtkWidget *lbl_bench_bw_read;
    GtkWidget *lbl_bench_bw_write;
    GtkWidget *lbl_bench_bw_copy;

    /* Benchmark tab — Pi */
    GtkWidget *btn_pi_run;
    GtkWidget *combo_pi_digits;
    GtkWidget *lbl_pi_status;
    GtkWidget *lbl_pi_time;

    /* SPD sub-page (RAM tab second row) — shows the selected DIMM's SPD */
    GtkWidget *lbl_spd_note;      /* "SPD of slot X — source path" row    */
    GtkWidget *lbl_spd_crc;       /* base CRC status                      */
    GtkWidget *lbl_spd_man, *lbl_spd_dram, *lbl_spd_part, *lbl_spd_serial;
    GtkWidget *lbl_spd_date, *lbl_spd_rev;
    GtkWidget *lbl_spd_geom, *lbl_spd_org, *lbl_spd_caps;
    GtkWidget *lbl_spd_volt, *lbl_spd_phys, *lbl_spd_mirror;
    /* profile selector + dynamically refilled decode grid */
    GtkWidget *combo_spd_profile;
    GtkWidget *grid_spd_prof;
    int        spd_prof_n;        /* items in spd_prof_map                */
    int        spd_prof_map[8];   /* 0=JEDEC, 1..5=XMP, 6..7=EXPO        */
    int        spd_prof_sel;

    /* Data */
    system_summary_t summary;
    int selected_module;
    int modules_populated;
} app_widgets_t;

/* Build the UI and start the refresh timer. Returns the GtkApplication. */
GtkApplication *ui_create(int argc, char **argv);

#endif
