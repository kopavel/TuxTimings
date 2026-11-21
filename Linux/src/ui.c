#include "ui.h"
#include "backend.h"
#include "bench.h"
#include "pi_bench.h"
#include <stdio.h>
#include <string.h>
#include <locale.h>
#include <unistd.h>

/* ── CSS theme (GitHub dark) ────────────────────────────────────────── */

#define APP_VERSION "v1.0.5"

/* float→double promotion in variadic snprintf calls is harmless here */
#pragma GCC diagnostic ignored "-Wdouble-promotion"

static const char *css_data =
    "window { background-color: #0D1117; }\n"
    ".header-title { color: #E6EDF3; font-size: 1.1em; font-weight: bold; }\n"
    ".header-muted { color: #8B949E; font-size: 0.85em; }\n"
    ".footer-muted { color: #8B949E; font-size: 0.8em; }\n"
    ".section-title { color: #C9D1D9; font-size: 0.9em; font-weight: bold; }\n"
    ".label { color: #8B949E; font-size: 0.85em; min-height: 1.4em; }\n"
    ".value-highlight { color: #3FB950; font-size: 0.85em; min-height: 1.4em; }\n"
    ".section-box { background-color: #161B22; border-radius: 6px; padding: 8px; }\n"
    "notebook { background: transparent; }\n"
    "notebook > header { background: transparent; border-bottom: 1px solid #30363D; }\n"
    "notebook > header > tabs > tab { color: #8B949E; background: transparent; padding: 6px 16px; font-size: 0.9em; }\n"
    "notebook > header > tabs > tab:checked { color: #E6EDF3; border-bottom: 2px solid #3FB950; }\n"
    "notebook > stack { background: transparent; }\n"
    "dropdown { background-color: #161B22; color: #E6EDF3; font-size: 0.9em; }\n"
    "dropdown > button { background-color: #161B22; background-image: none; box-shadow: none; color: #E6EDF3; border: 1px solid #30363D; }\n"
    /* Popover list */
    "popover { background-color: #161B22; background-image: none; color: #E6EDF3; border: 1px solid #30363D; }\n"
    "popover > contents { background-color: #161B22; background-image: none; color: #E6EDF3; box-shadow: none; }\n"
    "popover list, popover list row { background-color: #161B22; background-image: none; color: #E6EDF3; }\n"
    "popover list row:hover { background-color: #21262D; background-image: none; color: #E6EDF3; }\n"
    "popover list row:selected, popover list row:selected:focus, popover list row:selected:hover { background-color: #30363D; background-image: none; color: #E6EDF3; }\n"
    "popover entry, dropdown > popover entry { background-color: #0D1117; background-image: none; color: #E6EDF3; border: 1px solid #30363D; box-shadow: none; }\n"
    "arrow { color: #E6EDF3; }\n"
    "popover arrow { color: #161B22; background-color: #161B22; background-image: none; border-color: #30363D; }\n"
    "scrolledwindow { background: transparent; }\n"
    "textview, textview text { background-color: #0D1117; background-image: none; color: #E6EDF3; }\n"
    "scrollbar slider { background-color: #30363D; border-radius: 4px; min-width: 6px; min-height: 6px; margin: 0; border-width: 0; }\n"
    "button { background-color: #21262D; background-image: none; box-shadow: none; color: #E6EDF3; border: 1px solid #30363D; border-radius: 6px; padding: 4px 12px; font-size: 0.85em; }\n"
    "button:hover { background-color: #30363D; background-image: none; }\n"
    "button:disabled { opacity: 0.4; }\n"
    /* RAM-tab sizing: original (readable) sizes; vertical fit comes from the
     * Timings/Config sub-tabs, not from shrinking text. */
    ".compact .label { color: #8B949E; font-size: 0.85em; min-height: 1.4em; }\n"
    ".compact .value-highlight { color: #3FB950; font-size: 0.85em; min-height: 1.4em; }\n"
    ".compact .section-title { color: #C9D1D9; font-size: 0.9em; font-weight: bold; }\n"
    ".compact .section-box { background-color: #161B22; border-radius: 6px; padding: 8px; }\n"
    ".compact .footer-muted { font-size: 0.8em; }\n";

static void load_css(void)
{
    /* Dark mode baseline so unstyled surfaces (popovers, tooltips, scrollbars,
     * menu popups) come from Adwaita-dark instead of the light theme. */
    g_object_set(gtk_settings_get_default(),
                 "gtk-application-prefer-dark-theme", TRUE, NULL);

    GtkCssProvider *provider = gtk_css_provider_new();
    gtk_css_provider_load_from_string(provider, css_data);
    gtk_style_context_add_provider_for_display(
        gdk_display_get_default(),
        GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(provider);
}

/* ── HiDPI fix for running elevated via pkexec ──────────────────────────
 * The app re-execs itself as root (see main.c). As root, GTK cannot read
 * the user's desktop settings — dconf and the settings portal are per-user
 * — so on the Wayland backend it falls back to the X server's *scaled*
 * Xft.dpi (e.g. 192 dpi on a 200% display). But the Wayland compositor
 * ALSO applies the per-output scale factor, so everything ends up rendered
 * at double size. On Wayland HiDPI is conveyed solely through the output
 * scale factor, so the font DPI must be the unscaled default: reset
 * gtk-xft-dpi to -1 to undo the doubling while keeping the crisp 2x scale.
 * Only the Wayland backend is affected — on X11 the scaled Xft.dpi is the
 * legitimate HiDPI mechanism and must be left alone. */
static void fix_elevated_wayland_dpi(void)
{
    if (geteuid() != 0) return;
    GdkDisplay *display = gdk_display_get_default();
    if (display && g_strcmp0(G_OBJECT_TYPE_NAME(display), "GdkWaylandDisplay") == 0)
        g_object_set(gtk_settings_get_default(), "gtk-xft-dpi", -1, NULL);
}

/* ── Helpers ────────────────────────────────────────────────────────── */

static GtkWidget *make_label(const char *text, const char *css_class)
{
    GtkWidget *l = gtk_label_new(text);
    gtk_widget_add_css_class(l, css_class);
    gtk_label_set_xalign(GTK_LABEL(l), 0.0f);
    gtk_widget_set_hexpand(l, FALSE);
    return l;
}

static GtkWidget *make_value(const char *text)
{
    GtkWidget *l = gtk_label_new(text);
    gtk_widget_add_css_class(l, "value-highlight");
    gtk_label_set_xalign(GTK_LABEL(l), 0.0f);
    gtk_widget_set_hexpand(l, TRUE);
    return l;
}

static void grid_row(GtkWidget *grid, int row, const char *label_text, GtkWidget **out_val)
{
    GtkWidget *lbl = make_label(label_text, "label");
    *out_val = make_value("—");
    gtk_widget_set_hexpand(grid, TRUE);
    gtk_grid_attach(GTK_GRID(grid), lbl, 0, row, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), *out_val, 1, row, 1, 1);
}

static void set_label_text(GtkWidget *label, const char *text)
{
    gtk_label_set_text(GTK_LABEL(label), text);
}

static void set_label_fmt(GtkWidget *label, const char *fmt, ...)
{
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    gtk_label_set_text(GTK_LABEL(label), buf);
}

static GtkWidget *make_section_box(void)
{
    GtkWidget *frame = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_add_css_class(frame, "section-box");
    gtk_widget_set_hexpand(frame, TRUE);
    gtk_widget_set_vexpand(frame, TRUE);
    return frame;
}


/* ── Build RAM tab ──────────────────────────────────────────────────── */

static GtkWidget *build_spd_content(app_widgets_t *w);  /* fwd (SPD sub-page) */

