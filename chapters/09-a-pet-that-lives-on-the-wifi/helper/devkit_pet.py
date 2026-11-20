#!/usr/bin/env python3
"""The pet, on this computer — devkit-diary, chapter 9.

A small always-on-top window holding the pet — a puppy, kitten, bunny, fox or panda cub — drawn
by the same pets.js the phone page uses, in a transparent web view, so it looks exactly the same. The pet itself lives on the ESP32: this helper checks in about once a
second, and the board says whether the pet is here, how it is feeling, and when it has just
walked in from another computer. When it walks off the edge of this screen, the helper tells
the board, and the pet turns up on the next computer.

Installed and started by one command from the board (http://pet.local); after that it starts by
itself at login. Needs Python 3.9+ and PyQt6. Click the pet to stroke it, drag it to move it,
right-click for more.

Needs PyQt6 and PyQt6-WebEngine. Without the web view it still works, showing the pet as an emoji.
"""
import json
import math
import os
import queue
import random
import socket
import sys
import threading
import time
import urllib.parse
import urllib.request

from PyQt6.QtCore import QPoint, QRectF, Qt, QTimer, QUrl
from PyQt6.QtGui import QAction, QCursor, QDesktopServices, QFont, QPainter
from PyQt6.QtWidgets import QApplication, QMenu, QWidget

try:                                 # the web view that draws the pet; must load before the QApplication
    if os.environ.get("DEVKIT_PET_RENDER") == "none":
        raise ImportError
    from PyQt6.QtWebEngineCore import QWebEnginePage
    from PyQt6.QtWebEngineWidgets import QWebEngineView
    HAVE_WEB = True
except ImportError:
    HAVE_WEB = False

HOME = os.path.join(os.environ.get("LOCALAPPDATA") or os.path.expanduser("~"),
                    "devkit-pet" if os.name == "nt" else ".devkit-pet")
SIZE = 120                      # how tall the pet stands on screen, px — the phone page can change it
WALK = 0.9                      # walking speed, in pet-heights per second
RUN = 3.0                       # when it's been called away
# how long it stays put on a computer before travelling to another (seconds, min,max)
ROAM = tuple(float(v) for v in os.environ.get("DEVKIT_PET_ROAM", "180,600").split(","))
KINDS = ["kitten", "puppy", "bunny", "fox", "panda"]     # the board's kind numbers, in order
FOOD = {"kitten": "🐟", "puppy": "🦴", "bunny": "🥕", "fox": "🍗", "panda": "🎋"}
EMOJI = {"kitten": "🐱", "puppy": "🐶", "bunny": "🐰", "fox": "🦊", "panda": "🐼"}

