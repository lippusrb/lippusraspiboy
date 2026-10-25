#include "lippus.h"

static void write_u(char *dst, int *used, unsigned value) {
    char tmp[8];
    int n = 0;
    do {
        tmp[n++] = (char)('0' + (value % 10u));
        value /= 10u;
    } while (value && n < 8);
    while (n > 0) {
        dst[(*used)++] = tmp[--n];
    }
}

static void beep(uint32_t *until, unsigned hz) {
    snd_tone(hz);
    *until = sys_ticks() + 45u;
}

void lippus_main(void) {
    int paddle = 136;
    int ball_x = 160;
    int ball_y = 72;
    int vx = 2;
    int vy = 2;
    int score = 0;
    int lives = 3;
    int hold = 40;
    int over = 0;
    uint32_t beep_until = 0;

    for (;;) {
        lippus_pad_t pad = pad_poll();
        if (!over && pad_held(&pad, LIPPUS_LEFT)) {
            paddle -= 5;
        }
        if (!over && pad_held(&pad, LIPPUS_RIGHT)) {
            paddle += 5;
        }
        if (paddle < 6) {
            paddle = 6;
        }
        if (paddle > LIPPUS_SCREEN_W - 54) {
            paddle = LIPPUS_SCREEN_W - 54;
        }

        if (over) {
            if (pad_pressed(&pad, LIPPUS_A) || pad_pressed(&pad, LIPPUS_CLICK)) {
                score = 0;
                lives = 3;
                ball_x = 160;
                ball_y = 72;
                vx = 2;
                vy = 2;
                hold = 40;
                over = 0;
            }
        } else if (hold > 0) {
            hold--;
            ball_x = paddle + 24;
            ball_y = 200;
        } else {
            ball_x += vx;
            ball_y += vy;
            if (ball_x < 8 || ball_x > LIPPUS_SCREEN_W - 8) {
                vx = -vx;
                ball_x += vx;
                beep(&beep_until, 520);
            }
            if (ball_y < 22) {
                vy = -vy;
                ball_y = 22;
                beep(&beep_until, 640);
            }
            if (vy > 0 && ball_y >= 208 && ball_y <= 222 && ball_x >= paddle &&
                ball_x <= paddle + 48) {
                int speed = vy < 0 ? -vy : vy;
                if (speed < 6) {
                    speed++;
                }
                vy = -speed;
                int hit = ball_x - (paddle + 24);
                vx = hit / 7;
                if (vx == 0) {
                    vx = hit < 0 ? -1 : 1;
                }
                if (vx > 5) {
                    vx = 5;
                }
                if (vx < -5) {
                    vx = -5;
                }
                score++;
                beep(&beep_until, 880);
            } else if (ball_y > LIPPUS_SCREEN_H - 6) {
                lives--;
                beep(&beep_until, 130);
                ball_x = paddle + 24;
                ball_y = 200;
                vx = 2;
                vy = 2;
                hold = 45;
                if (lives <= 0) {
                    over = 1;
                }
            }
        }

        if ((int32_t)(sys_ticks() - beep_until) >= 0) {
            snd_stop();
        }

        gfx_clear(LIPPUS_RGB(10, 14, 28));
        gfx_fill_rect(0, 0, LIPPUS_SCREEN_W, 18, LIPPUS_RGB(18, 16, 36));
        gfx_text(8, 5, "PALLA", LIPPUS_AMBER, 1);

        char hud[32];
        int used = 0;
        hud[used++] = 'p';
        hud[used++] = 't';
        hud[used++] = ' ';
        write_u(hud, &used, (unsigned)score);
        hud[used++] = ' ';
        hud[used++] = 'x';
        write_u(hud, &used, (unsigned)(lives < 0 ? 0 : lives));
        hud[used] = 0;
        gfx_text(LIPPUS_SCREEN_W - gfx_text_width(hud, 1) - 8, 5, hud, LIPPUS_CREAM, 1);

        gfx_fill_rect(paddle, 214, 48, 6, LIPPUS_AMBER);
        gfx_fill_circle(ball_x, ball_y, 4, LIPPUS_WHITE);
        if (over) {
            gfx_fill_rect(70, 96, 180, 48, LIPPUS_RGB(20, 12, 32));
            gfx_rect(70, 96, 180, 48, LIPPUS_AMBER);
            gfx_text(104, 104, "GAME OVER", LIPPUS_AMBER, 1);
            gfx_text(112, 120, "A riprova", LIPPUS_CREAM, 1);
        }
        gfx_present();
        sys_frame(33);
    }
}
