#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <Preferences.h>

AsyncWebServer server(80);
Preferences prefs;

// --- ALL 29 ADJUSTABLE PARAMETERS ---
float tc_base, wt, t_rich, t_lean, s_min, s_max, h_freq, h_amp, j_low, j_high, j_div;
float det_l, det_h, lct, hct, pal, pat, avg_w, sig_w, db_m, db_c, dtb, doa_l, doa_r, dr_v, dr_d;
float at, af_l, af_h, as_l, as_h, oc;
unsigned long d_int, d_dur;

// WiFi & State
const int sensorIn = 1, simOut = 17;
unsigned long lastDipTime = 0, wifiTimer = 0;
const unsigned long WIFI_TIMEOUT = 1800000; // 30 mins
float currentOutput = 0.255, initialBias = 0.0, rollingAvgIn = 0.450;
int lowCounter = 0, highCounter = 0;
bool programRunning = false, wifiActive = true;

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html><head>
  <title>O2 MASTER ULTIMATE v3.3.1</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { font-family: monospace; background: #000; color: #0f0; padding: 15px; display: flex; flex-direction: column; align-items: center; }
    .container { width: 100%; max-width: 600px; }
    h2 { border-bottom: 2px solid #040; padding-bottom: 10px; text-align: center; margin-bottom: 20px; font-size: 1rem; }
    .section-box { border: 1px solid #020; padding: 12px; margin-bottom: 20px; background: #050505; border-radius: 5px; }
    .section-title { font-size: 0.75rem; color: #0a0; font-weight: bold; margin-bottom: 12px; text-transform: uppercase; border-left: 3px solid #0f0; padding-left: 8px; }
    .grid { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; }
    .field { display: flex; flex-direction: column; }
    .row { display: flex; gap: 10px; }
    label { font-size: 0.55rem; color: #080; margin-bottom: 3px; text-transform: uppercase; font-weight: bold; }
    input { background: #0a0a0a; border: 1px solid #0f0; color: #0f0; padding: 10px; font-size: 0.9rem; border-radius: 4px; width: 100%; box-sizing: border-box; }
    .btn { background: #040; color: #fff; border: 1px solid #0f0; padding: 18px; width: 100%; cursor: pointer; font-weight: bold; margin-top: 10px; }
    .btn-red { background: #400; border-color: #f00; }
  </style>
</head>
<body>
  <div class="container">
    <h2>O2_SIM_ULTIMATE_v3.3.1</h2>
    <div class="section-box"><div class="section-title">WIFI STATUS</div>
      <div style="font-size:0.7rem; color:#888; text-align:center;">WPA2 Active. Radio turns off after 30m of inactivity.</div>
    </div>
    <div class="section-box"><div class="section-title">1. TIMERS & STARTUP</div>
      <div class="grid">
        <div class="field"><label>Dip Int (ms)</label><input type="number" id="d_int"></div>
        <div class="field"><label>Dip Dur (ms)</label><input type="number" id="d_dur"></div>
        <div class="field"><label>Wake Thresh (V)</label><input type="number" step="0.001" id="wt"></div>
        <div class="field"><label>Cruise Base (V)</label><input type="number" step="0.001" id="tc_base"></div>
    </div></div>
    <div class="section-box"><div class="section-title">2. DETECTION & COUNTERS</div>
      <div class="grid">
        <div class="field"><label>Det Low (V)</label><input type="number" step="0.001" id="det_l"></div>
        <div class="field"><label>Det High (V)</label><input type="number" step="0.001" id="det_h"></div>
        <div class="field"><label>Low Count Limit</label><input type="number" id="lct"></div>
        <div class="field"><label>High Count Limit</label><input type="number" id="hct"></div>
    </div></div>
    <div class="section-box"><div class="section-title">3. PULSE & DIP ALPHA</div>
      <div class="grid">
        <div class="field"><label>Pulse Target Mult</label><input type="number" step="0.01" id="pal"></div>
        <div class="field"><label>Pulse Output Mult</label><input type="number" step="0.01" id="pat"></div>
        <div class="field"><label>Dip Target (V)</label><input type="number" step="0.001" id="dtb"></div>
        <div class="field"><label>Dip Alpha (L/R)</label><div class="row"><input type="number" step="0.01" id="doa_l"><input type="number" step="0.01" id="doa_r"></div></div>
        <div class="field"><label>Dip Rand (V/D)</label><div class="row"><input type="number" id="dr_v"><input type="number" id="dr_d"></div></div>
    </div></div>
    <div class="section-box"><div class="section-title">4. RESPONSE SPEED (ALPHAS)</div>
      <div class="grid">
        <div class="field"><label>Alpha Thresh</label><input type="number" step="0.01" id="at"></div>
        <div class="field"><label>Fast Alpha (L/H)</label><div class="row"><input type="number" step="0.01" id="af_l"><input type="number" step="0.01" id="af_h"></div></div>
        <div class="field"><label>Slow Alpha (L/H)</label><div class="row"><input type="number" step="0.01" id="as_l"><input type="number" step="0.01" id="as_h"></div></div>
        <div class="field"><label>Out Clamp (V)</label><input type="number" step="0.001" id="oc"></div>
    </div></div>
    <div class="section-box"><div class="section-title">5. AVERAGING & HEARTBEAT</div>
      <div class="grid">
        <div class="field"><label>Avg Weight</label><input type="number" step="0.0001" id="avg_w"></div>
        <div class="field"><label>Sig Weight</label><input type="number" step="0.0001" id="sig_w"></div>
        <div class="field"><label>Bias Mult</label><input type="number" step="0.001" id="db_m"></div>
        <div class="field"><label>Bias Center</label><input type="number" step="0.001" id="db_c"></div>
        <div class="field"><label>Sine Denom</label><input type="number" id="h_freq"></div>
        <div class="field"><label>Sine Amp</label><input type="number" step="0.001" id="h_amp"></div>
    </div></div>
    <div class="section-box"><div class="section-title">6. REALISM & JITTER</div>
      <div class="grid">
        <div class="field"><label>Jitter (Low/High)</label><div class="row"><input type="number" id="j_low"><input type="number" id="j_high"></div></div>
        <div class="field"><label>Jitter Divisor</label><input type="number" id="j_div"></div>
    </div></div>
    <button class="btn" onclick="apply()">APPLY TO RAM</button>
    <button class="btn btn-red" onclick="save()">SAVE TO FLASH</button>
  </div>
  <script>
    function load(){ fetch('/get').then(r=>r.json()).then(d=>{ Object.entries(d).forEach(([k,v])=>{ if(document.getElementById(k)) document.getElementById(k).value=v; }); }); }
    function apply(){ let p=new URLSearchParams(); document.querySelectorAll('input').forEach(i=>p.append(i.id,i.value)); fetch('/upd?'+p.toString()).then(()=>alert('Applied')); }
    function save(){ fetch('/save').then(()=>alert('Saved to Flash')); }
    window.onload = load;
  </script>
</body></html>
)rawliteral";
void setup() {
  Serial.begin(115200); prefs.begin("o2sim", false);
  tc_base = prefs.getFloat("tc_base", 0.795); wt = prefs.getFloat("wt", 0.150);
  det_l = prefs.getFloat("det_l", 0.08); det_h = prefs.getFloat("det_h", 0.85);
  lct = prefs.getFloat("lct", 110); hct = prefs.getFloat("hct", 80);
  pal = prefs.getFloat("pal", 0.15); pat = prefs.getFloat("pat", 0.85);
  avg_w = prefs.getFloat("avg_w", 0.998); sig_w = prefs.getFloat("sig_w", 0.002);
  db_m = prefs.getFloat("db_m", 0.015); db_c = prefs.getFloat("db_c", 0.450);
  d_int = prefs.getULong("d_int", 70000); d_dur = prefs.getULong("d_dur", 1000);
  dtb = prefs.getFloat("dtb", 0.400); doa_l = prefs.getFloat("doa_l", 0.70); doa_r = prefs.getFloat("doa_r", 0.30);
  dr_v = prefs.getFloat("dr_v", 50.0); dr_d = prefs.getFloat("dr_d", 1000.0);
  h_freq = prefs.getFloat("h_freq", 1500.0); h_amp = prefs.getFloat("h_amp", 0.020);
  j_low = prefs.getFloat("j_low", 0); j_high = prefs.getFloat("j_high", 5); j_div = prefs.getFloat("j_div", 1000.0);
  at = prefs.getFloat("at", 0.05); af_l = prefs.getFloat("af_l", 0.80); af_h = prefs.getFloat("af_h", 0.20);
  as_l = prefs.getFloat("as_l", 0.95); as_h = prefs.getFloat("as_h", 0.05); oc = prefs.getFloat("oc", 0.900);

  WiFi.softAP("CAT", "nocatalyticconverter");
  wifiTimer = millis();

  server.on("/get", HTTP_GET, [](AsyncWebServerRequest *r){
    String j = "{\"d_int\":"+String(d_int)+",\"d_dur\":"+String(d_dur)+",\"wt\":"+String(wt,3)+",\"tc_base\":"+String(tc_base,3)+",\"det_l\":"+String(det_l,3)+",\"det_h\":"+String(det_h,3);
    j += ",\"lct\":"+String(lct)+",\"hct\":"+String(hct)+",\"pal\":"+String(pal,2)+",\"pat\":"+String(pat,2)+",\"dtb\":"+String(dtb,3);
    j += ",\"doa_l\":"+String(doa_l,2)+",\"doa_r\":"+String(doa_r,2)+",\"dr_v\":"+String(dr_v,1)+",\"dr_d\":"+String(dr_d,1)+",\"at\":"+String(at,2);
    j += ",\"af_l\":"+String(af_l,2)+",\"af_h\":"+String(af_h,2)+",\"as_l\":"+String(as_l,2)+",\"as_h\":"+String(as_h,2);
    j += ",\"avg_w\":"+String(avg_w,4)+",\"sig_w\":"+String(sig_w,4)+",\"db_m\":"+String(db_m,3)+",\"db_c\":"+String(db_c,3);
    j += ",\"h_freq\":"+String(h_freq,1)+",\"h_amp\":"+String(h_amp,3)+",\"j_low\":"+String(j_low,1)+",\"j_high\":"+String(j_high,1)+",\"j_div\":"+String(j_div,1)+",\"oc\":"+String(oc,3)+"}";
    r->send(200, "application/json", j);
  });

  server.on("/upd", HTTP_GET, [](AsyncWebServerRequest *r){
    if(r->hasParam("d_int")) d_int = r->getParam("d_int")->value().toInt();
    if(r->hasParam("wt")) wt = r->getParam("wt")->value().toFloat();
    if(r->hasParam("tc_base")) tc_base = r->getParam("tc_base")->value().toFloat();
    if(r->hasParam("det_l")) det_l = r->getParam("det_l")->value().toFloat();
    if(r->hasParam("det_h")) det_h = r->getParam("det_h")->value().toFloat();
    if(r->hasParam("lct")) lct = r->getParam("lct")->value().toFloat();
    if(r->hasParam("hct")) hct = r->getParam("hct")->value().toFloat();
    if(r->hasParam("pal")) pal = r->getParam("pal")->value().toFloat();
    if(r->hasParam("pat")) pat = r->getParam("pat")->value().toFloat();
    if(r->hasParam("dtb")) dtb = r->getParam("dtb")->value().toFloat();
    if(r->hasParam("doa_l")) doa_l = r->getParam("doa_l")->value().toFloat();
    if(r->hasParam("doa_r")) doa_r = r->getParam("doa_r")->value().toFloat();
    if(r->hasParam("dr_v")) dr_v = r->getParam("dr_v")->value().toFloat();
    if(r->hasParam("dr_d")) dr_d = r->getParam("dr_d")->value().toFloat();
    if(r->hasParam("avg_w")) avg_w = r->getParam("avg_w")->value().toFloat();
    if(r->hasParam("sig_w")) sig_w = r->getParam("sig_w")->value().toFloat();
    if(r->hasParam("db_m")) db_m = r->getParam("db_m")->value().toFloat();
    if(r->hasParam("db_c")) db_c = r->getParam("db_c")->value().toFloat();
    if(r->hasParam("h_freq")) h_freq = r->getParam("h_freq")->value().toFloat();
    if(r->hasParam("h_amp")) h_amp = r->getParam("h_amp")->value().toFloat();
    if(r->hasParam("at")) at = r->getParam("at")->value().toFloat();
    if(r->hasParam("af_l")) af_l = r->getParam("af_l")->value().toFloat();
    if(r->hasParam("af_h")) af_h = r->getParam("af_h")->value().toFloat();
    if(r->hasParam("as_l")) as_l = r->getParam("as_l")->value().toFloat();
    if(r->hasParam("as_h")) as_h = r->getParam("as_h")->value().toFloat();
    if(r->hasParam("j_low")) j_low = r->getParam("j_low")->value().toFloat();
    if(r->hasParam("j_high")) j_high = r->getParam("j_high")->value().toFloat();
    if(r->hasParam("j_div")) j_div = r->getParam("j_div")->value().toFloat();
    if(r->hasParam("oc")) oc = r->getParam("oc")->value().toFloat();
    r->send(200, "text/plain", "OK");
  });

  server.on("/save", HTTP_GET, [](AsyncWebServerRequest *r){
    prefs.putULong("d_int", d_int); prefs.putFloat("wt", wt); prefs.putFloat("tc_base", tc_base);
    prefs.putFloat("det_l", det_l); prefs.putFloat("det_h", det_h); prefs.putFloat("lct", lct); prefs.putFloat("hct", hct);
    prefs.putFloat("pal", pal); prefs.putFloat("pat", pat); prefs.putFloat("dtb", dtb);
    prefs.putFloat("doa_l", doa_l); prefs.putFloat("doa_r", doa_r); prefs.putFloat("dr_v", dr_v); prefs.putFloat("dr_d", dr_d);
    prefs.putFloat("avg_w", avg_w); prefs.putFloat("sig_w", sig_w); prefs.putFloat("db_m", db_m); prefs.putFloat("db_c", db_c);
    prefs.putFloat("h_freq", h_freq); prefs.putFloat("h_amp", h_amp); prefs.putFloat("at", at);
    prefs.putFloat("af_l", af_l); prefs.putFloat("af_h", af_h); prefs.putFloat("as_l", as_l); prefs.putFloat("as_h", as_h);
    prefs.putFloat("j_low", j_low); prefs.putFloat("j_high", j_high); prefs.putFloat("j_div", j_div); prefs.putFloat("oc", oc);
    r->send(200, "text/plain", "SAVED");
  });

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *r){ r->send_P(200, "text/html", index_html); });
  server.begin();
  analogReadResolution(12); initialBias = (analogRead(sensorIn) / 4095.0) * 3.3;
}
void loop() {
  if (wifiActive) {
    if (WiFi.softAPgetStationNum() > 0) wifiTimer = millis();
    else if (millis() - wifiTimer > WIFI_TIMEOUT) {
      WiFi.softAPdisconnect(true); WiFi.mode(WIFI_OFF); wifiActive = false;
    }
  }

  float vIn = (analogRead(sensorIn) / 4095.0) * 3.3;
  if (!programRunning) { if (abs(vIn - initialBias) > wt) programRunning = true; return; }
  
  if (vIn < det_l) { lowCounter++; highCounter = 0; }
  else if (vIn > det_h) { highCounter++; lowCounter = 0; }
  else { if (lowCounter > 0) lowCounter--; if (highCounter > 0) highCounter--; }

  float target; unsigned long curT = millis();
  if (lowCounter > lct) {
    target = t_lean; currentOutput = (currentOutput * pal) + (target * pat);
  } else if (highCounter > hct) {
    target = t_rich; currentOutput = (currentOutput * pal) + (target * pat);
  } else if (curT - lastDipTime < d_dur) {
    target = dtb + (random(0, (int)dr_v) / dr_d);
    currentOutput = (currentOutput * doa_l) + (target * doa_r);
  } else if (curT - lastDipTime > d_int) {
    lastDipTime = curT; target = tc_base;
  } else {
    rollingAvgIn = (rollingAvgIn * avg_w) + (vIn * sig_w);
    float d_bias = (rollingAvgIn - db_c) * db_m;
    target = tc_base + d_bias + (sin(millis() / h_freq) * h_amp);
    if (target > s_max) target = s_max; if (target < s_min) target = s_min;
    float a_l = (abs(currentOutput - target) > at) ? af_l : as_l;
    float a_h = (abs(currentOutput - target) > at) ? af_h : as_h;
    currentOutput = (currentOutput * a_l) + (target * a_h);
  }
  float jitter = (random((int)j_low, (int)j_high) / j_div);
  float finalV = currentOutput + jitter;
  if (finalV > oc) finalV = oc; dacWrite(simOut, (int)((finalV / 3.3) * 255));
  delay(25);
}