# ── the pets: pets.js, shared with the phone page (put here by embed.py) ─────────────────────────────
PETS_JS = r'''// The five pets — drawn and animated on a canvas. devkit-diary, chapter 9.
//
// One file, used twice: the phone page draws the pet with it, and the desktop helper shows it in a
// transparent always-on-top web view. So the pet on a computer screen and the pet on the phone are
// the same drawing, down to the pixel.
//
// The API a host uses:
//   const S = petState("puppy")        a pet's animation state
//   petStep(S, dt)                     advance it by dt seconds (blinking, breathing, wagging, walking)
//   S.mode = "sit" | "walk" | "zoom" | "sleep"
//   petPet(S), petFeed(S)              a stroke (hearts, a hop) or a bite (its food, chewing)
//   petDraw(canvas, S, cx, groundY, size, dpr)   draw it, bottom-centre at (cx, groundY), size px tall
"use strict";
const TAU=Math.PI*2;
// ───────────────────────── drawing helpers ─────────────────────────
function shade(g,x,y,r,c){const gr=g.createRadialGradient(x-r*0.35,y-r*0.45,r*0.08,x,y,r*1.25);
  gr.addColorStop(0,c[0]);gr.addColorStop(0.55,c[1]);gr.addColorStop(1,c[2]);return gr}
function path(g,fn){g.beginPath();fn(g);g.closePath()}
function fillPath(g,fn,fill){path(g,fn);g.fillStyle=fill;g.fill()}
function rim(g,fn,w,a){g.save();path(g,fn);g.clip();g.lineWidth=w;g.strokeStyle="rgba(255,255,255,"+a+")";
  g.translate(-w*0.35,-w*0.6);path(g,fn);g.stroke();g.restore()}
const zoom=g=>{const t=g.getTransform();return Math.hypot(t.a,t.b)||1};
function inner(g,fn,w,c){g.save();path(g,fn);g.clip();g.lineWidth=w;g.strokeStyle=c;g.filter="blur("+(w*0.5*zoom(g))+"px)";
  g.translate(w*0.2,w*0.55);path(g,fn);g.stroke();g.restore()}
function tufts(g,x0,y0,x1,y1,n,len,fill,dir){g.fillStyle=fill;const ang=Math.atan2(dir[1],dir[0]);
  for(let i=0;i<n;i++){const t=(i+0.5)/n,x=x0+(x1-x0)*t,y=y0+(y1-y0)*t,w=Math.hypot(x1-x0,y1-y0)/n,l=len*(0.75+0.35*((i*7)%3)/2);
    g.beginPath();g.ellipse(x+dir[0]*l*0.35,y+dir[1]*l*0.35,l*0.62,w*0.62,ang,0,TAU);g.fill()}}
function strands(g,fn,cx,cy,rx,ry,n,col,seed){g.save();path(g,fn);g.clip();g.strokeStyle=col;g.lineCap="round";
  let r=seed||1;const rnd=()=>((r=(r*16807)%2147483647)/2147483647);
  for(let i=0;i<n;i++){const a=rnd()*TAU,d=Math.sqrt(rnd()),x=cx+Math.cos(a)*rx*d,y=cy+Math.sin(a)*ry*d,l=3+rnd()*4;
    g.lineWidth=0.7+rnd()*0.6;g.globalAlpha=0.12+rnd()*0.16;g.beginPath();g.moveTo(x,y);
    g.quadraticCurveTo(x+(rnd()-0.5)*4,y+l*0.5,x+(rnd()-0.5)*5,y+l);g.stroke()}g.restore()}
function blob(g,x,y,rx,ry,a){g.save();g.filter="blur("+Math.max(rx,ry)*0.35*zoom(g)+"px)";g.fillStyle="rgba(0,0,0,"+a+")";
  g.beginPath();g.ellipse(x,y,rx,ry,0,0,TAU);g.fill();g.restore()}
function blush(g,pts,r){for(const [x,y] of pts){const b=g.createRadialGradient(x,y,0,x,y,r);
  b.addColorStop(0,"rgba(255,135,155,.42)");b.addColorStop(1,"rgba(255,135,155,0)");g.fillStyle=b;g.beginPath();g.ellipse(x,y,r*1.05,r*0.75,0,0,TAU);g.fill()}}
// a tapered tube along a cubic curve (tails)
function tube(g,p,w0,w1,fill,bulge){
  const N=24,L=[],R=[];
  for(let i=0;i<=N;i++){const t=i/N,mt=1-t;
    const x=mt*mt*mt*p[0]+3*mt*mt*t*p[2]+3*mt*t*t*p[4]+t*t*t*p[6],y=mt*mt*mt*p[1]+3*mt*mt*t*p[3]+3*mt*t*t*p[5]+t*t*t*p[7];
    const dx=3*mt*mt*(p[2]-p[0])+6*mt*t*(p[4]-p[2])+3*t*t*(p[6]-p[4]),dy=3*mt*mt*(p[3]-p[1])+6*mt*t*(p[5]-p[3])+3*t*t*(p[7]-p[5]);
    const l=Math.hypot(dx,dy)||1,w=(w0+(w1-w0)*t+(bulge||0)*Math.sin(Math.PI*t))/2;
    L.push([x-dy/l*w,y+dx/l*w]);R.push([x+dy/l*w,y-dx/l*w])}
  const fn=g=>{g.moveTo(L[0][0],L[0][1]);for(const q of L)g.lineTo(q[0],q[1]);
    const e=L[N],f=R[N];g.quadraticCurveTo((e[0]+f[0])/2+(e[0]-L[N-1][0])*2,(e[1]+f[1])/2+(e[1]-L[N-1][1])*2,f[0],f[1]);
    for(let i=N;i>=0;i--)g.lineTo(R[i][0],R[i][1])};
  fillPath(g,fn,fill);return fn}
const spring=(t,f,d)=>Math.exp(-d*t)*Math.sin(f*t);

// ───────────────────────── shared body parts ─────────────────────────
function paw(g,x,y,w,col){                // a round little paw, the toes marked
  const p=g=>g.ellipse(x,y-w*0.36,w*0.66,w*0.4,0,0,TAU);
  fillPath(g,p,shade(g,x,y-w*0.42,w*0.72,col));
  g.strokeStyle="rgba(95,88,100,.4)";g.lineWidth=1.2;g.lineCap="round";g.beginPath();
  for(const dx of[-0.2,0.08,0.36]){g.moveTo(x+dx*w,y-w*0.1);g.lineTo(x+dx*w,y-w*0.32)}g.stroke()}
function leg(g,x,y,len,w,ang,col,sock){    // a chubby baby leg: full at the top, a soft wrist, a round paw
  g.save();g.translate(x,y);g.rotate(ang);
  const l=g=>{g.moveTo(-w*0.6,0);g.bezierCurveTo(-w*0.64,len*0.35,-w*0.44,len*0.6,-w*0.42,len*0.84);g.quadraticCurveTo(-w*0.42,len+2,0,len+2);
    g.quadraticCurveTo(w*0.42,len+2,w*0.42,len*0.84);g.bezierCurveTo(w*0.44,len*0.6,w*0.64,len*0.35,w*0.6,0)};
  fillPath(g,l,shade(g,-w*0.15,len*0.3,len*0.8,col));
  if(sock){g.save();path(g,l);g.clip();fillPath(g,g=>g.rect(-w,len*0.6,2*w,len),shade(g,0,len*0.85,w,sock));g.restore()}
  paw(g,w*0.06,len+4,w*1.3,sock||col);g.restore()}
function leg2(g,x,y,len,w,ang,bend,col,sock){   // a walking leg: swings at the top, bends at the knee, still chubby
  g.save();g.translate(x,y);g.rotate(ang);
  const up=g=>{g.moveTo(-w*0.62,0);g.bezierCurveTo(-w*0.66,len*0.25,-w*0.52,len*0.45,-w*0.46,len*0.55);g.lineTo(w*0.46,len*0.55);g.bezierCurveTo(w*0.52,len*0.45,w*0.66,len*0.25,w*0.62,0)};
  fillPath(g,up,shade(g,-w*0.15,len*0.2,len*0.55,col));g.translate(0,len*0.52);g.rotate(bend);
  const lo=g=>{g.moveTo(-w*0.46,0);g.bezierCurveTo(-w*0.48,len*0.2,-w*0.42,len*0.36,-w*0.42,len*0.46);g.lineTo(w*0.42,len*0.46);g.bezierCurveTo(w*0.42,len*0.36,w*0.48,len*0.2,w*0.46,0)};
  fillPath(g,g=>g.ellipse(0,0,w*0.5,w*0.44,0,0,TAU),shade(g,0,0,w*0.6,col));
  fillPath(g,lo,shade(g,0,len*0.25,len*0.45,sock||col));paw(g,w*0.06,len*0.48+3,w*1.3,sock||col);g.restore()}

// an eye, looking at you. e = {iris:[light,mid,dark], slit:bool, lid:colour of the closed-eye line}
function eye(g,x,y,s,st,e,u){
  u=u||1;g.save();g.translate(x,y);g.scale(s*u,u);
  if(st.happy>0.5||st.blink>0.85){g.lineWidth=3.4;g.strokeStyle=e.lid;g.lineCap="round";g.beginPath();
    if(st.happy>0.5){g.moveTo(-11,4);g.quadraticCurveTo(0,-8,11,4)}else{g.moveTo(-11,0);g.quadraticCurveTo(0,6,11,0)}g.stroke();g.restore();return}
  g.save();g.scale(1,Math.max(0.08,1-st.blink));
  fillPath(g,g=>g.ellipse(0,0,15,17,0,0,TAU),"#060608");
  const iris=g=>g.ellipse(0.5,1,12.8,14.8,0,0,TAU);
  fillPath(g,iris,(()=>{const gr=g.createRadialGradient(0,7,1,0,2,15);gr.addColorStop(0,e.iris[0]);gr.addColorStop(0.4,e.iris[1]);
    gr.addColorStop(0.78,e.iris[2]);gr.addColorStop(1,"#0d0805");return gr})());
  g.save();path(g,iris);g.clip();g.strokeStyle="rgba(255,240,200,.22)";g.lineWidth=0.8;
  for(let i=0;i<14;i++){const a=i/14*TAU;g.beginPath();g.moveTo(1+Math.cos(a)*7,2+Math.sin(a)*8);g.lineTo(1+Math.cos(a)*12,2+Math.sin(a)*13.5);g.stroke()}
  const lid=g.createLinearGradient(0,-16,0,-2);lid.addColorStop(0,"rgba(0,0,0,.6)");lid.addColorStop(1,"rgba(0,0,0,0)");g.fillStyle=lid;g.fillRect(-16,-17,32,16);g.restore();
  const wide=st.happy>0.2||st.tongue>0.5;                     // slit pupils go round when it's excited
  fillPath(g,g=>g.ellipse(1,2.5,e.slit&&!wide?3.2:8.2,9.2,0,0,TAU),"#040405");
  fillPath(g,g=>g.ellipse(-4.2,-5.8,5.4,6.2,-0.45,0,TAU),"rgba(255,255,255,.97)");
  fillPath(g,g=>g.ellipse(5,6.5,2.3,2.3,0,0,TAU),"rgba(255,255,255,.9)");
  fillPath(g,g=>g.ellipse(2,10.5,5.5,2,0,0,TAU),"rgba(255,230,180,.2)");
  g.restore();
  g.lineWidth=1.3;g.strokeStyle="rgba(255,255,255,.14)";g.beginPath();g.ellipse(0.5,2,15.5,17.5,0,0.35*Math.PI,0.65*Math.PI);g.stroke();
  g.restore();
}
function glossyNose(g,fn,cx,cy,r,col){fillPath(g,fn,shade(g,cx,cy,r,col));
  fillPath(g,g=>g.ellipse(cx-r*0.32,cy-r*0.32,r*0.32,r*0.17,-0.3,0,TAU),"rgba(255,255,255,.7)")}
const W3=c=>[c[0],c[1],c[2]];

// ───────────────────────── the five pets ─────────────────────────
const P={};
const BLACK=["#3a3c46","#1d1e24","#0b0b0e"],BLACKF=["#24252c","#111216","#050506"],WHITE=["#ffffff","#eef0f5","#d4d9e4"],WHITEF=["#eef0f5","#d8dce6","#b9bfcc"];

// ── border collie puppy ──
// ears at the sides of the head, folding down — all black, as a real puppy's are
function earDrop(g,near){
  const e=g=>{g.moveTo(-11,0);g.bezierCurveTo(-9,-9,6,-13,15,-8);g.bezierCurveTo(27,-2,32,14,28,30);g.bezierCurveTo(25,42,12,44,6,36);g.bezierCurveTo(0,28,-7,14,-11,0)};
  fillPath(g,e,shade(g,10,10,30,near?BLACK:BLACKF));
  fillPath(g,g=>{g.moveTo(-2,2);g.bezierCurveTo(2,-2,10,-2,14,4);g.bezierCurveTo(18,12,16,22,12,26);g.bezierCurveTo(8,18,2,10,-2,2)},"rgba(0,0,0,.35)");
  if(near)rim(g,e,3,0.22);tufts(g,6,38,22,34,3,6,near?"#221f20":"#151213",[0,1])}
// the puppy exactly as it was after the 'enhance it more' pass: round face, deep eyes, folded ears
const PC={
  black:"#1d1e24", blackHi:"#3a3c46", blackLo:"#0b0b0e",
  white:"#ffffff", whiteLo:"#d9dde7", whiteMid:"#eef0f5",
  pink:"#f39aa8", pinkLo:"#d96f82", tongue:"#ff8597", tongueLo:"#e0566d",
  iris:"#5b3a22", irisLo:"#24160c", nose:"#15151a",
};
function pupEar(g,near){
  const base=g=>{g.moveTo(-15,8);g.bezierCurveTo(-17,-8,-10,-26,2,-32);g.bezierCurveTo(10,-36,18,-34,20,-28);
    g.bezierCurveTo(22,-20,18,-6,15,8)};
  fillPath(g,base,shade(g,2,-14,30,[near?PC.blackHi:PC.black,PC.black,PC.blackLo]));
  fillPath(g,g=>{g.moveTo(-8,6);g.bezierCurveTo(-9,-6,-5,-16,1,-20);g.bezierCurveTo(5,-20,7,-12,6,-2);g.bezierCurveTo(5,2,3,5,2,7)},
    shade(g,0,-6,16,["#b56b7c","#8f4c5c","#5e2f3a"]));
  // the folded flap
  const flap=g=>{g.moveTo(-1,-31);g.bezierCurveTo(8,-38,20,-34,21,-27);g.bezierCurveTo(22,-18,18,-10,12,-6);g.bezierCurveTo(10,-14,6,-24,-1,-31)};
  fillPath(g,flap,shade(g,12,-24,16,[PC.blackHi,PC.black,PC.blackLo]));
  if(near)rim(g,base,3,0.2);
  g.lineWidth=1.2;g.strokeStyle="rgba(0,0,0,.45)";g.beginPath();g.moveTo(-1,-31);g.bezierCurveTo(6,-24,10,-14,12,-6);g.stroke();
}

function pupHead(g,st){
  // far ear (behind): short, soft, the tip folded forward
  g.save();g.translate(-22,-54);g.rotate(-0.12+st.earL);pupEar(g,false);g.restore();
  // skull
  const skull=g=>{g.moveTo(-60,-4);g.bezierCurveTo(-62,-46,-30,-64,2,-64);g.bezierCurveTo(38,-64,62,-44,64,-8);
    g.bezierCurveTo(66,22,54,42,34,50);g.bezierCurveTo(14,58,-16,58,-34,50);g.bezierCurveTo(-52,42,-59,22,-60,-4)};
  fillPath(g,skull,shade(g,0,-10,70,[PC.blackHi,PC.black,PC.blackLo]));
  strands(g,skull,0,-24,56,36,70,"#7a7e8c",7);
  rim(g,skull,5,0.26);
  // white blaze, cheeks and chin
  const blaze=g=>{g.moveTo(-1,-63);g.bezierCurveTo(7,-63,9,-42,11,-26);g.bezierCurveTo(13,-12,26,-4,42,4);g.bezierCurveTo(60,14,62,36,42,48);
    g.bezierCurveTo(22,58,-18,58,-36,48);g.bezierCurveTo(-54,38,-52,14,-36,6);g.bezierCurveTo(-22,-2,-10,-12,-8,-26);g.bezierCurveTo(-6,-42,-9,-63,-1,-63)};
  fillPath(g,blaze,shade(g,6,10,64,[PC.white,PC.whiteMid,PC.whiteLo]));
  inner(g,blaze,8,"rgba(120,130,160,.35)");
  strands(g,blaze,4,30,46,18,26,"#c9cfdb",3);
  // a little blush on the cheeks
  for(const [bx,by] of [[-30,24],[44,20]]){const bg=g.createRadialGradient(bx,by,0,bx,by,13);
    bg.addColorStop(0,"rgba(255,140,160,.42)");bg.addColorStop(1,"rgba(255,140,160,0)");g.fillStyle=bg;g.beginPath();g.ellipse(bx,by,14,10,0,0,TAU);g.fill()}
  tufts(g,-40,44,-28,52,3,7,PC.whiteMid,[-0.6,0.7]);tufts(g,30,52,44,44,3,7,PC.whiteMid,[0.6,0.7]);
  // muzzle
  const muz=g=>{g.ellipse(16,24,27,19,0,0,TAU)};
  fillPath(g,muz,shade(g,16,24,30,[PC.white,PC.whiteMid,"#cfd4df"]));
  // mouth (chew moves the jaw)
  const jaw=st.chew*3;
  g.lineCap="round";g.lineWidth=2.6;g.strokeStyle="#2a2228";
  g.beginPath();g.moveTo(16,22);g.lineTo(16,28+jaw);g.moveTo(16,28+jaw);g.quadraticCurveTo(10,35+jaw,2,32+jaw*0.5);
  g.moveTo(16,28+jaw);g.quadraticCurveTo(22,35+jaw,31,31+jaw*0.5);g.stroke();
  if(st.tongue>0.01){                    // a happy tongue, out a little
    const L=10*st.tongue;
    fillPath(g,g=>{g.moveTo(9,31+jaw);g.bezierCurveTo(9,34+L,23,34+L,23,31+jaw);g.bezierCurveTo(20,29+jaw,12,29+jaw,9,31+jaw)},
      shade(g,16,33+L/2,12,[PC.tongue,PC.tongue,PC.tongueLo]));
    g.strokeStyle="rgba(170,40,70,.5)";g.lineWidth=1.2;g.beginPath();g.moveTo(16,31+jaw);g.lineTo(16,30+jaw+L*0.8);g.stroke();
  }
  // nose
  const nose=g=>{g.moveTo(3,10);g.bezierCurveTo(3,1,29,1,29,10);g.bezierCurveTo(29,18,21,23,16,23);g.bezierCurveTo(11,23,3,18,3,10)};
  fillPath(g,nose,shade(g,16,12,16,["#4a4a55",PC.nose,"#000"]));
  fillPath(g,g=>{g.ellipse(11,7,4.6,2.4,-0.3,0,TAU)},"rgba(255,255,255,.75)");
  fillPath(g,g=>{g.ellipse(10.5,15,2.4,1.5,0.5,0,TAU)},"#000");              // nostrils
  fillPath(g,g=>{g.ellipse(21.5,15,2.4,1.5,-0.5,0,TAU)},"#000");
  // eyes
  for(const [ex,s] of [[-16,0.92],[26,1]]){
    g.save();g.translate(ex,-12);g.scale(s,1);
    if(st.happy>0.5||st.blink>0.85){            // closed: a soft smiling arc
      g.lineWidth=3.6;g.strokeStyle="#0b0b0e";g.lineCap="round";g.beginPath();
      if(st.happy>0.5){g.moveTo(-11,4);g.quadraticCurveTo(0,-8,11,4)}else{g.moveTo(-11,0);g.quadraticCurveTo(0,6,11,0)}
      g.stroke();
    }else{
      const open=1-st.blink;
      g.save();g.scale(1,Math.max(0.08,open));
      fillPath(g,g=>g.ellipse(0,0,15,17,0,0,TAU),"#060608");                         // socket
      const iris=g=>g.ellipse(0.5,1,12.8,14.8,0,0,TAU);
      fillPath(g,iris,(()=>{const gr=g.createRadialGradient(0,7,1,0,2,15);
        gr.addColorStop(0,"#c28a52");gr.addColorStop(0.35,"#8c5a30");gr.addColorStop(0.75,PC.iris);gr.addColorStop(1,"#140b05");return gr})());
      g.save();path(g,iris);g.clip();                                                 // iris fibres
      g.strokeStyle="rgba(230,170,110,.28)";g.lineWidth=0.8;
      for(let i=0;i<14;i++){const a=i/14*TAU;g.beginPath();g.moveTo(1+Math.cos(a)*7,2+Math.sin(a)*8);g.lineTo(1+Math.cos(a)*12,2+Math.sin(a)*13.5);g.stroke()}
      const lid=g.createLinearGradient(0,-16,0,-2);lid.addColorStop(0,"rgba(0,0,0,.65)");lid.addColorStop(1,"rgba(0,0,0,0)");
      g.fillStyle=lid;g.fillRect(-16,-17,32,16);g.restore();                          // the upper lid's shadow
      fillPath(g,g=>g.ellipse(1,2.5,8.2,9.2,0,0,TAU),"#040405");                     // pupil
      fillPath(g,g=>g.ellipse(-4.2,-5.8,5.4,6.2,-0.45,0,TAU),"rgba(255,255,255,.97)"); // key light
      fillPath(g,g=>g.ellipse(5,6.5,2.3,2.3,0,0,TAU),"rgba(255,255,255,.9)");          // fill light
      fillPath(g,g=>g.ellipse(2,10.5,5.5,2,0,0,TAU),"rgba(255,210,160,.22)");         // bounce from below
      g.restore();
      g.lineWidth=1.4;g.strokeStyle="rgba(255,255,255,.14)";g.beginPath();            // lower lid
      g.ellipse(0.5,2,15.5,17.5,0,0.35*Math.PI,0.65*Math.PI);g.stroke();
      // a soft brow that lifts when it's curious
      const by=-21-st.brow*4;
      fillPath(g,g=>g.ellipse(-2,by,6.5,3,-0.15-st.brow*0.3,0,TAU),"rgba(120,124,140,.55)");
    }
    g.restore();
  }
  blob(g,30,-44,16,7,0.35);
  // near ear: the same, a little in front
  g.save();g.translate(22,-56);g.rotate(0.1+st.earR);pupEar(g,true);g.restore();

}


P.puppy={name:"Border collie puppy",food:"🦴",coat:BLACK,coatFar:BLACKF,sheen:"#6f7381",bib:WHITE,haunch:BLACK,
  fore:WHITE,foreFar:WHITEF,hind:BLACK,hindFar:BLACKF,hindSock:WHITE,paw:WHITE,
  head(g,S){pupHead(g,S)},
  tail:null,headSit:[30,-136],headWalk:[60,-132],headSleep:[50,-46]};
P.puppy.tail=function(g,pose,S){
  if(pose==="sleep"){fillPath(g,g=>{g.moveTo(-84,-6);g.bezierCurveTo(-60,10,40,12,74,-4);g.bezierCurveTo(80,-10,72,-18,64,-14);g.bezierCurveTo(30,-2,-40,-4,-74,-18)},shade(g,0,-6,70,BLACK));
    fillPath(g,g=>{g.moveTo(58,-12);g.bezierCurveTo(70,-20,84,-12,76,-2);g.bezierCurveTo(70,4,60,0,58,-12)},shade(g,68,-8,12,WHITE));return}
  const sit=pose==="sit";g.save();g.translate(sit?-48:-62,sit?-22:-90);g.rotate((sit?0.05:-0.75)+S.wag*(sit?0.8:1));const len=sit?64:58;
  const t=g=>{g.moveTo(2,-13);g.bezierCurveTo(-len*0.3,-30,-len*0.72,-42,-len*0.98,-38);g.bezierCurveTo(-len-14,-34,-len-16,-14,-len*0.92,-6);g.bezierCurveTo(-len*0.7,4,-len*0.32,13,2,13)};
  fillPath(g,t,shade(g,-len*0.5,-16,len*0.75,BLACK));tufts(g,-len*0.85,-38,-len*0.2,-22,5,9,"#221f20",[0.15,-1]);tufts(g,-len*0.85,-2,-len*0.2,10,4,8,"#221f20",[0,1]);rim(g,t,4,0.16);
  fillPath(g,g=>{g.moveTo(-len*0.8,-38);g.bezierCurveTo(-len-6,-46,-len-22,-30,-len-12,-10);g.bezierCurveTo(-len*0.96,-2,-len*0.82,-10,-len*0.8,-38)},shade(g,-len-4,-26,20,WHITE));
  tufts(g,-len-16,-34,-len-14,-12,3,8,"#f7f4ef",[-1,0]);g.restore()};

// ── a head in the puppy's style, shared by the kitten, the bunny and the fox ──
// o: coat, sheen, muzzle colours; ear(g,near); iris colours, slit; nose: "cat" | "fox"; mouth: "cat" | "bunny" | "fox";
//    mask(g) for markings; whiskers
function cuteHead(g,S,o){
  g.save();g.translate(o.earAt[0][0],o.earAt[0][1]);g.rotate(o.earAt[0][2]+S.earL);o.ear(g,false);g.restore();
  const skull=g=>{g.moveTo(-60,-4);g.bezierCurveTo(-62,-46,-30,-64,2,-64);g.bezierCurveTo(38,-64,62,-44,64,-8);
    g.bezierCurveTo(66,22,54,42,34,50);g.bezierCurveTo(14,58,-16,58,-34,50);g.bezierCurveTo(-52,42,-59,22,-60,-4)};
  if(o.ruff)tufts(g,-60,-8,-40,46,6,14,o.coat[1],[-1,0.4]),tufts(g,44,46,64,-8,6,14,o.coat[1],[1,0.4]);
  fillPath(g,skull,shade(g,0,-10,70,o.coat));strands(g,skull,0,-24,56,36,60,o.sheen,7);rim(g,skull,5,0.28);
  if(o.mask)o.mask(g);
  // muzzle: two soft puffs and a chin
  const muz=g=>{g.moveTo(-6,22);g.bezierCurveTo(-8,8,8,4,16,12);g.bezierCurveTo(24,4,42,8,40,22);g.bezierCurveTo(40,36,28,42,16,42);g.bezierCurveTo(4,42,-6,36,-6,22)};
  fillPath(g,muz,shade(g,16,24,30,o.muzzle));inner(g,muz,7,"rgba(120,110,130,.28)");
  blush(g,[[-32,22],[48,18]],12);
  // nose
  if(o.nose==="fox"){const n=g=>{g.moveTo(8,10);g.bezierCurveTo(8,3,24,3,24,10);g.bezierCurveTo(24,16,19,19,16,19);g.bezierCurveTo(13,19,8,16,8,10)};
    fillPath(g,n,shade(g,16,11,12,["#4a4a55","#15151a","#000"]));fillPath(g,g=>g.ellipse(12.5,7.5,3.2,1.8,-0.3,0,TAU),"rgba(255,255,255,.7)")}
  else{const tw=o.twitch?Math.sin(S.t*9)*0.8*(1-S.happy):0;
    fillPath(g,g=>{g.moveTo(9,12+tw);g.bezierCurveTo(11,7+tw,21,7+tw,23,12+tw);g.bezierCurveTo(21,17+tw,18,19+tw,16,19+tw);g.bezierCurveTo(14,19+tw,11,17+tw,9,12+tw)},
      shade(g,16,12,9,["#ffc0cb","#f093a5","#c4627a"]));fillPath(g,g=>g.ellipse(13.5,10.5,2.6,1.4,-0.3,0,TAU),"rgba(255,255,255,.6)")}
  // mouth
  const jaw=S.chew*2;g.lineCap="round";g.lineWidth=2.2;g.strokeStyle=o.line||"#5a4450";g.beginPath();
  g.moveTo(16,19);g.lineTo(16,24+jaw);g.quadraticCurveTo(11,30+jaw,6,27);g.moveTo(16,24+jaw);g.quadraticCurveTo(21,30+jaw,26,27);g.stroke();
  if(o.mouth==="bunny")fillPath(g,g=>{g.rect(12.5,24+jaw,7,5+jaw*0.5)},"#ffffff"),g.strokeStyle="rgba(150,140,140,.6)",g.lineWidth=0.8,g.beginPath(),g.moveTo(16,24+jaw),g.lineTo(16,29+jaw*1.5),g.stroke();
  if(S.tongue>0.3&&o.mouth!=="bunny")fillPath(g,g=>g.ellipse(16,28+jaw,4.2,3.6*S.tongue,0,0,Math.PI),shade(g,16,29,6,["#ff9aab","#ff8597","#e0566d"]));
  if(o.whiskers){g.strokeStyle="rgba(255,255,255,.88)";g.lineWidth=0.9;g.beginPath();
    for(const s of[-1,1])for(const k of[-1,0,1]){const x=s<0?-2:34;g.moveTo(x,24+k*3);g.quadraticCurveTo(x+s*18,22+k*5,x+s*34,20+k*8)}g.stroke()}
  // eyes and brows, as the puppy's
  const E={iris:o.iris,slit:o.slit,lid:o.lid||"#2a2328"};
  eye(g,-16,-12,0.92,S,E,o.eyeU);eye(g,26,-12,1,S,E,o.eyeU);
  if(o.brow!=="none")for(const [bx,f] of [[-18,0.9],[24,1]])fillPath(g,g=>g.ellipse(bx,-33-S.brow*4,6.2*f,2.8,-0.15-S.brow*0.3,0,TAU),o.brow||"rgba(255,255,255,.35)");
  g.save();g.translate(o.earAt[1][0],o.earAt[1][1]);g.rotate(o.earAt[1][2]+S.earR);o.ear(g,true);g.restore();
  if(o.top)o.top(g);
}

// ── kitten: white, fluffy, blue-eyed ──
const KW=["#ffffff","#f2f0f6","#cfcadb"],KWF=["#efedf3","#dad6e2","#b3aec0"];
function kitEar(g,near){
  const e=g=>{g.moveTo(-17,10);g.bezierCurveTo(-16,-10,-6,-30,4,-40);g.bezierCurveTo(12,-28,18,-10,18,10)};
  fillPath(g,e,shade(g,2,-12,30,near?KW:KWF));
  fillPath(g,g=>{g.moveTo(-9,8);g.bezierCurveTo(-8,-6,-2,-20,4,-28);g.bezierCurveTo(9,-18,11,-6,10,8)},shade(g,2,-8,18,["#ffc8d2","#f2a3b2","#cf7d8e"]));
  tufts(g,-4,6,8,6,4,12,"rgba(255,255,255,.95)",[0,-1]);if(near)rim(g,e,3,0.3)}
P.kitten={name:"Kitten",food:"🐟",coat:KW,coatFar:KWF,sheen:"#ffffff",bib:KW,haunch:KW,
  fore:KW,foreFar:KWF,hind:KW,hindFar:KWF,paw:["#ffffff","#f6eef2","#e3c9d2"],
  head(g,S){cuteHead(g,S,{coat:KW,sheen:"#ffffff",muzzle:["#ffffff","#f6f4f9","#dcd8e6"],ear:kitEar,
    earAt:[[-26,-54,-0.16],[24,-56,0.16]],iris:["#d6f0ff","#7fbcef","#2f72b4"],slit:true,nose:"cat",mouth:"cat",whiskers:true,ruff:true,
    lid:"#8b8697",line:"#a07f8c",brow:"rgba(205,200,215,.6)",
    top:g=>tufts(g,-16,-62,14,-64,4,9,"#ffffff",[0,-1])})},
  tail(g,pose,S){const w=S.wag*0.4;g.save();
    if(pose==="sleep"){tube(g,[-84,-12,-56,10,40,14,80,-6],18,22,shade(g,0,-4,80,KW),6);g.restore();return}
    if(pose==="sit"){g.translate(-48,-18);g.rotate(w*0.5);tube(g,[0,0,-30,-4,-54,-36,-40,-72],18,22,shade(g,-34,-36,50,KW),8);tufts(g,-50,-58,-34,-80,3,8,"#ffffff",[0,-1])}
    else{g.translate(-62,-96);g.rotate(-0.1+w);tube(g,[0,0,-20,-18,-24,-56,-4,-80],18,22,shade(g,-14,-40,50,KW),8);tufts(g,-8,-84,6,-72,3,8,"#ffffff",[0.4,-1])}
    g.restore()},
  headSit:[30,-134],headWalk:[62,-128],headSleep:[50,-46]};

// ── bunny: cinnamon and white, tall ears, a little hop ──
const BC=["#f0d2b0","#d4a473","#9c6d44"],BCF=["#d7b28c","#b98a5c","#81572f"];
function bunEar(g,near){
  const e=g=>{g.moveTo(-11,8);g.bezierCurveTo(-18,-24,-15,-62,-4,-80);g.bezierCurveTo(4,-90,15,-78,14,-58);g.bezierCurveTo(13,-32,11,-10,10,8)};
  fillPath(g,e,shade(g,0,-36,50,near?BC:BCF));
  fillPath(g,g=>{g.moveTo(-4,2);g.bezierCurveTo(-9,-24,-7,-56,0,-70);g.bezierCurveTo(5,-74,8,-60,7,-44);g.bezierCurveTo(6,-26,5,-8,4,2)},shade(g,2,-34,32,["#ffd0d8","#f4a5b4","#cf7c8e"]));
  if(near)rim(g,e,3,0.3)}
P.bunny={name:"Bunny",food:"🥕",coat:BC,coatFar:BCF,sheen:"#fff0e0",bib:WHITE,haunch:BC,
  fore:BC,foreFar:BCF,hind:BC,hindFar:BCF,paw:["#ffffff","#f4eee8","#dcd2c6"],hop:true,short:0.7,
  head(g,S){cuteHead(g,S,{coat:BC,sheen:"#fff0e0",muzzle:["#ffffff","#f8f4ef","#e2d8cc"],ear:bunEar,
    earAt:[[-14,-56,-0.1+Math.sin(S.t*1.7)*0.04],[12,-58,0.06+Math.sin(S.t*1.7+1)*0.04]],
    iris:["#c48a5a","#7a4a28","#3a2214"],nose:"cat",mouth:"bunny",whiskers:true,twitch:true,lid:"#6a4a38",line:"#8a6a5a",brow:"rgba(255,240,225,.45)",
    mask:g=>fillPath(g,g=>{g.moveTo(-2,-62);g.bezierCurveTo(6,-62,8,-44,10,-30);g.bezierCurveTo(11,-20,6,-12,0,-10);g.bezierCurveTo(-6,-14,-8,-24,-7,-34);g.bezierCurveTo(-6,-46,-9,-62,-2,-62)},"rgba(255,255,255,.55)")})},
  tail(g,pose){const p=pose==="sleep"?[-82,-26]:pose==="sit"?[-50,-26]:[-66,-82];
    fillPath(g,g=>g.ellipse(p[0],p[1],13,12,0,0,TAU),shade(g,p[0],p[1],14,WHITE));tufts(g,p[0]-12,p[1]-8,p[0]-10,p[1]+8,4,6,"#ffffff",[-1,0])},
  headSit:[28,-122],headWalk:[60,-116],headSleep:[50,-42]};

// ── fox kit: bright orange, a white mask, black-tipped ears ──
const FX=["#ffb36e","#f07e34","#b5541a"],FXF=["#e8954f","#cf6626","#8d3f12"],FD=["#4a3a33","#2b211d","#140f0d"];
function foxEar(g,near){
  const e=g=>{g.moveTo(-17,10);g.bezierCurveTo(-16,-14,-6,-40,4,-54);g.bezierCurveTo(13,-38,19,-14,18,10)};
  fillPath(g,e,shade(g,2,-16,34,near?FX:FXF));
  fillPath(g,g=>{g.moveTo(-9,8);g.bezierCurveTo(-8,-10,-2,-30,4,-40);g.bezierCurveTo(9,-28,11,-10,10,8)},shade(g,2,-12,22,["#fff6ec","#f1dfcb","#c9b39b"]));
  fillPath(g,g=>{g.moveTo(-6,-36);g.bezierCurveTo(-2,-46,2,-52,4,-54);g.bezierCurveTo(7,-48,10,-42,12,-34);g.bezierCurveTo(6,-36,0,-37,-6,-36)},"#241b18");
  tufts(g,-4,6,8,6,3,10,"rgba(255,255,255,.9)",[0,-1]);if(near)rim(g,e,3,0.28)}
P.fox={name:"Fox kit",food:"🍗",coat:FX,coatFar:FXF,sheen:"#ffdcb8",bib:WHITE,haunch:FX,
  fore:FX,foreFar:FXF,hind:FX,hindFar:FXF,foreSock:FD,hindSock:FD,paw:FD,
  head(g,S){cuteHead(g,S,{coat:FX,sheen:"#ffdcb8",muzzle:["#ffffff","#f8f4ef","#e0d6ca"],ear:foxEar,
    earAt:[[-26,-54,-0.18],[24,-56,0.18]],iris:["#ffe08a","#f0a32c","#9a5a12"],slit:true,nose:"fox",mouth:"fox",whiskers:false,ruff:true,
    lid:"#3a2a24",line:"#4a3028",brow:"rgba(255,230,200,.5)",
    mask:g=>{                                             // the white mask: cheeks, round under the eyes
      const m=g=>{g.moveTo(-58,6);g.bezierCurveTo(-46,0,-30,2,-16,10);g.bezierCurveTo(-4,4,8,4,16,6);g.bezierCurveTo(26,4,40,2,52,8);
        g.bezierCurveTo(60,12,62,22,56,32);g.bezierCurveTo(46,48,24,56,4,56);g.bezierCurveTo(-24,56,-48,44,-56,30);g.bezierCurveTo(-60,22,-61,12,-58,6)};
      fillPath(g,m,shade(g,0,30,60,["#ffffff","#f8f4ef","#e0d6ca"]));tufts(g,-56,20,-40,48,4,11,"#fbf8f4",[-1,0.4]);tufts(g,42,48,58,20,4,11,"#fbf8f4",[1,0.4])}})},
  tail(g,pose,S){const w=S.wag*0.5;g.save();
    if(pose==="sleep"){tube(g,[-84,-14,-50,10,40,14,86,-6],22,26,shade(g,0,-4,80,FX),8);fillPath(g,g=>g.ellipse(84,-8,14,11,-0.3,0,TAU),shade(g,84,-8,14,WHITE));g.restore();return}
    if(pose==="sit"){g.translate(-46,-14);g.rotate(w*0.5);tube(g,[0,0,10,14,70,22,104,10],22,26,shade(g,50,8,60,FX),10);
      fillPath(g,g=>g.ellipse(104,8,15,12,-0.2,0,TAU),shade(g,104,8,15,WHITE));tufts(g,112,0,116,16,3,7,"#f6f3ee",[1,0])}
    else{g.translate(-62,-88);g.rotate(0.15+w);tube(g,[0,0,-24,4,-50,14,-72,8],22,26,shade(g,-36,6,50,FX),12);
      fillPath(g,g=>g.ellipse(-74,8,15,12,0,0,TAU),shade(g,-74,8,15,WHITE));tufts(g,-86,0,-86,16,3,7,"#f6f3ee",[-1,0])}
    g.restore()},
  headSit:[30,-134],headWalk:[62,-130],headSleep:[50,-46]};

function pandaEar(g,near){              // round, black, set on top of the head
  fillPath(g,g=>g.ellipse(0,-6,15,14,0,0,TAU),shade(g,0,-8,16,near?PB:PBF));
  fillPath(g,g=>g.ellipse(1,-4,7.5,6.5,0,0,TAU),"rgba(80,70,80,.45)");if(near)rim(g,g=>g.ellipse(0,-6,15,14,0,0,TAU),3,0.18)}
// ── giant panda cub: creamy white, thick black legs, the band over the shoulders ──
const PW=["#fdfaf2","#eee7d8","#c9bea8"],PWF=["#ece5d6","#d6cdbb","#ada28c"],PB=["#3b393d","#1d1c20","#0a090b"],PBF=["#28272b","#141316","#060506"];
P.panda={name:"Panda cub",food:"🎋",coat:PW,coatFar:PWF,sheen:"#ffffff",bib:PW,haunch:PB,
  fore:PB,foreFar:PBF,hind:PB,hindFar:PBF,paw:PB,chub:1.12,legW:1.5,short:true,
  marks(g,pose){
    if(pose==="walk")fillPath(g,g=>{g.moveTo(8,-118);g.bezierCurveTo(34,-122,62,-110,70,-86);g.bezierCurveTo(74,-64,66,-46,56,-38);g.bezierCurveTo(40,-40,30,-60,26,-76);g.bezierCurveTo(22,-92,14,-106,8,-118)},shade(g,44,-84,44,PB));
    if(pose==="sit")fillPath(g,g=>{g.moveTo(-16,-104);g.bezierCurveTo(16,-124,60,-114,68,-82);g.bezierCurveTo(72,-60,62,-40,50,-30);g.bezierCurveTo(32,-52,6,-78,-16,-104)},shade(g,30,-84,46,PB));
    if(pose==="sleep")fillPath(g,g=>{g.moveTo(16,-88);g.bezierCurveTo(46,-90,78,-72,86,-44);g.bezierCurveTo(72,-32,50,-38,34,-50);g.bezierCurveTo(26,-62,20,-76,16,-88)},shade(g,52,-62,38,PB))},
  head(g,S){cuteHead(g,S,{coat:PW,sheen:"#ffffff",muzzle:["#ffffff","#f6f1e6","#dcd2bf"],ear:pandaEar,
    earAt:[[-28,-52,-0.2],[26,-54,0.2]],iris:["#7a6460","#33292a","#0e0a09"],eyeU:0.82,nose:"fox",mouth:"fox",
    lid:"#efe8da",line:"#2a2228",brow:"none",
    mask:g=>{                                  // eye patches: ovals tipped down and outward, as a real panda's
      for(const[x,y,a,s]of[[-19,-8,0.55,0.92],[29,-8,-0.55,1]]){g.save();g.translate(x,y);g.rotate(a);g.scale(s,1);
        fillPath(g,g=>{g.moveTo(0,-17);g.bezierCurveTo(12,-17,16,-4,14,8);g.bezierCurveTo(12,20,4,24,-2,22);g.bezierCurveTo(-12,20,-16,6,-14,-6);g.bezierCurveTo(-12,-14,-8,-17,0,-17)},shade(g,0,0,20,PB));g.restore()}}})},
  tail(g,pose){const p=pose==="sleep"?[-86,-30]:pose==="sit"?[-52,-30]:[-70,-84];fillPath(g,g=>g.ellipse(p[0],p[1],10,9,0,0,TAU),shade(g,p[0],p[1],11,PW))},
  headSit:[28,-134],headWalk:[62,-108],headSleep:[50,-46]};


// ───────────────────────── poses, shared by every pet ─────────────────────────
function headAt(g,sp,S,pos,tilt,st){g.save();g.translate(pos[0],pos[1]);g.rotate(tilt);sp.head(g,st||S);g.restore()}
function sit(g,S,sp){
  const br=Math.sin(S.t*2.2)*0.012;g.save();g.scale((1+br*0.4)*(sp.slim||1)*(sp.chub||1.08),(1-br)*1.02);
  sp.tail(g,"sit",S);
  fillPath(g,g=>g.ellipse(-4,-36,36,34,0,0,TAU),shade(g,-4,-36,38,sp.haunch===PB?PBF:sp.coatFar));
  const torso=g=>{g.moveTo(-56,-14);g.bezierCurveTo(-64,-70,-28,-116,16,-118);g.bezierCurveTo(54,-120,70,-88,68,-48);g.bezierCurveTo(66,-14,52,0,26,0);g.lineTo(-34,0);g.bezierCurveTo(-50,0,-56,-6,-56,-14)};
  fillPath(g,torso,shade(g,4,-60,80,sp.coat));strands(g,torso,0,-60,60,56,70,sp.sheen,11);rim(g,torso,6,0.22);
  if(sp.marks)sp.marks(g,"sit");
  if(sp.bib!==sp.coat){const bib=g=>{g.moveTo(0,-118);g.bezierCurveTo(36,-120,62,-96,62,-62);g.bezierCurveTo(62,-36,52,-20,40,-12);g.bezierCurveTo(26,-26,8,-44,-2,-66);g.bezierCurveTo(-8,-84,-10,-108,0,-118)};
    fillPath(g,bib,shade(g,30,-76,56,sp.bib));inner(g,bib,8,"rgba(120,130,160,.3)");tufts(g,-2,-66,38,-14,7,10,sp.bib[1],[-0.45,0.85])}
  blob(g,32,-104,40,12,0.4);
  const hh=g=>g.ellipse(-22,-32,36,32,-0.2,0,TAU);
  fillPath(g,hh,shade(g,-20,-32,34,sp.haunch));strands(g,hh,-22,-34,30,26,22,sp.haunch===PB?"#6f7381":sp.sheen,13);rim(g,hh,5,0.18);
  if(sp.marks&&sp===P.kitten){g.save();path(g,hh);g.clip();sp.marks(g,"sit");g.restore()}
  const LW=sp.legW||1.3,LL=sp.legLen||46;
  paw(g,4,1,24*LW,sp.hindSock||sp.paw);
  leg(g,16,-LL-4,LL,18*LW,0.07,sp.foreFar,sp.foreSock&&W3(sp.foreSock));
  leg(g,46,-LL-4,LL,19*LW,-0.05,sp.fore,sp.foreSock);
  if(sp.fluff)sp.fluff(g,"sit");
  g.restore();
  headAt(g,sp,S,[sp.headSit[0],sp.headSit[1]+Math.sin(S.t*2.2+0.6)*1.2],S.tilt);
}
function walk(g,S,sp){
  const ph=S.walk*TAU,hop=sp.hop,sw=hop?Math.sin(ph):Math.sin(ph);
  const bob=hop?-Math.max(0,Math.sin(ph))*16:Math.abs(Math.sin(ph))*-5;
  const lr=typeof sp.short==="number"?sp.short:(sp.short?0.72:0.82);
  g.save();g.translate(0,bob);if(hop)g.rotate(-Math.sin(ph)*0.06);g.scale((sp.slim||1)*(sp.chub||1.08),1.03);
  sp.tail(g,"walk",S);
  const a1=hop?sw*0.5:sw*0.42,a2=hop?-sw*0.6:-sw*0.42;
  g.save();g.translate(0,(1-lr)*60);
  const LW=sp.legW||1.3;
  leg2(g,-40,-66,56*lr,18*LW,a1,Math.max(0,-Math.cos(ph))*0.55,sp.hindFar,sp.hindSock&&WHITEF);
  leg2(g,40,-62,54*lr,16*LW,a2,Math.max(0,Math.cos(ph))*-0.5,sp.foreFar,sp.foreSock&&W3(sp.foreSock));
  g.restore();
  g.save();g.translate(0,(1-lr)*60);fillPath(g,g=>g.ellipse(-46,-74,26,30,0.25,0,TAU),shade(g,-46,-80,30,sp.haunch));g.restore();
  const body=g=>{g.moveTo(-68,-66);g.bezierCurveTo(-74,-104,-38,-118,4,-116);g.bezierCurveTo(44,-114,72,-104,72,-76);g.bezierCurveTo(72,-52,56,-42,24,-42);g.lineTo(-34,-42);g.bezierCurveTo(-58,-42,-66,-50,-68,-66)};
  g.save();g.translate(0,(1-lr)*60);
  fillPath(g,body,shade(g,-4,-92,84,sp.coat));strands(g,body,-6,-86,62,30,70,sp.sheen,17);rim(g,body,6,0.22);
  if(sp.marks)sp.marks(g,"walk");
  if(sp.bib!==sp.coat){const chest=g=>{g.moveTo(36,-112);g.bezierCurveTo(66,-110,78,-88,72,-66);g.bezierCurveTo(66,-48,48,-42,24,-42);g.bezierCurveTo(2,-42,-22,-44,-34,-46);g.bezierCurveTo(-8,-56,20,-72,36,-112)};
    fillPath(g,chest,shade(g,42,-78,50,sp.bib));tufts(g,-28,-45,46,-43,8,7,sp.bib[1],[0,1])}
  blob(g,60,-104,30,10,0.35);
  if(sp.fluff)sp.fluff(g,"walk");
  leg2(g,-48,-62,58*lr,20*LW,hop?a1:-sw*0.48,Math.max(0,Math.cos(ph))*0.6,sp.hind,sp.hindSock);
  leg2(g,48,-58,56*lr,18*LW,hop?a2:sw*0.48,Math.max(0,-Math.cos(ph))*-0.55,sp.fore,sp.foreSock);
  g.restore();
  g.restore();
  headAt(g,sp,S,[sp.headWalk[0],sp.headWalk[1]+(1-lr)*60+bob+Math.sin(ph*2)*1.5],S.tilt*0.5-0.05);
}
function sleep(g,S,sp){
  const br=Math.sin(S.t*1.4)*0.025;g.save();g.scale(1+br*0.3,1-br);
  const body=g=>{g.moveTo(-90,0);g.bezierCurveTo(-100,-56,-52,-88,2,-88);g.bezierCurveTo(54,-88,90,-64,92,-32);g.bezierCurveTo(94,-8,76,0,50,0);g.lineTo(-90,0)};
  fillPath(g,body,shade(g,-6,-46,90,sp.coat));strands(g,body,-6,-44,80,38,80,sp.sheen,19);
  if(sp.marks){g.save();path(g,body);g.clip();sp.marks(g,"sleep");g.restore()}
  rim(g,body,6,0.22);
  sp.tail(g,"sleep",S);if(sp.fluff)sp.fluff(g,"sleep");
  paw(g,40,2,30*(sp.legW||1),sp.paw);paw(g,64,2,28*(sp.legW||1),sp.paw);
  g.restore();
  g.save();g.translate(sp.headSleep[0],sp.headSleep[1]+Math.sin(S.t*1.4)*1.5);g.rotate(0.18);g.scale(0.92,0.92);
  sp.head(g,Object.assign({},S,{blink:1,happy:0,tongue:0,earL:-0.9,earR:1.0,brow:0}));g.restore();
}


// ───────────────────────── what a host uses ─────────────────────────
const PET_KINDS=["puppy","kitten","bunny","fox","panda"];
function petState(kind){return{kind:P[kind]?kind:"puppy",t:0,mode:"sit",face:1,wag:0,tilt:0,blink:0,happy:0,tongue:0,earL:0,earR:0,
  chew:0,walk:0,brow:0,nextBlink:2,nextTilt:4,petUntil:0,feedUntil:0,hop:0,fx:[],earKick:-9}}
function petStep(S,dt){
  S.t+=dt;
  if(S.t>S.nextBlink){S.blinkT=0;S.nextBlink=S.t+2.5+Math.random()*3.5}
  if(S.blinkT!=null){S.blinkT+=dt;const b=S.blinkT/0.16;S.blink=b<1?Math.sin(Math.PI*b):0;if(b>=1)S.blinkT=null}
  if(S.t>S.nextTilt&&S.mode==="sit"){S.tiltT=0;S.tiltTo=(Math.random()<0.5?-1:1)*0.16;S.nextTilt=S.t+5+Math.random()*5}
  if(S.tiltT!=null){S.tiltT+=dt;const k=S.tiltT/2.4;S.tilt=S.tiltTo*Math.sin(Math.PI*Math.min(1,k));if(k>=1){S.tiltT=null;S.tilt=0}}
  const petting=S.t<S.petUntil,eating=S.t<S.feedUntil,moving=S.mode==="walk"||S.mode==="zoom";
  S.happy+=((petting?1:0)-S.happy)*Math.min(1,dt*8);
  S.tongue+=(((petting||(moving&&S.kind==="puppy"))?1:0)-S.tongue)*Math.min(1,dt*5);
  S.chew=eating?Math.abs(Math.sin(S.t*14)):0;
  S.brow=Math.min(1,Math.abs(S.tilt)/0.16)*0.9+S.happy*0.5;
  const wagSpeed=petting?18:(S.mode==="zoom"?16:S.mode==="sleep"?0:6),wagAmp=petting?0.35:(S.mode==="sleep"?0:0.16);
  S.wag=Math.sin(S.t*wagSpeed)*wagAmp*(S.kind==="kitten"||S.kind==="fox"?0.5:1);
  S.earR=moving?Math.sin(S.walk*TAU*2)*0.08:spring(S.t-S.earKick,14,4)*0.25;S.earL=S.earR*0.7;
  if(moving)S.walk+=dt*(S.mode==="zoom"?3.4:(P[S.kind].hop?1.4:1.8));
  if(S.hop>0)S.hop=Math.max(0,S.hop-dt);
  S.fx=S.fx.filter(f=>(f.age+=dt)<1.6);
}
function petPet(S){if(S.mode==="sleep")S.mode="sit";S.petUntil=S.t+1.8;S.hop=0.55;S.earKick=S.t;
  S.fx.push({ch:"\u2764\ufe0f",dx:-0.12,age:0},{ch:"\u2764\ufe0f",dx:0.14,age:-0.25})}
function petFeed(S){if(S.mode==="sleep")S.mode="sit";S.feedUntil=S.t+1.8;S.fx.push({ch:P[S.kind].food,dx:0.3,age:0})}
// draw the pet, with a soft ground shadow and a faint light outline (so a dark pet shows on a dark desktop)
function petDraw(cv,S,cx,gy,size,dpr){
  const g=cv.getContext("2d"),W=cv.width/dpr,H=cv.height/dpr,k=size/260;
  const L=cv._layers||(cv._layers=[document.createElement("canvas"),document.createElement("canvas")]);
  for(const c of L)if(c.width!==cv.width||c.height!==cv.height){c.width=cv.width;c.height=cv.height}
  g.setTransform(dpr,0,0,dpr,0,0);g.clearRect(0,0,W,H);
  const hopY=S.hop>0?-Math.sin(Math.PI*(1-S.hop/0.55))*size*0.22:0;
  g.save();g.translate(cx,gy);g.scale(1,0.22);
  const sr=(S.mode==="sleep"?100:S.mode==="walk"||S.mode==="zoom"?80:62)*k*(1+hopY/size*0.8);
  const sg=g.createRadialGradient(0,0,0,0,0,sr);sg.addColorStop(0,"rgba(0,0,0,.34)");sg.addColorStop(1,"rgba(0,0,0,0)");
  g.fillStyle=sg;g.beginPath();g.arc(0,0,sr,0,TAU);g.fill();g.restore();
  const lc=L[0].getContext("2d");lc.setTransform(dpr,0,0,dpr,0,0);lc.clearRect(0,0,W,H);
  lc.save();lc.translate(cx,gy+hopY);lc.scale(k*S.face,k);
  if(S.hop>0){const q=Math.sin(Math.PI*(1-S.hop/0.55));lc.scale(1-q*0.06,1+q*0.08)}
  ({sit,walk,sleep,zoom:walk})[S.mode](lc,S,P[S.kind]);lc.restore();
  const oc=L[1].getContext("2d");oc.setTransform(1,0,0,1,0,0);oc.clearRect(0,0,cv.width,cv.height);oc.drawImage(L[0],0,0);
  oc.globalCompositeOperation="source-in";oc.fillStyle="rgba(235,242,255,.32)";oc.fillRect(0,0,cv.width,cv.height);oc.globalCompositeOperation="source-over";
  g.save();g.setTransform(1,0,0,1,0,0);const o=1.3*dpr;for(let a=0;a<8;a++)g.drawImage(L[1],Math.cos(a/8*TAU)*o,Math.sin(a/8*TAU)*o);g.drawImage(L[0],0,0);g.restore();
  g.setTransform(dpr,0,0,dpr,0,0);
  for(const f of S.fx){if(f.age<0)continue;g.save();g.globalAlpha=Math.max(0,1-f.age/1.6);g.font=Math.round(22*Math.max(0.7,k*1.4))+"px serif";g.textAlign="center";
    g.fillText(f.ch,cx+f.dx*size,gy-size*1.0-f.age*40);g.restore()}
  if(S.mode==="sleep"){g.save();g.fillStyle="rgba(235,240,250,.95)";g.strokeStyle="rgba(40,50,70,.5)";g.lineWidth=2;g.font="bold "+Math.round(16*Math.max(0.8,k*1.3))+"px sans-serif";
    const z=(S.t%2.6)/2.6,z2=(z+0.5)%1;
    g.globalAlpha=Math.sin(Math.PI*z);g.strokeText("z",cx+size*0.4*S.face+z*14,gy-size*0.55-z*26);g.fillText("z",cx+size*0.4*S.face+z*14,gy-size*0.55-z*26);
    g.globalAlpha=Math.sin(Math.PI*z2);g.strokeText("Z",cx+size*0.5*S.face+z2*16,gy-size*0.62-z2*30);g.fillText("Z",cx+size*0.5*S.face+z2*16,gy-size*0.62-z2*30);g.restore()}
}
'''

