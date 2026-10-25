// ============================================================================
// The control page, served from flash. Deliberately tiny: no frameworks, no
// external assets, one request to load, then a 1 s poll of /api/status (and
// an immediate one when the tab returns to the foreground) so changes made
// with the hardware buttons appear on every connected phone.
// ============================================================================
#pragma once

#include <pgmspace.h>

static const char WEB_PAGE[] PROGMEM = R"HTML(<!doctype html>
<html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Traffic Light</title>
<style>
body{margin:0;background:#111;color:#eee;font:16px system-ui,sans-serif;text-align:center;-webkit-user-select:none;user-select:none}
main{max-width:360px;margin:0 auto;padding:16px}
.light{display:inline-flex;flex-direction:column;gap:14px;padding:16px;background:#222;border-radius:28px}
.lamp{width:104px;height:104px;border:0;border-radius:50%;opacity:.22}
.lamp.on{opacity:1;box-shadow:0 0 28px currentColor}
#red{background:#e22;color:#e22}#yellow{background:#fc0;color:#fc0}#green{background:#2c4;color:#2c4}
.seg{display:flex;margin:20px 0 12px;border:1px solid #555;border-radius:10px;overflow:hidden}
.seg button{flex:1;padding:14px 0;border:0;background:#222;color:#eee;font-size:16px}
.seg button.on{background:#eee;color:#111}
#fx{min-height:44px;visibility:hidden}
select{padding:10px;border-radius:8px;font-size:16px}
#now{display:block;margin-top:6px;color:#aaa;font-size:14px}
input{width:100%;margin-top:8px}
#net{min-height:20px;color:#f88;font-size:14px}
#speed{min-height:20px;margin:0 0 8px;color:#aaa;font-size:14px}
</style></head><body><main>
<div class="light"><button class="lamp" id="red"></button><button class="lamp" id="yellow"></button><button class="lamp" id="green"></button></div>
<div class="seg"><button id="auto">Auto</button><button id="manual">Manual</button><button id="effects">Effects</button></div>
<p id="speed"></p>
<div id="fx"><select id="effect"></select><span id="now"></span></div>
<label>Brightness <span id="pct"></span>%<input id="bright" type="range" min="0" max="100"></label>
<p id="net"></p>
</main><script>
var $=function(id){return document.getElementById(id)},sliding=false,timer=0;
function label(name){return name.replace(/_/g,' ')}
function offline(){$('net').textContent='Not connected to the costume'}
function loadEffects(){
fetch('/api/effects').then(function(r){return r.json()}).then(function(names){
var sel=$('effect');sel.options.length=0;sel.add(new Option('Cycle all','cycle'));
names.forEach(function(n){sel.add(new Option(label(n),n))});
});
}
function show(s){
// Lamps choose a color only in manual; elsewhere the hardware color buttons adjust speed.
['red','yellow','green'].forEach(function(c){$(c).className=s.lit==c?'lamp on':'lamp';$(c).disabled=s.mode!='manual'});
$('speed').textContent=s.mode=='auto'?'Cycle speed '+s.trafficSpeed/100+'\u00d7':s.mode=='effects'?'Animation speed '+s.effectSpeed/100+'\u00d7':'';
['auto','manual','effects'].forEach(function(m){$(m).className=s.mode==m?'on':''});
$('fx').style.visibility=s.mode=='effects'?'visible':'hidden';
var sel=$('effect');
if(!sel.options.length)loadEffects();
else if(document.activeElement!==sel)sel.value=s.cycle?'cycle':s.effect;
$('now').textContent=s.cycle?'Now: '+label(s.effect):'';
if(!sliding){$('bright').value=s.brightness;$('pct').textContent=s.brightness}
$('net').textContent='';
}
function poll(){
clearTimeout(timer);
fetch('/api/status').then(function(r){return r.json()}).then(show,offline)
.then(function(){timer=setTimeout(poll,1000)});
}
function post(path,body){
fetch(path,{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:body||''}).then(poll,offline);
}
['red','yellow','green'].forEach(function(c){$(c).onclick=function(){post('/api/color','color='+c)}});
['auto','manual','effects'].forEach(function(m){$(m).onclick=function(){post('/api/mode','mode='+m)}});
$('effect').onchange=function(){post('/api/effect','effect='+this.value);this.blur()};
$('bright').oninput=function(){sliding=true;$('pct').textContent=this.value};
$('bright').onchange=function(){sliding=false;post('/api/brightness','value='+this.value)};
document.addEventListener('visibilitychange',function(){if(!document.hidden)poll()});
poll();
</script></body></html>)HTML";