static GtkWidget *build_ram_tab(app_widgets_t *w)
{
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_add_css_class(vbox, "compact");
    gtk_widget_set_margin_top(vbox, 4);
    gtk_widget_set_margin_start(vbox, 4);
    gtk_widget_set_margin_end(vbox, 4);
    gtk_widget_set_margin_bottom(vbox, 4);
    gtk_widget_set_hexpand(vbox, TRUE);

    /* ── Top row: DIMM | DIMM info | Voltages ── */
    GtkWidget *top = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 3);
    gtk_widget_set_hexpand(top, TRUE);
    gtk_box_set_homogeneous(GTK_BOX(top), TRUE);

    /* DIMM speeds */
    GtkWidget *dimm_box = make_section_box();
    {
        GtkWidget *title = make_label("DIMM", "section-title");
        gtk_box_append(GTK_BOX(dimm_box), title);

        GtkWidget *g = gtk_grid_new();
        gtk_grid_set_row_spacing(GTK_GRID(g), 4);
        gtk_grid_set_column_spacing(GTK_GRID(g), 8);
        int r = 0;
        grid_row(g, r++, "Speed:", &w->lbl_speed);
        grid_row(g, r++, "MCLK:", &w->lbl_mclk);
        grid_row(g, r++, "FCLK:", &w->lbl_fclk);
        grid_row(g, r++, "UCLK:", &w->lbl_uclk);
        grid_row(g, r++, "BCLK:", &w->lbl_bclk);
        gtk_box_append(GTK_BOX(dimm_box), g);

        /* GDM / PowerDown / Temp — horizontal: labels row then values row */
        GtkWidget *g2 = gtk_grid_new();
        gtk_grid_set_row_spacing(GTK_GRID(g2), 2);
        gtk_grid_set_column_spacing(GTK_GRID(g2), 12);
        gtk_widget_set_margin_top(g2, 4);

        GtkWidget *lbl_gdm_h   = make_label("GDM",       "label");
        GtkWidget *lbl_pd_h    = make_label("PowerDown", "label");
        GtkWidget *lbl_temp_h  = make_label("Temp",      "label");
        w->lbl_gdm       = make_value("—");
        w->lbl_powerdown = make_value("—");
        w->lbl_spd_temp  = make_value("—");

        gtk_grid_attach(GTK_GRID(g2), lbl_gdm_h,        0, 0, 1, 1);
        gtk_grid_attach(GTK_GRID(g2), lbl_pd_h,         1, 0, 1, 1);
        gtk_grid_attach(GTK_GRID(g2), lbl_temp_h,       2, 0, 1, 1);
        gtk_grid_attach(GTK_GRID(g2), w->lbl_gdm,       0, 1, 1, 1);
        gtk_grid_attach(GTK_GRID(g2), w->lbl_powerdown, 1, 1, 1, 1);
        gtk_grid_attach(GTK_GRID(g2), w->lbl_spd_temp,  2, 1, 1, 1);
        gtk_box_append(GTK_BOX(dimm_box), g2);
    }
    gtk_box_append(GTK_BOX(top), dimm_box);

    /* DIMM info */
    GtkWidget *info_box = make_section_box();
    {
        GtkWidget *title = make_label("DIMM", "section-title");
        gtk_box_append(GTK_BOX(info_box), title);

        GtkWidget *g = gtk_grid_new();
        gtk_grid_set_row_spacing(GTK_GRID(g), 2);
        gtk_grid_set_column_spacing(GTK_GRID(g), 8);
        int r = 0;
        grid_row(g, r++, "Capacity:", &w->lbl_capacity);
        grid_row(g, r++, "Manufacturer:", &w->lbl_manufacturer);
        grid_row(g, r++, "Part Number:", &w->lbl_part_number);
        grid_row(g, r++, "Serial:", &w->lbl_serial_number);
        grid_row(g, r++, "Rank:", &w->lbl_rank);
        grid_row(g, r++, "Channel:", &w->lbl_dimm_channel);
        grid_row(g, r++, "Cmd2T:", &w->lbl_cmd2t);
        gtk_box_append(GTK_BOX(info_box), g);
    }
    gtk_box_append(GTK_BOX(top), info_box);

    /* Voltages — two columns to halve the vertical footprint */
    GtkWidget *volt_box = make_section_box();
    {
        GtkWidget *title = make_label("Voltages", "section-title");
        gtk_box_append(GTK_BOX(volt_box), title);

        GtkWidget *g = gtk_grid_new();
        gtk_grid_set_row_spacing(GTK_GRID(g), 2);
        gtk_grid_set_column_spacing(GTK_GRID(g), 8);
        struct { const char *label; GtkWidget **val; } vrows[] = {
            { "VSOC",        &w->lbl_vsoc },
            { "CLDO VDDP",   &w->lbl_vddp },
            { "VDDG CCD",    &w->lbl_vddg_ccd },
            { "VDDG IOD",    &w->lbl_vddg_iod },
            { "VDD MISC",    &w->lbl_vdd_misc },
            { "MEM VDD",     &w->lbl_mem_vdd },
            { "MEM VDDQ",    &w->lbl_mem_vddq },
            { "CPU VDDIO",   &w->lbl_cpu_vddio },
            { "MEM VPP",     &w->lbl_mem_vpp },
            { "VCORE",       &w->lbl_vcore },
            { "PPT",         &w->lbl_ppt },
        };
        for (unsigned i = 0; i < sizeof(vrows) / sizeof(vrows[0]); i++) {
            int col = (i % 2) * 2;
            int row = (int)(i / 2);
            GtkWidget *lbl = make_label(vrows[i].label, "label");
            *vrows[i].val = make_value("—");
            gtk_grid_attach(GTK_GRID(g), lbl, col, row, 1, 1);
            gtk_grid_attach(GTK_GRID(g), *vrows[i].val, col + 1, row, 1, 1);
        }
        gtk_box_append(GTK_BOX(volt_box), g);
    }
    gtk_box_append(GTK_BOX(top), volt_box);
    gtk_box_append(GTK_BOX(vbox), top);

    /* ── Channel indicator (follows the selected module) ── */
    GtkWidget *chan_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    {
        GtkWidget *lbl = make_label("Timings:", "section-title");
        gtk_box_append(GTK_BOX(chan_row), lbl);
        w->lbl_channel_note = make_label("", "footer-muted");
        gtk_widget_set_hexpand(w->lbl_channel_note, TRUE);
        gtk_box_append(GTK_BOX(chan_row), w->lbl_channel_note);
    }
    gtk_box_append(GTK_BOX(vbox), chan_row);

    /* ── Timing columns: Primary | Secondary | Tertiary | Extended ── */
    GtkWidget *mid = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_widget_set_hexpand(mid, TRUE);
    gtk_widget_set_vexpand(mid, TRUE);
    gtk_box_set_homogeneous(GTK_BOX(mid), TRUE);

    /* Primary */
    GtkWidget *prim_box = make_section_box();
    {
        gtk_box_append(GTK_BOX(prim_box), make_label("Primary Timings", "section-title"));
        GtkWidget *g = gtk_grid_new();
        gtk_grid_set_row_spacing(GTK_GRID(g), 2);
        gtk_grid_set_column_spacing(GTK_GRID(g), 8);
        int r = 0;
        grid_row(g, r++, "tCL", &w->lbl_tcl);
        grid_row(g, r++, "tRCDRD", &w->lbl_trcd_rd);
        grid_row(g, r++, "tRCDWR", &w->lbl_trcd_wr);
        grid_row(g, r++, "tRP", &w->lbl_trp);
        grid_row(g, r++, "tRAS", &w->lbl_tras);
        grid_row(g, r++, "tRC", &w->lbl_trc);
        grid_row(g, r++, "tRRDS", &w->lbl_trrds);
        grid_row(g, r++, "tRRDL", &w->lbl_trrdl);
        grid_row(g, r++, "tFAW", &w->lbl_tfaw);
        grid_row(g, r++, "tWR", &w->lbl_twr);
        grid_row(g, r++, "tCWL", &w->lbl_tcwl);
        grid_row(g, r++, "tRFC (ns)", &w->lbl_trfc_ns);
        grid_row(g, r++, "tRFC", &w->lbl_rfc);
        grid_row(g, r++, "tRFC2", &w->lbl_rfc2);
        grid_row(g, r++, "tRFCsb", &w->lbl_rfcsb);
        gtk_box_append(GTK_BOX(prim_box), g);
    }
    gtk_box_append(GTK_BOX(mid), prim_box);

    /* Secondary */
    GtkWidget *sec_box = make_section_box();
    {
        gtk_box_append(GTK_BOX(sec_box), make_label("Secondary Timings", "section-title"));
        GtkWidget *g = gtk_grid_new();
        gtk_grid_set_row_spacing(GTK_GRID(g), 2);
        gtk_grid_set_column_spacing(GTK_GRID(g), 8);
        int r = 0;
        grid_row(g, r++, "tRTP", &w->lbl_rtp);
        grid_row(g, r++, "tWTRS", &w->lbl_wtrs);
        grid_row(g, r++, "tWTRL", &w->lbl_wtrl);
        grid_row(g, r++, "tRDWR", &w->lbl_rdwr);
        grid_row(g, r++, "tWRRD", &w->lbl_wrrd);
        grid_row(g, r++, "tRDRDSC", &w->lbl_rdrd_sc);
        grid_row(g, r++, "tRDRDSD", &w->lbl_rdrd_sd);
        grid_row(g, r++, "tRDRDDD", &w->lbl_rdrd_dd);
        grid_row(g, r++, "tWRWRSC", &w->lbl_wrwr_sc);
        grid_row(g, r++, "tWRWRSD", &w->lbl_wrwr_sd);
        grid_row(g, r++, "tWRWRDD", &w->lbl_wrwr_dd);
        grid_row(g, r++, "tREFI", &w->lbl_refi);
        grid_row(g, r++, "tREFI (ns)", &w->lbl_trefi_ns);
        grid_row(g, r++, "tWRPRE", &w->lbl_wrpre);
        grid_row(g, r++, "tRDPRE", &w->lbl_rdpre);
        gtk_box_append(GTK_BOX(sec_box), g);
    }
    gtk_box_append(GTK_BOX(mid), sec_box);

    /* Tertiary */
    GtkWidget *tert_box = make_section_box();
    {
        gtk_box_append(GTK_BOX(tert_box), make_label("Tertiary Timings", "section-title"));
        GtkWidget *g = gtk_grid_new();
        gtk_grid_set_row_spacing(GTK_GRID(g), 2);
        gtk_grid_set_column_spacing(GTK_GRID(g), 8);
        int r = 0;
        grid_row(g, r++, "tRDRDSCL", &w->lbl_rdrd_scl);
        grid_row(g, r++, "tWRWRSCL", &w->lbl_wrwr_scl);
        grid_row(g, r++, "tCKE", &w->lbl_cke);
        grid_row(g, r++, "tXP", &w->lbl_xp);
        grid_row(g, r++, "tTRCPAGE", &w->lbl_trc_page);
        grid_row(g, r++, "tMOD", &w->lbl_mod);
        grid_row(g, r++, "tMODPDA", &w->lbl_mod_pda);
        grid_row(g, r++, "tMRD", &w->lbl_mrd);
        grid_row(g, r++, "tMRDPDA", &w->lbl_mrd_pda);
        grid_row(g, r++, "tSTAG", &w->lbl_stag);
        grid_row(g, r++, "tSTAGsb", &w->lbl_stag_sb);
        grid_row(g, r++, "tPHYWRL", &w->lbl_phy_wrl);
        grid_row(g, r++, "tPHYRDL", &w->lbl_phy_rdl);
        grid_row(g, r++, "tPHYWRD", &w->lbl_phy_wrd);
        gtk_box_append(GTK_BOX(tert_box), g);
    }
    gtk_box_append(GTK_BOX(mid), tert_box);

    /* Extended */
    GtkWidget *ext_box = make_section_box();
    {
        gtk_box_append(GTK_BOX(ext_box), make_label("Extended Timings", "section-title"));
        GtkWidget *g = gtk_grid_new();
        gtk_grid_set_row_spacing(GTK_GRID(g), 2);
        gtk_grid_set_column_spacing(GTK_GRID(g), 8);
        int r = 0;
        grid_row(g, r++, "tRPpb", &w->lbl_rp_pb);
        grid_row(g, r++, "tRCpb", &w->lbl_rc_pb);
        grid_row(g, r++, "tPPD", &w->lbl_ppd);
        grid_row(g, r++, "MW", &w->lbl_mw);
        grid_row(g, r++, "tRDPOST", &w->lbl_rdpost);
        grid_row(g, r++, "tWRPOST", &w->lbl_wrpost);
        grid_row(g, r++, "tRDRDBan", &w->lbl_rdrd_ban);
        grid_row(g, r++, "tWRWRBan", &w->lbl_wrwr_ban);
        grid_row(g, r++, "tDLLK", &w->lbl_dllk);
        grid_row(g, r++, "tXS", &w->lbl_xs);
        grid_row(g, r++, "tMRRI", &w->lbl_mrri);
        grid_row(g, r++, "tMRR", &w->lbl_mrr);
        grid_row(g, r++, "tMRW", &w->lbl_mrw);
        grid_row(g, r++, "CtrlSwitchClks", &w->lbl_ctrl_switch);
        grid_row(g, r++, "tCSH", &w->lbl_csh);
        grid_row(g, r++, "tCACSH", &w->lbl_cacsh);
        grid_row(g, r++, "tSRX2SRX", &w->lbl_srx2srx);
        grid_row(g, r++, "tSTAB", &w->lbl_stab);
        grid_row(g, r++, "tZQCS", &w->lbl_zqcs);
        grid_row(g, r++, "tZQOperCal", &w->lbl_zqoper);
        grid_row(g, r++, "ZQCSInterval", &w->lbl_zqcs_int);
        gtk_box_append(GTK_BOX(ext_box), g);
    }
    gtk_box_append(GTK_BOX(mid), ext_box);

    /* ── Bottom row: Config | Electrical ── */
    GtkWidget *bot = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_widget_set_hexpand(bot, TRUE);
    gtk_box_set_homogeneous(GTK_BOX(bot), TRUE);

    GtkWidget *cfg_box = make_section_box();
    {
        gtk_box_append(GTK_BOX(cfg_box), make_label("Controller Config", "section-title"));
        GtkWidget *g = gtk_grid_new();
        gtk_grid_set_row_spacing(GTK_GRID(g), 2);
        gtk_grid_set_column_spacing(GTK_GRID(g), 8);
        int r = 0;
        grid_row(g, r++, "Ranks:", &w->lbl_channel_ranks);
        grid_row(g, r++, "MEMCLK ratio:", &w->lbl_memclk_ratio);
        grid_row(g, r++, "Refresh Mode:", &w->lbl_refresh_mode);
        grid_row(g, r++, "FGR:", &w->lbl_fgr);
        grid_row(g, r++, "BGS:", &w->lbl_bgs);
        grid_row(g, r++, "BGS Alt:", &w->lbl_bgs_alt);
        grid_row(g, r++, "BankGroupEn:", &w->lbl_bank_group_en);
        grid_row(g, r++, "ECC:", &w->lbl_ecc);
        grid_row(g, r++, "Burst Length:", &w->lbl_burst_length);
        grid_row(g, r++, "Burst Ctrl:", &w->lbl_burst_ctrl);
        grid_row(g, r++, "PowerDown Mode:", &w->lbl_pd_mode);
        grid_row(g, r++, "Aggr.PowerDown:", &w->lbl_aggr_pd);
        grid_row(g, r++, "ODTS:", &w->lbl_odts);
        grid_row(g, r++, "ODTS Read Int.:", &w->lbl_odts_int);
        grid_row(g, r++, "UCLK>FCLK:", &w->lbl_uclk_gt_fclk);
        grid_row(g, r++, "WCK:CK:", &w->lbl_wck_ratio);
        grid_row(g, r++, "Nitro r/t/c:", &w->lbl_nitro);
        grid_row(g, r++, "Nitro DlyMode:", &w->lbl_nitro_mode);
        gtk_box_append(GTK_BOX(cfg_box), g);
    }
    gtk_box_append(GTK_BOX(bot), cfg_box);

    GtkWidget *elec_box = make_section_box();
    {
        gtk_box_append(GTK_BOX(elec_box), make_label("Electrical / Termination (AOD)", "section-title"));
        GtkWidget *g = gtk_grid_new();
        gtk_grid_set_row_spacing(GTK_GRID(g), 2);
        gtk_grid_set_column_spacing(GTK_GRID(g), 8);
        int r = 0;
        grid_row(g, r++, "ProcODT:", &w->lbl_proc_odt);
        grid_row(g, r++, "ProcODT PullUp:", &w->lbl_proc_odt_pu);
        grid_row(g, r++, "ProcODT PullDn:", &w->lbl_proc_odt_pd);
        grid_row(g, r++, "RTT Nom WR:", &w->lbl_rtt_nom_wr);
        grid_row(g, r++, "RTT Nom RD:", &w->lbl_rtt_nom_rd);
        grid_row(g, r++, "RTT WR:", &w->lbl_rtt_wr);
        grid_row(g, r++, "RTT Park:", &w->lbl_rtt_park);
        grid_row(g, r++, "RTT Park DQS:", &w->lbl_rtt_park_dqs);
        grid_row(g, r++, "CADBUS DS:", &w->lbl_cadbus_ds);
        grid_row(g, r++, "Proc Data DQ DS:", &w->lbl_proc_dq_ds);
        grid_row(g, r++, "ProcDqDs PullUp:", &w->lbl_proc_dq_ds_pu);
        grid_row(g, r++, "ProcDqDs PullDn:", &w->lbl_proc_dq_ds_pd);
        grid_row(g, r++, "ProcCsDs:", &w->lbl_proc_cs_ds);
        grid_row(g, r++, "ProcCkDs:", &w->lbl_proc_ck_ds);
        grid_row(g, r++, "DRAM Data DS:", &w->lbl_dram_ds);
        grid_row(g, r++, "DramDqDs PullUp:", &w->lbl_dram_dq_ds_pu);
        grid_row(g, r++, "DramDqDs PullDn:", &w->lbl_dram_dq_ds_pd);
        gtk_box_append(GTK_BOX(elec_box), g);
    }
    gtk_box_append(GTK_BOX(bot), elec_box);

    /* Single row: timings and config/electrical as sub-tabs */
    GtkWidget *sub = gtk_notebook_new();
    gtk_widget_set_vexpand(sub, TRUE);
    gtk_widget_set_hexpand(sub, TRUE);
    gtk_notebook_append_page(GTK_NOTEBOOK(sub), mid, gtk_label_new("Timings"));
    gtk_notebook_append_page(GTK_NOTEBOOK(sub), bot, gtk_label_new("Config / Electrical"));
    gtk_notebook_append_page(GTK_NOTEBOOK(sub), build_spd_content(w), gtk_label_new("SPD"));
    gtk_box_append(GTK_BOX(vbox), sub);

    /* Footer */
    GtkWidget *footer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    GtkWidget *footer_text = make_label("Timings & config: SMN (per UMC). Electrical: AOD ACPI table. Voltages & FCLK: PM table.", "footer-muted");
    gtk_widget_set_hexpand(footer_text, TRUE);
    w->lbl_footer_type = make_label("DDR5", "value-highlight");
    gtk_box_append(GTK_BOX(footer), footer_text);
    gtk_box_append(GTK_BOX(footer), w->lbl_footer_type);
    gtk_widget_set_margin_top(footer, 8);
    gtk_box_append(GTK_BOX(vbox), footer);

    /* Fallback for displays shorter than the content: scroll, don't clip */
    GtkWidget *scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), vbox);
    return scroll;
}