# ── talking to the board ─────────────────────────────────────────────────────────────────────


def _load(name, default=""):
    try:
        with open(os.path.join(HOME, name)) as f:
            return f.read().strip()
    except OSError:
        return default


def _ident():
    ident = _load("id")
    if len(ident) != 8:
        ident = "%08x" % random.getrandbits(32)
        os.makedirs(HOME, exist_ok=True)
        with open(os.path.join(HOME, "id"), "w") as f:
            f.write(ident)
    return ident


class Link(threading.Thread):
    """Beats to the board once a second in the background; the window only ever reads `state`."""

    def __init__(self):
        super().__init__(daemon=True)
        self.hosts = (_load("board") or "pet.local 192.168.4.1").split()
        self.host = None
        self.id = _ident()
        self.name = socket.gethostname().split(".")[0][:24] or "computer"
        self.state = None                    # the board's last answer
        self.fresh = 0.0                     # when it came
        self.posts = queue.Queue()
        self.paused_until = 0.0
        self.http = urllib.request.build_opener(urllib.request.ProxyHandler({}))   # never via a proxy

    def _call(self, path, params, post=False):
        order = ([self.host] if self.host else []) + [h for h in self.hosts if h != self.host]
        for host in order:
            try:
                q = urllib.parse.urlencode(params)
                url = "http://%s%s" % (host, path) + ("" if post else "?" + q)
                req = urllib.request.Request(url, data=q.encode() if post else None)
                with self.http.open(req, timeout=2.5) as r:
                    out = json.loads(r.read().decode())
                self.host = host
                return out
            except Exception:
                continue
        self.host = None
        return None

    def post(self, path, **params):
        params["id"] = self.id
        self.posts.put((path, params))

    def run(self):
        while True:
            t0 = time.monotonic()
            while not self.posts.empty():
                path, params = self.posts.get()
                self._call(path, params, post=True)
            if time.time() < self.paused_until:
                self.state = None
            else:
                lt = time.localtime()
                out = self._call("/beat", {"id": self.id, "n": self.name,
                                           "h": "%.2f" % (lt.tm_hour + lt.tm_min / 60)})
                if out and out.get("ok"):
                    self.state, self.fresh = out, time.monotonic()
            time.sleep(max(0.05, 1.0 - (time.monotonic() - t0)))

    def snapshot(self):
        if self.state and time.monotonic() - self.fresh < 12:
            return self.state
        return None


