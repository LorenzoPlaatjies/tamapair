// tp_portal.h — the phone page a pod shows during Wi-Fi setup (served by tp_link.cpp).
// Black + retro, same look as the pod: headings/buttons are drawn with facelab's 3x5 pixel font (the G table below).
// Three tabs: SETUP (tap a Wi-Fi -> type its password), HOW IT WORKS, HELP. No typing of Wi-Fi names, no emojis.
// Preview it in a browser without a board: python3 tools/portal-preview/serve.py
// Keep it small and self-contained: the phone has no internet while it's on the pod's network
// (no web fonts, no CDNs). Endpoints: GET /nets  POST /join (s, p)  GET /status
#pragma once
static const char TP_PORTAL_HTML[] = R"TPHTML(<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<meta name="color-scheme" content="dark">
<title>tamapair</title>
<style>
:root{--w:#f7fbf7;--pink:#ff6e9c;--blue:#6bd2ff;--grey:#8c8a9c;--dim:#2b2a33}
*{box-sizing:border-box;-webkit-tap-highlight-color:transparent;border-radius:0}
html{background:#000}
body{margin:0;min-height:100vh;background:#000;color:var(--w);font:15px/1.5 ui-monospace,Menlo,Consolas,"Courier New",monospace;display:flex;justify-content:center}
main{width:100%;max-width:400px;padding:20px 16px 40px}
.pet{border:2px solid var(--dim);padding:10px;margin:0 auto 18px;width:200px}
.pet svg{display:block;width:100%}
.eye{animation:blink 4s infinite;transform-origin:center 25px;transform-box:view-box}
@keyframes blink{0%,46%,50%,100%{transform:scaleY(1)}48%{transform:scaleY(.1)}}
.busy .eye{animation:look 1.2s steps(4) infinite}
@keyframes look{0%,100%{transform:translateX(-4px)}50%{transform:translateX(4px)}}
.px{display:flex;flex-wrap:wrap;gap:6px 8px;line-height:0}
.px svg{display:block;fill:currentColor}
.c{justify-content:center}
nav{display:flex;border:2px solid var(--w);margin:0 0 22px}
nav button{flex:1 1 auto;background:#000;color:var(--grey);border:0;border-left:2px solid var(--w);padding:12px 6px;font:inherit}
nav button:first-child{border-left:0}
nav button.on{background:var(--w);color:#000}
h1,h2{margin:0;font:inherit}
h1{color:var(--pink);margin:0 0 16px}
h2{color:var(--blue);margin:22px 0 8px}
p{margin:0 0 10px}
.row{display:flex;align-items:center;justify-content:space-between;gap:10px;width:100%;text-align:left;background:#000;color:var(--w);
border:2px solid var(--w);margin:0 0 8px;padding:14px 12px;font:inherit;font-size:16px;word-break:break-word}
.row:before{content:">";color:var(--pink);margin-right:2px}
.row span{flex:1}
.row small{color:var(--grey);font-size:12px;white-space:nowrap}
.row:active,.btn:active{transform:translate(3px,3px);box-shadow:none}
.btn{display:block;width:100%;border:2px solid var(--w);background:#000;color:var(--w);padding:15px 10px;margin:14px 0 0;font:inherit}
.go{background:var(--pink);border-color:var(--pink);color:#000;box-shadow:4px 4px 0 var(--w)}
.btn:disabled{color:var(--grey);border-color:var(--dim)}
.net{border-left:4px solid var(--pink);padding:4px 10px;margin:0 0 16px;font-size:17px;word-break:break-word}
.field{display:flex;margin:0 0 4px}
input{flex:1;min-width:0;font:inherit;font-size:17px;padding:13px 12px;border:2px solid var(--w);background:#000;color:var(--w);outline:none}
input:focus{border-color:var(--pink)}
#show{border:2px solid var(--w);border-left:0;background:#000;color:var(--w);padding:0 12px;font:inherit}
.note{color:var(--grey);font-size:13px;margin:16px 0 0}
.oops{border:2px solid var(--pink);color:var(--pink);padding:10px 12px;margin:0 0 14px}
.cur{display:inline-block;width:12px;height:20px;background:var(--pink);margin-left:8px;animation:cur 1s steps(1) infinite}
@keyframes cur{50%{opacity:0}}
details{border:2px solid var(--dim);margin:0 0 8px}
summary{list-style:none;padding:13px 12px;color:var(--w);cursor:pointer}
summary::-webkit-details-marker{display:none}
details[open]{border-color:var(--w)}
details[open] summary{color:var(--pink)}
details p{padding:0 12px;color:var(--w)}
b{color:var(--pink);font-weight:400}
footer{margin:34px 0 0;color:var(--grey)}
[hidden]{display:none!important}
</style></head><body><main>
<div class="pet" id="pet"><svg viewBox="0 0 80 42" shape-rendering="crispEdges">
<g fill="#f7fbf7"><rect class="eye" x="15" y="10" width="13" height="19" rx="4"/><rect class="eye" x="52" y="10" width="13" height="19" rx="4"/></g>
<g fill="#ff6e9c"><rect x="11" y="30" width="10" height="3"/><rect x="59" y="30" width="10" height="3"/></g>
<path id="mouth" d="M36 32h2v2h4v-2h2v2h-2v2h-4v-2h-2z" fill="#f7fbf7"/></svg></div>

<nav><button class="on" data-t="setup"><span class="px c" data-u="2">SETUP</span></button><button data-t="how"><span class="px c" data-u="2">HOW IT WORKS</span></button><button data-t="help"><span class="px c" data-u="2">HELP</span></button></nav>

<section id="setup">
<div id="s1"><h1 class="px" data-u="4">PICK YOUR WI-FI</h1>
<div class="oops" id="o1" hidden></div>
<div id="nets"></div>
<button class="btn" id="again" type="button"><span class="px c" id="againt">LOOK AGAIN</span></button>
<p class="note">Tap your Wi-Fi. Not there? See HELP.</p></div>

<div id="s2" hidden><h1 class="px" data-u="4">PASSWORD FOR</h1>
<p class="net" id="nn"></p>
<div class="oops" id="o2" hidden></div>
<div class="field"><input id="p" type="password" autocomplete="current-password" autocapitalize="off" autocorrect="off" spellcheck="false" enterkeyhint="go" placeholder="wi-fi password" aria-label="Wi-Fi password">
<button id="show" type="button"><span class="px" data-u="2" id="showt">SHOW</span></button></div>
<button class="btn go" id="go" type="button"><span class="px c" data-u="4">CONNECT</span></button>
<button class="btn" id="back" type="button"><span class="px c">&lt; OTHER WI-FI</span></button>
<p class="note">Your password stays on the pod. It is never shared.</p></div>

<div id="s3" hidden><h1><span class="px" data-u="4" id="wt" style="display:inline-flex">CONNECTING</span><i class="cur" id="cur"></i></h1>
<p id="wm">Watch the face on the little screen.</p></div>
</section>

<section id="how" hidden>
<h1 class="px" data-u="4">HOW IT WORKS</h1>
<p>Two pods. One is <b>Lorenzo's</b>, one is <b>Jade's</b>. Each pod shows a little face, and the face is the message.</p>
<h2 class="px">1 MOODS</h2>
<p>Swipe the face to choose how you feel. Stay on it for 3 seconds and it is sent. A heart floats off.</p>
<p>Your pod shows your mood, then your partner's mood on their colour, back and forth.</p>
<h2 class="px">2 FOOD</h2>
<p>Press the side button and tap FEED. Swipe to a snack, then tap the one in the middle.</p>
<p>It drops onto your partner's screen. Their pet catches it and munches.</p>
<h2 class="px">3 POKE</h2>
<p>Tap the face to play with it.</p>
<h2 class="px">4 MENU</h2>
<p>The side button opens three cards: HOME, FEED and SETTINGS. Settings has blush, swap time, their colour and brightness.</p>
<h2 class="px">5 ANYWHERE</h2>
<p>Each pod uses its own Wi-Fi to reach an online mailbox that only these two pods share. Same room or far apart, it works the same.</p>
<p>If your partner's pod is off, you see the offline face. Their last mood comes back when they do.</p>
</section>

<section id="help" hidden>
<h1 class="px" data-u="4">TROUBLESHOOTING</h1>
<details><summary><span class="px" data-u="2">MY WI-FI IS NOT IN THE LIST</span></summary>
<p>Move the pod closer to your router and tap LOOK AGAIN.</p>
<p>Pods only see 2.4 GHz Wi-Fi. Most home routers have it. Hidden networks do not show.</p>
<p>iPhone hotspot: turn on Maximise Compatibility in Personal Hotspot.</p></details>
<details><summary><span class="px" data-u="2">IT SAYS WRONG PASSWORD</span></summary>
<p>Tap SHOW to see what you typed. Capital letters count. Then tap CONNECT again.</p></details>
<details><summary><span class="px" data-u="2">MY PHONE SAYS NO INTERNET</span></summary>
<p>That is normal. The pod's own network is only for setup. Choose to stay connected, then finish here.</p></details>
<details><summary><span class="px" data-u="2">THIS PAGE DID NOT OPEN BY ITSELF</span></summary>
<p>Stay on the pod's network (tamapair-...) and open 192.168.4.1 in your browser.</p></details>
<details><summary><span class="px" data-u="2">NEW HOME OR NEW WI-FI</span></summary>
<p>Hold the side button for 5 seconds. The QR code comes back. Scan it and set up again.</p></details>
<details><summary><span class="px" data-u="2">NO QR CODE ON THE SCREEN</span></summary>
<p>The pod is already online, or the code was hidden. Press the side button, tap SETTINGS, then tap the status line at the bottom.</p></details>
<details><summary><span class="px" data-u="2">MY PARTNER SHOWS OFFLINE</span></summary>
<p>Their pod is switched off or has lost its Wi-Fi. Nothing to fix on yours.</p></details>
<details><summary><span class="px" data-u="2">STILL STUCK</span></summary>
<p>Unplug the pod, wait a moment, and plug it back in.</p></details>
</section>

<footer class="px c" data-u="2">TAMAPAIR - LORENZO + JADE</footer>
</main><script>
// facelab's 3x5 font (firmware/facelab/facelab.ino glyph()); 25-bit entries are the 5-wide M and W
const G={A:"010101111101101",B:"110101110101110",C:"111100100100111",D:"110101101101110",E:"111100110100111",F:"111100110100100",
G:"111100101101111",H:"101101111101101",I:"111010010010111",J:"001001001101111",K:"101101110101101",L:"100100100100111",
M:"1000111011101011000110001",N:"110101101101101",O:"111101101101111",P:"110101110100100",Q:"111101101111001",R:"110101110101101",
S:"111100111001111",T:"111010010010010",U:"101101101101111",V:"101101101101010",W:"1000110001101011101110001",X:"101101010101101",
Y:"101101010010010",Z:"111001010100111","-":"000000111000000","+":"000010111010000","<":"001010100010001",
0:"111101101101111",1:"010110010010111",2:"111001111100111",3:"111001111001111",4:"101101111001001",5:"111100111001111",
6:"111100111101111",7:"111001001001001",8:"111101111101111",9:"111101111001111"};
const $=id=>document.getElementById(id),P=$("p");let cur="";
function px(el,t){t=(t||el.textContent).trim().toUpperCase();const u=+el.dataset.u||3;el.textContent="";el.style.gap=2*u+"px "+3*u+"px";el.setAttribute("role","img");el.setAttribute("aria-label",t);
 t.split(/\s+/).forEach(w=>{let x=0,d="";for(const ch of w){const g=G[ch]||"",n=g.length>15?5:3;
  for(let i=0;i<g.length;i++)if(g[i]=="1")d+="M"+(x+i%n)+" "+(i/n|0)+"h1v1h-1z";x+=n+1}
  el.insertAdjacentHTML("beforeend",'<svg width="'+(x-1)*u+'" height="'+5*u+'" viewBox="0 0 '+(x-1)+' 5" shape-rendering="crispEdges"><path d="'+d+'"/></svg>')})}
document.querySelectorAll(".px").forEach(e=>px(e));
document.querySelectorAll("nav button").forEach(b=>b.onclick=()=>{document.querySelectorAll("nav button").forEach(o=>o.classList.toggle("on",o==b));
 ["setup","how","help"].forEach(s=>$(s).hidden=s!=b.dataset.t);scrollTo(0,0)});
function face(state){$("pet").className="pet"+(state=="busy"?" busy":"");
 $("mouth").setAttribute("d",state=="sad"?"M36 34h8v2h-8z":state=="yay"?"M34 31h12v2h-2v2h-8v-2h-2z":"M36 32h2v2h4v-2h2v2h-2v2h-4v-2h-2z")}
function step(n){[1,2,3].forEach(i=>$("s"+i).hidden=i!=n)}
function oops(n,t){const o=$("o"+n);o.textContent=t;o.hidden=false;face("sad");step(n)}
function pick(n){cur=n;$("nn").textContent=n;P.value="";$("o1").hidden=$("o2").hidden=true;face();step(2);P.focus()}
function nets(){fetch("/nets").then(r=>r.json()).then(j=>{const b=$("nets");b.textContent="";
 j.n.forEach(n=>{const c=document.createElement("button"),s=document.createElement("span");c.type="button";c.className="row";s.textContent=n;c.appendChild(s);
  if(n==j.saved){const t=document.createElement("small");t.textContent="last used";c.appendChild(t)}c.onclick=()=>pick(n);b.appendChild(c)});
 if(!j.n.length){const p=document.createElement("p");p.textContent=j.busy?"Looking around...":"No Wi-Fi found yet. Move me closer to your router, then look again.";b.appendChild(p)}
 $("again").disabled=j.busy;px($("againt"),j.busy?"LOOKING":"LOOK AGAIN");if(j.busy)setTimeout(nets,2000)}).catch(()=>setTimeout(nets,2000))}
$("again").onclick=()=>{$("again").disabled=true;px($("againt"),"LOOKING");fetch("/nets?again=1").catch(()=>{});setTimeout(nets,4000)};
$("back").onclick=()=>{face();step(1)};
$("show").onclick=()=>{const h=P.type=="password";P.type=h?"text":"password";px($("showt"),h?"HIDE":"SHOW")};
P.onkeydown=e=>{if(e.key=="Enter")go()};$("go").onclick=go;
function wait(t,m,busy){px($("wt"),t);$("wm").textContent=m;$("cur").hidden=!busy;step(3)}
function go(){if(!cur){step(1);return}$("o2").hidden=true;P.blur();face("busy");wait("CONNECTING","Watch the face on the little screen.",1);
 fetch("/join",{method:"POST",body:new URLSearchParams({s:cur,p:P.value})}).then(()=>poll(0)).catch(()=>poll(0))}
function poll(n){setTimeout(()=>fetch("/status").then(r=>r.json()).then(j=>{
 if(j.st=="ok"){face("yay");wait("ONLINE","All set. You can close this page now.")}
 else if(j.st=="nonet")oops(1,"I can't see that Wi-Fi from here. Move me closer, or pick another one.");
 else if(j.st=="fail"){oops(2,"That didn't work. Double-check the password.");P.focus();P.select()}
 else poll(n+1)}).catch(()=>{if(n>6){face("yay");wait("ALMOST DONE","If the face is back on the little screen, we're all set. You can close this page.")}else poll(n+1)}),1000)}
nets();
</script></body></html>)TPHTML";