/* ── Build CPU tab ──────────────────────────────────────────────────── */

static GtkWidget *build_cpu_tab(app_widgets_t *w)
{
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_margin_top(vbox, 8);
    gtk_widget_set_margin_start(vbox, 8);
    gtk_widget_set_margin_end(vbox, 8);
    gtk_widget_set_margin_bottom(vbox, 8);
    gtk_widget_set_hexpand(vbox, TRUE);

    GtkWidget *top = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_widget_set_hexpand(top, TRUE);
    gtk_box_set_homogeneous(GTK_BOX(top), TRUE);

    /* Left: VID & per-core voltages */
    GtkWidget *volt_box = make_section_box();
    {
        gtk_box_append(GTK_BOX(volt_box), make_label("VID & Core Voltages", "section-title"));
        GtkWidget *g = gtk_grid_new();
        gtk_grid_set_row_spacing(GTK_GRID(g), 2);
        gtk_grid_set_column_spacing(GTK_GRID(g), 8);
        int r = 0;
        grid_row(g, r++, "VID:", &w->lbl_vid);
        for (int i = 0; i < MAX_CORES; i++) {
            char lbl[8];
            snprintf(lbl, sizeof(lbl), "C%d:", i);
            grid_row(g, r++, lbl, &w->lbl_core_volt[i]);
            w->lbl_core_volt_lbl[i] = gtk_grid_get_child_at(GTK_GRID(g), 0, r - 1);
            gtk_widget_set_visible(w->lbl_core_volt[i],     FALSE);
            gtk_widget_set_visible(w->lbl_core_volt_lbl[i], FALSE);
        }
        w->cpu_core_volt_rows = 0;
        gtk_box_append(GTK_BOX(volt_box), g);
    }
    gtk_box_append(GTK_BOX(top), volt_box);

    /* Right: Temperatures & Fans */
    GtkWidget *temp_box = make_section_box();
    {
        gtk_box_append(GTK_BOX(temp_box), make_label("Temp & Fans", "section-title"));
        GtkWidget *g = gtk_grid_new();
        gtk_grid_set_row_spacing(GTK_GRID(g), 2);
        gtk_grid_set_column_spacing(GTK_GRID(g), 8);
        int r = 0;
        grid_row(g, r++, "Fans:",         &w->lbl_fans);
        gtk_label_set_wrap(GTK_LABEL(w->lbl_fans), TRUE);
        gtk_box_append(GTK_BOX(temp_box), g);

        gtk_box_append(GTK_BOX(temp_box), make_label("Temps:", "label"));
        w->lbl_tdie = make_value("—");
        gtk_label_set_wrap(GTK_LABEL(w->lbl_tdie), TRUE);
        gtk_box_append(GTK_BOX(temp_box), w->lbl_tdie);

        gtk_box_append(GTK_BOX(temp_box), make_label("Core temps / load / freq:", "label"));
        w->lbl_core_temps = make_value("—");
        gtk_label_set_wrap(GTK_LABEL(w->lbl_core_temps), TRUE);
        gtk_box_append(GTK_BOX(temp_box), w->lbl_core_temps);
    }
    gtk_box_append(GTK_BOX(top), temp_box);
    gtk_box_append(GTK_BOX(vbox), top);

    return vbox;
}

/* The channel whose UMC registers are shown: the one hosting the selected
 * module; falls back to the first populated channel (e.g. before SMBIOS
 * data arrives, or when module->channel mapping is unknown). */
static const dram_channel_t *cur_channel(const app_widgets_t *w)
{
    const system_summary_t *s = &w->summary;
    int mi = w->selected_module;
    if (mi >= 0 && mi < s->module_count) {
        int16_t ci = s->modules[mi].channel_idx;
        if (ci >= 0 && ci < s->channel_count && s->channels[ci].present)
            return &s->channels[ci];
    }
    for (int i = 0; i < s->channel_count; i++)
        if (s->channels[i].present)
            return &s->channels[i];
    return NULL;
}

/* N/A-safe setters: unavailable values never render as 0 or empty. */
static void set_u(GtkWidget *l, const dram_channel_t *d, uint32_t v)
{
    if (d) set_label_fmt(l, "%u", v);
    else   set_label_text(l, "N/A");
}
static void set_f(GtkWidget *l, const dram_channel_t *d, float v)
{
    if (d) set_label_fmt(l, "%.0f", v);
    else   set_label_text(l, "N/A");
}
static void set_b(GtkWidget *l, const dram_channel_t *d, bool v)
{
    if (d) set_label_text(l, v ? "Enabled" : "Disabled");
    else   set_label_text(l, "N/A");
}
static void set_s(GtkWidget *l, const dram_channel_t *d, const char *s)
{
    if (d && s && s[0]) set_label_text(l, s);
    else                set_label_text(l, "N/A");
}
static void set_e(GtkWidget *l, bool has, const char *text)
{
    if (has && text && text[0]) set_label_text(l, text);
    else                        set_label_text(l, "N/A");
}

/* ── SPD sub-page (RAM tab second row: Timings | Config/Electrical | SPD) ─
 * Shows the DDR5 SPD of the DIMM selected in the header dropdown, read
 * from the kernel-exposed SPD5118 EEPROMs: static module info on the left,
 * a profile selector (JEDEC base / XMP 3.0 / EXPO) on the right whose grid
 * fully decodes the chosen profile (ps values + nCK at that profile's tCK).
 * Decoding lives in spd_decode.c (provenance: spdr + DDR5SPDEditor — see
 * that file's header). Every unavailable value renders as N/A.          */

static const backend_spd_entry_t *s_spd_cur;  /* entry shown right now    */

static void spd_fill_profile_grid(app_widgets_t *w);

static void grid_clear(GtkWidget *g)
{
    GtkWidget *ch = gtk_widget_get_first_child(g);
    while (ch) {
        GtkWidget *next = gtk_widget_get_next_sibling(ch);
        gtk_grid_remove(GTK_GRID(g), ch);
        ch = next;
    }
}

static void grid_row_txt(GtkWidget *g, int *r, const char *label, const char *value)
{
    GtkWidget *l = make_label(label, "label");
    GtkWidget *v = make_value(value);
    if (!value[0]) gtk_label_set_text(GTK_LABEL(v), "N/A");
    gtk_grid_attach(GTK_GRID(g), l, 0, *r, 1, 1);
    gtk_grid_attach(GTK_GRID(g), v, 1, *r, 1, 1);
    (*r)++;
}

/* "16.640 ns (40 nCK)" — nCK via the JESD400-5B-corrected conversion     */
static void fmt_ps(char *o, size_t n, uint32_t ps, uint32_t tck_ps)
{
    if (!ps) { snprintf(o, n, "N/A"); return; }
    if (tck_ps)
        snprintf(o, n, "%u.%03u ns  (%u nCK)", ps / 1000u, ps % 1000u,
                 spd_ps_to_nck(ps, tck_ps));
    else
        snprintf(o, n, "%u.%03u ns", ps / 1000u, ps % 1000u);
}

/* inline-friendly wrapper: fmt_ps(o,...) then o */
static const char *PS(char *o, size_t n, uint32_t ps, uint32_t tck)
{
    fmt_ps(o, n, ps, tck);
    return o;
}

static void fmt_mv(char *o, size_t n, uint16_t mv)
{
    if (!mv) { snprintf(o, n, "N/A"); return; }
    snprintf(o, n, "%u.%03u V", mv / 1000u, mv % 1000u);
}

/* profile code: 0 = JEDEC base, 1..5 = XMP P1..P3/User1..User2,
 * 6..7 = EXPO P1..P2 */
static void spd_profile_slot_label(int code, char *out, size_t n)
{
    if (code >= 6) snprintf(out, n, "EXPO P%d", code - 5);
    else if (code <= 3) snprintf(out, n, "XMP P%d", code);
    else snprintf(out, n, "XMP User %d", code - 3);
}

static void spd_profile_item_name(int code, const spd_info_t *s,
                                  char *out, size_t n)
{
    if (code == 0) {
        snprintf(out, n, "JEDEC base%s",
                 s->timings.has && s->timings.base_data_rate_mt_s
                 ? " timings" : "");
        return;
    }
    const spd_profile_t *p = (code >= 6)
        ? &s->expo.profile[code - 6] : &s->xmp.profile[code - 1];
    char slot[24];
    spd_profile_slot_label(code, slot, sizeof(slot));
    if (p->has_name && p->data_rate_mt_s)
        snprintf(out, n, "%s — %s (%u MT/s)", slot, p->name,
                 p->data_rate_mt_s);
    else if (p->data_rate_mt_s)
        snprintf(out, n, "%s — %u MT/s", slot, p->data_rate_mt_s);
    else
        snprintf(out, n, "%s", slot);
}

static void spd_populate_profiles(app_widgets_t *w, const spd_info_t *s)
{
    int prev_code = (w->spd_prof_sel >= 0 && w->spd_prof_sel < w->spd_prof_n)
                  ? w->spd_prof_map[w->spd_prof_sel] : 0;
    w->spd_prof_n = 0;
    int map[8];
    if (s->timings.has) map[w->spd_prof_n++] = 0;
    for (int i = 0; i < 5; i++)
        if (s->xmp.present && s->xmp.profile[i].present && w->spd_prof_n < 8)
            map[w->spd_prof_n++] = 1 + i;
    for (int i = 0; i < 2; i++)
        if (s->expo.present && s->expo.profile[i].present && w->spd_prof_n < 8)
            map[w->spd_prof_n++] = 6 + i;

    GtkStringList *model = GTK_STRING_LIST(
        gtk_drop_down_get_model(GTK_DROP_DOWN(w->combo_spd_profile)));
    guint old = g_list_model_get_n_items(G_LIST_MODEL(model));
    for (guint i = old; i > 0; i--) gtk_string_list_remove(model, i - 1);

    int keep = 0;
    char item[96];
    for (int i = 0; i < w->spd_prof_n; i++) {
        w->spd_prof_map[i] = map[i];
        spd_profile_item_name(map[i], s, item, sizeof(item));
        gtk_string_list_append(model, item);
        if (map[i] == prev_code) keep = i;
    }
    w->spd_prof_sel = keep;
    gtk_drop_down_set_selected(GTK_DROP_DOWN(w->combo_spd_profile),
                               (guint)keep);
}