# ── the window ───────────────────────────────────────────────────────────────────────────────


HOST_HTML = """<!doctype html><html><head><meta charset="utf-8"><style>html,body{margin:0;background:transparent;overflow:hidden}
canvas{position:fixed;left:0;top:0;width:100%;height:100%;cursor:grab}</style></head><body><canvas id="c"></canvas>
<script>__PETS__</script><script>
let S=petState("puppy"),size=120,dpr=window.devicePixelRatio||1;
const cv=document.getElementById("c");
function fit(){dpr=window.devicePixelRatio||1;cv.width=Math.round(innerWidth*dpr);cv.height=Math.round(innerHeight*dpr)}
addEventListener("resize",fit);fit();
window.pet={
  set(kind,mode,face,sz){if(kind!==S.kind){S=petState(kind)}S.mode=mode;S.face=face;size=sz},
  pet(){petPet(S)},feed(){petFeed(S)},hop(){S.hop=0.45},say(ch){S.fx.push({ch:ch,dx:0.28,age:0})},
  play(){S.hop=0.55;setTimeout(()=>S.hop=0.55,620);setTimeout(()=>S.hop=0.55,1240)}};
let last=performance.now();
function frame(now){const dt=Math.min(0.05,(now-last)/1000);last=now;petStep(S,dt);
  petDraw(cv,S,innerWidth/2,innerHeight-6,size,dpr);requestAnimationFrame(frame)}
requestAnimationFrame(frame);
// the mouse goes back to Python, which moves the window
cv.addEventListener("pointerdown",e=>{if(e.button===0){cv.setPointerCapture(e.pointerId);console.log("PET:down "+e.screenX+" "+e.screenY)}});
cv.addEventListener("pointermove",e=>{if(e.buttons&1)console.log("PET:move "+e.screenX+" "+e.screenY)});
cv.addEventListener("pointerup",e=>{if(e.button===0)console.log("PET:up")});
cv.addEventListener("contextmenu",e=>{e.preventDefault();console.log("PET:menu")});
</script></body></html>"""

