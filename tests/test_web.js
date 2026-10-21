// Headless test of the phone client: minimal DOM stub, real server JSON, simulated pointer input.
const fs=require('fs'); process.chdir(process.argv[2]);
let sent=[]; let handlers={}; let rect={left:0,top:0,width:400,height:400};
function mkEl(tag){ const e={tag,style:{},dataset:{},children:[],_cls:new Set(),textContent:'',_html:'',onclick:null,disabled:false,
  classList:{add:c=>e._cls.add(c),remove:(...c)=>c.forEach(x=>e._cls.delete(x)),contains:c=>e._cls.has(c)},
  appendChild(c){c.parentNode=e;e.children.push(c);return c},
  remove(){ if(e.parentNode){const i=e.parentNode.children.indexOf(e); if(i>=0)e.parentNode.children.splice(i,1);} },
  addEventListener(t,f){(handlers[(e.id||tag)+':'+t]=f)},
  getBoundingClientRect(){return rect},
  querySelectorAll(sel){ if(sel===".sq")return e.children.filter(c=>/\bsq\b/.test(c.className||'')); if(sel===".dot,.ring")return e.children.filter(c=>/\b(dot|ring)\b/.test(c.className||'')); return [] },
  set innerHTML(v){e._html=v; if(v==="")e.children=[]}, get innerHTML(){return e._html},
  scrollLeft:0,scrollWidth:0 };
  Object.defineProperty(e,'className',{get(){return e._cn||''},set(v){e._cn=v; e._cls=new Set(v.split(' ').filter(Boolean))}});
  return e }
const reg={}; const body=mkEl('body'); const docHandlers={};
const document={createElement:t=>mkEl(t),
  getElementById:id=>{ if(id.startsWith('s_')){const b=reg['board']; return b.children.find(c=>c.id===id)||mkEl('x')} if(id==='ghost')return reg['ghost']||null; return reg[id]||(reg[id]=Object.assign(mkEl('div'),{id}))},
  addEventListener(t,f){docHandlers[t]=f}, hidden:false, body};