static void spd_fill_profile_grid(app_widgets_t *w)
{
    GtkWidget *g = w->grid_spd_prof;
    grid_clear(g);
    int r = 0;
    if (!s_spd_cur || !s_spd_cur->decode_ok ||
        w->spd_prof_sel < 0 || w->spd_prof_sel >= w->spd_prof_n) {
        grid_row_txt(g, &r, "Profile", "");
        return;
    }
    const spd_info_t *s = &s_spd_cur->info;
    char buf[256], buf2[96];
    int code = w->spd_prof_map[w->spd_prof_sel];

    if (code == 0) {
        const spd_base_timings_t *t = &s->timings;
        uint32_t tck = t->tckavg_min_ps;
        grid_row_txt(g, &r, "Base speed",
                     t->base_data_rate_mt_s
                     ? (snprintf(buf, sizeof(buf), "%u MT/s (tCKmin %u ps)",
                                 t->base_data_rate_mt_s, t->tckavg_min_ps), buf)
                     : "N/A");
        snprintf(buf, sizeof(buf), "%u – %u ps", t->tckavg_min_ps, t->tckavg_max_ps);
        grid_row_txt(g, &r, "tCK range", buf);
        grid_row_txt(g, &r, "CL set",
                     t->cl_min ? (snprintf(buf, sizeof(buf), "%u – %u (even)",
                                           t->cl_min, t->cl_max), buf) : "N/A");
        grid_row_txt(g, &r, "tAA", PS(buf,  sizeof(buf),  t->taa_ps,  tck));
        grid_row_txt(g, &r, "tRCD", PS(buf2, sizeof(buf2), t->trcd_ps, tck));
        (void)buf2;
        grid_row_txt(g, &r, "tRCD", PS(buf,  sizeof(buf),  t->trcd_ps, tck));
        grid_row_txt(g, &r, "tRP", PS(buf,  sizeof(buf),  t->trp_ps,  tck));
        grid_row_txt(g, &r, "tRAS", PS(buf,  sizeof(buf),  t->tras_ps, tck));
        grid_row_txt(g, &r, "tRC", PS(buf,  sizeof(buf),  t->trc_ps,  tck));
        grid_row_txt(g, &r, "tWR", PS(buf,  sizeof(buf),  t->twr_ps,  tck));
        grid_row_txt(g, &r, "tRFC1", PS(buf, sizeof(buf),  t->trfc1_ps, tck));
        grid_row_txt(g, &r, "tRFC2", PS(buf, sizeof(buf),  t->trfc2_ps, tck));
        grid_row_txt(g, &r, "tRFCsb", PS(buf,sizeof(buf),  t->trfcsb_ps, tck));
        if (t->trfc1_dlr_ps) {
            snprintf(buf, sizeof(buf), "%u / %u / %u ns",
                     t->trfc1_dlr_ps / 1000u, t->trfc2_dlr_ps / 1000u,
                     t->trfcsb_dlr_ps / 1000u);
            grid_row_txt(g, &r, "tRFC dLR", buf);
        }
        return;
    }

    int is_expo = code >= 6;
    const spd_profile_t *p = is_expo ? &s->expo.profile[code - 6]
                                     : &s->xmp.profile[code - 1];
    uint32_t tck = p->tck_ps;

    if (p->has_name) grid_row_txt(g, &r, "Name", p->name);
    snprintf(buf, sizeof(buf), "%u%s (tCK %u ps)", p->data_rate_mt_s,
             p->data_rate_mt_s ? " MT/s" : "", p->tck_ps);
    grid_row_txt(g, &r, "Speed", p->data_rate_mt_s ? buf : "");
    if (p->has_cl) {
        snprintf(buf, sizeof(buf), "%u%s", p->cl,
                 p->cl_mask ? (p->cl_in_mask ? "  (in CL set)" : "  (not in CL set!)")
                            : "  (derived from tAA/tCK)");
        grid_row_txt(g, &r, "CAS latency", buf);
    } else {
        grid_row_txt(g, &r, "CAS latency", "");
    }
    grid_row_txt(g, &r, "tAA", PS(buf, sizeof(buf), p->taa_ps,  tck));
    grid_row_txt(g, &r, "tRCD", PS(buf, sizeof(buf), p->trcd_ps, tck));
    grid_row_txt(g, &r, "tRP", PS(buf, sizeof(buf), p->trp_ps,  tck));
    grid_row_txt(g, &r, "tRAS", PS(buf, sizeof(buf), p->tras_ps, tck));
    grid_row_txt(g, &r, "tRC", PS(buf, sizeof(buf), p->trc_ps,  tck));
    grid_row_txt(g, &r, "tWR", PS(buf, sizeof(buf), p->twr_ps,  tck));
    {
        const char *rfc_lbl[3] = { "tRFC1", "tRFC2", "tRFCsb" };
        const uint32_t rfc_val[3] = { p->trfc1_ns, p->trfc2_ns, p->trfcsb_ns };
        for (int i = 0; i < 3; i++) {
            if (!rfc_val[i]) continue;
            snprintf(buf, sizeof(buf), "%u ns  (%u nCK)", rfc_val[i],
                     spd_ps_to_nck(rfc_val[i] * 1000u, tck));
            grid_row_txt(g, &r, rfc_lbl[i], buf);
        }
    }
    if (p->cl_mask) {
        snprintf(buf, sizeof(buf), "%u – %u (even)", p->cl_min, p->cl_max);
        grid_row_txt(g, &r, "CL set", buf);
    }
    fmt_mv(buf,  sizeof(buf),  p->vdd_mv);
    fmt_mv(buf2, sizeof(buf2), p->vddq_mv);
    snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), " / %s", buf2);
    {
        char b3[96]; fmt_mv(b3, sizeof(b3), p->vpp_mv);
        snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), " / %s", b3);
    }
    grid_row_txt(g, &r, "VDD/VDDQ/VPP", buf);
    if (p->has_vmemctrl) { fmt_mv(buf, sizeof(buf), p->vmemctrl_mv);
                           grid_row_txt(g, &r, "VMemCtrl", buf); }
    if (!is_expo) {
        grid_row_txt(g, &r, "Command rate",
                     p->has_command_rate ? p->command_rate : "");
        snprintf(buf, sizeof(buf), "%s / %s",
                 p->has_dm_boost ? (p->dm_boost ? "on" : "off") : "N/A",
                 p->has_rtmc     ? (p->rtmc     ? "on" : "off") : "N/A");
        grid_row_txt(g, &r, "DM Boost/RTMC", buf);
        if (p->has_lower_limits &&
            (p->trrd_l_ll | p->tccd_l_ll | p->tccd_l_wr_ll | p->tccd_l_wr2_ll |
             p->tccd_l_wtr_ll | p->tccd_s_wtr_ll | p->trtp_ll | p->tfaw_ll)) {
            snprintf(buf, sizeof(buf), "%u/%u/%u/%u/%u/%u/%u/%u nCK",
                     p->trrd_l_ll, p->tccd_l_ll, p->tccd_l_wr_ll,
                     p->tccd_l_wr2_ll, p->tccd_l_wtr_ll, p->tccd_s_wtr_ll,
                     p->trtp_ll, p->tfaw_ll);
            grid_row_txt(g, &r, "LCLK min RRD_L/CCD_L/CCD_L_WR/CCD_L_WR2/"
                               "CCD_L_WTR/CCD_S_WTR/RTP/FAW", buf);
        }
        grid_row_txt(g, &r, "Profile CRC", p->crc_valid ? "OK" : "BAD");
    } else if (s->expo.block_crc.has) {
        grid_row_txt(g, &r, "EXPO block CRC",
                     s->expo.block_crc.matches ? "OK" : "BAD");
    }
}

static void spd_on_profile_changed(GtkDropDown *dd, GParamSpec *pspec,
                                   gpointer user_data)
{
    (void)pspec;
    app_widgets_t *w = user_data;
    int sel = (int)gtk_drop_down_get_selected(dd);
    if (sel == w->spd_prof_sel) return;
    w->spd_prof_sel = sel;
    spd_fill_profile_grid(w);
}

static GtkWidget *build_spd_content(app_widgets_t *w)
{
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_add_css_class(vbox, "compact");
    gtk_widget_set_margin_top(vbox, 4);
    gtk_widget_set_margin_start(vbox, 4);
    gtk_widget_set_margin_end(vbox, 4);
    gtk_widget_set_margin_bottom(vbox, 4);
    gtk_widget_set_hexpand(vbox, TRUE);

    w->lbl_spd_note = make_label("", "footer-muted");
    gtk_widget_set_hexpand(w->lbl_spd_note, TRUE);
    gtk_label_set_wrap(GTK_LABEL(w->lbl_spd_note), TRUE);
    gtk_box_append(GTK_BOX(vbox), w->lbl_spd_note);

    GtkWidget *top = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 3);
    gtk_widget_set_hexpand(top, TRUE);

    /* Left: static module info */
    GtkWidget *mod_box = make_section_box();
    gtk_widget_set_hexpand(mod_box, TRUE);
    {
        gtk_box_append(GTK_BOX(mod_box), make_label("Module", "section-title"));
        GtkWidget *g = gtk_grid_new();
        gtk_grid_set_row_spacing(GTK_GRID(g), 2);
        gtk_grid_set_column_spacing(GTK_GRID(g), 8);
        int r = 0;
        grid_row(g, r++, "Manufacturer:", &w->lbl_spd_man);
        grid_row(g, r++, "DRAM:",          &w->lbl_spd_dram);
        grid_row(g, r++, "Part:",          &w->lbl_spd_part);
        grid_row(g, r++, "Serial:",        &w->lbl_spd_serial);
        grid_row(g, r++, "Date:",          &w->lbl_spd_date);
        grid_row(g, r++, "Rev / step:",    &w->lbl_spd_rev);
        grid_row(g, r++, "Geometry:",      &w->lbl_spd_geom);
        grid_row(g, r++, "Organization:",  &w->lbl_spd_org);
        grid_row(g, r++, "Capacity:",      &w->lbl_spd_caps);
        grid_row(g, r++, "Base voltages:", &w->lbl_spd_volt);
        grid_row(g, r++, "Physical:",      &w->lbl_spd_phys);
        grid_row(g, r++, "Rank1 mirror:",  &w->lbl_spd_mirror);
        grid_row(g, r++, "Base CRC:",      &w->lbl_spd_crc);
        gtk_box_append(GTK_BOX(mod_box), g);
    }
    gtk_box_append(GTK_BOX(top), mod_box);

    /* Right: profile selector + full decode of the selected profile */
    GtkWidget *prof_box = make_section_box();
    gtk_widget_set_hexpand(prof_box, TRUE);
    {
        gtk_box_append(GTK_BOX(prof_box),
                       make_label("Profiles (JEDEC / XMP 3.0 / EXPO)",
                                  "section-title"));
        w->combo_spd_profile = gtk_drop_down_new(
            G_LIST_MODEL(gtk_string_list_new(NULL)), NULL);
        gtk_widget_set_hexpand(w->combo_spd_profile, TRUE);
        g_signal_connect(w->combo_spd_profile, "notify::selected",
                         G_CALLBACK(spd_on_profile_changed), w);
        gtk_box_append(GTK_BOX(prof_box), w->combo_spd_profile);

        GtkWidget *g = gtk_grid_new();
        gtk_grid_set_row_spacing(GTK_GRID(g), 2);
        gtk_grid_set_column_spacing(GTK_GRID(g), 8);
        gtk_widget_set_margin_top(g, 4);
        w->grid_spd_prof = g;
        gtk_box_append(GTK_BOX(prof_box), g);
    }
    gtk_box_append(GTK_BOX(top), prof_box);
    gtk_box_append(GTK_BOX(vbox), top);

    return vbox;
}