if HAVE_WEB:
    class _Bridge(QWebEnginePage):
        """Messages from the page's console: the mouse, as "PET:down x y" and so on."""
        def __init__(self, view, cb):
            super().__init__(view)
            self.cb = cb

        def javaScriptConsoleMessage(self, level, msg, line, source):
            if msg.startswith("PET:"):
                self.cb(msg[4:].split())


class Face:
    """What draws the pet. With the web view: pets.js, exactly as the phone page draws it."""

    def __init__(self, owner):
        self.owner, self.view, self.last = owner, None, None
        if not HAVE_WEB:
            return
        try:
            v = QWebEngineView(owner)
            v.setPage(_Bridge(v, owner.on_bridge))
            v.page().setBackgroundColor(Qt.GlobalColor.transparent)
            v.setStyleSheet("background: transparent")
            v.setContextMenuPolicy(Qt.ContextMenuPolicy.NoContextMenu)
            v.setHtml(HOST_HTML.replace("__PETS__", PETS_JS), QUrl("about:blank"))
            self.view = v
        except Exception:
            self.view = None

    def fit(self, w, h):
        if self.view:
            self.view.setGeometry(0, 0, w, h)

    def js(self, code):
        if self.view:
            self.view.page().runJavaScript("window.pet&&" + code)

    def show(self, kind, mode, face, size):
        state = (kind, mode, face, size)
        if state != self.last:                       # only tell the page when something changed
            self.last = state
            self.js("pet.set(%s,%s,%d,%d)" % (json.dumps(kind), json.dumps(mode), face, size))


