const sharp=require('sharp');
const fs=require('node:fs');
const path=require('node:path');
const dir=process.argv[2]||path.join(__dirname,'../design');
(async()=>{
for(const f of fs.readdirSync(dir).filter(f=>f.endsWith('.svg'))){
const p=path.join(dir,f);
await sharp(p,{density:216}).png().toFile(p.replace('.svg','.png'));
await sharp(p,{density:72}).png().toFile(p.replace('.svg','-320.png'));
}
})();