/* Fill the static module-info labels (entry NULL → N/A). */
static void spd_update_labels(app_widgets_t *w, const backend_spd_entry_t *e)
{
    char buf[600];

    if (e == s_spd_cur)
        return;                     /* static data — nothing to redo */
    s_spd_cur = e;

    if (!e || !e->decode_ok) {
        const char *err = (e && e->info.error[0])
                        ? e->info.error
                        : "No SPD for this DIMM — no readable SPD EEPROM (spd5118) found for it";
        set_label_text(w->lbl_spd_note, err);
        GtkWidget *labels[] = {
            w->lbl_spd_crc, w->lbl_spd_man, w->lbl_spd_dram, w->lbl_spd_part,
            w->lbl_spd_serial, w->lbl_spd_date, w->lbl_spd_rev,
            w->lbl_spd_geom, w->lbl_spd_org, w->lbl_spd_caps, w->lbl_spd_volt,
            w->lbl_spd_phys, w->lbl_spd_mirror,
        };
        for (size_t i = 0; i < sizeof(labels)/sizeof(labels[0]); i++)
            set_label_text(labels[i], "N/A");
        w->spd_prof_n = 0;
        GtkStringList *model = GTK_STRING_LIST(
            gtk_drop_down_get_model(GTK_DROP_DOWN(w->combo_spd_profile)));
        guint old = g_list_model_get_n_items(G_LIST_MODEL(model));
        for (guint i = old; i > 0; i--) gtk_string_list_remove(model, i - 1);
        spd_fill_profile_grid(w);
        return;
    }
    const spd_info_t *s = &e->info;

    snprintf(buf, sizeof(buf), "SPD of module %s — %s (%d bytes)",
             e->slot_label[0] ? e->slot_label : "?", e->source, e->image_len);
    set_label_text(w->lbl_spd_note, buf);

    if (s->base_crc.has)
        set_label_fmt(w->lbl_spd_crc, "%s (0x%04X / 0x%04X)",
                      s->base_crc.matches ? "OK" : "BAD",
                      s->base_crc.computed, s->base_crc.stored);
    else
        set_label_text(w->lbl_spd_crc, "N/A");

    if (s->manufacturing.has) {
        if (s->manufacturing.module_mfr[0])
            set_label_text(w->lbl_spd_man, s->manufacturing.module_mfr);
        else
            set_label_fmt(w->lbl_spd_man, "JEP-106 bank %u code 0x%02X",
                          s->manufacturing.module_mfr_bank,
                          s->manufacturing.module_mfr_code);
        if (s->manufacturing.dram_mfr[0])
            set_label_fmt(w->lbl_spd_dram, "%s (step 0x%02X)",
                          s->manufacturing.dram_mfr, s->manufacturing.dram_stepping);
        else
            set_label_fmt(w->lbl_spd_dram, "JEP-106 bank %u code 0x%02X",
                          s->manufacturing.dram_mfr_bank,
                          s->manufacturing.dram_mfr_code);
        set_e(w->lbl_spd_part, s->manufacturing.part_number[0] != '\0',
              s->manufacturing.part_number);
        set_label_fmt(w->lbl_spd_serial, "%08X", s->manufacturing.serial);
        set_label_fmt(w->lbl_spd_date, "week %u of %u",
                      s->manufacturing.week, s->manufacturing.year);
        set_label_fmt(w->lbl_spd_rev, "0x%02X / 0x%02X",
                      s->manufacturing.revision_code,
                      s->manufacturing.dram_stepping);
    } else {
        set_label_text(w->lbl_spd_man, "N/A");
        set_label_text(w->lbl_spd_dram, "N/A");
        set_label_text(w->lbl_spd_part, "N/A");
        set_label_text(w->lbl_spd_serial, "N/A");
        set_label_text(w->lbl_spd_date, "N/A");
        set_label_text(w->lbl_spd_rev, "N/A");
    }

    if (s->identity.has) {
        const spd_identity_t *id = &s->identity;
        set_label_fmt(w->lbl_spd_geom, "x%u, %u Gb/die %s, %uR x %u ch, %u+%u bit bus",
                      id->io_width_bits, id->density_per_die_gb,
                      id->package_type_str, id->package_ranks_per_channel,
                      id->channels_per_dimm, id->primary_bus_width_bits,
                      id->ecc_width_bits);
        set_label_fmt(w->lbl_spd_org, "%s%s, R%u C%u, %u BG x %u banks, SPD rev %X.%X",
                      id->module_type_str, id->hybrid ? " (hybrid)" : "",
                      id->row_address_bits, id->column_address_bits,
                      id->bank_groups, id->banks_per_bank_group,
                      id->spd_rev_major, id->spd_rev_minor);
        if (id->has_capacity)
            set_label_fmt(w->lbl_spd_caps, "%.0f GB",
                          (double)id->capacity_bytes / 1e9);
        else
            set_label_text(w->lbl_spd_caps, "N/A");
        if (id->has_voltages)
            set_label_fmt(w->lbl_spd_volt, "%u.%03u / %u.%03u / %u.%03u V",
                          id->vdd_nominal_mv,  id->vdd_nominal_mv % 1000,
                          id->vddq_nominal_mv, id->vddq_nominal_mv % 1000,
                          id->vpp_nominal_mv,  id->vpp_nominal_mv % 1000);
        else
            set_label_text(w->lbl_spd_volt, "N/A");
    } else {
        set_label_text(w->lbl_spd_geom, "N/A");
        set_label_text(w->lbl_spd_org, "N/A");
        set_label_text(w->lbl_spd_caps, "N/A");
        set_label_text(w->lbl_spd_volt, "N/A");
    }

    if (s->module.has) {
        set_label_fmt(w->lbl_spd_phys, "%u mm, card %s",
                      s->module.nominal_height_mm, s->module.raw_card);
        set_label_text(w->lbl_spd_mirror,
                       s->module.rank1_address_mirrored ? "Yes" : "No");
    } else if (s->module.not_decoded_reason[0]) {
        set_label_text(w->lbl_spd_phys, s->module.not_decoded_reason);
        set_label_text(w->lbl_spd_mirror, "N/A");
    } else {
        set_label_text(w->lbl_spd_phys, "N/A");
        set_label_text(w->lbl_spd_mirror, "N/A");
    }

    spd_populate_profiles(w, s);
    spd_fill_profile_grid(w);
}

/* Compare ignoring surrounding whitespace and case. */
static bool part_eq(const char *a, const char *b)
{
    char ta[STR_LEN], tb[STR_LEN];
    size_t la = strlen(a), lb = strlen(b);
    while (la && (a[la-1] == ' ' || a[la-1] == '\t')) la--;
    while (lb && (b[lb-1] == ' ' || b[lb-1] == '\t')) lb--;
    if (la >= sizeof(ta) || lb >= sizeof(tb)) return false;
    memcpy(ta, a, la); ta[la] = '\0';
    memcpy(tb, b, lb); tb[lb] = '\0';
    return la && lb && strcasecmp(ta, tb) == 0;
}

/* Show the SPD of the currently selected DIMM. Pairing: serial (backend,
 * decimal or hex) first; otherwise same-kit entries are matched by order —
 * SPD hub addresses follow physical DIMM order, modules by slot label. */
static void spd_update_for_module(app_widgets_t *w)
{
    int n = backend_spd_load();
    int mi = w->selected_module;
    if (mi < 0 || mi >= w->summary.module_count) {
        spd_update_labels(w, NULL);
        return;
    }
    const memory_module_t *m = &w->summary.modules[mi];

    for (int i = 0; i < n; i++) {
        const backend_spd_entry_t *e = backend_spd_get(i);
        if (e && e->module_idx == mi) { spd_update_labels(w, e); return; }
    }

    int spd_idx[MAX_MODULES], spd_n = 0;
    int mod_idx[MAX_MODULES], mod_n = 0;
    for (int i = 0; i < n; i++) {
        const backend_spd_entry_t *e = backend_spd_get(i);
        if (e && e->decode_ok && e->module_idx < 0 &&
            part_eq(m->part_number, e->info.manufacturing.part_number))
            spd_idx[spd_n++] = i;
    }
    for (int i = 0; i < w->summary.module_count; i++) {
        const memory_module_t *mm = &w->summary.modules[i];
        bool taken = false;
        for (int k = 0; k < n; k++) {
            const backend_spd_entry_t *e = backend_spd_get(k);
            if (e && e->module_idx == i) { taken = true; break; }
        }
        if (!taken && part_eq(m->part_number, mm->part_number))
            mod_idx[mod_n++] = i;
    }
    if (spd_n == 0 || spd_n != mod_n) {
        spd_update_labels(w, NULL);
        return;
    }
    for (int i = 0; i < mod_n; i++)            /* modules by slot label */
        for (int j = i + 1; j < mod_n; j++)
            if (strcmp(w->summary.modules[mod_idx[j]].slot_label,
                       w->summary.modules[mod_idx[i]].slot_label) < 0) {
                int t = mod_idx[i]; mod_idx[i] = mod_idx[j]; mod_idx[j] = t;
            }
    for (int i = 0; i < spd_n; i++)            /* SPD by bus+address */
        for (int j = i + 1; j < spd_n; j++) {
            int bi, ai, bj, aj;
            sscanf(backend_spd_get(spd_idx[i])->source,
                   "/sys/bus/i2c/devices/%d-%x", &bi, &ai);
            sscanf(backend_spd_get(spd_idx[j])->source,
                   "/sys/bus/i2c/devices/%d-%x", &bj, &aj);
            if (bj < bi || (bj == bi && aj < ai)) {
                int t = spd_idx[i]; spd_idx[i] = spd_idx[j]; spd_idx[j] = t;
            }
        }
    for (int i = 0; i < mod_n; i++)
        if (mod_idx[i] == mi) {
            spd_update_labels(w, backend_spd_get(spd_idx[i]));
            return;
        }
    spd_update_labels(w, NULL);
}

/* ── Refresh data → UI ──────────────────────────────────────────────── */

