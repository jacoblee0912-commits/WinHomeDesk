from PIL import Image,ImageDraw
from pathlib import Path
w=Path('work');scale=4
# Original vector-style icon: two blue monitors joined by a green connection.
# Draw from primitives, not a copy of the Microsoft RDCMan artwork.
im=Image.new('RGBA',(256*scale,256*scale),(0,0,0,0));d=ImageDraw.Draw(im)
def rect(box,fill,r=0,outline=None,width=1):
 box=tuple(int(v*scale) for v in box)
 d.rounded_rectangle(box,radius=r*scale,fill=fill,outline=outline,width=width*scale)
def line(points,fill,width):d.line([(int(x*scale),int(y*scale)) for x,y in points],fill=fill,width=width*scale,joint='curve')
rect((16,20,164,126),'#23364D',12)
rect((25,29,155,109),'#278ACD',5)
rect((34,37,146,68),'#42B9EE',3)
rect((76,126,104,143),'#52718D',3)
rect((47,141,131,150),'#23364D',4)
rect((88,111,240,220),'#23364D',12)
rect((97,120,231,203),'#166DB6',5)
rect((106,129,222,161),'#3AA6E0',3)
rect((151,220,179,235),'#52718D',3)
rect((121,232,209,242),'#23364D',4)
line([(27,170),(27,204),(62,204)],'#19A97B',13)
line([(48,190),(63,204),(48,218)],'#19A97B',11)
im=im.resize((256,256),Image.Resampling.LANCZOS)
im.save(w/'SurfaceDesk.ico',sizes=[(16,16),(20,20),(24,24),(32,32),(48,48),(64,64),(128,128),(256,256)])
im.save(w/'icon017-preview.png')
(w/'SurfaceDesk-icon.svg').write_text('''<svg xmlns="http://www.w3.org/2000/svg" width="256" height="256" viewBox="0 0 256 256">
<rect x="16" y="20" width="148" height="106" rx="12" fill="#23364D"/><rect x="25" y="29" width="130" height="80" rx="5" fill="#278ACD"/>
<rect x="34" y="37" width="112" height="31" rx="3" fill="#42B9EE"/><path d="M76 126h28v17H76z" fill="#52718D"/><rect x="47" y="141" width="84" height="9" rx="4" fill="#23364D"/>
<rect x="88" y="111" width="152" height="109" rx="12" fill="#23364D"/><rect x="97" y="120" width="134" height="83" rx="5" fill="#166DB6"/>
<rect x="106" y="129" width="116" height="32" rx="3" fill="#3AA6E0"/><path d="M151 220h28v15h-28z" fill="#52718D"/><rect x="121" y="232" width="88" height="10" rx="4" fill="#23364D"/>
<path d="M27 170v34h35M48 190l15 14-15 14" fill="none" stroke="#19A97B" stroke-width="12" stroke-linejoin="round"/>
</svg>''',encoding='utf-8')
print('Original multi-resolution icon generated')