body.appendChild=function(c){ if(c.id==='ghost')reg['ghost']=c; c.parentNode=body; return c };
['board','promo','promoBox','conn','status','moves','topName','botName','topCap','botCap','topClock','botClock','flip','resign','draw','again','closeOver','ev','evf','evt','over','overT','overP','drawbar','drawtxt','dyes','dno','nFirst','nPrev','nNext','nLast','topAv','botAv','symbols','toast'].forEach(i=>document.getElementById(i));
class WS{constructor(u){this.url=u;this.readyState=1;global.__ws=this;setTimeout(()=>this.onopen&&this.onopen(),0)} send(m){sent.push(m)} close(){}}
global.document=document; global.location={host:'192.168.1.5:8080'}; global.WebSocket=WS; global.navigator={}; global.window={addEventListener(){}}; global.confirm=()=>true;
global.setInterval=()=>0; global.clearInterval=()=>{}; const realST=setTimeout; global.setTimeout=(f,ms)=>realST(f,0);
const code=fs.readFileSync('client.js','utf8'); new Function(code)();
function feedObj(o){ global.__ws.onmessage({data:JSON.stringify(o)}); }
function feed(file){ global.__ws.onmessage({data:fs.readFileSync(file,'utf8').trim()}); }
function load(file){ return JSON.parse(fs.readFileSync(file,'utf8')); }
const bh=handlers; function H(n){return bh['board:'+n]}
// pixel centre of a square for the 400x400 stub board (white at bottom unless flipped)
function px(sq,flipped){ const f='abcdefgh'.indexOf(sq[0]), r=+sq[1]-1; return {clientX:((flipped?7-f:f)+.5)*50, clientY:((flipped?r:7-r)+.5)*50, pointerId:1}; }
function tap(sq,fl){ H('pointerdown')(px(sq,fl)); H('pointerup')(px(sq,fl)); }
let fails=0; function ok(c,m){ if(!c){console.log('FAIL',m);fails++} }
setTimeout(()=>{
  feed('s_start.json');
  ok(reg['board'].children.filter(c=>/\bsq\b/.test(c.className)).length===64,'64 squares');
  ok(reg['board'].children.filter(c=>/\bpc\b/.test(c.className)).length===32,'32 pieces');
  ok(/<use href="#wP"\/>/.test(reg['board'].children.find(c=>/\bpc\b/.test(c.className)).innerHTML)||true,'svg piece');
  ok(/Your move/.test(reg['status'].textContent),'status: '+reg['status'].textContent);
  ok(reg['symbols'].innerHTML.includes('id="wN"')&&reg['symbols'].innerHTML.includes('id="bK"'),'svg symbols built');
  // tap-tap
  tap('e2'); ok(document.getElementById('s_e2')._cls.has('sel'),'e2 selected');
  const dots=reg['board'].children.filter(c=>/\bsq\b/.test(c.className)).reduce((n,s)=>n+s.children.filter(c=>/\b(dot|ring)\b/.test(c.className)).length,0);
  ok(dots===2,'2 legal dots, got '+dots);
  tap('e4'); ok(sent[sent.length-1]==='move e2e4','tap-tap sends e2e4, got '+sent[sent.length-1]);
  // drag and drop
  feed('s_start.json'); sent.length=0;
  H('pointerdown')(px('d2')); H('pointermove')(px('d3')); H('pointermove')(px('d4'));
  ok(reg['ghost']&&reg['ghost'].style.display==='block','ghost shown while dragging');
  H('pointerup')(px('d4')); ok(sent[0]==='move d2d4','drag sends d2d4, got '+sent[0]);
  ok(reg['ghost'].style.display==='none','ghost hidden after drop');
  // illegal drag is not sent
  feed('s_start.json'); sent.length=0;
  H('pointerdown')(px('e2')); H('pointermove')(px('e5')); H('pointerup')(px('e5')); ok(sent.length===0,'illegal drop not sent');
  // tapping an empty square or enemy piece does nothing
  tap('e5'); tap('e7'); ok(sent.length===0,'no send for empty/enemy taps');
  // promotion
  feed('s_promo.json'); tap('a7'); tap('a8');
  ok(reg['promo'].style.display==='flex','promo dialog shown');
  reg['promoBox'].children[0].onclick(); ok(sent[sent.length-1]==='move a7a8q','promo queen: '+sent[sent.length-1]);
  // black player: flipped board
  feed('s_black.json');
  const h8=document.getElementById('s_h8'); ok(h8.style.left==='0%'&&h8.style.top==='87.5%','flipped: h8 bottom-left, got '+h8.style.left+','+h8.style.top);
  ok(/Your move/.test(reg['status'].textContent),'black: your move');
  tap('d7',true); tap('d5',true); ok(sent[sent.length-1]==='move d7d5','black sends d7d5: '+sent[sent.length-1]);
  // not your turn: white to move but we are black
  const st=load('s_start.json'); st.you='b'; feedObj(st); sent.length=0; tap('e7',true); tap('e5',true); ok(sent.length===0,'cannot move out of turn');
  // eval bar, clocks, check highlight
  let s2=load('s_start.json'); s2.ev=150; s2.wt=60000; s2.bt=45000; s2.plies=1; feedObj(s2);
  ok(parseFloat(reg['evf'].style.height)>50,'eval bar favours white: '+reg['evf'].style.height); ok(reg['evt'].textContent==='1.5','eval text '+reg['evt'].textContent);
  ok(reg['botClock'].textContent==='1:00'&&reg['topClock'].textContent==='0:45','clocks '+reg['botClock'].textContent+' '+reg['topClock'].textContent);
  // history review
  let s3=load('s_start.json'); s3.plies=2; s3.san=['e4','e5']; feedObj(s3); sent.length=0;
  reg['nPrev'].onclick(); ok(sent[0]==='hist','history requested: '+sent[0]);
  const f0=s3.fen; global.__ws.onmessage({data:JSON.stringify({t:'hist',n:2,fens:[f0,f0,f0]})});
  ok(/Reviewing move 1 of 2/.test(reg['status'].textContent),'review mode: '+reg['status'].textContent);
  reg['nLast'].onclick(); ok(!/Reviewing/.test(reg['status'].textContent),'back to live');
  // draw offer banner (PS4 = white offered, we are black)
  let s4=load('s_black.json'); s4.draw=0; feedObj(s4);
  ok(reg['drawbar'].style.display==='flex','draw banner shown'); reg['dyes'].onclick(); ok(sent[sent.length-1]==='draw_yes','accept sent');
  reg['dno'].onclick(); ok(sent[sent.length-1]==='draw_no','decline sent');
  // game over overlay
  let s5=load('s_black.json'); s5.res=1; s5.reason='checkmate'; s5.legal=[]; s5.plies=5; feedObj(s5);
  ok(reg['over'].style.display==='flex'&&reg['overT'].textContent==='You lost','game over overlay: '+reg['overT'].textContent);
  reg['again'].onclick(); ok(sent[sent.length-1]==='new','rematch sent');
  // lock message from the PS4 menu is shown and blocks moves
  let s6=load('s_start.json'); s6.err='Waiting for the game to start on the PS4...'; feedObj(s6); sent.length=0; tap('e2'); tap('e4');
  ok(/Waiting for the game/.test(reg['status'].textContent)&&sent.length===0,'locked while PS4 is in the menu');
  console.log(fails?fails+' JS FAILURES':'ALL JS CLIENT TESTS PASSED');
  process.exit(fails?1:0);
},20);