static void refresh_ui(app_widgets_t *w)
{
    backend_read_summary(&w->summary);
    const system_summary_t *s = &w->summary;
    const smu_metrics_t *m = &s->metrics;

    const dram_channel_t *d = cur_channel(w);
    if (d) {
        int mi = w->selected_module;
        if (mi >= 0 && mi < s->module_count && s->modules[mi].channel_idx >= 0)
            set_label_fmt(w->lbl_channel_note, "%s — UMC registers of the channel holding module %s",
                          d->label, s->modules[mi].slot_label);
        else
            set_label_fmt(w->lbl_channel_note, "%s — SMN offset 0x%06X",
                          d->label, d->smn_offset);
    } else {
        set_label_text(w->lbl_channel_note, "No populated memory channel detected");
    }

    /* Header */
    set_label_text(w->lbl_cpu_name, s->cpu.processor_name[0] ? s->cpu.processor_name : s->cpu.name);
    set_label_fmt(w->lbl_codename, "%s  ·  SMU %s  ·  %s",
                  s->cpu.codename, s->cpu.smu_version, s->cpu.pm_table_version);
    set_label_text(w->lbl_board_info, s->board.display_line);

    /* Module dropdown — populate once */
    if (s->module_count > 0 && !w->modules_populated) {
        GtkStringList *model = GTK_STRING_LIST(gtk_drop_down_get_model(GTK_DROP_DOWN(w->combo_modules)));
        /* Remove placeholder */
        guint n = g_list_model_get_n_items(G_LIST_MODEL(model));
        for (guint i = n; i > 0; i--)
            gtk_string_list_remove(model, i - 1);
        for (int i = 0; i < s->module_count; i++)
            gtk_string_list_append(model, s->modules[i].slot_display);
        gtk_drop_down_set_selected(GTK_DROP_DOWN(w->combo_modules), 0);
        w->selected_module = 0;
        w->modules_populated = 1;
    }

    int mi = w->selected_module;

    /* DIMM speeds */
    if (s->memory.frequency > 0.0f)
        set_label_fmt(w->lbl_speed, "%.0f MT/s", s->memory.frequency);
    else
        set_label_text(w->lbl_speed, "—");
    set_label_fmt(w->lbl_mclk, "%.0f MHz", m->mclk_mhz);
    set_label_fmt(w->lbl_fclk, "%.0f MHz", m->fclk_mhz);
    set_label_fmt(w->lbl_uclk, "%.0f MHz", m->uclk_mhz);
    set_label_fmt(w->lbl_bclk, "%.1f MHz", m->bclk_mhz);
    set_label_text(w->lbl_gdm, d ? (d->t.gdm_enabled ? "True" : "False") : "N/A");
    set_label_text(w->lbl_powerdown, d ? (d->t.power_down_enabled ? "True" : "False") : "N/A");

    /* SPD temp */
    if (mi >= 0 && mi < m->spd_temps_count)
        set_label_fmt(w->lbl_spd_temp, "%.1f °C", m->spd_temps_c[mi]);
    else
        set_label_text(w->lbl_spd_temp, "—");

    /* DIMM info */
    if (mi >= 0 && mi < s->module_count) {
        const memory_module_t *mod = &s->modules[mi];
        set_label_text(w->lbl_capacity, mod->capacity_display);
        set_label_text(w->lbl_manufacturer, mod->manufacturer[0] ? mod->manufacturer : "—");
        set_label_text(w->lbl_part_number, mod->part_number[0] ? mod->part_number : "—");
        set_label_text(w->lbl_serial_number, mod->serial_number[0] ? mod->serial_number : "—");
        const char *rank_str = mod->rank == RANK_QR ? "QR" : mod->rank == RANK_DR ? "DR" : "SR";
        set_label_text(w->lbl_rank, rank_str);
        if (mod->channel_idx >= 0 && mod->channel_idx < s->channel_count &&
            s->channels[mod->channel_idx].present)
            set_label_text(w->lbl_dimm_channel, s->channels[mod->channel_idx].label);
        else
            set_label_text(w->lbl_dimm_channel, "N/A");
    }
    set_s(w->lbl_cmd2t, d, d ? d->t.cmd2t : NULL);

/* Helper: show voltage or "—" if unavailable (0) */
#define SET_VOLT(lbl, val) \
    ((val) > 0 ? set_label_fmt((lbl), "%.4fV", (val)) : set_label_text((lbl), "—"))

    /* Voltages */
    SET_VOLT(w->lbl_vsoc,     m->vsoc);
    SET_VOLT(w->lbl_vddp,     m->vddp);
    SET_VOLT(w->lbl_vddg_ccd, m->vddg_ccd);
    SET_VOLT(w->lbl_vddg_iod, m->vddg_iod);
    SET_VOLT(w->lbl_vdd_misc, m->vdd_misc);
    SET_VOLT(w->lbl_mem_vdd,  m->mem_vdd);
    SET_VOLT(w->lbl_mem_vddq, m->mem_vddq);
    SET_VOLT(w->lbl_cpu_vddio, m->cpu_vddio);
    SET_VOLT(w->lbl_mem_vpp,  m->mem_vpp);
    SET_VOLT(w->lbl_vcore,    m->vcore);
    set_label_fmt(w->lbl_ppt, "%.1fW", m->ppt_w);

    /* Primary timings (selected UMC channel) */
    set_u(w->lbl_tcl, d, d ? d->t.tcl : 0);
    set_u(w->lbl_trcd_rd, d, d ? d->t.trcd_rd : 0);
    set_u(w->lbl_trcd_wr, d, d ? d->t.trcd_wr : 0);
    set_u(w->lbl_trp, d, d ? d->t.trp : 0);
    set_u(w->lbl_tras, d, d ? d->t.tras : 0);
    set_u(w->lbl_trc, d, d ? d->t.trc : 0);
    set_u(w->lbl_trrds, d, d ? d->t.trrds : 0);
    set_u(w->lbl_trrdl, d, d ? d->t.trrdl : 0);
    set_u(w->lbl_tfaw, d, d ? d->t.tfaw : 0);
    set_u(w->lbl_twr, d, d ? d->t.twr : 0);
    set_u(w->lbl_tcwl, d, d ? d->t.tcwl : 0);
    if (d && d->is_ddr5 && d->t.rfc2 && d->refresh_mode[0] &&
        strcmp(d->refresh_mode, "NORMAL") != 0)
        set_label_fmt(w->lbl_trfc_ns, "%.0f  (tRFC2: %.0f)",
                      (double)d->t.trfc_ns, (double)d->t.trfc2_ns);
    else
        set_f(w->lbl_trfc_ns, d, d ? d->t.trfc_ns : 0);
    set_u(w->lbl_rfc, d, d ? d->t.rfc : 0);
    set_u(w->lbl_rfc2, d, d ? d->t.rfc2 : 0);
    set_u(w->lbl_rfcsb, d, d ? d->t.rfcsb : 0);

    /* Secondary timings */
    set_u(w->lbl_rtp, d, d ? d->t.rtp : 0);
    set_u(w->lbl_wtrs, d, d ? d->t.wtrs : 0);
    set_u(w->lbl_wtrl, d, d ? d->t.wtrl : 0);
    set_u(w->lbl_rdwr, d, d ? d->t.rdwr : 0);
    set_u(w->lbl_wrrd, d, d ? d->t.wrrd : 0);
    set_u(w->lbl_rdrd_sc, d, d ? d->t.rdrd_sc : 0);
    set_u(w->lbl_rdrd_sd, d, d ? d->t.rdrd_sd : 0);
    set_u(w->lbl_rdrd_dd, d, d ? d->t.rdrd_dd : 0);
    set_u(w->lbl_wrwr_sc, d, d ? d->t.wrwr_sc : 0);
    set_u(w->lbl_wrwr_sd, d, d ? d->t.wrwr_sd : 0);
    set_u(w->lbl_wrwr_dd, d, d ? d->t.wrwr_dd : 0);
    set_u(w->lbl_refi, d, d ? d->t.refi : 0);
    set_f(w->lbl_trefi_ns, d, d ? d->t.trefi_ns : 0);
    set_u(w->lbl_wrpre, d, d ? d->t.wrpre : 0);
    set_u(w->lbl_rdpre, d, d ? d->t.rdpre : 0);

    /* Tertiary timings */
    set_u(w->lbl_rdrd_scl, d, d ? d->t.rdrd_scl : 0);
    set_u(w->lbl_wrwr_scl, d, d ? d->t.wrwr_scl : 0);
    set_u(w->lbl_cke, d, d ? d->t.cke : 0);
    set_u(w->lbl_xp, d, d ? d->t.xp : 0);
    set_u(w->lbl_trc_page, d, d ? d->t.trc_page : 0);
    set_u(w->lbl_mod, d, d ? d->t.mod : 0);
    set_u(w->lbl_mod_pda, d, d ? d->t.mod_pda : 0);
    set_u(w->lbl_mrd, d, d ? d->t.mrd : 0);
    set_u(w->lbl_mrd_pda, d, d ? d->t.mrd_pda : 0);
    set_u(w->lbl_stag, d, d ? d->t.stag : 0);
    set_u(w->lbl_stag_sb, d, d ? d->t.stag_sb : 0);
    set_u(w->lbl_phy_wrl, d, d ? d->t.phy_wrl : 0);
    set_u(w->lbl_phy_rdl, d, d ? d->t.phy_rdl : 0);
    set_u(w->lbl_phy_wrd, d, d ? d->t.phy_wrd : 0);

    /* Extended timings */
    set_u(w->lbl_rp_pb, d, d ? d->rp_pb : 0);
    set_u(w->lbl_rc_pb, d, d ? d->rc_pb : 0);
    set_u(w->lbl_ppd, d, d ? d->ppd : 0);
    set_u(w->lbl_mw, d, d ? d->mw : 0);
    set_u(w->lbl_rdpost, d, d ? d->rdpost : 0);
    set_u(w->lbl_wrpost, d, d ? d->wrpost : 0);
    set_u(w->lbl_rdrd_ban, d, d ? d->rdrd_ban : 0);
    set_u(w->lbl_wrwr_ban, d, d ? d->wrwr_ban : 0);
    set_u(w->lbl_dllk, d, d ? d->dllk : 0);
    set_u(w->lbl_xs, d, d ? d->xs : 0);
    set_u(w->lbl_mrri, d, d ? d->mrri : 0);
    set_u(w->lbl_mrr, d, d ? d->mrr : 0);
    set_u(w->lbl_mrw, d, d ? d->mrw : 0);
    set_u(w->lbl_ctrl_switch, d, d ? d->ctrl_switch_clks : 0);
    set_u(w->lbl_csh, d, d ? d->csh : 0);
    set_u(w->lbl_cacsh, d, d ? d->cacsh : 0);
    set_u(w->lbl_srx2srx, d, d ? d->srx2srx : 0);
    set_u(w->lbl_stab, d, d ? d->tstab : 0);
    set_u(w->lbl_zqcs, d, d ? d->tzqcs : 0);
    set_u(w->lbl_zqoper, d, d ? d->tzq_oper_cal : 0);
    set_u(w->lbl_zqcs_int, d, d ? d->zqcs_interval : 0);

    /* Controller config */
    if (d && d->has_ranks) {
        char rb[96];
        const char *r0 = d->ranks[0] == 2 ? "DR" : d->ranks[0] == 1 ? "SR" : "-";
        const char *r1 = d->ranks[1] == 2 ? "DR" : d->ranks[1] == 1 ? "SR" : "-";

        /* Label the two UMC DIMM slots (CS0/CS2) with physical slot names.
         * No numbering convention is assumed: when the number of modules
         * SMBIOS reports on this channel equals the number of populated
         * chip-select presence bits, the names are paired in slot order
         * (SMBIOS order -> populated CS positions, lowest first). The empty
         * slot's name is extrapolated from its neighbour's digit so 0-based
         * (A0/A1) and 1-based (A1/A2) silkscreens both work. On any count
         * mismatch keep generic DIMM0/DIMM1 rather than guess. */
        int ci = (int)(d - s->channels);
        char slot_name[2][32] = { { 0 }, { 0 } };
        int slot_digit[2] = { -1, -1 };
        char pop_label[2][32]; int pop_digit[2]; int npop_lbl = 0;
        for (int i = 0; i < s->module_count; i++) {
            const memory_module_t *m = &s->modules[i];
            if (m->channel_idx != ci || !m->slot_label[0])
                continue;
            if (npop_lbl >= 2) {
                npop_lbl = 3;   /* >2 modules on a channel: force mismatch */
                continue;
            }
            char sl = 0;
            int num = -1;
            if (sscanf(m->slot_label, "%c%d", &sl, &num) != 2 || num < 0)
                continue;
            /* keep list sorted by slot digit */
            int at = npop_lbl;
            while (at > 0 && pop_digit[at - 1] > num) {
                snprintf(pop_label[at], sizeof(pop_label[at]), "%.31s", pop_label[at - 1]);
                pop_digit[at] = pop_digit[at - 1];
                at--;
            }
            snprintf(pop_label[at], sizeof(pop_label[at]), "%.31s", m->slot_label);
            pop_digit[at] = num;
            if (npop_lbl < 2)
                npop_lbl++;
        }
        int npop_cs = (d->has_dimm[0] ? 1 : 0) + (d->has_dimm[1] ? 1 : 0);
        if (npop_lbl == npop_cs && npop_lbl > 0) {
            int li = 0;
            for (int p = 0; p < 2; p++) {
                if (!d->has_dimm[p])
                    continue;
                snprintf(slot_name[p], sizeof(slot_name[p]), "%s", pop_label[li]);
                slot_digit[p] = pop_digit[li];
                li++;
            }
            /* extrapolate the unpopulated slot's name from its neighbour */
            for (int p = 0; p < 2; p++) {
                if (slot_name[p][0] || !slot_name[1 - p][0])
                    continue;
                int ex = slot_digit[1 - p] + (p - (1 - p));
                if (ex < 0)
                    continue;
                snprintf(slot_name[p], 32, "%c%d", slot_name[1 - p][0], ex);
            }
        }

        if (slot_name[0][0] && slot_name[1][0])
            snprintf(rb, sizeof(rb), "%s %s / %s %s",
                     slot_name[0], d->has_dimm[0] ? r0 : "empty",
                     slot_name[1], d->has_dimm[1] ? r1 : "empty");
        else
            snprintf(rb, sizeof(rb), "DIMM0 %s / DIMM1 %s",
                     d->has_dimm[0] ? r0 : "empty", d->has_dimm[1] ? r1 : "empty");
        set_label_text(w->lbl_channel_ranks, rb);
    } else {
        set_label_text(w->lbl_channel_ranks, "N/A");
    }
    if (d && d->ratio > 0.0f)
        set_label_fmt(w->lbl_memclk_ratio, "%.2f (%.0f MT/s)",
                      (double)d->ratio, (double)d->t.frequency_hint_mhz);
    else
        set_label_text(w->lbl_memclk_ratio, "N/A");
    set_s(w->lbl_refresh_mode, d, d ? d->refresh_mode : NULL);
    set_u(w->lbl_fgr, d, d ? d->fgr : 0);
    set_b(w->lbl_bgs, d, d ? d->bgs : false);
    set_b(w->lbl_bgs_alt, d, d ? d->bgs_alt : false);
    set_b(w->lbl_bank_group_en, d, d ? d->bank_group_en : false);
    set_b(w->lbl_ecc, d, d ? d->ecc_enabled : false);
    set_s(w->lbl_burst_length, d, d ? d->burst_length : NULL);
    set_s(w->lbl_burst_ctrl, d, d ? d->burst_ctrl : NULL);
    set_s(w->lbl_pd_mode, d, d ? d->power_down_mode : NULL);
    set_b(w->lbl_aggr_pd, d, d ? d->aggressive_power_down : false);
    set_b(w->lbl_odts, d, d ? d->odts_en : false);
    set_u(w->lbl_odts_int, d, d ? d->odts_read_interval : 0);
    set_b(w->lbl_uclk_gt_fclk, d, d ? d->uclk_gt_fclk != 0 : false);
    set_s(w->lbl_wck_ratio, d, d ? d->wck_ck_ratio : NULL);
    set_s(w->lbl_nitro, d, d ? d->nitro_rx_tx_ctrl : NULL);
    set_s(w->lbl_nitro_mode, d, d ? d->nitro_dly_mode : NULL);

    /* Electrical (AOD) — raw codes decoded to impedances in dram_decode.c */
    {
        const aod_mem_info_t *a = &s->aod;
        char tmp[48];
        bool pu_pd = a->has_proc_odt_pu && a->has_proc_odt_pd &&
                     (a->proc_odt_pu_raw != a->proc_odt_pd_raw);
        if (a->has_proc_odt && !pu_pd) {
            set_e(w->lbl_proc_odt, true, proc_odt_decode(a->proc_odt_raw));
            set_label_text(w->lbl_proc_odt_pu, "");
            set_label_text(w->lbl_proc_odt_pd, "");
        } else {
            set_label_text(w->lbl_proc_odt, a->available ? "→" : "N/A");
            set_e(w->lbl_proc_odt_pu, a->has_proc_odt_pu, proc_odt_decode(a->proc_odt_pu_raw));
            set_e(w->lbl_proc_odt_pd, a->has_proc_odt_pd, proc_odt_decode(a->proc_odt_pd_raw));
        }
        rtt_decode(a->rtt_nom_wr_raw, tmp, sizeof(tmp));
        set_e(w->lbl_rtt_nom_wr, a->has_rtt_nom_wr, tmp);
        rtt_decode(a->rtt_nom_rd_raw, tmp, sizeof(tmp));
        set_e(w->lbl_rtt_nom_rd, a->has_rtt_nom_rd, tmp);
        rtt_decode(a->rtt_wr_raw, tmp, sizeof(tmp));
        set_e(w->lbl_rtt_wr, a->has_rtt_wr, tmp);
        rtt_decode(a->rtt_park_raw, tmp, sizeof(tmp));
        set_e(w->lbl_rtt_park, a->has_rtt_park, tmp);
        rtt_decode(a->rtt_park_dqs_raw, tmp, sizeof(tmp));
        set_e(w->lbl_rtt_park_dqs, a->has_rtt_park_dqs, tmp);
        set_e(w->lbl_cadbus_ds, a->has_cad_bus_ds, cad_bus_ds_decode(a->cad_bus_ds_raw));
        set_e(w->lbl_proc_dq_ds, a->has_proc_data_ds && !a->has_proc_dq_ds_pu,
              proc_data_ds_decode(a->proc_data_ds_raw));
        if (a->has_proc_dq_ds_pu) {
            set_label_text(w->lbl_proc_dq_ds, "");
            set_e(w->lbl_proc_dq_ds_pu, true, proc_data_ds_decode(a->proc_dq_ds_pu_raw));
            set_e(w->lbl_proc_dq_ds_pd, a->has_proc_dq_ds_pd, proc_data_ds_decode(a->proc_dq_ds_pd_raw));
        } else {
            set_label_text(w->lbl_proc_dq_ds_pu, "");
            set_label_text(w->lbl_proc_dq_ds_pd, "");
        }
        set_e(w->lbl_proc_cs_ds, a->has_proc_cs_ds, proc_impedance_decode(a->proc_cs_ds_raw));
        set_e(w->lbl_proc_ck_ds, a->has_proc_ck_ds, proc_impedance_decode(a->proc_ck_ds_raw));
        set_e(w->lbl_dram_ds, a->has_dram_data_ds && !a->has_dram_dq_ds_pu,
              dram_data_ds_decode(a->dram_data_ds_raw));
        if (a->has_dram_dq_ds_pu) {
            set_label_text(w->lbl_dram_ds, "");
            set_e(w->lbl_dram_dq_ds_pu, true, dram_data_ds_decode(a->dram_dq_ds_pu_raw));
            set_e(w->lbl_dram_dq_ds_pd, a->has_dram_dq_ds_pd, dram_data_ds_decode(a->dram_dq_ds_pd_raw));
        } else {
            set_label_text(w->lbl_dram_dq_ds_pu, "");
            set_label_text(w->lbl_dram_dq_ds_pd, "");
        }
    }

    /* Footer mem type */
    const char *mem_str = s->memory.type == MEM_DDR5 ? "DDR5" :
                          s->memory.type == MEM_DDR4 ? "DDR4" : "—";
    set_label_text(w->lbl_footer_type, mem_str);

    /* CPU tab — VID & per-core voltages */
    SET_VOLT(w->lbl_vid, m->vid);
    {
        int count = m->core_voltages_count < MAX_CORES ? m->core_voltages_count : MAX_CORES;
        for (int i = 0; i < MAX_CORES; i++) {
            gboolean vis = (i < count);
            gtk_widget_set_visible(w->lbl_core_volt[i],     vis);
            gtk_widget_set_visible(w->lbl_core_volt_lbl[i], vis);
            if (vis) SET_VOLT(w->lbl_core_volt[i], m->core_voltages[i]);
        }
    }

    /* CPU tab — Temperatures (single blob row) */
    {
        char tbuf[256];
        int  toff = 0;
        if (m->has_tdie)        toff += snprintf(tbuf+toff, sizeof(tbuf)-toff, "Tdie: %.1f\xC2\xB0""C  ", m->tdie_c);
        if (m->has_tctl)        toff += snprintf(tbuf+toff, sizeof(tbuf)-toff, "Tctl: %.1f\xC2\xB0""C  ", m->tctl_c);
        if (m->has_tccd1)       toff += snprintf(tbuf+toff, sizeof(tbuf)-toff, "Tccd1: %.1f\xC2\xB0""C  ", m->tccd1_c);
        if (m->has_tccd2)       toff += snprintf(tbuf+toff, sizeof(tbuf)-toff, "Tccd2: %.1f\xC2\xB0""C  ", m->tccd2_c);
        if (m->has_iod_hotspot) toff += snprintf(tbuf+toff, sizeof(tbuf)-toff, "IOD: %.1f\xC2\xB0""C", m->iod_hotspot_c);
        if (toff == 0) snprintf(tbuf, sizeof(tbuf), "—");
        set_label_text(w->lbl_tdie, tbuf);
    }

    {
        int count = m->core_temps_count;
        if (m->core_usage_count > count) count = m->core_usage_count;
        if (m->core_freq_count  > count) count = m->core_freq_count;
        if (count > MAX_CORES) count = MAX_CORES;
        if (count == 0) {
            set_label_text(w->lbl_core_temps, "—");
        } else {
            char buf[2048];
            int  off = 0;
            for (int i = 0; i < count; i++) {
                float temp  = (i < m->core_temps_count) ? m->core_temps_c[i]  : 0;
                float usage = (i < m->core_usage_count) ? m->core_usage_pct[i]: 0;
                float freq  = (i < m->core_freq_count)  ? m->core_freq_mhz[i] : 0;
                off += snprintf(buf+off, sizeof(buf)-off,
                                "C%d: %.1f\xC2\xB0""C  %.0f%%  %.0fMHz\n",
                                i, temp, usage, freq);
            }
            /* trim trailing newline */
            if (off > 0 && buf[off-1] == '\n') buf[off-1] = '\0';
            set_label_text(w->lbl_core_temps, buf);
        }
    }

    /* Fans */
    {
        char buf[1024];
        int off = 0;
        for (int i = 0; i < s->fan_count; i++)
            off += snprintf(buf + off, sizeof(buf) - off, "%s: %d RPM  ", s->fans[i].label, s->fans[i].rpm);
        if (off == 0)
            snprintf(buf, sizeof(buf),
                     "N/A — no kernel interface for this board's fan controller (EC-only)");
        set_label_text(w->lbl_fans, buf);
    }

    /* SPD page (static data — updates only when pairing changes) */
    spd_update_for_module(w);
}

