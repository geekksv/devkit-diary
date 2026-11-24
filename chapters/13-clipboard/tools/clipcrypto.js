// The page's side of helper/clipcrypto.py, with WebCrypto. The same bytes in, the same bytes out.
// (Copied into the page between the CRYPTO markers; tools/crypto_test.py checks this file and the
// page's copy are the same.)
// CRYPTO-BEGIN
const CLIP_V=1,CLIP_ITER=200000,CLIP_PREFIX=new TextEncoder().encode('devkit-clip/1');
const cat=(...a)=>{const n=a.reduce((s,x)=>s+x.length,0),o=new Uint8Array(n);let i=0;for(const x of a){o.set(x,i);i+=x.length;}return o;};
// The three keys as raw bytes, to keep on this phone (like the helper's config file), and back.
async function clipDeriveRaw(pass,salt,iter){
 const pw=new TextEncoder().encode(pass.normalize('NFC'));
 const base=await crypto.subtle.importKey('raw',pw,'PBKDF2',false,['deriveBits']);
 const master=new Uint8Array(await crypto.subtle.deriveBits({name:'PBKDF2',hash:'SHA-256',salt:cat(CLIP_PREFIX,salt),iterations:iter||CLIP_ITER},base,256));
 const mk=await crypto.subtle.importKey('raw',master,{name:'HMAC',hash:'SHA-256'},false,['sign']);
 const sub=async l=>new Uint8Array(await crypto.subtle.sign('HMAC',mk,new TextEncoder().encode(l)));
 return {enc:await sub('enc'),mac:await sub('mac'),auth:await sub('auth')};}
async function clipImport(raw){
 return {enc:await crypto.subtle.importKey('raw',raw.enc,'AES-CTR',false,['encrypt','decrypt']),
  mac:await crypto.subtle.importKey('raw',raw.mac,{name:'HMAC',hash:'SHA-256'},false,['sign','verify']),
  auth:await crypto.subtle.importKey('raw',raw.auth,{name:'HMAC',hash:'SHA-256'},false,['sign'])};}
async function clipDerive(pass,salt,iter){
 const pw=new TextEncoder().encode(pass.normalize('NFC'));
 const base=await crypto.subtle.importKey('raw',pw,'PBKDF2',false,['deriveBits']);
 const master=new Uint8Array(await crypto.subtle.deriveBits({name:'PBKDF2',hash:'SHA-256',salt:cat(CLIP_PREFIX,salt),iterations:iter||CLIP_ITER},base,256));
 const mk=await crypto.subtle.importKey('raw',master,{name:'HMAC',hash:'SHA-256'},false,['sign']);
 const sub=async l=>new Uint8Array(await crypto.subtle.sign('HMAC',mk,new TextEncoder().encode(l)));
 const [enc,mac,auth]=await Promise.all([sub('enc'),sub('mac'),sub('auth')]);
 return {enc:await crypto.subtle.importKey('raw',enc,'AES-CTR',false,['encrypt','decrypt']),
  mac:await crypto.subtle.importKey('raw',mac,{name:'HMAC',hash:'SHA-256'},false,['sign','verify']),
  auth:await crypto.subtle.importKey('raw',auth,{name:'HMAC',hash:'SHA-256'},false,['sign'])};}
async function clipSeal(k,plain,iv){
 iv=iv||crypto.getRandomValues(new Uint8Array(16));
 const ct=new Uint8Array(await crypto.subtle.encrypt({name:'AES-CTR',counter:iv,length:64},k.enc,plain));
 const body=cat(new Uint8Array([CLIP_V]),iv,ct);
 return cat(body,new Uint8Array(await crypto.subtle.sign('HMAC',k.mac,body)));}
async function clipUnseal(k,blob){
 if(blob.length<49||blob[0]!==CLIP_V)throw new Error('not a clipboard item');
 const body=blob.subarray(0,blob.length-32),tag=blob.subarray(blob.length-32);
 if(!await crypto.subtle.verify('HMAC',k.mac,tag,body))throw new Error('wrong passphrase, or the item was changed');
 return new Uint8Array(await crypto.subtle.decrypt({name:'AES-CTR',counter:blob.subarray(1,17),length:64},k.enc,blob.subarray(17,blob.length-32)));}
async function clipAnswer(k,challenge){
 return [...new Uint8Array(await crypto.subtle.sign('HMAC',k.auth,challenge))].map(b=>b.toString(16).padStart(2,'0')).join('');}
// CRYPTO-END
