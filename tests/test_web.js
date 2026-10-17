const fs=require('fs'); process.chdir(process.argv[2]);
let sent=[]; let handlers={}; let timers=[];
function mkEl(tag){ const e={tag,style:{},dataset:{},children:[],_cls:new Set(),textContent:'',_html:'',onclick:null,disabled:false,
  classList:{add:c=>e._cls.add(c),remove:(...c)=>c.forEach(x=>e._cls.delete(x)),contains:c=>e._cls.has(c)},
  appendChild(c){c.parentNode=e;e.children.push(c);return c},
  remove(){ if(e.parentNode){const i=e.parentNode.children.indexOf(e); if(i>=0)e.parentNode.children.splice(i,1);} },
  addEventListener(t,f){(handlers[(e.id||tag)+':'+t]=f)},
  querySelectorAll(sel){ if(sel===".sq")return e.children.filter(c=>/\bsq\b/.test(c.className||'')); if(sel===".dot,.ring")return e.children.filter(c=>/\b(dot|ring)\b/.test(c.className||'')); return [] },
  set innerHTML(v){e._html=v; if(v==="")e.children=[]}, get innerHTML(){return e._html},
  scrollTop:0,scrollHeight:0 };
  Object.defineProperty(e,'className',{get(){return e._cn||''},set(v){e._cn=v; e._cls=new Set(v.split(' ').filter(Boolean))}});
  return e }
const reg={}; const body=mkEl('body');
const document={createElement:t=>mkEl(t),getElementById:id=>{ if(id.startsWith('s_')){const b=reg['board']; return b.children.find(c=>c.id===id)||mkEl('x')} return reg[id]||(reg[id]=Object.assign(mkEl('div'),{id}))}, body};
['board','promo','promoBox','conn','status','moves','topName','botName','topCap','botCap','topClock','botClock','flip','resign','again'].forEach(i=>document.getElementById(i));
reg['board'].dataset={};
const win={AudioContext:undefined}; 
class WS{constructor(u){this.url=u;this.readyState=1;global.__ws=this;setTimeout(()=>this.onopen&&this.onopen(),0)} send(m){sent.push(m)} close(){}}
global.document=document; global.location={host:'192.168.1.5:8080'}; global.WebSocket=WS; global.navigator={}; global.window=win; global.confirm=()=>true;
global.setInterval=(f)=>0; const realST=setTimeout; global.setTimeout=(f,ms)=>realST(f,0);
const code=fs.readFileSync('client.js','utf8'); new Function(code)();
function feed(file){ const s=fs.readFileSync(file,'utf8').trim(); global.__ws.onmessage({data:s}); }
function click(sq){ const t=document.getElementById('s_'+sq); handlers['board:click']({target:t}); }
let fails=0; function ok(c,m){ if(!c){console.log('FAIL',m);fails++} }
setTimeout(()=>{
  feed('s_start.json');
  ok(reg['board'].children.filter(c=>/\bsq\b/.test(c.className)).length===64,'64 squares');
  ok(reg['board'].children.filter(c=>/\bpc\b/.test(c.className)).length===32,'32 pieces');
  ok(/Your move/.test(reg['status'].textContent),'status your move: '+reg['status'].textContent);
  click('e2'); ok(document.getElementById('s_e2')._cls.has('sel'),'e2 selected');
  const dots=reg['board'].children.filter(c=>/\bsq\b/.test(c.className)).reduce((n,s)=>n+s.children.filter(c=>/\b(dot|ring)\b/.test(c.className)).length,0);
  ok(dots===2,'2 legal dots for e2 pawn, got '+dots);
  click('e4'); ok(sent[sent.length-1]==='move e2e4','sent move e2e4, got '+sent[sent.length-1]);
  // promotion
  feed('s_promo.json'); click('a7'); click('a8');
  ok(reg['promo'].style.display==='flex','promo dialog shown');
  reg['promoBox'].children[0].onclick(); ok(sent[sent.length-1]==='move a7a8q','promo queen: '+sent[sent.length-1]);
  // black player: board flipped, not my turn message when white to move? (state is black to move, you=b)
  feed('s_black.json');
  ok(document.getElementById('s_a8').style.top==='87.5%'||document.getElementById('s_a8').style.top==='0%','board built');
  const h8=document.getElementById('s_h8'); ok(h8.style.left==='0%'&&h8.style.top==='87.5%','flipped for black: h8 bottom-left, got '+h8.style.left+','+h8.style.top);
  ok(/Your move/.test(reg['status'].textContent),'black: your move');
  click('d7'); click('d5'); ok(sent[sent.length-1]==='move d7d5','black sends d7d5: '+sent[sent.length-1]);
  console.log(fails?fails+' JS FAILURES':'ALL JS CLIENT TESTS PASSED');
  process.exit(fails?1:0);
},20);
