#include "screen-fluke8050.h"

#include "esp32-cpu1.h"
#include "esp_timer.h"
#include "inttypes.h"

#ifndef BIT
#define BIT(X) (1 << X)
#endif

// U10
typedef enum indicators {
    IND_REL = BIT(0),
    IND_BAT = BIT(1),
    IND_HV = BIT(2),
    IND_DB = BIT(3)
} indicators_t;

// U11
typedef enum sign {
    SIGN_PLUS = BIT(0),
    SIGN_MINUS = BIT(1),
    SIGN_ONE = BIT(2),
    SIGN_BP = BIT(3)  // Blanking Period for LCD, Always Set.
} sign_t;

// U12-U15
typedef enum digit {
    CD4056_0 = 0,
    CD4056_1 = 1,
    CD4056_2 = 2,
    CD4056_3 = 3,
    CD4056_4 = 4,
    CD4056_5 = 5,
    CD4056_6 = 6,
    CD4056_7 = 7,
    CD4056_8 = 8,
    CD4056_9 = 9,
    CD4056_L = 10,
    CD4056_H = 11,
    CD4056_P = 12,
    CD4056_A = 13,
    CD4056_DASH = 14,
    CD4056_OFF = 15
} digit_t;

// U16
typedef enum decimals {
    D0 = BIT(0),
    D1 = BIT(1),
    D2 = BIT(2),
    D3 = BIT(3)
} decimals_t;

typedef struct fluke8050_data {
    indicators_t indicator_mask;
    sign_t sign_mask;
    decimals_t decimal_mask;

    digit_t digits[4];
    uint16_t call_cnt;
    int64_t last_call_s;

    lv_obj_t *window;

    lv_obj_t *title;

    lv_obj_t *ind_rel;
    lv_obj_t *ind_bat;
    lv_obj_t *ind_hv;
    lv_obj_t *ind_db;

    lv_obj_t *sign;  // Plus or minus
    lv_obj_t *one;   // 1 or 1. or empty

    lv_obj_t *figs[4];
} fluke8050_data_t;

static uint32_t last = 0;
void draw_fluke8050_title(fluke8050_data_t *data) {
    uint32_t now = cpu1_counter;
    lv_point_t p;
    char uptime[26] = "Fluke 8050a 9223372036854 ";
    snprintf(uptime, strlen(uptime), "Fluke 8050a %" PRId32, now - last);
    last = now;

    _lv_txt_get_size(&p, uptime, &lv_font_montserrat_12, 0, 0, LV_COORD_MAX,
                     LV_TXT_FLAG_EXPAND);

    lv_coord_t swidth = lv_obj_get_width(data->window);
    lv_coord_t w = lv_obj_get_width(data->title);
    lv_obj_set_pos(data->title, swidth / 2 - w / 2, 0);
    lv_label_set_text(data->title, uptime);
}

#define get_nibble(A, B) (A & (0x0f << B)) >> B

void draw_text_with_flag(lv_obj_t *o, uint8_t flags, uint8_t flag, char *text) {
    if (flags & flag) {
        lv_label_set_text(o, text);
    } else {
        char buff[] = "#001000 text#";
        snprintf(buff, strlen(buff), "#001000 %s#", text);
        lv_label_set_text(o, buff);
    }
}

void draw_digit(lv_obj_t *o, uint8_t digit, bool dp) {
    char txt[3] = "  ";
    if (dp) {
        txt[0] = '.';
    }
    switch (digit) {
        case CD4056_0:
            txt[1] = '0';
            break;
        case CD4056_1:
            txt[1] = '1';
            break;
        case CD4056_2:
            txt[1] = '2';
            break;
        case CD4056_3:
            txt[1] = '3';
            break;
        case CD4056_4:
            txt[1] = '4';
            break;
        case CD4056_5:
            txt[1] = '5';
            break;
        case CD4056_6:
            txt[1] = '6';
            break;
        case CD4056_7:
            txt[1] = '7';
            break;
        case CD4056_8:
            txt[1] = '8';
            break;
        case CD4056_9:
            txt[1] = '9';
            break;
        case CD4056_L:
            txt[1] = 'L';
            break;
        case CD4056_H:
            txt[1] = 'H';
            break;
        case CD4056_P:
            txt[1] = 'P';
            break;
        case CD4056_A:
            txt[1] = 'A';
            break;
        case CD4056_DASH:
            txt[1] = '-';
            break;
        case CD4056_OFF:
            txt[1] = ' ';
            break;
    }
    lv_label_set_text(o, txt);
}