class Pet(QWidget):
    def __init__(self, link):
        flags = (Qt.WindowType.FramelessWindowHint | Qt.WindowType.WindowStaysOnTopHint
                 | Qt.WindowType.Tool | Qt.WindowType.WindowDoesNotAcceptFocus
                 | Qt.WindowType.NoDropShadowWindowHint)
        super().__init__(None, flags)
        self.setAttribute(Qt.WidgetAttribute.WA_TranslucentBackground)
        self.setAttribute(Qt.WidgetAttribute.WA_ShowWithoutActivating)
        self.setAttribute(Qt.WidgetAttribute.WA_MacAlwaysShowToolWindow)   # don't vanish with focus
        self.link = link
        self.face = Face(self)
        self.resize_pet(SIZE)

        self.mode = "away"           # away enter stay exit flee drag sleep
        self.x = self.y = 0.0
        self.spot = self.load_spot()  # where it lives on this computer: (centre x, feet y), global px
        self.feat_at = 0.0           # when it next turns round or hops, while staying put
        self.facing = 1
        self.target = 0.0
        self.next_exit = 0.0
        self.exit_side = 1
        self.left_at = 0.0
        self.arr_seen = None
        self.ev_seen = None
        self.kind = "puppy"
        self.pet = {}
        self.bubble_at = 0.0         # when it last asked for food
        self.press = None
        self.drag_off = QPoint()
        self.last = time.monotonic()
        self.mac_done = False

        self.tick = QTimer(self)
        self.tick.timeout.connect(self.step)
        self.tick.start(33)

    # geometry ----------------------------------------------------------------------------
    def resize_pet(self, size):
        self.size = max(70, min(240, int(size)))
        # room for the widest pose (asleep, stretched out) and for hearts above its head
        self.w, self.h = int(self.size * 2.2), int(self.size * 1.65)
        self.setFixedSize(self.w, self.h)
        self.face.fit(self.w, self.h)

    def pose(self):
        """What pets.js should draw for what the pet is doing."""
        return {"sleep": "sleep", "enter": "walk", "exit": "walk", "drag": "walk", "flee": "zoom"}.get(self.mode, "sit")

    def place(self):
        self.move(int(self.x), int(self.y))

    def walk_to(self, x, mode):
        self.target, self.mode = x, mode
        self.facing = 1 if x > self.x else -1

    # its spot: where you last put it on this computer -------------------------------------
    def load_spot(self):
        try:
            cx, by = (float(v) for v in _load("spot").split())
            return cx, by
        except ValueError:
            g = QApplication.primaryScreen().availableGeometry()
            return g.left() + g.width() * 0.8, g.bottom() + 1.0

    def save_spot(self):
        try:
            with open(os.path.join(HOME, "spot"), "w") as f:
                f.write("%.0f %.0f" % self.spot)
        except OSError:
            pass

    def spot_xy(self):
        """The window position for the spot, kept on screen whatever the pet's size."""
        v = QApplication.primaryScreen().virtualGeometry()
        x = min(max(self.spot[0] - self.w / 2, v.left()), v.right() - self.w)
        y = min(max(self.spot[1] - self.h, v.top()), v.bottom() - self.h + 1)
        return x, y

    def spot_screen(self):
        """(left, right) of the screen its spot is on — the edges it walks in and out by."""
        scr = QApplication.screenAt(QPoint(int(self.spot[0]), int(self.spot[1]) - 5)) or QApplication.primaryScreen()
        g = scr.geometry()
        return g.left(), g.right()

    # the frame loop ----------------------------------------------------------------------
    def step(self):
        now = time.monotonic()
        dt, self.last = min(0.1, now - self.last), now
        s = self.link.snapshot()
        pet = (s or {}).get("pet")
        here = bool(s and pet and s.get("here"))

        if pet:
            k = pet.get("k", 0)
            self.pet, self.kind = pet, KINDS[k] if 0 <= k < len(KINDS) else "puppy"
            if pet.get("sz") and int(pet["sz"]) != self.size:
                self.resize_pet(pet["sz"])
                if self.mode in ("stay", "sleep"):
                    self.x, self.y = self.spot_xy()
            self.setToolTip("%s · %s" % (pet.get("n", ""), pet.get("mood", "")))

        if not here:
            if self.mode not in ("away", "flee", "drag"):
                self.leave_by_edge("flee")                    # called away, or the board went quiet
        else:
            arr = s.get("arr")
            if arr != self.arr_seen and self.mode != "drag":
                if self.arr_seen is not None or self.mode == "away":
                    self.arrive(s.get("side", 0))
                self.arr_seen = arr
            elif self.mode == "away" and now - self.left_at > 3:
                self.arrive(s.get("side", 0))                 # a leave that didn't land
            ev = s.get("ev")
            if ev != self.ev_seen:
                if self.ev_seen is not None and self.mode != "away":
                    self.react(s.get("evk", ""))
                self.ev_seen = ev
            asleep = bool(pet.get("sleep"))
            if asleep and self.mode == "stay":
                self.mode = "sleep"
            elif not asleep and self.mode == "sleep":
                self.mode = "stay"
            # now and then it travels to another computer — the only time it walks
            if self.mode == "stay" and now > self.next_exit:
                if s.get("others", 0) > 0 and not asleep:
                    self.leave_by_edge("exit")
                else:
                    self.next_exit = now + random.uniform(*ROAM)

        self.move_pet(dt, now)
        if self.mode != "away":
            self.place()
            self.face.show(self.kind, self.pose(), self.facing, self.size)
            if not self.isVisible():
                self.show()
                self.mac_float()
            if not self.face.view:
                self.update()

    def arrive(self, side):
        """In from the edge it came by, at the height of its spot, walking to the spot."""
        left, right = self.spot_screen()
        tx, ty = self.spot_xy()
        self.x, self.y = (left - self.w if side == 0 else right + 1), ty
        self.next_exit = time.monotonic() + random.uniform(*ROAM)
        self.walk_to(tx, "enter")

    def leave_by_edge(self, mode):
        """Off the nearer edge of its screen (sometimes the far one, when it's travelling)."""
        left, right = self.spot_screen()
        self.exit_side = 0 if self.x + self.w / 2 < (left + right) / 2 else 1
        if mode == "exit" and random.random() < 0.25:
            self.exit_side ^= 1
        self.walk_to(left - self.w - 5 if self.exit_side == 0 else right + 5, mode)

    def move_pet(self, dt, now):
        m = self.mode
        if m in ("enter", "exit", "flee"):
            speed = (RUN if m == "flee" else WALK) * self.size
            d = self.target - self.x
            if abs(d) <= speed * dt:
                self.x = self.target
                if m == "exit":
                    self.link.post("/leave", s=self.exit_side)
                    self.vanish()
                elif m == "flee":
                    self.vanish()
                else:                                        # home: settle on its spot
                    self.mode = "sleep" if self.pet.get("sleep") else "stay"
                    self.facing = -1 if self.x + self.w / 2 > sum(self.spot_screen()) / 2 else 1
                    self.feat_at = now + random.uniform(6, 14)
                    self.next_exit = now + random.uniform(*ROAM)   # time here counts from settling
            else:
                self.x += math.copysign(speed * dt, d)
                self.facing = 1 if d > 0 else -1
        elif m == "stay" and now > self.feat_at:
            # small things while it stays put: look the other way, or a little hop
            if random.random() < 0.6:
                self.facing = -self.facing
            else:
                self.face.js("pet.hop()")
            self.feat_at = now + random.uniform(12, 30)
        if m == "stay" and self.pet.get("mood") == "hungry" and now > self.bubble_at + 25:
            self.bubble_at = now
            self.face.js("pet.say(%s)" % json.dumps(FOOD.get(self.kind, "🍽")))

    def vanish(self):
        self.mode, self.left_at = "away", time.monotonic()
        self.hide()

    def react(self, kind):          # (not "event": that name is QWidget's own)
        if kind in ("pet", "poke", "adopt"):
            self.face.js("pet.pet()")
        elif kind == "feed":
            self.face.js("pet.feed()")
        elif kind == "play":
            self.face.js("pet.play()")

    # without the web view: the pet as an emoji ------------------------------------------
    def paintEvent(self, _):
        if self.face.view:
            return
        p = QPainter(self)
        p.setFont(QFont("Apple Color Emoji" if sys.platform == "darwin" else "Segoe UI Emoji", int(self.size * 0.55)))
        p.drawText(QRectF(0, 0, self.w, self.h - 4), Qt.AlignmentFlag.AlignHCenter | Qt.AlignmentFlag.AlignBottom,
                   EMOJI.get(self.kind, "🐾") + ("💤" if self.mode == "sleep" else ""))
        p.end()

    # the mouse: click to stroke, drag to put it somewhere new ---------------------------
    def press_at(self, gp):
        self.press = gp
        self.drag_off = gp - QPoint(int(self.x), int(self.y))

    def move_to(self, gp):
        if self.press is None or self.mode in ("away", "flee"):   # catch it walking in, or setting off
            return
        if self.mode != "drag" and (gp - self.press).manhattanLength() > 5:
            self.mode = "drag"
        if self.mode == "drag":
            self.x, self.y = gp.x() - self.drag_off.x(), gp.y() - self.drag_off.y()
            self.place()

    def release(self):
        if self.press is None:
            return
        self.press = None
        if self.mode == "drag":                       # dropped: this is its spot now, and it stays
            self.spot = (self.x + self.w / 2, self.y + self.h)
            self.save_spot()
            self.x, self.y = self.spot_xy()
            self.mode = "sleep" if self.pet.get("sleep") else "stay"
            self.next_exit = time.monotonic() + random.uniform(*ROAM)
        else:                                         # a click: a stroke
            self.link.post("/poke")
            self.face.js("pet.pet()")

    def on_bridge(self, msg):                         # the web view's mouse
        if msg[0] == "down" and len(msg) == 3:
            self.press_at(QPoint(int(float(msg[1])), int(float(msg[2]))))
        elif msg[0] == "move" and len(msg) == 3:
            self.move_to(QPoint(int(float(msg[1])), int(float(msg[2]))))
        elif msg[0] == "up":
            self.release()
        elif msg[0] == "menu":
            self.menu(QCursor.pos())

    def mousePressEvent(self, e):
        if e.button() == Qt.MouseButton.LeftButton:
            self.press_at(e.globalPosition().toPoint())

    def mouseMoveEvent(self, e):
        self.move_to(e.globalPosition().toPoint())

    def mouseReleaseEvent(self, e):
        if e.button() == Qt.MouseButton.LeftButton:
            self.release()

    def contextMenuEvent(self, e):
        self.menu(e.globalPos())

    def menu(self, at):
        name = self.pet.get("n") or "your pet"
        m = QMenu()
        page = QAction("Open %s's page" % name, m)
        page.triggered.connect(lambda: QDesktopServices.openUrl(
            QUrl("http://%s/" % (self.link.host or self.link.hosts[0]))))
        go = QAction("Send to another computer", m)
        go.setEnabled(bool((self.link.snapshot() or {}).get("others")) and self.mode == "stay")
        go.triggered.connect(lambda: self.leave_by_edge("exit"))
        rest = QAction("Pause here for 15 minutes", m)
        rest.triggered.connect(self.pause)
        bye = QAction("Quit", m)
        bye.triggered.connect(QApplication.quit)
        for a in (page, go, rest):
            m.addAction(a)
        m.addSeparator()
        m.addAction(bye)
        m.exec(at)

    def pause(self):
        self.link.paused_until = time.time() + 15 * 60   # the board moves the pet on in a few s

    # macOS: on every desktop, over full-screen apps, and no Dock icon ---------------------
    def mac_float(self):
        if QApplication.platformName() != "cocoa" or self.mac_done:
            return
        self.mac_done = True
        try:
            win = _msg(int(self.winId()), b"window")
            # canJoinAllSpaces | stationary | ignoresCycle | fullScreenAuxiliary
            _msg(win, b"setCollectionBehavior:", 1 | 16 | 64 | 256, argtypes=(ctypes.c_ulong,), restype=None)
        except Exception:
            pass


