import re,sys
d=sys.argv[1]
src=open(d+'/led.svg').read()
cols={'red':('#e01818','#ff6060'),'green':('#18c038','#70ff80'),'blue':('#1850f0','#70a8ff'),
 'yellow':('#ffd800','#fff070'),'orange':('#ff7a00','#ffb040'),'white':('#f2f2ee','#ffffff'),'purple':('#9a30e0','#d080ff')}
light=re.search(r'<g class="light".*?</g>',src,re.S).group(0)
key='fill="#ff2020"\n        opacity=".65"'
assert key in src
for n,(off,on) in cols.items():
    s=src.replace(light,'')
    s=s.replace(key,'fill="%s"\n        opacity="%s"'%(off,'.8' if n!='white' else '.55'))
    open(d+'/led_%s.svg'%n,'w').write(s)
    s=src.replace(light,'<ellipse cx="7.1" cy="9" rx="4.5" ry="5" fill="#ffffff" opacity=".55"/><ellipse cx="7.1" cy="9" rx="2.2" ry="2.4" fill="#ffffff"/>')
    s=s.replace(key,'fill="%s"\n        opacity=".97"'%on)
    open(d+'/led_%s_on.svg'%n,'w').write(s)
