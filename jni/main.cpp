#include <mod/amlmod.h>

#include <float.h>
#include <math.h>
#include <stdio.h>

#include <aml-psdk/game_sa/Events.h>
#include <aml-psdk/game_sa/engine/RsGlobal.h>
#include <aml-psdk/game_sa/engine/Sprite2d.h>
#include <aml-psdk/game_sa/engine/Font.h>
#include <aml-psdk/game_sa/entity/PlayerPed.h>
#include <aml-psdk/game_sa/entity/Vehicle.h>
#include <aml-psdk/game_sa/entity/Automobile.h>

MYMOD(net.rafi.sa.speedometer, SA Speedometer, 1.0, Rafi)

// ---------- helper ----------
static inline float Clamp01(float v)
{
    if(!(v > 0.0f)) return 0.0f; // juga menangani NaN
    if(v > 1.0f) return 1.0f;
    return v;
}

// kotak kecil yang dipakai untuk membentuk busur (cuma pakai DrawRect, sama seperti HUD asli)
static inline void Dot(float x, float y, float half, const CRGBA& c)
{
    CSprite2d::DrawRect(CRect(x - half, y + half, x + half, y - half), c);
}

static CRGBA SpeedColor(float t)
{
    // hijau -> kuning -> merah
    if(t < 0.5f)
    {
        float k = t / 0.5f;
        return CRGBA((unsigned char)(60 + 195 * k), (unsigned char)(220 - 10 * k), (unsigned char)(90 - 50 * k), 255);
    }
    float k = (t - 0.5f) / 0.5f;
    return CRGBA(255 - (unsigned char)(15 * k), (unsigned char)(210 - 160 * k), (unsigned char)(40 + 10 * k), 255);
}

// busur setengah lingkaran (kiri -> kanan lewat atas). t=0 kiri, t=1 kanan
static void DrawArc(float cx, float cy, float r, float half, float step, float fill, bool speedColors, const CRGBA& fillCol, const CRGBA& bgCol)
{
    const float PI = 3.14159265f;
    float arcLen = PI * r;
    int n = (int)(arcLen / step);
    if(n < 8) n = 8;
    if(n > 120) n = 120;
    for(int i = 0; i <= n; ++i)
    {
        float t = (float)i / (float)n;
        float a = PI * (1.0f - t);
        float x = cx + cosf(a) * r;
        float y = cy - sinf(a) * r;
        if(t <= fill) Dot(x, y, half, speedColors ? SpeedColor(t) : fillCol);
        else          Dot(x, y, half, bgCol);
    }
}

// ---------- state ----------
static CVehicle* g_pLastVeh = NULL;
static float g_fShownSpeed = 0.0f;
static float g_fShownGas = 0.0f;
static float g_fMaxScale = 240.0f;

static float GetBaseScale(CVehicle* v)
{
    if(v->IsPlane() || v->IsFakeAircraft()) return 450.0f;
    if(v->IsHeli()) return 250.0f;
    return 240.0f;
}

static float GetGas(CVehicle* v)
{
    float g = v->m_fGasPedal;
    if(v->IsPlane())
    {
        g = static_cast<CPlane*>(v)->m_fThrottleControl;
    }
    else if(v->IsHeli())
    {
        float h = fabsf(static_cast<CHeli*>(v)->m_fAccelerationBreakStatus);
        if(h > g) g = h;
    }
    if(!__builtin_isfinite(g)) g = 0.0f;
    return Clamp01(fabsf(g));
}

static int g_nLogFrames = 0;
static void Step(const char* m)
{
    if(g_nLogFrames > 3) return;
    FILE* f = fopen("/storage/emulated/0/Android/data/com.rockstargames.gtasa/files/speedometer_log.txt", "a");
    if(!f) return;
    fprintf(f, "%s\n", m);
    fflush(f);
    fclose(f);
}

