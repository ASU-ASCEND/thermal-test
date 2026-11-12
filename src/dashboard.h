#pragma once

const char kDashboard[] PROGMEM = R"HTML(
<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Thermal test</title>
<style>
:root{color-scheme:light;--ink:#173148;--muted:#536d7a;--paper:#f3f7f8;--line:#cad9de;--blue:#086b91;--orange:#b44b28}
*{box-sizing:border-box}body{margin:0;background:var(--paper);color:var(--ink);font:16px/1.5 system-ui,-apple-system,sans-serif}main{max-width:1050px;margin:auto;padding:clamp(20px,4vw,48px)}header{display:flex;justify-content:space-between;align-items:baseline;gap:12px;flex-wrap:wrap}h1{font-size:clamp(26px,4vw,38px);letter-spacing:-.035em;margin:0}h2{font-size:17px;margin:0}p{margin:0}.status{color:var(--muted);font-size:14px}.values{display:grid;grid-template-columns:repeat(2,1fr);gap:16px;margin:28px 0}.value{background:#fff;padding:20px 22px;border-top:4px solid var(--blue)}.value:nth-child(2){border-color:var(--orange)}.label{color:var(--muted);font-size:14px}.number{font-size:clamp(32px,5vw,48px);font-weight:650;letter-spacing:-.04em;line-height:1.25}.unit{font-size:18px;color:var(--muted);font-weight:400}.plot{background:#fff;border:1px solid var(--line);padding:20px}.plot-head{display:flex;justify-content:space-between;gap:15px;flex-wrap:wrap}.legend{display:flex;gap:18px;font-size:14px;color:var(--muted)}.dot{display:inline-block;width:10px;height:10px;border-radius:50%;background:var(--blue);margin-right:6px}.dot.orange{background:var(--orange)}canvas{display:block;width:100%;height:280px;margin-top:12px}.foot{display:flex;flex-wrap:wrap;gap:12px 28px;margin-top:18px;font-size:14px;color:var(--muted)}.foot strong{color:var(--ink);font-weight:600}@media(max-width:500px){.values{gap:8px}.value{padding:15px 12px}canvas{height:220px}.legend{gap:10px}}
</style>
</head>
<body><main>
<header><h1>Thermal test</h1><p class="status" id="connection">Connecting to Pico…</p></header>
<section class="values" aria-label="Current temperatures">
  <div class="value"><div class="label">External temperature · I2C0</div><div class="number"><span id="t0">—</span><span class="unit"> °C</span></div></div>
  <div class="value"><div class="label">Internal temperature · I2C1</div><div class="number"><span id="t1">—</span><span class="unit"> °C</span></div></div>
</section>
<section class="plot"><div class="plot-head"><h2>Recent temperature</h2><div class="legend"><span><i class="dot"></i>External</span><span><i class="dot orange"></i>Internal</span></div></div><canvas id="chart" aria-label="Recent temperature readings"></canvas><p class="status">Last 5 minutes · one sample per second</p></section>
<div class="foot"><span>SD card: <strong id="sd">checking</strong></span><span>Clock: <strong id="clock">checking</strong></span><span>Log: <strong id="log">—</strong></span></div>
</main><script>
const chart=document.querySelector('#chart'),ctx=chart.getContext('2d');let history=[];
function draw(){const dpr=window.devicePixelRatio||1,w=chart.clientWidth,h=chart.clientHeight;chart.width=w*dpr;chart.height=h*dpr;ctx.setTransform(dpr,0,0,dpr,0,0);ctx.clearRect(0,0,w,h);const all=history.flatMap(p=>[p[1],p[2]]).filter(Number.isFinite);if(!all.length){ctx.fillStyle='#536d7a';ctx.fillText('Waiting for readings',12,28);return}let lo=Math.min(...all),hi=Math.max(...all);const pad=Math.max((hi-lo)*.15,.25);lo-=pad;hi+=pad;const left=50,right=w-10,top=12,bottom=h-25;ctx.font='12px system-ui';ctx.textAlign='right';for(let i=0;i<=4;i++){const y=top+(bottom-top)*i/4;ctx.strokeStyle='#dfe9ec';ctx.beginPath();ctx.moveTo(left,y);ctx.lineTo(right,y);ctx.stroke();ctx.fillStyle='#536d7a';ctx.fillText((hi-(hi-lo)*i/4).toFixed(1),left-8,y+4)}const end=history[history.length-1][0],start=Math.max(0,end-300000),span=Math.max(end-start,1000);[1,2].forEach((col)=>{ctx.strokeStyle=col===1?'#086b91':'#b44b28';ctx.lineWidth=2;ctx.beginPath();let started=false;for(const p of history){if(!Number.isFinite(p[col])){started=false;continue}const x=left+(p[0]-start)/span*(right-left),y=bottom-(p[col]-lo)/(hi-lo)*(bottom-top);if(started)ctx.lineTo(x,y);else{ctx.moveTo(x,y);started=true}}ctx.stroke()})}
function updateValue(id,value){document.querySelector(id).textContent=Number.isFinite(value)?value.toFixed(2):'—'}
async function refresh(){try{const response=await fetch('/api/status',{cache:'no-store'});if(!response.ok)throw Error();const data=await response.json();history=data.history;updateValue('#t0',data.t0);updateValue('#t1',data.t1);document.querySelector('#sd').textContent=data.sd;document.querySelector('#clock').textContent=data.clock;document.querySelector('#log').textContent=data.log||'not recording';document.querySelector('#connection').textContent='Live · updated '+new Date().toLocaleTimeString();draw()}catch(e){document.querySelector('#connection').textContent='Connection lost · retrying'}}
window.addEventListener('resize',draw);refresh();setInterval(refresh,1000);
</script></body></html>
)HTML";