if sys.platform == "darwin":
    import ctypes
    import ctypes.util

    _objc = ctypes.cdll.LoadLibrary(ctypes.util.find_library("objc"))
    _objc.objc_getClass.restype = ctypes.c_void_p
    _objc.objc_getClass.argtypes = [ctypes.c_char_p]
    _objc.sel_registerName.restype = ctypes.c_void_p
    _objc.sel_registerName.argtypes = [ctypes.c_char_p]

    def _msg(obj, sel, *args, argtypes=(), restype=ctypes.c_void_p):
        f = _objc.objc_msgSend
        f.restype = restype
        f.argtypes = [ctypes.c_void_p, ctypes.c_void_p] + list(argtypes)
        return f(obj, _objc.sel_registerName(sel), *args)

    def _accessory():
        app = _msg(_objc.objc_getClass(b"NSApplication"), b"sharedApplication")
        _msg(app, b"setActivationPolicy:", 1, argtypes=(ctypes.c_long,), restype=ctypes.c_bool)


def _only_one():
    """A second copy (say, after reinstalling) just exits; returns the socket to keep it held."""
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
        s.bind(("127.0.0.1", int(os.environ.get("DEVKIT_PET_PORT", "47331"))))
        s.listen(1)
        return s
    except OSError:
        sys.exit(0)


def main():
    lock = _only_one()
    app = QApplication(sys.argv)
    app.setQuitOnLastWindowClosed(False)
    if QApplication.platformName() == "cocoa":
        try:
            _accessory()
        except Exception:
            pass
    link = Link()
    link.start()
    pet = Pet(link)
    app.exec()
    del lock, pet


if __name__ == "__main__":
    main()