void fluke8050_screen_worker(lv_obj_t *screen, void *priv) {
    fluke8050_data_t *pdata = priv;
    int64_t new_call_ms = esp_timer_get_time() / 1000LL;
    int64_t new_call_s = new_call_ms / 1000;
    if (new_call_s != pdata->last_call_s) {
        draw_fluke8050_title(pdata);

        // update output state

        pdata->indicator_mask = get_nibble(new_call_s, 0);
        draw_text_with_flag(pdata->ind_bat, pdata->indicator_mask, IND_BAT,
                            "BT");
        draw_text_with_flag(pdata->ind_db, pdata->indicator_mask, IND_DB, "DB");
        draw_text_with_flag(pdata->ind_hv, pdata->indicator_mask, IND_HV, "HV");
        draw_text_with_flag(pdata->ind_rel, pdata->indicator_mask, IND_REL,
                            "REL");

        pdata->sign_mask = get_nibble(new_call_s, 0);
        pdata->sign_mask |= SIGN_BP;
        if (pdata->sign_mask & SIGN_PLUS) {
            lv_label_set_text(pdata->sign, "+");
        } else if (pdata->sign_mask & SIGN_MINUS) {
            lv_label_set_text(pdata->sign, "-");
        } else {
            lv_label_set_text(pdata->sign, " ");
        }

        if (pdata->sign_mask & SIGN_ONE) {
            lv_label_set_text(pdata->one, "1");
        } else {
            lv_label_set_text(pdata->one, "  ");
        }

        pdata->decimal_mask = get_nibble(new_call_s, 0);

        pdata->digits[0] = get_nibble(new_call_s, 0);
        draw_digit(pdata->figs[0], pdata->digits[0], pdata->decimal_mask & D0);

        pdata->digits[1] = get_nibble(new_call_s, 0);
        draw_digit(pdata->figs[1], pdata->digits[1], pdata->decimal_mask & D1);

        pdata->digits[2] = get_nibble(new_call_s, 0);
        draw_digit(pdata->figs[2], pdata->digits[2], pdata->decimal_mask & D2);

        pdata->digits[3] = get_nibble(new_call_s, 0);
        draw_digit(pdata->figs[3], pdata->digits[3], pdata->decimal_mask & D3);

        pdata->last_call_s = new_call_s;
    }
}

#define CREATE_INIT(A, B, X, Y) \
    X = lv_label_create(A, B);  \
    lv_label_set_text(X, Y);    \
    lv_label_set_recolor(X, true);

void *fluke8050_screen_init(lv_obj_t *screen) {
    fluke8050_data_t *priv = calloc(1, sizeof(fluke8050_data_t));
    priv->window = screen;

    lv_coord_t swidth = lv_obj_get_width(priv->window);
    lv_coord_t w;

    static lv_style_t title_style;
    lv_style_init(&title_style);
    lv_style_set_text_font(&title_style, LV_STATE_DEFAULT,
                           &lv_font_montserrat_12);

    CREATE_INIT(priv->window, NULL, priv->title, "Fluke 8050");
    lv_obj_add_style(priv->title, LV_LABEL_PART_MAIN, &title_style);
    draw_fluke8050_title(priv);

    static lv_style_t ind_style;
    lv_style_init(&ind_style);
    lv_style_set_text_font(&ind_style, LV_STATE_DEFAULT,
                           &lv_font_montserrat_16);

    CREATE_INIT(priv->window, NULL, priv->ind_db, "dB");
    lv_obj_add_style(priv->ind_db, LV_LABEL_PART_MAIN, &ind_style);
    lv_obj_set_size(priv->ind_db, 12, 18);
    w = lv_obj_get_width(priv->ind_db);
    lv_obj_set_pos(priv->ind_db, swidth - w, 0);

    CREATE_INIT(priv->window, NULL, priv->ind_hv, "HV");
    lv_obj_add_style(priv->ind_hv, LV_LABEL_PART_MAIN, &ind_style);
    lv_obj_set_size(priv->ind_hv, 12, 18);
    w = lv_obj_get_width(priv->ind_hv);
    lv_obj_set_pos(priv->ind_hv, swidth - w, 18);

    CREATE_INIT(priv->window, NULL, priv->ind_rel, "REL");
    lv_obj_add_style(priv->ind_rel, LV_LABEL_PART_MAIN, &ind_style);
    lv_obj_set_size(priv->ind_rel, 12, 18);
    w = lv_obj_get_width(priv->ind_rel);
    lv_obj_set_pos(priv->ind_rel, swidth - w, 36);

    CREATE_INIT(priv->window, NULL, priv->ind_bat, "BT");
    lv_obj_add_style(priv->ind_bat, LV_LABEL_PART_MAIN, &ind_style);
    lv_obj_set_pos(priv->ind_bat, 0, 0);

    static lv_style_t num_style;
    lv_style_init(&num_style);
    lv_style_set_text_font(&num_style, LV_STATE_DEFAULT,
                           &lv_font_montserrat_40);
    uint8_t top = 18;

    CREATE_INIT(priv->window, NULL, priv->sign, "+");
    lv_obj_add_style(priv->sign, LV_LABEL_PART_MAIN, &num_style);
    lv_obj_set_pos(priv->sign, 0, top);

    CREATE_INIT(priv->window, NULL, priv->one, "1");
    lv_obj_add_style(priv->one, LV_LABEL_PART_MAIN, &num_style);
    lv_obj_set_pos(priv->one, 32, top);

    for (int i = 0; i < 4; i++) {
        CREATE_INIT(priv->window, NULL, priv->figs[i], ".9");
        lv_obj_add_style(priv->figs[i], LV_LABEL_PART_MAIN, &num_style);
        lv_obj_set_pos(priv->figs[i], 48 + i * 37, top);
    }

#define GPIO1 (1 << 25)
#define GPIO2 (1 << 26)
    uint32_t w1ts[] = {GPIO1, GPIO2, 0};
    uint32_t w1tc[] = {0, GPIO1, GPIO2};
    init_gpios(2, 3);
    write_set_bank(0, 0, 3, w1ts);
    write_clear_bank(0, 0, 3, w1tc);
    set_active_bank(0);
    return priv;
}