static void DrawSpeedometer()
{
    CPlayerPed* player = FindPlayerPed(-1);
    if(!player) return;
    CVehicle* veh = NULL;
    if(player->bInVehicle) veh = player->m_pVehicle;
    if(!veh)
    {
        g_pLastVeh = NULL;
        return;
    }

    ++g_nLogFrames;
    Step("1 kendaraan ketemu");
    // ---- ambil data (aman dari NaN) ----
    float speed = veh->m_vecMoveSpeed.Magnitude() * 180.0f; // 1 unit game ~ 180 km/jam
    if(!__builtin_isfinite(speed) || speed < 0.0f) speed = 0.0f;
    float gas = GetGas(veh);

    if(veh != g_pLastVeh)
    {
        g_pLastVeh = veh;
        g_fShownSpeed = speed;
        g_fShownGas = gas;
        g_fMaxScale = GetBaseScale(veh);
    }
    g_fShownSpeed += (speed - g_fShownSpeed) * 0.25f;
    g_fShownGas   += (gas - g_fShownGas) * 0.35f;

    // skala maksimum naik otomatis kalau kendaraan lebih ngebut dari skala awal
    while(g_fShownSpeed > g_fMaxScale && g_fMaxScale < 2000.0f) g_fMaxScale += 60.0f;

    float frac = Clamp01(g_fShownSpeed / g_fMaxScale);
    float gasFrac = Clamp01(g_fShownGas);

    // ---- geometri ----
    float W = (float)RsGlobal.maximumWidth;
    float H = (float)RsGlobal.maximumHeight;
    float s = H / 448.0f;                 // skala seragam supaya lingkaran tetap bulat
    float cx = W * 0.5f;
    float cy = H - 12.0f * s;             // pusat setengah lingkaran, mepet bawah tengah
    float R = 64.0f * s;

    Step("2 data siap");
    // latar gelap setengah lingkaran
    {
        float rr = R + 6.0f * s;
        for(float y = 0.0f; y <= rr; y += 3.0f * s)
        {
            float w = sqrtf(fmaxf(rr * rr - y * y, 0.0f));
            CSprite2d::DrawRect(CRect(cx - w, cy - y + 1.5f * s, cx + w, cy - y - 1.5f * s), CRGBA(0, 0, 0, 120));
        }
    }

    Step("3 latar selesai");
    // busur kecepatan (luar)
    DrawArc(cx, cy, R, 3.6f * s, 3.0f * s, frac, true, CRGBA(255, 255, 255, 255), CRGBA(45, 45, 45, 220));

    // busur gas (dalam)
    DrawArc(cx, cy, R * 0.72f, 2.6f * s, 2.6f * s, gasFrac, false, CRGBA(60, 190, 255, 255), CRGBA(45, 45, 45, 220));

    Step("4 busur selesai");
    // garis penunjuk
    {
        const float PI = 3.14159265f;
        float a = PI * (1.0f - frac);
        for(int i = 3; i <= 12; ++i)
        {
            float rr = R * 0.62f * ((float)i / 12.0f) + R * 0.0f;
            Dot(cx + cosf(a) * rr, cy - sinf(a) * rr, 1.6f * s, CRGBA(255, 255, 255, 255));
        }
    }

    Step("5 jarum selesai");
    // ---- teks ----
    char buf[24];
    CFont::SetOrientation(ALIGN_CENTER);
    CFont::SetBackground(false, false);
    CFont::SetWrapx(W);
    CFont::SetCentreSize(W);
    CFont::SetFontStyle(FONT_SUBTITLES);
    CFont::SetProportional(true);
    CFont::SetDropShadowPosition(1);
    CFont::SetDropColor(CRGBA(0, 0, 0, 255));

    snprintf(buf, sizeof(buf), "%d", (int)(g_fShownSpeed + 0.5f));
    CFont::SetColor(CRGBA(255, 255, 255, 255));
    CFont::SetScale((0.55f * W) / 640.0f, (1.1f * H) / 448.0f);
    CFont::PrintString(cx, cy - 46.0f * s, buf);

    CFont::SetColor(CRGBA(200, 200, 200, 255));
    CFont::SetScale((0.26f * W) / 640.0f, (0.52f * H) / 448.0f);
    CFont::PrintString(cx, cy - 22.0f * s, "KM/H");

    snprintf(buf, sizeof(buf), "GAS %d%%", (int)(gasFrac * 100.0f + 0.5f));
    CFont::SetColor(CRGBA(60, 190, 255, 255));
    CFont::SetScale((0.26f * W) / 640.0f, (0.52f * H) / 448.0f);
    CFont::PrintString(cx, cy - 11.0f * s, buf);

    Step("6 teks dikirim");
    CFont::RenderFontBuffer();
    Step("7 selesai");
}

ON_MOD_LOAD()
{
    Events::drawHudEvent += []()
    {
        DrawSpeedometer();
    };
}
