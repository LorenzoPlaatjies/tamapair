// tp_portal.h — the cute phone page a pod shows during Wi-Fi setup (served by tp_link.cpp).
// Preview it in a browser without a board: python3 tools/portal-preview/serve.py
// Keep it small and self-contained: the phone has no internet while it's on the pod's network
// (no web fonts, no CDNs). Endpoints: GET /nets  POST /join (s, p)  GET /status
#pragma once
static const char TP_PORTAL_HTML[] = R"TPHTML(<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<title>tamapair</title>
<style>
:root{--pink:#ff6e9c;--pinkd:#e04f80;--ink:#3a1f2b;--soft:#fff3f7;--line:#ffd3e1;--mut:#9a7584}
*{box-sizing:border-box;-webkit-tap-highlight-color:transparent}
body{margin:0;min-height:100vh;background:linear-gradient(180deg,#ffe4ee,#fff6f9 60%);color:var(--ink);
font:17px/1.4 ui-rounded,"SF Pro Rounded",system-ui,-apple-system,"Segoe UI",Roboto,sans-serif;display:flex;justify-content:center}
main{width:100%;max-width:400px;padding:28px 20px 40px}
.pet{background:#000;border-radius:22px;padding:14px;margin:0 auto 22px;width:220px;box-shadow:0 10px 30px #ff6e9c40,inset 0 0 0 4px #2a0e22}
.pet svg{display:block;width:100%;image-rendering:pixelated}
.eye{animation:blink 4s infinite;transform-origin:center 25px;transform-box:view-box}
@keyframes blink{0%,46%,50%,100%{transform:scaleY(1)}48%{transform:scaleY(.1)}}
.busy .eye{animation:look 1.2s ease-in-out infinite}
@keyframes look{0%,100%{transform:translateX(-4px)}50%{transform:translateX(4px)}}
h1{font-size:26px;margin:0 0 4px;text-align:center;letter-spacing:-.3px}
.sub{text-align:center;color:var(--mut);margin:0 0 24px}
label{display:block;font-weight:600;margin:0 0 8px}
.nets{display:flex;flex-wrap:wrap;gap:8px;margin:0 0 10px}
.chip{border:2px solid var(--line);background:#fff;color:var(--ink);border-radius:999px;padding:8px 14px;font:inherit;font-size:15px;cursor:pointer}
.chip.on{background:var(--pink);border-color:var(--pink);color:#fff}
.chip.more{border-style:dashed;color:var(--mut)}
input{width:100%;font:inherit;padding:14px 16px;border:2px solid var(--line);border-radius:16px;background:#fff;color:var(--ink);outline:none}
input:focus{border-color:var(--pink)}
.field{margin:0 0 18px;position:relative}
.eyebtn{position:absolute;right:8px;bottom:8px;border:0;background:var(--soft);border-radius:10px;padding:6px 10px;font:inherit;font-size:14px;color:var(--mut)}
.go{width:100%;border:0;border-radius:18px;padding:16px;background:var(--pink);color:#fff;font:inherit;font-weight:700;font-size:19px;box-shadow:0 6px 0 var(--pinkd);margin-top:6px}
.go:active{transform:translateY(3px);box-shadow:0 3px 0 var(--pinkd)}
.go:disabled{opacity:.5}
.note{font-size:14px;color:var(--mut);text-align:center;margin:18px 0 0}
.msg{text-align:center;font-size:19px;margin:8px 0 0}
.oops{background:#fff;border:2px solid var(--line);border-radius:16px;padding:12px 14px;margin:0 0 18px;text-align:center}
[hidden]{display:none!important}
</style></head><body><main>
<div class="pet" id="pet"><svg viewBox="0 0 80 42" shape-rendering="crispEdges">
<g fill="#f7fbf7"><rect class="eye" x="15" y="10" width="13" height="19" rx="4"/><rect class="eye" x="52" y="10" width="13" height="19" rx="4"/></g>
<g id="blush" fill="#ff6e9c" opacity=".85"><rect x="9" y="30" width="7" height="3"/><rect x="64" y="30" width="7" height="3"/></g>
<path id="mouth" d="M36 32h2v2h4v-2h2v2h-2v2h-4v-2h-2z" fill="#f7fbf7"/></svg></div>

<section id="form">
<h1>Hi! Let's get me online</h1>
<p class="sub">Pick your Wi‑Fi and type its password.</p>
<div class="oops" id="oops" hidden></div>
<div class="field"><label for="s">Wi‑Fi name</label>
<div class="nets" id="nets"><span class="note" style="margin:0">Looking around…</span></div>
<input id="s" autocomplete="off" autocapitalize="off" spellcheck="false" placeholder="or type it here"></div>
<div class="field"><label for="p">Wi‑Fi password</label>
<input id="p" type="password" autocomplete="current-password" placeholder="password">
<button class="eyebtn" id="show" type="button">show</button></div>
<button class="go" id="go">Connect 💗</button>
<p class="note">Your password stays inside me. I never share it.</p>
</section>

<section id="wait" hidden><h1 id="wt">Connecting…</h1><p class="msg" id="wm">Watch my face on the little screen!</p></section>
</main><script>
const $=id=>document.getElementById(id),S=$("s"),P=$("p");
function face(state){$("pet").className="pet"+(state=="busy"?" busy":"");
 $("mouth").setAttribute("d",state=="sad"?"M36 34h8v2h-8z":state=="yay"?"M34 31h12v2h-2v2h-8v-2h-2z":"M36 32h2v2h4v-2h2v2h-2v2h-4v-2h-2z")}
function pick(n,el){S.value=n;document.querySelectorAll(".chip").forEach(c=>c.classList.toggle("on",c==el));P.focus()}
function nets(){fetch("/nets").then(r=>r.json()).then(j=>{const b=$("nets");b.innerHTML="";
 j.n.forEach(n=>{const c=document.createElement("button");c.type="button";c.className="chip"+(n==S.value?" on":"");c.textContent=n;c.onclick=()=>pick(n,c);b.appendChild(c)});
 const m=document.createElement("button");m.type="button";m.className="chip more";m.textContent=j.busy?"looking…":"↻ look again";
 m.onclick=()=>{fetch("/nets?again=1");m.textContent="looking…";setTimeout(nets,4000)};b.appendChild(m);
 if(j.saved&&!S.value)S.value=j.saved;if(j.busy)setTimeout(nets,2000)}).catch(()=>setTimeout(nets,2000))}
$("show").onclick=()=>{const h=P.type=="password";P.type=h?"text":"password";$("show").textContent=h?"hide":"show"};
function show(f){$("form").hidden=!f;$("wait").hidden=f}
function oops(t){$("oops").textContent=t;$("oops").hidden=false;face("sad");show(true)}
$("go").onclick=()=>{const s=S.value.trim();if(!s){oops("Which Wi‑Fi? Tap one above or type its name.");return}
 $("oops").hidden=true;show(false);face("busy");$("wt").textContent="Connecting…";$("wm").textContent="Watch my face on the little screen!";
 const b=new URLSearchParams({s,p:P.value});fetch("/join",{method:"POST",body:b}).then(()=>poll(0)).catch(()=>poll(0))};
function poll(n){setTimeout(()=>fetch("/status").then(r=>r.json()).then(j=>{
 if(j.st=="ok"){face("yay");$("wt").textContent="Yay, I'm online! 🎉";$("wm").textContent="You can close this page now.";}
 else if(j.st=="nonet")oops("I can't see that Wi‑Fi from here. Check the name, or move me closer?");
 else if(j.st=="fail")oops("Hmm, that didn't work. Double‑check the password?");
 else poll(n+1)}).catch(()=>{if(n>6){face("yay");$("wt").textContent="Almost done!";$("wm").textContent="If my face is back on the little screen, we're all set. You can close this page.";}else poll(n+1)}),1000)}
nets();
</script></body></html>)TPHTML";