/* Timer callback */
static gboolean on_refresh(gpointer user_data)
{
    setlocale(LC_NUMERIC, "C");
    app_widgets_t *w = (app_widgets_t *)user_data;
    refresh_ui(w);
    return G_SOURCE_CONTINUE;
}

/* Debug dump button clicked */
static void on_debug_dump(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    GtkWidget *parent = GTK_WIDGET(user_data);

    char *text = backend_read_debug_dump();
    if (!text) {
        text = strdup("(failed to read debug data)");
        if (!text) return;
    }

    GtkWidget *win = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(win), "Debug Dump");
    gtk_window_set_default_size(GTK_WINDOW(win), 700, 600);
    gtk_window_set_transient_for(GTK_WINDOW(win), GTK_WINDOW(gtk_widget_get_root(parent)));
    gtk_window_set_modal(GTK_WINDOW(win), FALSE);

    GtkWidget *scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

    GtkWidget *tv = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(tv), FALSE);
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(tv), TRUE);
    gtk_text_view_set_left_margin(GTK_TEXT_VIEW(tv), 8);
    gtk_text_view_set_right_margin(GTK_TEXT_VIEW(tv), 8);
    gtk_text_view_set_top_margin(GTK_TEXT_VIEW(tv), 8);
    GtkTextBuffer *tbuf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(tv));
    gtk_text_buffer_set_text(tbuf, text, -1);
    free(text);

    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), tv);
    gtk_window_set_child(GTK_WINDOW(win), scroll);
    gtk_window_present(GTK_WINDOW(win));
}

/* ── Benchmark tab ──────────────────────────────────────────────────── */

typedef struct {
    app_widgets_t  *w;
    bench_results_t results;
} bench_job_t;

static gboolean bench_done(gpointer data)
{
    bench_job_t *job = data;
    app_widgets_t *w = job->w;
    bench_results_t *r = &job->results;

    set_label_fmt(w->lbl_bench_lat_l1,   "%.1f ns",  r->lat_l1_ns);
    set_label_fmt(w->lbl_bench_lat_l2,   "%.1f ns",  r->lat_l2_ns);
    set_label_fmt(w->lbl_bench_lat_l3,   "%.1f ns",  r->lat_l3_ns);
    set_label_fmt(w->lbl_bench_lat_dram, "%.1f ns",  r->lat_dram_ns);
    set_label_fmt(w->lbl_bench_bw_read,  "%.0f MB/s", r->bw_read_mbs);
    set_label_fmt(w->lbl_bench_bw_write, "%.0f MB/s", r->bw_write_mbs);
    set_label_fmt(w->lbl_bench_bw_copy,  "%.0f MB/s", r->bw_copy_mbs);

    set_label_text(w->lbl_bench_status, "Done");
    gtk_widget_set_sensitive(w->btn_bench_run, TRUE);
    free(job);
    return G_SOURCE_REMOVE;
}

static gpointer bench_thread(gpointer data)
{
    bench_job_t *job = data;
    bench_run(&job->results);
    g_idle_add(bench_done, job);
    return NULL;
}

