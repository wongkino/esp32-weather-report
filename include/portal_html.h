#pragma once

#include <Arduino.h>

// SoftAP／STA 設定頁（PROGMEM）
static const char PORTAL_HTML[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html lang="zh-Hant">
<head>
<meta charset="utf-8"/>
<meta name="viewport" content="width=device-width,initial-scale=1"/>
<title>天氣站</title>
<style>
:root{
  --ink:#132536;--muted:#5a7184;--line:#c9d7e3;--panel:#ffffffd9;
  --sky0:#d9eef8;--sky1:#b7d9ef;--sky2:#8ebfdc;--accent:#1769a5;--ok:#1f7a4c;--bad:#b42318;
}
*{box-sizing:border-box}
html,body{margin:0;min-height:100%}
body{
  font-family:"PingFang TC","Hiragino Sans CNS","Noto Sans TC","Microsoft JhengHei",sans-serif;
  color:var(--ink);
  background:
    radial-gradient(1200px 500px at 10% -10%,#fff8 0%,transparent 55%),
    linear-gradient(165deg,var(--sky0) 0%,var(--sky1) 42%,var(--sky2) 100%);
}
.wrap{max-width:440px;margin:0 auto;padding:28px 18px 48px}
.brand{margin:0 0 6px;font-size:clamp(2rem,8vw,2.6rem);font-weight:700;letter-spacing:.04em;line-height:1.1}
.lead{margin:0 0 22px;color:var(--muted);font-size:1rem;line-height:1.5}
.badge{
  display:inline-flex;align-items:center;gap:8px;margin:0 0 18px;padding:8px 12px;
  border-radius:999px;background:#ffffffb8;border:1px solid var(--line);color:var(--muted);font-size:.86rem
}
.badge i{width:8px;height:8px;border-radius:50%;background:#8aa0b2}
.badge.on i{background:var(--ok);box-shadow:0 0 0 3px #1f7a4c22}
.panel{
  background:var(--panel);border:1px solid #ffffffaa;border-radius:18px;
  padding:18px 16px 16px;margin:0 0 14px;backdrop-filter:blur(10px);
  box-shadow:0 10px 30px #13253614
}
.panel h2{margin:0 0 4px;font-size:1.05rem}
.panel .sub{margin:0 0 14px;color:var(--muted);font-size:.88rem;line-height:1.45}
label{display:block;margin:12px 0 6px;color:var(--muted);font-size:.84rem}
select,input{
  width:100%;padding:12px 14px;border-radius:12px;border:1px solid var(--line);
  background:#fff;color:var(--ink);font:inherit;font-size:1rem
}
select:focus,input:focus{outline:2px solid #1769a555;border-color:var(--accent)}
.actions{display:grid;gap:8px;margin-top:14px}
.row{display:grid;grid-template-columns:1fr 1fr;gap:8px}
button{
  width:100%;padding:13px 14px;border:0;border-radius:12px;font:inherit;font-size:1rem;
  font-weight:650;background:var(--accent);color:#fff;cursor:pointer
}
button.secondary{background:#e8f1f7;color:var(--ink)}
button:active{transform:translateY(1px)}
.msg{
  margin-top:12px;min-height:1.2em;padding:10px 12px;border-radius:12px;
  background:#fff6;border:1px solid transparent;color:var(--bad);font-size:.92rem;line-height:1.4
}
.msg.ok{color:var(--ok);border-color:#1f7a4c33;background:#eaf7efcc}
.msg.info{color:var(--muted);border-color:var(--line);background:#ffffffa8}
.hidden{display:none!important}
@media (max-width:380px){.row{grid-template-columns:1fr}}
</style>
</head>
<body>
<main class="wrap">
  <h1 class="brand">天氣站</h1>
  <p class="lead" id="modeHint">設定 Wi-Fi 與天氣地區</p>
  <div class="badge" id="statusBadge"><i></i><span id="statusText">準備中</span></div>

  <section class="panel" id="wifiSection">
    <h2>連接 Wi-Fi</h2>
    <p class="sub">選擇網路並輸入密碼，裝置會記住設定。</p>
    <label for="ssid">附近網路</label>
    <select id="ssid"></select>
    <label for="manual">或手動輸入 SSID</label>
    <input id="manual" placeholder="網路名稱（可選）" autocomplete="off"/>
    <label for="pass">密碼</label>
    <input id="pass" type="password" placeholder="Wi-Fi 密碼" autocomplete="current-password"/>
    <div class="actions">
      <button id="refresh" class="secondary" type="button">重新掃描</button>
    </div>
  </section>

  <section class="panel" id="districtSection">
    <h2>天氣地區</h2>
    <p class="sub">顯示該區溫度測站資料。</p>
    <label for="district">選擇地區</label>
    <select id="district"></select>
  </section>

  <div class="actions" id="saveWrap">
    <button id="save" type="button">儲存並連接</button>
  </div>
  <div class="msg info" id="msg"></div>

  <section class="panel hidden" id="touchSection">
    <h2>觸控校準</h2>
    <p class="sub">在裝置螢幕依紅點順序點五下。完成後可用黃點驗證位置。</p>
    <p class="sub" id="touchHint">來源：—</p>
    <div class="actions">
      <button id="touchStart" type="button">開始校準</button>
      <div class="row">
        <button id="touchReset" class="secondary" type="button">還原預設</button>
        <button id="touchDone" class="secondary" type="button">結束校準</button>
      </div>
    </div>
    <div class="msg info" id="touchMsg"></div>
  </section>
</main>

<script>
function sleep(ms){return new Promise(r=>setTimeout(r,ms));}
function setMsg(el,text,kind){el.textContent=text||'';el.className='msg '+(kind||'');}

async function loadDistricts(){
  const r=await fetch('/districts');
  const data=await r.json();
  const sel=document.getElementById('district');
  sel.innerHTML='';
  (data.items||[]).forEach(d=>{
    const o=document.createElement('option');
    o.value=d.i;o.textContent=d.label;
    if(d.i===data.current) o.selected=true;
    sel.appendChild(o);
  });
}

async function loadScan(force){
  const msg=document.getElementById('msg');
  setMsg(msg,'掃描中…','info');
  try{
    if(force) await fetch('/scan?refresh=1');
    let list=null;
    for(let i=0;i<40;i++){
      const r=await fetch('/scan');
      if(!r.ok) throw new Error('HTTP '+r.status);
      const data=await r.json();
      if(data.status==='scanning'){setMsg(msg,'掃描中… ('+(i+1)+')','info');await sleep(400);continue;}
      if(data.status==='error'){setMsg(msg,data.error||'掃描失敗','');return;}
      list=data.networks||[];break;
    }
    if(!list){setMsg(msg,'掃描逾時，請再試','');return;}
    const sel=document.getElementById('ssid');
    sel.innerHTML='';
    list.forEach(s=>{
      if(!s.ssid) return;
      const o=document.createElement('option');
      o.value=s.ssid;
      o.textContent=s.ssid+'  ('+s.rssi+'dBm'+(s.secure?' · 加密':'')+')';
      sel.appendChild(o);
    });
    setMsg(msg,list.length?('找到 '+list.length+' 個網路'):'未找到網路，可手動輸入 SSID','ok');
  }catch(e){setMsg(msg,'掃描失敗：'+e.message,'');}
}

async function init(){
  const cfg=await fetch('/config').then(r=>r.json()).catch(()=>({mode:'ap'}));
  const wifiSection=document.getElementById('wifiSection');
  const saveBtn=document.getElementById('save');
  const hint=document.getElementById('modeHint');
  const touch=document.getElementById('touchSection');
  const badge=document.getElementById('statusBadge');
  const statusText=document.getElementById('statusText');
  await loadDistricts();

  if(cfg.mode==='sta'){
    wifiSection.classList.add('hidden');
    saveBtn.textContent='儲存地區';
    hint.textContent='已連上網路，可更改天氣地區或校準觸控。';
    statusText.textContent='已連線 · '+(cfg.ip||'');
    badge.classList.add('on');
    touch.classList.remove('hidden');
    setMsg(document.getElementById('msg'),'','info');

    async function refreshTouch(){
      const s=await fetch('/touch/status').then(r=>r.json());
      const src=s.source==='nvs'?'NVS（重新校準）':'內建預設';
      let text='來源：'+src;
      if(s.calibrating) text+=' · 進行中 '+(s.step+1)+'/'+s.points+' · '+s.prompt;
      else if(s.prompt) text+=' · '+s.prompt;
      document.getElementById('touchHint').textContent=text;
      return s;
    }
    await refreshTouch();
    let touchPoll=null;
    function stopTouchPoll(){if(touchPoll){clearInterval(touchPoll);touchPoll=null;}}
    const touchMsg=document.getElementById('touchMsg');
    document.getElementById('touchStart').onclick=async()=>{
      setMsg(touchMsg,'已開始，請看裝置螢幕紅點…','info');
      await fetch('/touch/cal/start',{method:'POST'});
      stopTouchPoll();
      touchPoll=setInterval(async()=>{
        const s=await refreshTouch();
        if(!s.calibrating){
          stopTouchPoll();
          setMsg(touchMsg,'校準完成，可在螢幕驗證或按「結束校準」','ok');
        }else{
          setMsg(touchMsg,s.prompt+' ('+(s.step+1)+'/'+s.points+')','info');
        }
      },800);
    };
    document.getElementById('touchReset').onclick=async()=>{
      stopTouchPoll();
      const r=await fetch('/touch/cal/reset',{method:'POST'});
      const j=await r.json();
      setMsg(touchMsg,j.ok?'已還原內建預設':(j.error||'失敗'),j.ok?'ok':'');
      refreshTouch();
    };
    document.getElementById('touchDone').onclick=async()=>{
      stopTouchPoll();
      await fetch('/touch/cal/done',{method:'POST'});
      setMsg(touchMsg,'已結束校準畫面','ok');
      refreshTouch();
    };
  }else{
    hint.textContent='請選擇 Wi-Fi 與天氣地區，完成後裝置會自動連線。';
    statusText.textContent='設定模式 · esp32-weather';
    document.getElementById('refresh').onclick=()=>loadScan(true);
    loadScan(false);
  }

  saveBtn.onclick=async()=>{
    const msg=document.getElementById('msg');
    const district=parseInt(document.getElementById('district').value,10);
    if(cfg.mode==='sta'){
      setMsg(msg,'儲存中…','info');
      try{
        const r=await fetch('/district',{method:'POST',headers:{'Content-Type':'application/json'},
          body:JSON.stringify({district})});
        const j=await r.json();
        setMsg(msg,j.ok?('已儲存：'+(j.label||'')):(j.error||'儲存失敗'),j.ok?'ok':'');
      }catch(e){setMsg(msg,'請求失敗','');}
      return;
    }
    const manual=document.getElementById('manual').value.trim();
    const ssid=manual||document.getElementById('ssid').value;
    const pass=document.getElementById('pass').value;
    if(!ssid){setMsg(msg,'請選擇或輸入 SSID','');return;}
    setMsg(msg,'連接中，請稍候…','info');
    try{
      const r=await fetch('/connect',{method:'POST',headers:{'Content-Type':'application/json'},
        body:JSON.stringify({ssid,pass,district})});
      const j=await r.json();
      setMsg(msg,j.ok?('已連接：'+(j.ip||'')):(j.error||'連接失敗'),j.ok?'ok':'');
      if(j.ok){
        statusText.textContent='已連線 · '+(j.ip||'');
        document.getElementById('statusBadge').classList.add('on');
      }
    }catch(e){setMsg(msg,'請求失敗或裝置已切換網路','');}
  };
}
init();
</script>
</body>
</html>
)HTML";
