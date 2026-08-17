const state={ph:7.1,temp:25.4,level:78,battery:89,pump:false,auto:true,phMin:6.5,phMax:8.5,history:[],alerts:[],events:[]};
const $=id=>document.getElementById(id);
const now=()=>new Date().toLocaleTimeString('fr-FR');
function addEvent(icon,title,detail){state.events.unshift({icon,title,detail,time:now()});state.events=state.events.slice(0,6);renderEvents()}
function addAlert(icon,title,detail){state.alerts.unshift({icon,title,detail,time:now()});state.alerts=state.alerts.slice(0,50);renderAlerts()}
function classify(){return state.ph<state.phMin||state.ph>state.phMax?'danger':'normal'}
function updateStatus(){
 const bad=classify()==='danger';
 $('waterStatus').textContent=bad?'EAU À TRAITER':'EAU PROPRE';
 $('statusIcon').textContent=bad?(state.pump?'🟡':'🔴'):'🟢';
 $('statusMessage').textContent=bad?(state.pump?'Traitement automatique en cours.':'Anomalie détectée : traitement requis.'):'Les paramètres sont dans la plage configurée.';
 $('statusCard').style.background=bad?'linear-gradient(135deg,#fff5e7,#fff)':'linear-gradient(135deg,#e8fbf2,#fff)';
 $('phBadge').textContent=bad?'ANOMALIE':'NORMAL'; $('phBadge').className='badge '+(bad?'danger':'');
 $('pumpState').textContent=state.pump?'EN FONCTIONNEMENT':'ARRÊTÉE';
 $('pumpEquipment').textContent=state.pump?'EN FONCTIONNEMENT':'ARRÊTÉE';
 $('processTreatment').className='process-step '+(state.pump?'active':'');
 $('processDone').className='process-step '+(!state.pump&&!bad?'done':'');
 $('levelMessage').textContent=state.level<15?'Niveau critique — pompe bloquée':state.level<30?'Niveau faible':'Niveau suffisant';
 $('levelMeter').style.width=state.level+'%'; $('batteryMeter').style.width=state.battery+'%';
 $('phMeter').style.width=Math.max(0,Math.min(100,((state.ph-4)/7)*100))+'%';
 $('batteryBadge').textContent=state.battery<20?'FAIBLE':'BONNE';
 $('batteryBadge').className='badge '+(state.battery<20?'warning':'');
}
function renderMetrics(){
 $('phValue').textContent=state.ph.toFixed(2); $('tempValue').textContent=state.temp.toFixed(1);
 $('levelValue').textContent=Math.round(state.level); $('batteryValue').textContent=Math.round(state.battery);
 $('lastUpdate').textContent=now(); updateStatus(); drawChart();
}
function renderEvents(){
 $('eventsList').innerHTML=state.events.map(e=>`<div class="event"><span class="eicon">${e.icon}</span><div><b>${e.title}</b><small>${e.detail} · ${e.time}</small></div></div>`).join('')||'<div class="event">Aucun événement</div>';
}
function renderAlerts(){
 $('alertsList').innerHTML=state.alerts.map(a=>`<div class="alert-item"><div class="aicon">${a.icon}</div><div><b>${a.title}</b><small>${a.detail} · ${a.time}</small></div></div>`).join('')||'<div class="panel">Aucune alerte enregistrée.</div>';
}
function renderHistory(){
 $('historyBody').innerHTML=state.history.slice().reverse().map(h=>`<tr><td>${h.time}</td><td>${h.ph.toFixed(2)}</td><td>${h.temp.toFixed(1)} °C</td><td>${Math.round(h.level)} %</td><td>${Math.round(h.battery)} %</td><td>${h.status}</td></tr>`).join('');
}
function drawChart(){
 const c=$('phChart'),ctx=c.getContext('2d'),w=c.width,h=c.height;
 ctx.clearRect(0,0,w,h); ctx.font='10px system-ui';
 ctx.strokeStyle='#e5eeea'; ctx.lineWidth=1;
 [6.5,7,7.5,8,8.5].forEach(v=>{const y=h-25-((v-6)/3)*180;ctx.beginPath();ctx.moveTo(35,y);ctx.lineTo(w-10,y);ctx.stroke();ctx.fillStyle='#8da099';ctx.fillText(v.toFixed(1),5,y+3)});
 const data=state.history.slice(-24).map(x=>x.ph); if(data.length<2)return;
 ctx.strokeStyle='#15a36a';ctx.lineWidth=3;ctx.beginPath();
 data.forEach((v,i)=>{const x=35+i*((w-50)/(data.length-1));const y=h-25-((v-6)/3)*180;i?ctx.lineTo(x,y):ctx.moveTo(x,y)});ctx.stroke();
}
function simulate(){
 const badChance=Math.random()<.14;
 if(badChance) state.ph=+(Math.random()<.5?(5.8+Math.random()*.5):(8.8+Math.random()*.5)).toFixed(2);
 else state.ph=+(6.8+Math.random()*1.0).toFixed(2);
 state.temp=+(24.2+Math.random()*3.2).toFixed(1);
 state.level=Math.max(8,state.level+(Math.random()-.46)*2);
 state.battery=Math.max(12,state.battery+(Math.random()-.35)*.5);
 const bad=classify()==='danger';
 if(state.auto){
   const blocked=state.level<15;
   if(bad&&!blocked&&!state.pump){state.pump=true;addEvent('🔴','Anomalie détectée','Pompe activée automatiquement');addAlert('🔴','Traitement automatique démarré','pH hors de la plage configurée');}
   if(!bad&&state.pump){state.pump=false;addEvent('🟢','Traitement terminé','pH revenu dans la plage');addAlert('🟢','Eau propre','Pompe arrêtée automatiquement');}
   if(blocked&&state.pump){state.pump=false;addAlert('⚠️','Pompe arrêtée','Niveau du réservoir trop faible');}
 }
 state.history.push({time:now(),ph:state.ph,temp:state.temp,level:state.level,battery:state.battery,status:bad?'À traiter':'Propre'});
 state.history=state.history.slice(-100);
 renderMetrics();renderHistory();renderAlerts();
}
function setView(view){
 document.querySelectorAll('.view').forEach(v=>v.classList.remove('active-view')); $(view).classList.add('active-view');
 document.querySelectorAll('.nav-item').forEach(b=>b.classList.toggle('active',b.dataset.view===view));
 window.scrollTo({top:0,behavior:'smooth'});
}
document.querySelectorAll('.nav-item').forEach(b=>b.onclick=()=>setView(b.dataset.view));
document.querySelectorAll('[data-view-target]').forEach(b=>b.onclick=()=>setView(b.dataset.viewTarget));
$('notifyBtn').onclick=()=>alert(state.alerts.length?`${state.alerts.length} événement(s) dans l'historique.`:'Aucune nouvelle alerte.');
$('autoBtn').onclick=()=>{state.auto=!state.auto;$('autoBtn').classList.toggle('on',state.auto);$('autoBtn').firstChild.textContent=state.auto?'AUTO ':'MANUEL ';$('modeLabel').textContent=state.auto?'AUTOMATIQUE':'MANUEL';};
$('autoSetting').onclick=()=>{$('autoBtn').click();$('autoSetting').classList.toggle('on',state.auto);$('autoSetting').firstChild.textContent=state.auto?'AUTO ':'OFF ';};
$('notifSetting').onclick=()=>{$('notifSetting').classList.toggle('on');$('notifSetting').firstChild.textContent=$('notifSetting').classList.contains('on')?'ON ':'OFF ';};
$('saveSettings').onclick=()=>{state.phMin=parseFloat($('phMin').value);state.phMax=parseFloat($('phMax').value);addEvent('⚙️','Seuils mis à jour',`pH ${state.phMin} — ${state.phMax}`);updateStatus();alert('Configuration enregistrée.');};
$('clearAlerts').onclick=()=>{state.alerts=[];renderAlerts()};
$('exportBtn').onclick=()=>{let csv='Date,pH,Temperature,Niveau,Batterie,Etat\n'+state.history.map(h=>`${h.time},${h.ph},${h.temp},${Math.round(h.level)},${Math.round(h.battery)},${h.status}`).join('\n');let a=document.createElement('a');a.href=URL.createObjectURL(new Blob([csv],{type:'text/csv'}));a.download='ranomadio-solar-historique.csv';a.click();};
$('year').textContent=new Date().getFullYear();
addEvent('🟢','Système initialisé','ESP32 prêt — mode automatique');
for(let i=0;i<12;i++){state.history.push({time:`--:${String(i*2).padStart(2,'0')}`,ph:7+(Math.random()-.5)*.5,temp:25+(Math.random()-.5)*2,level:78+i*.2,battery:89,status:'Propre'})}
renderEvents();renderAlerts();renderHistory();renderMetrics();setInterval(simulate,4000);