static void on_bench_run(GtkButton *btn, gpointer user_data)
{
    app_widgets_t *w = user_data;
    gtk_widget_set_sensitive(GTK_WIDGET(btn), FALSE);
    set_label_text(w->lbl_bench_status, "Running…");

    bench_job_t *job = malloc(sizeof(*job));
    if (!job) return;
    job->w = w;
    memset(&job->results, 0, sizeof(job->results));
    g_thread_unref(g_thread_new("bench", bench_thread, job));
}

/* ── Pi benchmark tab ───────────────────────────────────────────────── */

typedef struct {
    app_widgets_t *w;
    pi_results_t   results;
    int            n_digits;
} pi_job_t;

static gboolean pi_done(gpointer data)
{
    pi_job_t *job = data;
    app_widgets_t *w = job->w;
    pi_results_t *r = &job->results;

    if (r->time_sec < 1.0)
        set_label_fmt(w->lbl_pi_time, "%.1f ms", r->time_sec * 1000.0);
    else
        set_label_fmt(w->lbl_pi_time, "%.3f s", r->time_sec);

    set_label_text(w->lbl_pi_status, "Done");
    gtk_widget_set_sensitive(w->btn_pi_run, TRUE);
    gtk_widget_set_sensitive(w->combo_pi_digits, TRUE);
    free(job);
    return G_SOURCE_REMOVE;
}

static gpointer pi_thread(gpointer data)
{
    pi_job_t *job = data;
    pi_bench_run(job->n_digits, &job->results);
    g_idle_add(pi_done, job);
    return NULL;
}

static void on_pi_run(GtkButton *btn, gpointer user_data)
{
    app_widgets_t *w = user_data;
    gtk_widget_set_sensitive(GTK_WIDGET(btn), FALSE);
    gtk_widget_set_sensitive(w->combo_pi_digits, FALSE);
    set_label_text(w->lbl_pi_status, "Running…");

    static const int digit_counts[] = { 1000000, 10000000, 100000000, 200000000 };
    guint sel = gtk_drop_down_get_selected(GTK_DROP_DOWN(w->combo_pi_digits));
    if (sel >= G_N_ELEMENTS(digit_counts)) sel = 0;

    pi_job_t *job = malloc(sizeof(*job));
    if (!job) return;
    job->w        = w;
    job->n_digits = digit_counts[sel];
    memset(&job->results, 0, sizeof(job->results));
    g_thread_unref(g_thread_new("pi", pi_thread, job));
}

static GtkWidget *build_bench_tab(app_widgets_t *w)
{
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_margin_top(vbox, 8);
    gtk_widget_set_margin_start(vbox, 8);
    gtk_widget_set_margin_end(vbox, 8);
    gtk_widget_set_margin_bottom(vbox, 8);

    /* Run button row */
    GtkWidget *btn_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    w->btn_bench_run = gtk_button_new_with_label("Run Benchmark");
    g_signal_connect(w->btn_bench_run, "clicked", G_CALLBACK(on_bench_run), w);
    w->lbl_bench_status = make_label("Ready", "header-muted");
    gtk_widget_set_valign(w->lbl_bench_status, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(btn_row), w->btn_bench_run);
    gtk_box_append(GTK_BOX(btn_row), w->lbl_bench_status);
    gtk_box_append(GTK_BOX(vbox), btn_row);

    /* Two-column row: Latency | Bandwidth */
    GtkWidget *cols = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_widget_set_hexpand(cols, TRUE);
    gtk_box_set_homogeneous(GTK_BOX(cols), TRUE);

    /* Latency section */
    GtkWidget *lat_box = make_section_box();
    {
        GtkWidget *title = make_label("Cache & DRAM Latency", "section-title");
        gtk_box_append(GTK_BOX(lat_box), title);

        GtkWidget *g = gtk_grid_new();
        gtk_grid_set_row_spacing(GTK_GRID(g), 4);
        gtk_grid_set_column_spacing(GTK_GRID(g), 8);
        int r = 0;
        grid_row(g, r++, "L1 Cache:",  &w->lbl_bench_lat_l1);
        grid_row(g, r++, "L2 Cache:",  &w->lbl_bench_lat_l2);
        grid_row(g, r++, "L3 Cache:",  &w->lbl_bench_lat_l3);
        grid_row(g, r++, "DRAM:",       &w->lbl_bench_lat_dram);
        gtk_box_append(GTK_BOX(lat_box), g);
    }

    /* Bandwidth section */
    GtkWidget *bw_box = make_section_box();
    {
        GtkWidget *title = make_label("DRAM Bandwidth", "section-title");
        gtk_box_append(GTK_BOX(bw_box), title);

        GtkWidget *g = gtk_grid_new();
        gtk_grid_set_row_spacing(GTK_GRID(g), 4);
        gtk_grid_set_column_spacing(GTK_GRID(g), 8);
        int r = 0;
        grid_row(g, r++, "Read:",  &w->lbl_bench_bw_read);
        grid_row(g, r++, "Write:", &w->lbl_bench_bw_write);
        grid_row(g, r++, "Copy:",  &w->lbl_bench_bw_copy);
        gtk_box_append(GTK_BOX(bw_box), g);
    }

    gtk_box_append(GTK_BOX(cols), lat_box);
    gtk_box_append(GTK_BOX(cols), bw_box);
    gtk_box_append(GTK_BOX(vbox), cols);

    /* ── Pi benchmark section ─────────────────────────────────────────── */
    GtkWidget *pi_box = make_section_box();
    gtk_widget_set_hexpand(pi_box, TRUE);

    GtkWidget *pi_title = make_label("Pi Computation", "section-title");
    gtk_box_append(GTK_BOX(pi_box), pi_title);

    /* Controls: [digit dropdown] [Run Pi button] [status] */
    GtkWidget *pi_ctrl = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_margin_top(pi_ctrl, 4);

    static const char *digit_opts[] = { "1 M digits", "10 M digits", "100 M digits", "200 M digits", NULL };
    w->combo_pi_digits = gtk_drop_down_new_from_strings(digit_opts);
    gtk_drop_down_set_selected(GTK_DROP_DOWN(w->combo_pi_digits), 0); /* default: 1 M */

    w->btn_pi_run = gtk_button_new_with_label("Run Pi");
    g_signal_connect(w->btn_pi_run, "clicked", G_CALLBACK(on_pi_run), w);

    w->lbl_pi_status = make_label("Ready", "header-muted");
    gtk_widget_set_valign(w->lbl_pi_status, GTK_ALIGN_CENTER);

    gtk_box_append(GTK_BOX(pi_ctrl), w->combo_pi_digits);
    gtk_box_append(GTK_BOX(pi_ctrl), w->btn_pi_run);
    gtk_box_append(GTK_BOX(pi_ctrl), w->lbl_pi_status);
    gtk_box_append(GTK_BOX(pi_box), pi_ctrl);

    /* Results grid */
    GtkWidget *pi_grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(pi_grid), 4);
    gtk_grid_set_column_spacing(GTK_GRID(pi_grid), 8);
    gtk_widget_set_margin_top(pi_grid, 4);
    {
        int pr = 0;
        grid_row(pi_grid, pr++, "Time:", &w->lbl_pi_time);
    }
    gtk_box_append(GTK_BOX(pi_box), pi_grid);

    gtk_box_append(GTK_BOX(vbox), pi_box);

    return vbox;
}

/* Module dropdown selection changed */
static void on_module_changed(GtkDropDown *dropdown, GParamSpec *pspec, gpointer user_data)
{
    (void)pspec;
    app_widgets_t *w = (app_widgets_t *)user_data;
    w->selected_module = (int)gtk_drop_down_get_selected(dropdown);
    spd_update_for_module(w);   /* SPD page follows the selected DIMM */
}

/* ── App activate ───────────────────────────────────────────────────── */

static app_widgets_t s_widgets;

static void on_activate(GtkApplication *app, gpointer user_data)
{
    (void)user_data;
    app_widgets_t *w = &s_widgets;
    memset(w, 0, sizeof(*w));
    w->selected_module = 0;

    /* GTK resets LC_NUMERIC; force C locale for dot decimal separators */
    setlocale(LC_NUMERIC, "C");

    load_css();

    /* Undo the font-DPI doubling that occurs when elevated on Wayland */
    fix_elevated_wayland_dpi();

    /* Window */
    w->window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(w->window), "TuxTimings");
    gtk_window_set_default_size(GTK_WINDOW(w->window), 760, 820);
    gtk_window_set_resizable(GTK_WINDOW(w->window), TRUE);
    gtk_widget_set_size_request(GTK_WIDGET(w->window), 640, 640);

    /* Window icon: use icon theme name (works when installed to hicolor) */
    gtk_window_set_icon_name(GTK_WINDOW(w->window), "tuxtimings");

    /* Main layout */
    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_margin_start(main_box, 8);
    gtk_widget_set_margin_end(main_box, 8);
    gtk_widget_set_margin_top(main_box, 4);
    gtk_widget_set_margin_bottom(main_box, 6);
    gtk_window_set_child(GTK_WINDOW(w->window), main_box);

    /* Header */
    GtkWidget *header = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    GtkWidget *header_top = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    w->lbl_cpu_name = make_label("TuxTimings", "header-title");
    gtk_widget_set_hexpand(w->lbl_cpu_name, TRUE);
    gtk_box_append(GTK_BOX(header_top), w->lbl_cpu_name);

    /* Module dropdown */
    const char *placeholder[] = {"(detecting...)", NULL};
    GtkStringList *module_model = gtk_string_list_new(placeholder);
    w->combo_modules = gtk_drop_down_new(G_LIST_MODEL(module_model), NULL);
    gtk_widget_set_size_request(w->combo_modules, 200, -1);
    g_signal_connect(w->combo_modules, "notify::selected", G_CALLBACK(on_module_changed), w);
    gtk_box_append(GTK_BOX(header_top), w->combo_modules);

    GtkWidget *btn_debug = gtk_button_new_with_label("Debug");
    g_signal_connect(btn_debug, "clicked", G_CALLBACK(on_debug_dump), w->window);
    gtk_box_append(GTK_BOX(header_top), btn_debug);

    gtk_box_append(GTK_BOX(header), header_top);

    w->lbl_codename = make_label("", "header-muted");
    gtk_box_append(GTK_BOX(header), w->lbl_codename);

    /* Board info row: board string left, version right */
    GtkWidget *board_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    w->lbl_board_info = make_label("", "footer-muted");
    gtk_widget_set_hexpand(w->lbl_board_info, TRUE);
    GtkWidget *lbl_version = make_label(APP_VERSION, "footer-muted");
    gtk_label_set_xalign(GTK_LABEL(lbl_version), 1.0f);
    gtk_box_append(GTK_BOX(board_row), w->lbl_board_info);
    gtk_box_append(GTK_BOX(board_row), lbl_version);
    gtk_box_append(GTK_BOX(header), board_row);
    gtk_box_append(GTK_BOX(main_box), header);

    /* Notebook (tabs) */
    GtkWidget *notebook = gtk_notebook_new();
    gtk_widget_set_vexpand(notebook, TRUE);

    GtkWidget *ram_tab = build_ram_tab(w);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), ram_tab, gtk_label_new("RAM"));

    /* CPU/Bench content can exceed the window on short displays; let them
     * scroll so the window's min height stays below the content min (GTK
     * warns otherwise: "Trying to measure ... needs at least ..."). */
    GtkWidget *cpu_scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(cpu_scroll),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(cpu_scroll), build_cpu_tab(w));
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), cpu_scroll, gtk_label_new("CPU"));

    GtkWidget *bench_scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(bench_scroll),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(bench_scroll), build_bench_tab(w));
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), bench_scroll, gtk_label_new("Benchmark"));

    /* SPD lives in the RAM tab's second row (Timings | Config/Electrical | SPD) */

    gtk_box_append(GTK_BOX(main_box), notebook);

    /* Initial data load */
    refresh_ui(w);

    /* 1-second refresh timer */
    g_timeout_add(1000, on_refresh, w);



    gtk_window_present(GTK_WINDOW(w->window));
}

/* ── Public API ─────────────────────────────────────────────────────── */

GtkApplication *ui_create(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    GtkApplication *app = gtk_application_new("com.tuxtimings.app", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(on_activate), NULL);
    return app;
}
