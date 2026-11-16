// ============================================================
//  Deauth-only web UI (single page, embedded as a string).
//  Talks to the API implemented in deauthonly.ino.
// ============================================================
#ifndef DEAUTH_UI_H
#define DEAUTH_UI_H

static const char PAGE_APP[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="zh-CN"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>BW16 Deauth</title>
<style>
:root{--bg:#0d1117;--card:#161b22;--line:#232a35;--txt:#e6edf3;--mut:#8b949e;--acc:#2f81f7;--ok:#3fb950;--err:#f85149;--warn:#d29922}
*{box-sizing:border-box;-webkit-tap-highlight-color:transparent}
body{margin:0;background:var(--bg);color:var(--txt);font:14px/1.5 system-ui,-apple-system,"Segoe UI",Roboto,sans-serif}
.wrap{max-width:760px;margin:0 auto;padding:14px 12px 40px}
h1{font-size:20px;margin:6px 0 10px;font-weight:700;display:flex;align-items:baseline;gap:8px;flex-wrap:wrap}
h1 b{color:var(--acc)}
.madeby{margin-left:auto;font-size:11.5px;font-weight:400;color:var(--mut);letter-spacing:.5px;white-space:nowrap}
.row{display:flex;gap:8px;align-items:center;flex-wrap:wrap}
.pill{background:var(--card);border:1px solid var(--line);border-radius:999px;padding:3px 10px;font-size:12px;color:var(--mut);display:inline-flex;align-items:center;gap:6px}
.dot{width:8px;height:8px;border-radius:50%;background:var(--mut)}
.dot.attack{background:var(--err)}.dot.idle{background:var(--ok)}.dot.scanning{background:var(--warn)}
.card{background:var(--card);border:1px solid var(--line);border-radius:12px;padding:12px;margin:12px 0}
.card h3{margin:0 0 10px;font-size:15px;display:flex;justify-content:space-between;align-items:center;gap:8px}
table{width:100%;border-collapse:collapse;font-size:12.5px}
th,td{padding:7px 5px;text-align:left;border-bottom:1px solid var(--line);white-space:nowrap}
#netTbl{table-layout:fixed}
#netTbl th:nth-child(2),#netTbl td:nth-child(2){width:36px}
#netTbl th:nth-child(3),#netTbl td:nth-child(3){width:80px}
#netTbl th:nth-child(4),#netTbl td:nth-child(4){width:104px}
#netTbl th:nth-child(5),#netTbl td:nth-child(5){width:26px;text-align:center}
#netTbl td:nth-child(1){white-space:normal;word-break:break-all}
#netTbl td:nth-child(4){font-size:10.5px;overflow:hidden;text-overflow:ellipsis}
.sig{display:inline-flex;align-items:flex-end;gap:1.5px;height:9px;vertical-align:middle;margin-right:4px}
.sig i{width:3px;background:#30363d;border-radius:1px}
.sig i:nth-child(1){height:4px}.sig i:nth-child(2){height:6px}.sig i:nth-child(3){height:8px}.sig i:nth-child(4){height:9px}
.sig i.on{background:#3fb950}
.sig i.on.w2{background:#d29922}
th{color:var(--mut);font-weight:500;font-size:11.5px}
tbody tr{cursor:pointer}
tbody tr.sel{background:rgba(47,129,247,.12)}
.badge{display:inline-block;padding:1px 7px;border-radius:6px;font-size:11px}
.b24{background:rgba(63,185,80,.16);color:#56d364}
.b5{background:rgba(210,153,34,.18);color:#e3b341}
.mono{font-family:ui-monospace,SFMono-Regular,Menlo,monospace}
.mut{color:var(--mut)}
.empty{color:var(--mut);text-align:center;padding:16px 0}
.btn{background:var(--acc);color:#fff;border:0;border-radius:9px;padding:9px 14px;font-size:13.5px;font-weight:600}
.btn.red{background:var(--err)}.btn.gray{background:#30363d}.btn.sm{padding:6px 10px;font-size:12.5px}
.btn:disabled{opacity:.55}
select,input{background:#0d1117;color:var(--txt);border:1px solid var(--line);border-radius:9px;padding:8px 10px;font-size:13px}
input{width:190px}
.f{display:flex;gap:10px;align-items:center;margin:8px 0}
.lbl{width:132px;color:var(--mut);font-size:12.5px;flex:none}
.stat{display:flex;gap:8px;margin:10px 0}
.stat>div{flex:1;background:#0d1117;border:1px solid var(--line);border-radius:10px;padding:8px;text-align:center}
.stat b{display:block;font-size:19px}
.stat span{font-size:11.5px;color:var(--mut)}
.hint{color:var(--mut);font-size:11.5px;margin-top:8px;line-height:1.6}
#evlog{max-height:190px;overflow:auto;font-size:12px}
#evlog div{padding:2px 0;color:var(--mut)}
#evlog .ok{color:var(--ok)}#evlog .err{color:var(--err)}#evlog .warn{color:var(--warn)}
#toasts{position:fixed;left:50%;bottom:22px;transform:translateX(-50%);display:flex;flex-direction:column;gap:8px;z-index:9}
.toast{background:#1f2630;border:1px solid var(--line);border-left:3px solid var(--acc);border-radius:9px;padding:9px 14px;font-size:13px;max-width:86vw}
.toast.err{border-left-color:var(--err)}.toast.ok{border-left-color:var(--ok)}
.hide{display:none}
.brand{text-align:center;font-size:11px;letter-spacing:1.5px;color:var(--acc);margin:0 0 4px}
.ledlegend{display:flex;gap:6px;flex-wrap:wrap;align-items:center;margin:0 0 8px;font-size:11.5px;color:var(--mut)}
.ledlegend b{font-weight:500;color:var(--txt)}
</style></head><body><div class="wrap">
<h1>BW16 <b>Deauth</b><span class="madeby">壹纸寂寞的大胖鱼制作</span></h1>
<div class="row" style="margin-bottom:8px">
 <span class="pill"><i class="dot idle" id="modeDot"></i><span id="modeTxt">IDLE</span></span>
 <span class="pill">AP <span class="mono" id="apSsid">-</span> · <span id="apIp">-</span></span>
 <span class="pill">客户端 <b id="apCl">0</b></span>
 <span class="pill">Heap <b id="heap">-</b></span>
</div>
<div class="ledlegend">板载 RGB 灯：
 <span class="pill"><i class="dot" style="background:#3fb950"></i>空闲</span>
 <span class="pill"><i class="dot" style="background:#f85149"></i>攻击 2.4G</span>
 <span class="pill"><i class="dot" style="background:#2f81f7"></i>攻击 5G</span>
</div>

<div class="card">
 <h3>网络扫描 <span class="row"><select id="fltBand"><option value="-1">全部频段</option><option value="0">2.4GHz</option><option value="1">5GHz</option></select><button class="btn sm" id="btnScan">重新扫描</button></span></h3>
 <table id="netTbl"><thead><tr><th>SSID</th><th>信道</th><th>信号</th><th>BSSID</th><th></th></tr></thead><tbody></tbody></table>
 <div class="hint">点击行可多选目标网络。扫描同时覆盖 2.4GHz 与 5GHz。<b>扫描期间热点会断开约 5 秒</b>（单射频无法边扫描边广播），扫描结束后热点自动恢复，手机重新连回即可。</div>
</div>

<div class="card">
 <h3>攻击参数</h3>
 <div class="f"><span class="lbl">帧类型</span><select id="atkMethod"><option value="deauth">Deauthentication</option><option value="disassoc">Disassociation</option><option value="both">Deauth + Disassoc</option></select></div>
 <div class="f"><span class="lbl">发送间隔 (ms)</span><select id="atkInt"><option value="100" selected>100</option><option value="200">200</option><option value="500">500</option><option value="1000">1000</option></select></div>
 <div class="f"><span class="lbl">目标设备 MAC</span><input id="atkMac" placeholder="aa:bb:cc:dd:ee:ff" maxlength="17" autocomplete="off"></div>
 <div class="hint" id="apCliHint">留空 = 自动嗅探目标网络的客户端（支持随机 MAC）；也可填“要做掉的设备”的 MAC（不是路由器 MAC）。本热点在线设备（点击填入）：-</div>
 <div class="stat">
  <div><b id="stFrames">0</b><span>已发帧数</span></div>
  <div><b id="stCli">0</b><span>捕获客户端</span></div>
  <div><b id="stTgt">0</b><span>目标网络</span></div>
 </div>
 <div class="row" style="margin:10px 0">
  <button class="btn red" id="btnAtkStart">▶ 启动 Deauth</button>
  <button class="btn gray hide" id="btnAtkStop">■ 停止</button>
 </div>
 <div class="hint">攻击开始会把热点锁到目标信道（手机会短暂掉线并连回）。停止后自动恢复原信道。板载 RGB 灯：空闲绿色 / 攻击 2.4G 红色 / 攻击 5G 蓝色。</div>
</div>

<div class="card">
 <h3>热点密码</h3>
 <div class="f"><span class="lbl">新密码 (8-63)</span><input id="apPass" placeholder="至少 8 位" maxlength="63" autocomplete="off"></div>
 <div class="row"><button class="btn sm" id="btnSetPass">保存并重建热点</button></div>
 <div class="hint">保存后热点会用新密码重建（当前手机会断开），请用新密码重新连接。</div>
</div>

<div class="card"><h3>事件日志</h3><div id="evlog"></div></div>
<div class="hint" style="text-align:center;margin-top:18px">由 DeepSeek 自主制作 · BW16 Deauth</div>
</div><div id="toasts"></div>
<script>
"use strict";
var $=function(s){return document.querySelector(s)};
var st={nets:[],sel:{},scanActive:0,scanTimer:0,scanId:0,scanBase:0,evSeen:0,netsSig:''};
function esc(s){return (s==null?'':String(s)).replace(/[&<>"']/g,function(c){return{'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]})}
function toast(m,c){var d=document.createElement('div');d.className='toast '+(c||'');d.textContent=m;$('#toasts').appendChild(d);setTimeout(function(){d.remove()},4200)}
function api(m,p,b){var o={method:m,headers:{}};if(b!==undefined){o.headers['Content-Type']='application/json';o.body=JSON.stringify(b)}return fetch(p,o).then(function(r){return r.json()})}
function bandOf(n){return (n.ch>14)?1:0}
function rssiHtml(r){var k=r>-55?4:r>-70?3:r>-85?2:1;var h='<span class="sig">';for(var i=1;i<=4;i++)h+='<i class="'+(i<=k?(k===1||k===2?'on w2':'on'):'')+'"></i>';return h+'</span><span class="mono" style="font-size:10.5px">'+r+'</span>'}
function renderNets(){var f=+$('#fltBand').value;var tb=$('#netTbl tbody');tb.innerHTML='';var cnt=0;
 st.nets.forEach(function(n){if(f>=0&&bandOf(n)!=f)return;cnt++;var tr=document.createElement('tr');if(st.sel[n.i])tr.className='sel';
  tr.innerHTML='<td><b>'+esc(n.s)+'</b> <span class="badge '+(bandOf(n)?'b5':'b24')+'">'+(bandOf(n)?'5G':'2.4G')+'</span> <span class="badge">'+esc(n.e||'open')+'</span></td><td>'+n.ch+'</td><td>'+rssiHtml(n.r)+'</td><td class="mono">'+esc(n.m)+'</td><td>'+(st.sel[n.i]?'✅':'⬜')+'</td>';
  tr.onclick=function(){if(st.sel[n.i])delete st.sel[n.i];else st.sel[n.i]=n;renderNets()};tb.appendChild(tr)});
 if(!cnt)tb.innerHTML='<tr><td colspan="5" class="empty">'+(st.nets.length?'当前频段筛选下没有网络（共 '+st.nets.length+' 个）':'没有网络，点击"重新扫描"')+'</td></tr>'}
function setScanNets(L){var old={};for(var k in st.sel)old[st.sel[k].m]=1;st.nets=L;var kk={};L.forEach(function(n){if(old[n.m])kk[n.i]=n});st.sel=kk;
 if(L.length){var f=+$('#fltBand').value;if(f>=0&&!L.some(function(n){return ((n.ch>14)?1:0)==f})){$('#fltBand').value='-1';toast('当前频段筛选下没有网络，已切回全部频段')}}
 renderNets()}
function scanBtnUI(on){var b=$('#btnScan');b.disabled=on;b.textContent=on?'扫描中…':'重新扫描'}
function doScan(){scanBtnUI(true);st.scanBase=st.scanId||0;st.scanActive=1;clearTimeout(st.scanTimer);
 st.scanTimer=setTimeout(function(){if(st.scanActive){st.scanActive=0;scanBtnUI(false);toast('扫描超时，未收到结果','err')}},35000);
 api('POST','/api/scan',{}).then(function(r){if(r&&r.err)toast('扫描：'+r.err,'err')}).catch(function(){if(st.scanActive){st.scanActive=0;clearTimeout(st.scanTimer);scanBtnUI(false);toast('扫描请求失败','err')}})}
$('#btnScan').onclick=doScan;$('#fltBand').onchange=renderNets;
function selList(){return Object.keys(st.sel).map(function(k){return st.sel[k]}).sort(function(a,b){return a.i-b.i})}
function poll(){fetch('/api/status').then(function(r){r.json().then(applyStatus)}).catch(function(){})}
function applyStatus(s){
 $('#modeDot').className='dot '+(s.attack&&s.attack.running?'attack':(s.scan&&s.scan.running?'scanning':'idle'));
 $('#modeTxt').textContent=(s.mode||'idle').toUpperCase();
 $('#apSsid').textContent=s.ap.ssid;$('#apIp').textContent=s.ap.ip;$('#apCl').textContent=s.ap.clients||0;
 $('#heap').textContent=Math.round(s.heap/1024)+'K';
 $('#stFrames').textContent=s.attack.frames||0;$('#stCli').textContent=s.attack.clients||0;$('#stTgt').textContent=s.attack.targets?s.attack.targets.length:0;
 $('#btnAtkStart').classList.toggle('hide',!!(s.attack&&s.attack.running));
 $('#btnAtkStop').classList.toggle('hide',!(s.attack&&s.attack.running));
 var am=(s.ap&&s.ap.cliMacs)||[],ah=$('#apCliHint');
 if(ah){ah.innerHTML='留空 = 自动嗅探目标网络的客户端（支持随机 MAC）；也可填“要做掉的设备”的 MAC（不是路由器 MAC）。本热点在线设备（点击填入）：'+(am.length?am.map(function(m){return '<a href="#" data-m="'+esc(m)+'" class="mono" style="color:#2f81f7;margin-right:10px;text-decoration:underline">'+esc(m)+'</a>'}).join(''):'<span class="mut">无</span>');
  Array.prototype.forEach.call(ah.querySelectorAll('a'),function(a){a.onclick=function(e){e.preventDefault();$('#atkMac').value=a.getAttribute('data-m');return false}})}
 if(s.scan){var sid=s.scan.id||0;
  if(sid>st.scanId){if(s.nets)setScanNets(s.nets);
   if(st.scanActive&&sid>st.scanBase){st.scanActive=0;clearTimeout(st.scanTimer);toast('扫描完成，发现 '+st.nets.length+' 个网络','ok')}}
  st.scanId=sid;scanBtnUI(!!(s.scan.running||st.scanActive))}
 else if(st.scanActive){st.scanActive=0;clearTimeout(st.scanTimer);scanBtnUI(false)}
 if(s.nets&&s.nets.length){
  var sig=s.nets.length+'|'+(s.nets[0]&&s.nets[0].m||'')+'|'+(s.nets[s.nets.length-1]&&s.nets[s.nets.length-1].m||'');
  if(sig!==st.netsSig){st.netsSig=sig;setScanNets(s.nets)}
 }
 if(s.ev){var el=$('#evlog');s.ev.forEach(function(e){if(e[0]<=st.evSeen)return;st.evSeen=e[0];var d=document.createElement('div');if(e[2]=='w')d.className='warn';if(e[2]=='e')d.className='err';if(e[2]=='o')d.className='ok';d.textContent=e[1];el.prepend(d);while(el.children.length>60)el.lastChild.remove()})}
}
$('#btnAtkStart').onclick=function(){var L=selList();if(!L.length)return toast('请先选择目标网络','err');
 api('POST','/api/attack/start',{targets:L.map(function(n){return n.i}),method:$('#atkMethod').value,interval:+$('#atkInt').value,mac:$('#atkMac').value.trim()}).then(function(r){if(r&&r.err)return toast('启动失败：'+r.err,'err');toast('攻击已启动','ok')}).catch(function(){toast('启动失败','err')})};
$('#btnAtkStop').onclick=function(){api('POST','/api/attack/stop',{}).then(function(){toast('已停止')})};
$('#btnSetPass').onclick=function(){var p=$('#apPass').value;if(p.length<8)return toast('密码至少 8 位','err');
 api('POST','/api/setpass',{pass:p}).then(function(r){if(r&&r.err)return toast('保存失败：'+r.err,'err');toast('已保存，热点正在用新密码重建','ok')}).catch(function(){toast('已保存（热点即将断开，请用新密码重连）','ok')})};
poll();setInterval(poll,1500);
api('GET','/api/scan').then(function(r){st.nets=r.nets||[];renderNets()}).catch(function(){});
</script></body></html>)rawliteral";

#endif
